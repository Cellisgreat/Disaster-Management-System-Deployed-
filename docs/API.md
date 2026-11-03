# REST API reference

Base URL: `http://localhost:8080`
Content type: `application/json` (requests **and** responses).
Authentication: HttpOnly cookie `dms_session`, issued by `POST /api/auth/login`.

Every error looks like:

```json
{ "error": "Human readable message", "code": "machine_code" }
```

| Status | When |
|---|---|
| 200 / 201 | success |
| 400 | validation failure (`invalid_input`, `invalid_area`, `invalid_service`, …) |
| 401 | not signed in / bad credentials (`unauthorized`, `invalid_credentials`) |
| 403 | wrong role, CSRF, admin creation attempt by a citizen (`forbidden`, `csrf`) |
| 404 | unknown id (`not_found`) |
| 409 | duplicate username or already-completed request (`username_taken`, `already_completed`) |
| 415 | body is not JSON |
| 429 | login throttled |
| 500 | persistence failure (`storage`) |

---

## Meta

### `GET /api/health`
```json
{"status":"ok","uptimeSeconds":142,"areas":20,"roads":38,"volunteers":20,"requests":6}
```

### `GET /api/meta`
Version, service list, roles, disclaimer text.

### `GET /api/public/summary`
Landing-page counters: volunteers, availableVolunteers, activeRequests, completedRequests,
peopleCovered, peopleAffected, areas, roads, services.

---

## Authentication

### `POST /api/auth/login`
```json
{ "username": "Admin", "password": "admin123" }
```
→ `200`
```json
{
  "authenticated": true,
  "user": { "username": "Admin", "role": "ADMIN", "isAdmin": true },
  "redirect": "/admin.html",
  "notice": "Your password hash was upgraded to PBKDF2-HMAC-SHA256."
}
```
`notice` only appears when a legacy hash was migrated.

### `POST /api/auth/register`
```json
{ "username": "citizen.x", "password": "secret123" }
```
→ `201`, sets the session cookie, `redirect: "/person.html"`.
An `admin: true` field is honoured **only** when the caller is a signed-in administrator;
otherwise the account is created as PERSON.

### `POST /api/auth/logout` → `{"ok":true}` and clears the cookie.

### `GET /api/auth/me`
```json
{ "authenticated": false }
```
or the same body as login (without `notice`).

### `POST /api/auth/password`
```json
{ "currentPassword": "admin123", "newPassword": "newSecret1" }
```
Invalidates all other sessions and issues a fresh one.

---

## City graph

### `GET /api/areas`
```json
{"areas":[
  {"id":0,"name":"Hospital","x":150,"y":295,"volunteers":2,"availableVolunteers":0,"openRoads":4}
]}
```

### `GET /api/roads`
```json
{"roads":[
  {"id":23,"fromAreaId":8,"toAreaId":17,"fromAreaName":"Port","toAreaName":"Dam",
   "distanceKm":4.5,"blocked":false,"status":"Open"}
]}
```

### `GET /api/graph`
`areas`, `roads`, `stats:{areas,roads,openRoads,blockedRoads,connected}`,
`canvas:{width,height}`.

### `GET /api/routes?from=8&to=17`  (session required)
```json
{
  "found": true,
  "from": { "id": 8, "name": "Port" },
  "to": { "id": 17, "name": "Dam" },
  "totalKm": 4.5,
  "pathText": "Port -> Dam",
  "path": [{"id":8,"name":"Port","cumulativeKm":0},{"id":17,"name":"Dam","cumulativeKm":4.5}],
  "steps": [{"roadId":23,"fromAreaId":8,"fromAreaName":"Port","toAreaId":17,
             "toAreaName":"Dam","distanceKm":4.5}],
  "roadIds": [23]
}
```
Blocked roads are excluded by the C++ Dijkstra implementation. When no open path exists,
`found` is `false` and a `message` explains why. Unknown ids → `400 invalid_area`.

---

## Requests

### `POST /api/requests` (session required)
```json
{ "service": "Medical Help", "areaId": 2, "peopleAffected": 60, "message": "Shelter needs medics" }
```
→ `201`
```json
{
  "message": "Request recorded and volunteers dispatched.",
  "request": { "id": 7, "service": "Medical Help", "areaName": "School",
               "peopleAffected": 60, "peopleCovered": 60, "peopleRemaining": 0,
               "progressPct": 100, "status": "Active", "owner": "citizen.x",
               "assignedVolunteerCount": 1, "assignedVolunteers": [ ... ] },
  "allocations": [
    { "volunteerId": 3, "name": "Dr. Mehta", "areaId": 0, "areaName": "Hospital",
      "service": "Medical Help", "capacity": 30, "distanceKm": 3.0, "local": false }
  ],
  "peopleRemaining": 0
}
```
`allocations` is ordered exactly as the engine chose them: volunteers already in the
affected area first, then by shortest travel distance.

### `GET /api/requests/my` → `{"requests":[ ... ]}` — only the caller's own requests.

### `GET /api/requests/{id}` (session) — own request, or any request for admins;
another citizen's request → `403`.

---

## Administrator: volunteers

### `GET /api/admin/volunteers`
```json
{"volunteers":[
  {"id":5,"name":"Priya Nair","areaId":8,"areaName":"Port","service":"Rescue",
   "capacity":20,"available":false,"activeRequestIds":[1],"assignmentCount":1,
   "status":"Assigned","assignedAreas":["Port"]}
]}
```

### `POST /api/admin/volunteers`
```json
{ "name": "Asha Kumari", "areaId": 5, "service": "Food", "capacity": 40 }
```
→ `201` with the created volunteer (persisted to `volunteers_db.csv`).

### `POST /api/admin/volunteers/reset`
```json
{ "reactivated": 0, "skippedAssigned": 10,
  "message": "Availability reset. 10 volunteer(s) kept assigned to active requests." }
```
Volunteers referenced by an ACTIVE request are never released by this operation.

---

## Administrator: requests

### `GET /api/admin/requests` → all requests, newest first.
### `GET /api/admin/requests/{id}` → single request with assigned volunteer details.
### `PATCH /api/admin/requests/{id}/complete`
```json
{ "message": "Request marked as completed. Assigned volunteers were released.",
  "releasedVolunteers": [{"id":16,"name":"Fatima Noor","available":true}],
  "request": { "id": 7, "status": "Completed", ... } }
```
Second call → `409 already_completed`.

---

## Administrator: roads

### `PATCH /api/admin/roads/{id}`
```json
{ "blocked": true }
```
`blocked` is optional — omit it to toggle. Response:
```json
{ "message": "Road blocked. Dijkstra will avoid it.",
  "road": { "id":23,"fromAreaName":"Port","toAreaName":"Dam","distanceKm":4.5,
            "blocked":true,"status":"Blocked" },
  "cityStats": { "openRoads": 37, "blockedRoads": 1, "connected": true } }
```
The change is written to `roads_db.csv` before the response is sent; a write failure rolls
the in-memory flag back and returns `500`.

---

## Administrator: accounts & dashboard

### `GET /api/admin/accounts`
```json
{"accounts":[{"username":"Admin","role":"ADMIN","isAdmin":true},
             {"username":"citizen.ravi","role":"PERSON","isAdmin":false}],
 "admins":1}
```
Password hashes are never serialised.

### `POST /api/admin/accounts`
```json
{ "username": "ops.admin", "password": "password9", "role": "ADMIN" }
```
→ `201`. `role` may be `ADMIN` or `PERSON`; only admins can call this endpoint.

### `GET /api/admin/dashboard`
```json
{
  "generatedAt": 1762267000,
  "me": { "username": "Admin", "role": "ADMIN", "isAdmin": true },
  "stats": {
    "volunteers": {"total":20,"available":13,"assigned":7,
                   "byService":{"Rescue":7,"Medical Help":6,"Food":7},
                   "availableByService":{"Rescue":5,"Medical Help":5,"Food":3}},
    "requests": {"total":6,"active":4,"completed":2,"underCovered":2,"unassigned":0,
                 "byService":{"Rescue":2,"Medical Help":2,"Food":2}},
    "people": {"affectedTotal":530,"coveredTotal":355,"affectedActive":425,
               "coveredActive":250,"coveragePct":66.98},
    "city": {"areas":20,"roads":38,"openRoads":38,"blockedRoads":0},
    "accounts": {"total":5,"admins":1}
  },
  "recentRequests": [ ... ],
  "volunteerOverview": {"total":20,"available":13,"assigned":7},
  "serviceDistribution": [ {"service":"Rescue","volunteers":7,"available":5,"requests":2}, ... ],
  "alerts": [
    {"severity":"warning","title":"Assistance still required",
     "message":"Medical Help request #2 in Residential Area covers 30 of 45 people (15 still waiting).",
     "requestId":2}
  ],
  "areas": [ {"id":0,"name":"Hospital"}, ... ]
}
```
Alert severities: `critical` (no volunteers assigned, network disconnected),
`warning` (under-covered request, no available volunteer of a service), `info` (blocked roads).
