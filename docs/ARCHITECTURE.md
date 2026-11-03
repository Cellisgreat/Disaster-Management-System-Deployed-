# Architecture

## 1. From console application to web application

The original project was a single-process, single-threaded console program: every menu
handler talked to the user with `cin`/`cout` and then mutated global arrays.

The web release keeps the same modules and algorithms but splits the code into three layers:

```
                 ┌──────────────────────────────────────────────┐
   browser       │  public/*.html  css/app.css  js/*.js         │
                 └───────────────┬──────────────────────────────┘
                                 │  fetch() JSON + HttpOnly cookie
                 ┌───────────────▼──────────────────────────────┐
   transport     │  src/api_routes.cpp   (cpp-httplib)           │
                 │  src/api_json.cpp     (serialisation)         │
                 └───────────────┬──────────────────────────────┘
                                 │  plain C++ calls, no I/O
                 ┌───────────────▼──────────────────────────────┐
   domain        │  CityGraph · DisasterManagement · Authentication│
                 │  PriorityQueue · crypto · SessionStore         │
                 └───────────────┬──────────────────────────────┘
                                 │  atomic file writes
                 ┌───────────────▼──────────────────────────────┐
   persistence   │  data/*.csv   (csv_utils.cpp)                 │
                 └──────────────────────────────────────────────┘
```

Rules that keep the layering honest:

* No domain module includes `httplib.h`, `json.hpp`, `<iostream>` or touches `cin`/`cout`.
* No frontend file computes a business result (no route calculation, no allocation) — it
  renders API responses.
* All console I/O lives in `menus.cpp` / `input_utils.cpp`, so `--cli` reuses the domain
  layer unchanged.

## 2. Why the console code had to be refactored

An HTTP handler cannot block on `readLine()`. Each interactive function became a
parameter-in / result-out service, for example:

```cpp
// before (console):
void assignVolunteersToRequest();      // prompts, mutates globals, prints

// after (shared by web + console):
RequestOutcome DisasterManagement::submitRequest(
        ServiceType service, int areaId, int peopleAffected,
        const std::string &message, const std::string &ownerUsername,
        const CityGraph &graph);
```

`RequestOutcome` carries everything the caller needs: the stored `Request`, the ordered
`AllocationEntry` list (who was dispatched, from where, how far), the uncovered headcount,
and an `OpResult` with `ok`/`error`/`code`. The web handler turns it into JSON; the console
menu prints it.

## 3. Request lifecycle (example: submit a disaster request)

1. `POST /api/requests` → `guardMutation()` rejects foreign `Origin` (CSRF),
   `requireSession()` resolves the cookie to a `Session` (401 if missing/expired).
2. Body is parsed by `nlohmann/json`; `readString`/`readInt` validate presence and type.
3. `store.mutex` is locked — **the whole read-modify-write cycle runs under one lock**.
4. `DataStore::dm.submitRequest(...)` runs the original algorithm:
   * candidates = volunteers of the requested service that are available and not already on
     an active request;
   * volunteers whose base area equals the affected area are tagged `local`;
   * everyone else gets a distance from `CityGraph::shortestDistance()` (Dijkstra);
   * sort: local first, then ascending distance;
   * allocate until the headcount is covered or candidates run out, flipping each
     volunteer to unavailable;
   * build the `Request`, push it into the array, rebuild the assignment index;
   * persist `requests_db.csv` **and** `volunteers_db.csv`; on any write failure the whole
     operation is rolled back in memory and `500` is returned.
5. `requestJson()` + `allocationJson()` serialise the result; the handler answers `201`.
6. The browser shows the confirmation panel with the allocated volunteers.

## 4. Threading and concurrency

`cpp-httplib` serves requests from a thread pool. Safety comes from:

| Mechanism | Protects |
|---|---|
| `DataStore::mutex` (`std::recursive_mutex`) | every read-modify-write cycle; serialises allocation, completion, road changes, registration |
| `SessionStore::mutex` | session map |
| `LoginThrottle::mutex` | failure counters |
| atomic file writes (`x.tmp` + `rename`) | CSV integrity if the process dies mid-save |
| assignment index (`volunteer id → active request count`) | prevents double booking, also repairs hand-edited CSVs at load time |

Because the whole allocation cycle happens under one lock, two simultaneous submissions can
never both "see" the same volunteer as available. `tests/acceptance.sh` fires eight parallel
requests and then asserts that no volunteer appears on two active requests.

Locking is coarse (one global lock). For this data size (hundreds of records) that is far
faster than any realistic load, and it removes an entire class of bugs; a finer-grained
design would be the next step if the system ever needed multiple writers per second.

## 5. Graph module notes

* Storage: `Area areas_[50]`, `Road roads_[200]`, and an adjacency list `Edge *adj_[50]`
  rebuilt from the road array (`rebuildAdjacency`), with both directions added because roads
  are undirected.
* Modified flag `adjDirty_` keeps the list in sync after loads.
* Dijkstra uses the **custom `PriorityQueue`** (sorted singly linked list, O(n) insert,
  O(1) extract-min). Stale queue entries are skipped with a `settled[]` array, and the search
  stops as soon as the destination is settled (optimal for non-negative weights).
* Blocked roads are filtered inside the relaxation loop (`if (!r || r->blocked) continue;`),
  so no separate graph copy is needed when a road is closed.
* Neighbours are sorted before relaxation so equal-distance ties resolve deterministically —
  route output is stable across runs, which makes the tests meaningful.

## 6. Schematic map layout

`areas_db.csv` has no coordinates, so `data/area_layout.csv` supplies presentation-only
`x,y` pairs (canvas 1000×700). The server includes them in `GET /api/areas` and
`GET /api/graph`; if the file is missing, `DataStore::layoutFor()` falls back to a
deterministic ring layout. The frontend never hardcodes positions, so adding an area (and a
layout row) immediately shows up on the map.

`CityMap` (public/js/city-map.js) draws three layers — edges, route, nodes — in an SVG
viewBox with pointer pan/zoom. Road labels are shown only on edges long enough to hold them,
which keeps the dense parts of the network readable.

## 7. Error philosophy

* Validation happens **in the domain layer** (e.g. `addVolunteer` rejects a 1-character name)
  so both the web API and the console get identical rules.
* The API layer maps domain error codes to HTTP statuses in one place
  (`result.code == "forbidden" ? 403 : ...`), and never leaks internals: storage errors are
  logged to stderr, the client receives "Could not save data.".
* The frontend always shows `data.error` from the server, never a made-up message.
