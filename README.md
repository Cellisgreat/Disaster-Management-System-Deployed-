# CivicShield — Disaster Management System (C++ backend + web frontend)

A city-scale disaster coordination platform. Citizens report emergencies, a **C++ dispatch
engine** allocates the nearest suitable volunteers using **Dijkstra's algorithm**, and
administrators manage the volunteer pool, requests and the road network from an operations
centre that runs in the browser.

> **The business logic is still C++.** The graph, the custom linked-list priority queue,
> Dijkstra, the volunteer-allocation rules and all CSV persistence live in the original C++
> modules. The website talks to them over a small REST API served by the same executable
> (`cpp-httplib` + `nlohmann/json`). Nothing is re-implemented in JavaScript — the frontend
> only renders what the backend returns.

---

## Table of contents

1. [What you get](#1-what-you-get)
2. [Demo credentials](#2-demo-credentials)
3. [Requirements](#3-requirements)
4. [Build & run on Windows (VS Code + MinGW-w64)](#4-build--run-on-windows-vs-code--mingw-w64)
5. [Build & run on Linux / macOS](#5-build--run-on-linux--macos)
6. [Using the website](#6-using-the-website)
7. [Architecture](#7-architecture)
8. [REST API reference](#8-rest-api-reference)
9. [Data & persistence](#9-data--persistence)
10. [Security model](#10-security-model)
11. [Legacy console interface](#11-legacy-console-interface)
12. [Tests](#12-tests)
13. [Files: what was modified, what is new](#13-files-what-was-modified-what-is-new)
14. [Troubleshooting](#14-troubleshooting)

---

## 1. What you get

| Area | Delivered |
|---|---|
| Backend | Modular C++17 HTTP server (`src/`), REST JSON API, session auth, CSV persistence |
| Preserved DSA | Adjacency-list city graph, custom sorted-linked-list priority queue, Dijkstra, array-based record storage, original allocation algorithm |
| Frontend | 5 pages: landing, login, register, admin console (8 views), citizen console (5 views) |
| City map | Interactive SVG of all 20 areas / 38 roads generated from the database, blocked roads dashed red, Dijkstra path highlighted |
| Admin features | Dashboard, volunteer management (search/sort/filter/add/reset), request triage + completion, road block/unblock, route finder, account creation, password change |
| Citizen features | Request submission with live allocation result, My Requests, route finder, password change |
| Tests | `tests/test_core.cpp` (123 assertions) + `tests/acceptance.sh` (50 HTTP checks) — all passing |

---

## 2. Demo credentials

| Role | Username | Password |
|---|---|---|
| Administrator | `Admin` | `admin123` |
| Citizen | `citizen.ravi` | `ravi1234` |
| Citizen | `citizen.meera` | `meera1234` |

The demo administrator's password is stored in the database with the **original educational
hash** on purpose: the first successful login demonstrates the automatic **migration to
PBKDF2-HMAC-SHA256**. Change the demo password from **Account settings → Change password**
(or use the *Reset demo password* button there to restore `admin123` for a demo).

Self-registration (`/register.html`) always creates a **PERSON** account. Only an
administrator can create further administrators (**Accounts → Create account**).

---

## 3. Requirements

| Component | Version | Notes |
|---|---|---|
| C++ compiler | MinGW-w64 `g++` 8.1+ (Windows) / g++ 9+ / clang 10+ | C++17 required |
| `cpp-httplib` | 0.18.3 | **bundled** in `third_party/httplib.h` — nothing to install |
| `nlohmann/json` | 3.11.3 | **bundled** in `third_party/json.hpp` — nothing to install |
| Threads | `-pthread` (POSIX) / `-lws2_32` (Windows) | already in the build files |
| Browser | any modern browser | Chrome, Edge, Firefox, Safari |
| Optional | `make`, `bash`, `python3` | only for `make test` / `make accept` |

Everything else is standard library. There is **no** Node.js, database server or package
manager in the runtime path.

> **C++ standard:** the original single-player console build compiled with `-std=c++11`.
> `cpp-httplib` 0.18 uses C++17 features, so the web build compiles with `-std=c++17`.
> The project's own code is still C++11-compatible apart from the HTTP layer
> (`std::mutex`, `std::recursive_mutex`, lambdas, `nullptr` — all C++11) — no C++20 is used.

---

## 4. Build & run on Windows (VS Code + MinGW-w64)

### 4.1 One-time setup

1. **Install MinGW-w64** (the `g++` compiler). Easiest option is MSYS2:
   * install MSYS2 → open *MSYS2 UCRT64* terminal → `pacman -S mingw-w64-ucrt-x86_64-gcc`
   * add `C:\msys64\ucrt64\bin` to your **PATH** (or use the standalone MinGW-w64 build
     and add `C:\mingw64\bin`).
   * verify in a **new** terminal: `g++ --version` → must print 8.1 or newer.
2. **Install VS Code** + the **C/C++ extension** (`ms-vscode.cpptools`).
3. Unzip the project anywhere, e.g. `C:\Projects\CivicShield`, and open that folder in VS Code.

### 4.2 Build

**Option A — build.bat (simplest)**

```bat
cd C:\Projects\CivicShield
build.bat
```

**Option B — VS Code task**
`Ctrl+Shift+B` → *Build (MinGW-w64 / g++)* (defined in `.vscode/tasks.json`).

**Option C — manual command** (this is the web-build equivalent of the original one-liner):

```bat
g++ -std=c++17 -O2 -Wall -Wextra -Ithird_party -Isrc ^
  src/csv_utils.cpp src/models.cpp src/crypto.cpp src/priority_queue.cpp ^
  src/city_graph.cpp src/disaster_management.cpp src/authentication.cpp ^
  src/sessions.cpp src/store.cpp src/api_json.cpp src/api_routes.cpp ^
  src/input_utils.cpp src/menus.cpp src/main.cpp ^
  -o DisasterManagementServer.exe -lws2_32 -lpthread
```

### 4.3 Run

```bat
run.bat
```
or
```bat
DisasterManagementServer.exe --port 8080
```
or in VS Code: *Terminal → Run Task → Run web server (port 8080)*.

You should see:

```
[store] loaded 20 areas, 38 roads (0 blocked), 20 volunteers, 6 requests, 5 accounts

  CivicShield Disaster Management System  (C++ backend)
  ----------------------------------------------------------
  Frontend : http://localhost:8080/
  API      : http://localhost:8080/api/health
  Data     : data
  Assets   : public
  Demo     : Admin / admin123  (change it from Account settings)
```

Open **http://localhost:8080/** in your browser. (Keep `data\` and `public\` next to the
executable — the server resolves them relative to the working directory. If you launch it
from another folder, pass `--data-dir` and `--public-dir`.)

### 4.4 Windows Firewall

The first run may show a firewall prompt (the server binds `0.0.0.0`). Allow it for
**private networks** only. If you want it reachable from your phone on the same Wi-Fi,
that is what enables it; otherwise you can block it and still use `localhost`.

---

## 5. Build & run on Linux / macOS

```bash
make            # builds ./DisasterManagementServer
make run        # starts it on port 8080
# or:
g++ -std=c++17 -O2 -Wall -Wextra -Ithird_party -Isrc src/*.cpp -o DisasterManagementServer -pthread
./DisasterManagementServer --port 8080
```

Command-line options:

| Option | Default | Purpose |
|---|---|---|
| `--port <n>` | `8080` | HTTP port |
| `--host <addr>` | `0.0.0.0` | bind address |
| `--data-dir <path>` | `data` | folder holding the CSV databases |
| `--public-dir <path>` | `public` | folder holding the frontend |
| `--cli` | off | run the **legacy console** application instead of the web server |
| `--check` | off | load the databases, print a summary, exit (CI smoke test) |
| `--verbose` | off | log every HTTP request |

---

## 6. Using the website

### Landing page (`/`)
Live counters and the real city graph pulled from `/api/public/summary` and `/api/graph`,
how-it-works, capabilities, the full network diagram and the demo credentials.

### Sign in / register (`/login.html`, `/register.html`)
Server-side authentication, role-based redirect (`ADMIN → /admin.html`,
`PERSON → /person.html`), friendly inline validation, login throttling, demo hint card.

### Administrator console (`/admin.html`)

| View | What it does |
|---|---|
| **Dashboard** | 10 live statistics (volunteers total/available/assigned, requests total/active/completed, people affected/covered, areas, roads, blocked roads), action-required alerts, availability donut, service-wise bars, recent requests, quick actions |
| **Volunteers** | Sortable + searchable table with service/area/availability filters, add-volunteer form (writes `volunteers_db.csv`), bulk availability reset that refuses to release volunteers on active requests |
| **Disaster requests** | Tabs (all/active/completed/needs assistance), coverage progress (`70 / 100`, "30 waiting"), full detail dialog incl. assigned volunteers, mark-completed with confirmation |
| **City network** | Interactive map, area inspector (click a node), blocked-road list, click any road to block/unblock |
| **Route finder** | Source/destination selects → Dijkstra result + route drawn on the map, segment-by-segment distances |
| **Road management** | Full road table with filters, block/unblock with confirmation dialogs |
| **Accounts** | List of accounts (no hashes exposed), create ADMIN/PERSON accounts |
| **Account settings** | Change password, reset demo password, backend status, demo disclaimer |

### Citizen console (`/person.html`)

Overview with your own statistics and city map · **Submit request** (service, area,
headcount, message) → shows request ID, coverage, remaining need and which volunteers were
dispatched (name, base area, distance) · **My requests** with live progress ·
**Shortest route** · **Account settings**.

Requests are scoped server-side: `/api/requests/my` returns only the signed-in account's
requests, and `/api/requests/{id}` refuses other citizens' records.

---

## 7. Architecture

```
  Browser (HTML/CSS/JS)
     │  fetch() + HttpOnly session cookie, JSON only
     ▼
  cpp-httplib  ──►  api_routes.cpp   (validation, authN/authZ, HTTP codes)
                         │
                         ▼
                    store.h/.cpp   (DataStore: one mutex, owns everything)
                    ├── CityGraph            city_graph.cpp        ← adjacency list + Dijkstra
                    │     └── PriorityQueue  priority_queue.cpp    ← custom linked list
                    ├── DisasterManagement   disaster_management.cpp ← allocation engine
                    ├── Authentication       authentication.cpp    ← roles, PBKDF2 verify/migrate
                    │     └── crypto.cpp     ← SHA-256 / HMAC / PBKDF2
                    └── SessionStore         sessions.cpp
                         │
                         ▼
                    csv_utils.cpp  ──►  data/*.csv  (atomic write: .tmp + rename)
```

* `api_json.cpp` is the only place that knows the JSON field names — so the API shape is
  defined once and used by every endpoint.
* Business modules contain **no** console or HTTP code; `menus.cpp` + `input_utils.cpp`
  hold all console I/O (used only by `--cli`), so the same functions serve both interfaces.
* One `std::recursive_mutex` in `DataStore` guards every read-modify-write cycle. That is
  what makes concurrent submissions safe (proved by the parallel test in
  `tests/acceptance.sh`).

See **`docs/ARCHITECTURE.md`** for the request lifecycle and threading model, and
**`docs/API.md`** for the complete endpoint reference.

---

## 8. REST API reference

All endpoints return JSON. Errors are `{"error": "...", "code": "..."}` with a matching
HTTP status (`400/401/403/404/409/415/429/500`). Authentication is the HttpOnly cookie
`dms_session`; roles are enforced **server-side** on every admin route.

| Method & path | Auth | Description |
|---|---|---|
| `GET /api/health` | – | liveness + counts |
| `GET /api/meta` | – | version, services, disclaimer |
| `GET /api/public/summary` | – | landing-page counters |
| `POST /api/auth/login` | – | `{username,password}` → session cookie |
| `POST /api/auth/register` | – | self-registration (always PERSON) |
| `POST /api/auth/logout` | session | destroys the session |
| `GET /api/auth/me` | – | `{authenticated, user, redirect}` |
| `POST /api/auth/password` | session | change own password |
| `GET /api/areas` | – | 20 areas (+ schematic x/y, volunteer counts) |
| `GET /api/roads` | – | 38 roads with open/blocked state |
| `GET /api/graph` | – | areas + roads + network stats + canvas size |
| `GET /api/routes?from=ID&to=ID` | session | Dijkstra result (blocked roads excluded) |
| `POST /api/requests` | session | submit request → allocation result |
| `GET /api/requests/my` | session | own requests only |
| `GET /api/requests/{id}` | session | own request (admins: any) |
| `GET /api/admin/volunteers` | ADMIN | all volunteers + assignment info |
| `POST /api/admin/volunteers` | ADMIN | add volunteer (persists to CSV) |
| `POST /api/admin/volunteers/reset` | ADMIN | reset availability (protects active assignments) |
| `GET /api/admin/requests` | ADMIN | all requests |
| `GET /api/admin/requests/{id}` | ADMIN | request detail |
| `PATCH /api/admin/requests/{id}/complete` | ADMIN | complete + release volunteers |
| `PATCH /api/admin/roads/{id}` | ADMIN | `{blocked:true|false}` (default: toggle) |
| `GET /api/admin/accounts` | ADMIN | account list (never hashes) |
| `POST /api/admin/accounts` | ADMIN | create ADMIN or PERSON account |
| `GET /api/admin/dashboard` | ADMIN | all statistics, alerts, recent requests |

Example:

```bash
curl -c cookies.txt -H 'Content-Type: application/json' \
     -d '{"username":"Admin","password":"admin123"}' http://localhost:8080/api/auth/login

curl -b cookies.txt "http://localhost:8080/api/routes?from=8&to=17"
# {"found":true,"pathText":"Port -> Dam","totalKm":4.5, ... }
```

---

## 9. Data & persistence

| File | Contents |
|---|---|
| `data/areas_db.csv` | `id,name` — 20 areas |
| `data/roads_db.csv` | `id,from_area_id,to_area_id,distance_km,blocked` — 38 roads |
| `data/volunteers_db.csv` | `id,name,area_id,service,capacity,available` |
| `data/requests_db.csv` | `id,service,area_id,people_affected,people_covered,assigned_volunteers,status,message,owner_username` |
| `data/users_db.csv` | `username,password_hash,role` |
| `data/area_layout.csv` | `id,x,y` — schematic map coordinates (presentation only) |

* **Atomic writes.** Every save writes `<file>.tmp` then renames over the real file, so an
  interrupted save can never truncate a database.
* **Backward compatible.** The new `owner_username` column is *appended last*. Rows written
  by the original 8-column build still load (owner = empty → visible to admins only), and
  loading repairs availability (`a volunteer listed on an ACTIVE request is never
  "available"`).
* **Sequential IDs.** New volunteers/requests get `max(id)+1`, so deleting rows by hand never
  causes collisions.

### How to verify persistence yourself

```bash
# 1. note the current counts
curl -b cookies.txt http://localhost:8080/api/admin/dashboard | grep -o '"total":[0-9]*'

# 2. add a volunteer in the UI (Volunteers → Add volunteer) or via the API
# 3. stop the server (Ctrl+C), start it again
# 4. log in again (sessions are in-memory by design) and look at the volunteer:
```

The volunteer is still there, and `data/volunteers_db.csv` contains the new row — the
databases, not the process, are the source of truth.

---

## 10. Security model

| Concern | Implementation |
|---|---|
| Password storage | PBKDF2-HMAC-SHA256, 120 000 iterations, 16-byte random salt, `pbkdf2$sha256$iter$salt$hash` |
| Legacy accounts | Automatically re-hashed after the first successful login |
| Sessions | 256-bit random token, HttpOnly + SameSite=Strict cookie, server-side store, 8 h TTL |
| Authorisation | `requireAdmin()` on every admin route — hiding buttons is never the control |
| Role escalation | `POST /api/auth/register` ignores `admin:true` unless the caller is a signed-in admin |
| Login throttling | 8 failures / 5 min per (IP, username) → `429` |
| CSRF | Mutating requests with a foreign `Origin` header are rejected (`403`) |
| Input validation | Server-side range/format checks with machine-readable `code`s; all text is length-capped and control characters stripped |
| CSV injection | Fields are quoted/escaped on write, newlines removed |
| Session hygiene | Password change invalidates every other session |
| Data leaks | Password hashes are never serialised; API errors never reveal whether a username exists |
| Headers | `X-Content-Type-Options: nosniff`, `Referrer-Policy: no-referrer` |

This is an **educational demonstration system**. It is not hardened for internet exposure
(no TLS, no audit log, in-memory sessions) and must never be presented as a real emergency
dispatch service. The UI states this on the landing page, both sidebars and the settings
pages.

---

## 11. Legacy console interface

The original terminal menus are preserved and still exercise exactly the same C++ code:

```bash
./DisasterManagementServer --cli
```

Admin menu: view volunteers / requests, shortest route, add volunteer, view roads, block and
unblock roads, reset availability, complete a request, create an admin account, change
password. Citizen menu: submit request, shortest route, my requests, change password.

---

## 12. Tests

### C++ core suite (no HTTP)

```bash
make test                        # or:
g++ -std=c++17 -Ithird_party -Isrc tests/test_core.cpp src/csv_utils.cpp src/models.cpp \
    src/crypto.cpp src/priority_queue.cpp src/city_graph.cpp src/disaster_management.cpp \
    src/authentication.cpp -o build/test_core
./build/test_core data
```

Covers: priority-queue ordering/FIFO, CSV quoting and tolerance, SHA-256 test vector,
PBKDF2 verify + legacy migration, 20-area/38-road graph, Dijkstra distances, blocked-road
exclusion, unreachability, **local-first then nearest allocation**, double-booking
prevention, reset protection, completion releasing volunteers, insufficient capacity,
owner scoping, statistics, legacy-CSV compatibility.

**Result: 123 passed, 0 failed.**

### HTTP acceptance suite

```bash
make accept        # or: bash tests/acceptance.sh [--port 8090]
```

Builds if needed, copies the databases to a temporary folder (your real data is untouched),
starts the server on a free port, and runs the 10 acceptance scenarios from the brief plus a
parallel double-booking test. **Result: 50 passed, 0 failed.**

---

## 13. Files: what was modified, what is new

### Original modules kept and refactored (console I/O removed, structured results returned)

| File | Change |
|---|---|
| `src/city_graph.h/.cpp` | **Preserved**: area/road arrays, adjacency list, Dijkstra. Added: `loadAreas/loadRoads/saveAreas/saveRoads`, structured `RouteResult`, `shortestDistance()`, `describePath()`, blocked/connectivity stats. Adjacency list rebuilt on load; blocked roads filtered in the Dijkstra relaxation step. |
| `src/priority_queue.h/.cpp` | **Preserved** as-is in behaviour (sorted singly linked list). Added: `peek`, `size`, `clear`, non-copyable guard. |
| `src/disaster_management.h/.cpp` | **Allocation algorithm unchanged**. Refactored to parameter-in/result-out (`submitRequest`, `completeRequest`, `addVolunteer`, `resetVolunteerAvailability`, `computeStats`). Added: `ownerUsername` on requests, `RequestOutcome`/`AllocationEntry` details, assignment index preventing double booking, rollback on save failure. |
| `src/authentication.h/.cpp` | Refactored to `login/registerUser/changePassword` returning `AuthResult`/`OpResult`. Added: PBKDF2 hashing, legacy hash migration, username/password validation, admin creation rules, demo-admin seeding. |
| `src/menus.h/.cpp` | **Kept** as the legacy console UI (`--cli`), rewritten to call the same service functions instead of duplicating logic. |
| `src/input_utils.h/.cpp` | **Kept** for the console build (robust `readIntInRange`, confirmations, etc.). |
| `src/config.h` | Extended with capacity limits, DB file names, session settings and a `RuntimeConfig` (data dir, public dir, host, port). |

### New backend files

| File | Purpose |
|---|---|
| `src/main.cpp` | Entry point: option parsing, `--cli` / `--check` / web server modes |
| `src/models.h/.cpp` | `Area`, `Road`, `Volunteer`, `Request`, `User`, enums + tolerant CSV parsing |
| `src/csv_utils.h/.cpp` | RFC-style CSV parse/escape, atomic write, tolerant number parsing |
| `src/crypto.h/.cpp` | SHA-256, HMAC, PBKDF2, legacy hash, constant-time compare, token generation |
| `src/sessions.h/.cpp` | Thread-safe in-memory session store |
| `src/store.h/.cpp` | `DataStore`: owns graph/DM/auth/sessions, the global mutex, map layout |
| `src/api_json.h/.cpp` | Model → JSON serialisation (single source of API shape) |
| `src/api_routes.h/.cpp` | Every REST route: validation, authN/authZ, status codes, static file serving, login throttle |
| `third_party/httplib.h`, `third_party/json.hpp` | Vendored header-only dependencies (pinned versions) |

### New frontend files

```
public/index.html  login.html  register.html  admin.html  person.html
public/css/app.css
public/js/core.js          API client, session guard, toasts, dialogs, formatting
public/js/city-map.js      interactive SVG city-network renderer
public/js/route-finder.js  shared shortest-route widget
public/js/auth.js          login + registration
public/js/landing.js       landing page
public/js/admin.js         admin console (hash router, 8 views)
public/js/person.js        citizen console (5 views)
```

### New tooling, tests and docs

```
Makefile  build.bat  run.bat  .gitignore  .vscode/{tasks,launch,c_cpp_properties}.json
tests/test_core.cpp  tests/acceptance.sh  tests/check_json.py
tools/gen_seed.py  tools/gen_users.py     (regenerate the seed CSVs)
docs/API.md  docs/ARCHITECTURE.md  docs/CHANGES.md
docs/SCREENSHOTS.md  docs/screenshots/*.jpg     (captured from the running server)
```

---

## 14. Screenshots

See **`docs/SCREENSHOTS.md`** for the landing page, both consoles, the interactive city map,
a Dijkstra route result, a request detail dialog and the mobile/tablet layouts.

**Interface theme.** The UI is styled as an emergency operations centre (see `docs/CHANGES.md`,
"Release 2 — visual theme"): a crimson command bar carrying the CivicShield brand, the page
title and a live-incident pill; a navy navigation sidebar whose active item is a red pill;
a white workspace; and amber (pending) / orange (in progress) / green (completed) status
accents. All of it lives in `public/css/app.css` — there are no external fonts, icon packs or
CDNs, so the app also renders correctly offline.

---

## 15. Troubleshooting

| Symptom | Fix |
|---|---|
| `g++: command not found` | MinGW-w64 is not on `PATH`; add `C:\msys64\ucrt64\bin` (or `C:\mingw64\bin`) and reopen the terminal |
| `fatal error: httplib.h: No such file` | build without `-Ithird_party`; add it back, or run `build.bat` |
| `undefined reference to WSAStartup` / socket errors | you are on Windows without `-lws2_32 -lpthread` |
| `Could not start the HTTP server on 0.0.0.0:8080` | port busy — use `--port 8081` or stop the other process (`netstat -ano | findstr :8080`) |
| Page loads but shows no data | the frontend is there but the API is unreachable: check `http://localhost:8080/api/health` |
| "Cannot reach the C++ backend" toast | the server stopped — restart it and reload |
| Port 8080 in use by VS Code / another app | `DisasterManagementServer.exe --port 8091` |
| Data looks stale after editing CSVs by hand | restart the server: the CSVs are loaded at start-up |
| `[city_graph] roads: skipped N malformed row(s)` | a road row has an unknown area id, a non-positive distance or a self-loop; fix or delete that row |
| Forgot the demo admin password | stop the server, replace the `Admin` line in `data/users_db.csv` with `Admin,001ae6d543a006d1,ADMIN` (the original hash of `admin123`), restart and sign in |

---

*CivicShield — educational capstone project. Not a certified emergency dispatch service; in a
real emergency contact your local emergency number.*


---

## Cloud deployment (Docker: Render / Railway)

The repository includes a Dockerfile for Linux deployment. The container reads the
platform-provided `PORT` variable, binds to `0.0.0.0`, and stores CSV files in
`DMS_DATA_DIR` (default `/data`). On first startup, missing CSV files are copied from
the bundled seed data. Existing files in the mounted volume are never overwritten.

### Build and smoke-test locally

```bash
docker build -t civicshield .
docker run --rm -p 8080:8080 civicshield
```

Open `http://localhost:8080/` and check `http://localhost:8080/api/health`.

To test persistence locally, create a named volume:

```bash
docker volume create civicshield-data
docker run --name civicshield -p 8080:8080 \
  -v civicshield-data:/data civicshield
```

### Render

1. Push the project directory (the directory containing `Dockerfile`) to GitHub.
2. In Render, choose **New → Web Service**, connect the repository, and select **Docker** as the runtime.
3. Set the root directory to the project directory if the Dockerfile is not at repository root.
4. Add a persistent disk mounted at `/data` (for example, 1 GB).
5. Deploy. Render supplies `PORT`; the container uses it automatically.
6. Open the generated `onrender.com` URL.

A Render service without a persistent disk will lose CSV changes when its instance is replaced.

### Railway

1. Push the project directory (the directory containing `Dockerfile`) to GitHub.
2. Create a Railway project and deploy from the GitHub repository.
3. Ensure the service's Dockerfile path points to this project's `Dockerfile`.
4. Add a Railway Volume and mount it at `/data`.
5. Deploy. Railway supplies `PORT`; the container uses it automatically.
6. Open the generated public domain.

A Railway deployment without a mounted volume uses ephemeral container storage.

### Deployment configuration

| Variable | Default | Purpose |
|---|---|---|
| `PORT` | `8080` | HTTP port supplied by the hosting platform |
| `HOST` | `0.0.0.0` | Bind address |
| `DMS_DATA_DIR` | `/data` | Directory containing persistent CSV files |

The container serves the unchanged frontend from `/app/public`. The original Windows
`build.bat` and local development workflow remain available.
