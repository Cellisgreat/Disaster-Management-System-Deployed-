# Change log: original console project → web release

## Preserved without behavioural change

| What | Where |
|---|---|
| Adjacency-list city graph | `src/city_graph.cpp` (`Edge`, `adj_`, `rebuildAdjacency`) |
| Custom sorted linked-list priority queue | `src/priority_queue.cpp` (used by Dijkstra) |
| Dijkstra shortest path, blocked roads excluded | `src/city_graph.cpp::shortestPath` |
| Array-based storage (`Area[]`, `Road[]`, `Volunteer[]`, `Request[]`, `User[]`) | every domain module |
| Volunteer allocation: local first → nearest by Dijkstra → until covered | `src/disaster_management.cpp::submitRequest` |
| CSV databases and their formats | `data/*.csv` (request file gains one appended column) |
| Admin + citizen capability sets | `admin.html` / `person.html` / `menus.cpp` |

## Refactored (interface changed, logic kept)

1. **Console I/O removed from business modules.** Every interactive handler became a
   function that takes parameters and returns a structured result (`OpResult`,
   `AuthResult`, `RequestOutcome`, `RouteResult`, `ResetOutcome`, `SystemStats`).
2. **Persistence made explicit.** Modules gained `load*/save*` methods with atomic writes and
   tolerant parsing; paths are configurable at runtime (`--data-dir`).
3. **`Authentication`** now exposes `login`, `registerUser`, `changePassword`; validation
   helpers are static so the API can produce matching messages.
4. **`CityGraph`** returns `RouteResult` (node sequence, per-segment road ids, cumulative
   distances) instead of printing, and gained `shortestDistance`, `describePath`,
   `blockedRoadCount`, `isConnected`.
5. **`menus.cpp` / `input_utils.cpp`** kept as the legacy CLI (`--cli`), now delegating to the
   refactored services instead of holding duplicate logic.
6. **`config.h`** extended (capacity limits, DB names, session TTL, `RuntimeConfig`).

## Added — backend

* `main.cpp` (web server + CLI + `--check`), `models.*`, `csv_utils.*`, `crypto.*` (SHA-256 /
  HMAC / PBKDF2 / legacy hash), `sessions.*`, `store.*`, `api_json.*`, `api_routes.*`.
* Full REST API (see `docs/API.md`) with validation, authentication, role checks, JSON
  errors and correct status codes.
* Vendored `third_party/httplib.h` (cpp-httplib 0.18.3) and `third_party/json.hpp`
  (nlohmann/json 3.11.3).

## Added — data model

* `Request::ownerUsername` — new **last** column in `requests_db.csv`; legacy 8-column rows
  still load (owner empty = visible to administrators only).
* `Volunteer` availability is repaired on load: anyone referenced by an ACTIVE request is
  forced unavailable, so a hand-edited CSV cannot create a double booking.
* Assignment index (`volunteer id → number of active requests`) used by the reset and
  completion safeguards.
* `data/area_layout.csv` — presentation-only schematic coordinates for the map.

## Added — security

* PBKDF2-HMAC-SHA256 password hashing (120 000 iterations, random salt) replacing the
  unsalted educational hash; automatic migration of existing accounts on successful login.
* Server-side sessions with 256-bit tokens in HttpOnly + SameSite=Strict cookies, 8 hour TTL,
  invalidation on password change.
* Role enforcement on every admin endpoint; self-registration can never request ADMIN.
* Login throttling, CSRF origin check, input validation/sanitisation, CSV injection escaping,
  `nosniff` / `no-referrer` headers, no password hashes in any response.

## Added — frontend

Landing page, login, registration, administrator console (dashboard, volunteers, requests,
city network, route finder, road management, accounts, settings) and the citizen console
(overview, submit request, my requests, route finder, settings) — all responsive, with a
custom interactive SVG city map, toasts, dialogs and accessible tables.

## Added — tooling, tests, docs

`Makefile`, `build.bat`, `run.bat`, `.vscode/*` (build/run/debug tasks), `tools/gen_seed.py`,
`tools/gen_users.py`, `tests/test_core.cpp` (123 assertions), `tests/acceptance.sh`
(50 HTTP checks incl. a parallel double-booking test), `docs/API.md`,
`docs/ARCHITECTURE.md`, `docs/CHANGES.md`, screenshots in `docs/screenshots/`.

## Release 2 — visual theme (no behavioural change)

The interface was reskinned to an "Emergency Command" operations-centre look. Only
`public/css/app.css` and the markup of the two console shells (`public/admin.html`,
`public/person.html`) changed; no C++, JavaScript or API surface was touched, and every
existing selector/id (`#refresh-btn`, `#page-title`, `#page-crumbs`, `#live-indicator`,
`data-logout`, `.hamburger`, …) is still present.

| Element | Before | Now |
|---|---|---|
| Console header | navy top bar inside the content column | full-width crimson command bar (`--red #d8261c`) holding the brand, page title, a white "Live operations / Monitoring active" incident pill, refresh action and avatar |
| Navigation | navy sidebar with navy active item | navy gradient sidebar (`--navy-950…700`) with a red gradient pill for the active item and amber alert counters |
| Workspace | light grey | white cards on a near-white canvas |
| Status colours | blue / green / amber | amber = pending, orange = in progress/assigned, green = completed/available, crimson = critical/blocked |
| Landing page | white nav bar | crimson nav bar, warm-tinted hero, tri-coloured feature icons and step markers |
| Map | blue route highlight | navy route highlight, crimson affected areas/blocked roads, green destination |
| Mobile | sidebar drawer with grid-column overflow | single column (`minmax(0,1fr)`), drawer starts below the command bar, icon-only actions under 560 px |

## Deliberate non-goals

* No database engine, no JavaScript business logic, no mock data.
* Sessions are in memory (a restart signs everyone out) — the CSV files remain the single
  source of truth for data, which keeps the project deployable as one executable plus files.
* No TLS: this is a demonstration system, documented as such in the UI and the README.
