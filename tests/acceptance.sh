#!/usr/bin/env bash
# ============================================================================
#  tests/acceptance.sh  --  end-to-end acceptance tests over the real HTTP API
# ----------------------------------------------------------------------------
#  Runs against a *throw-away copy* of the CSV databases, so your real data is
#  never touched. Needs: bash, curl, python3 (used only to read JSON).
#
#  Usage (from the project root):
#      bash tests/acceptance.sh                  # builds if necessary
#      bash tests/acceptance.sh --port 8090
#
#  Maps to the acceptance criteria:
#      Test 1  authentication + role isolation
#      Test 2  volunteer registration
#      Test 3  disaster request + automatic allocation
#      Test 4  local-first / nearest-first allocation
#      Test 5  shortest path comes from C++ Dijkstra
#      Test 6  road blocking changes routing
#      Test 7  request completion releases volunteers
#      Test 8  persistence across a restart
#      Test 9  data consistency (no double assignment, idempotent operations)
# ============================================================================
set -u

PORT=8123
BIN=./DisasterManagementServer
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RUN_DIR="$(mktemp -d)"
COOKIE_ADMIN="$RUN_DIR/admin.cookies"
COOKIE_PERSON="$RUN_DIR/person.cookies"
SERVER_LOG="$RUN_DIR/server.log"
PID=""

PASS=0
FAIL=0

while [ $# -gt 0 ]; do
  case "$1" in
    --port) PORT="$2"; PORT_EXPLICIT=1; shift 2 ;;
    *) echo "unknown option: $1"; exit 2 ;;
  esac
done

# ------------------------------------------------------------------ helpers --
ok()   { PASS=$((PASS+1)); printf "  \033[32mPASS\033[0m %s\n" "$1"; }
bad()  { FAIL=$((FAIL+1)); printf "  \033[31mFAIL\033[0m %s\n" "$1"; }
head1() { printf "\n\033[1m== %s ==\033[0m\n" "$1"; }

# json_get <file> <python expression using `d`>   (single quotes around the
# expression are optional and stripped before evaluation)
json_get() {
  local file="$1" expr="$2"
  expr="${expr#\'}"; expr="${expr%\'}"
  expr="$(printf '%s' "$expr" | tr '\n\r' '  ')"   # eval() dislikes indented newlines
  python3 -c "import json,sys;d=json.load(open(sys.argv[1]));print(eval(sys.argv[2]))" \
    "$file" "$expr" 2>/dev/null
}

expect_eq() {  # expect_eq expected actual message
  if [ "$1" = "$2" ]; then ok "$3"; else bad "$3 (expected '$1', got '$2')"; fi
}

start_server() {
  [ -f "$SERVER_LOG" ] && mv "$SERVER_LOG" "$SERVER_LOG.prev"
  # "exec" makes the subshell become the server, so $PID is the real process
  ( cd "$ROOT" && exec "$BIN" --port "$PORT" --data-dir "$RUN_DIR/data" \
      --public-dir "$ROOT/public" >> "$SERVER_LOG" 2>&1 ) &
  PID=$!
  for _ in $(seq 1 150); do
    if curl -s --max-time 1 "$BASE/api/health" | grep -q '"status":"ok"'; then return 0; fi
    if ! kill -0 "$PID" 2>/dev/null; then
      echo "server exited during start-up; log:"; cat "$SERVER_LOG"; exit 1
    fi
    sleep 0.1
  done
  echo "server did not become healthy; log:"; cat "$SERVER_LOG"; exit 1
}

stop_server() {
  if [ -n "$PID" ] && kill -0 "$PID" 2>/dev/null; then
    kill -TERM "$PID" 2>/dev/null
    for _ in $(seq 1 60); do kill -0 "$PID" 2>/dev/null || break; sleep 0.1; done
    kill -9 "$PID" 2>/dev/null
    wait "$PID" 2>/dev/null
  fi
  PID=""
}

cleanup() { stop_server; rm -rf "$RUN_DIR"; }
trap cleanup EXIT

# pick a free TCP port unless one was supplied explicitly
port_is_free() { python3 -c "import socket,sys
s=socket.socket(); s.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
try:
    s.bind(('127.0.0.1',int(sys.argv[1]))); print('free')
except OSError:
    print('busy')
finally:
    s.close()" "$1"; }

if [ -z "${PORT_EXPLICIT:-}" ]; then
  for candidate in $(seq 8123 8200); do
    if [ "$(port_is_free "$candidate")" = "free" ]; then PORT="$candidate"; break; fi
  done
fi
BASE="http://127.0.0.1:$PORT"

# api <method> <path> <cookie-file> [json-body] -> HTTP status on stdout,
# response JSON written to $RUN_DIR/last.json
api() {
  local method="$1" path="$2" cookies="$3" body="${4:-}"
  if [ -n "$body" ]; then
    curl -s -X "$method" -b "$cookies" -c "$cookies" -H 'Content-Type: application/json' \
      -d "$body" -o "$RUN_DIR/last.json" -w '%{http_code}' "$BASE$path"
  else
    curl -s -X "$method" -b "$cookies" -c "$cookies" \
      -o "$RUN_DIR/last.json" -w '%{http_code}' "$BASE$path"
  fi
}

# ------------------------------------------------------------------- set-up --
cd "$ROOT" || exit 1   # helper paths below are relative to the project root
if [ ! -x "$BIN" ]; then
  echo "building $BIN ..."
  g++ -std=c++17 -O2 -Ithird_party -Isrc src/*.cpp -o "$BIN" -pthread || exit 1
fi

mkdir -p "$RUN_DIR/data"
cp data/areas_db.csv data/roads_db.csv data/volunteers_db.csv \
   data/requests_db.csv data/users_db.csv data/area_layout.csv "$RUN_DIR/data/"
: > "$COOKIE_ADMIN"; : > "$COOKIE_PERSON"

start_server
echo "server started on $BASE (scratch data: $RUN_DIR/data)"

# ============================================================================
head1 "Test 1 — Authentication and role isolation"

CODE=$(api POST /api/auth/register "$COOKIE_PERSON" \
  '{"username":"acc.tester","password":"tester123"}')
expect_eq 201 "$CODE" "a citizen can self-register"
[ "$(json_get "$RUN_DIR/last.json" "'d[\"user\"][\"role\"]'")" = "PERSON" ] \
  && ok "self-registration always yields the PERSON role" \
  || bad "self-registration role should be PERSON"

CODE=$(api GET /api/admin/volunteers "$COOKIE_PERSON")
expect_eq 403 "$CODE" "a PERSON cannot read admin endpoints"

CODE=$(api POST /api/admin/volunteers "$COOKIE_PERSON" \
  '{"name":"Sneaky","areaId":1,"service":"Rescue","capacity":10}')
expect_eq 403 "$CODE" "a PERSON cannot create volunteers"

CODE=$(api PATCH /api/admin/roads/0 "$COOKIE_PERSON" '{"blocked":true}')
expect_eq 403 "$CODE" "a PERSON cannot block roads"

CODE=$(api POST /api/auth/register "$COOKIE_PERSON" \
  '{"username":"evil.admin","password":"evil1234","admin":true}')
expect_eq 403 "$CODE" "a PERSON cannot register an administrator"

CODE=$(api POST /api/auth/login "$COOKIE_PERSON" '{"username":"acc.tester","password":"nope"}')
expect_eq 401 "$CODE" "a wrong password is rejected"

CODE=$(api POST /api/auth/login "$COOKIE_ADMIN" '{"username":"Admin","password":"admin123"}')
expect_eq 200 "$CODE" "the documented demo administrator can sign in"
grep -q 'pbkdf2\$sha256' "$RUN_DIR/data/users_db.csv" \
  && ok "the legacy password hash was migrated to PBKDF2 on first login" \
  || bad "expected the demo password hash to be migrated"

CODE=$(api GET /api/auth/me "$COOKIE_ADMIN")
expect_eq 200 "$CODE" "GET /api/auth/me works for a live session"
[ "$(json_get "$RUN_DIR/last.json" "'d[\"user\"][\"isAdmin\"]'")" = "True" ] \
  && ok "the session reports administrator privileges" || bad "admin flag missing"

# ============================================================================
head1 "Test 2 — Volunteer registration (admin)"

BEFORE=$(api GET /api/admin/dashboard "$COOKIE_ADMIN" >/dev/null; json_get "$RUN_DIR/last.json" "'d[\"stats\"][\"volunteers\"][\"total\"]'")
CODE=$(api POST /api/admin/volunteers "$COOKIE_ADMIN" \
  '{"name":"Acceptance Medic","areaId":2,"service":"Medical Help","capacity":60}')
expect_eq 201 "$CODE" "admin can add a volunteer"
NEW_ID=$(json_get "$RUN_DIR/last.json" "'d[\"volunteer\"][\"id\"]'")
grep -q "Acceptance Medic" "$RUN_DIR/data/volunteers_db.csv" \
  && ok "the volunteer is written to volunteers_db.csv (id $NEW_ID)" \
  || bad "volunteer missing from the CSV database"
AFTER=$(api GET /api/admin/dashboard "$COOKIE_ADMIN" >/dev/null; json_get "$RUN_DIR/last.json" "'d[\"stats\"][\"volunteers\"][\"total\"]'")
expect_eq "$((BEFORE+1))" "$AFTER" "the dashboard total increased by exactly one"

CODE=$(api POST /api/admin/volunteers "$COOKIE_ADMIN" \
  '{"name":"X","areaId":999,"service":"Laser","capacity":-4}')
expect_eq 400 "$CODE" "invalid volunteer input is rejected"

# ============================================================================
head1 "Test 3 — Disaster request triggers automatic allocation"

CODE=$(api POST /api/requests "$COOKIE_PERSON" \
  '{"service":"Medical Help","areaId":2,"peopleAffected":60,"message":"Acceptance test request"}')
expect_eq 201 "$CODE" "a citizen can submit a request"
REQ_ID=$(json_get "$RUN_DIR/last.json" "'d[\"request\"][\"id\"]'")
N_ASSIGNED=$(json_get "$RUN_DIR/last.json" "'len(d[\"allocations\"])'")
[ "$N_ASSIGNED" -ge 1 ] && ok "the allocation engine assigned $N_ASSIGNED volunteer(s)" \
  || bad "no volunteer was allocated"
COVERED=$(json_get "$RUN_DIR/last.json" "'d[\"request\"][\"peopleCovered\"]'")
[ "$COVERED" -gt 0 ] && ok "coverage recorded: $COVERED people" || bad "coverage was not recorded"
[ "$(json_get "$RUN_DIR/last.json" "'d[\"request\"][\"owner\"]'")" = "acc.tester" ] \
  && ok "the request is linked to the signed-in citizen" || bad "owner not stored"

VOL_ID=$(json_get "$RUN_DIR/last.json" "'d[\"allocations\"][0][\"volunteerId\"]'")
api GET /api/admin/volunteers "$COOKIE_ADMIN" >/dev/null
[ "$(json_get "$RUN_DIR/last.json" "'[v for v in d[\"volunteers\"] if v[\"id\"]==$VOL_ID][0][\"available\"]'")" = "False" ] \
  && ok "the allocated volunteer is now marked unavailable" \
  || bad "allocated volunteer is still available"

CODE=$(api GET /api/requests/my "$COOKIE_PERSON")
expect_eq 200 "$CODE" "the citizen can list their own requests"
[ "$(json_get "$RUN_DIR/last.json" "'len(d[\"requests\"])'")" -ge 1 ] \
  && ok "their request appears in /api/requests/my" || bad "own request missing"

CODE=$(api GET "/api/requests/$REQ_ID" "$COOKIE_ADMIN")
expect_eq 200 "$CODE" "an administrator can open the request detail"

# ============================================================================
head1 "Test 4 — Local volunteers first, then nearest by distance"

api POST /api/requests "$COOKIE_PERSON" \
  '{"service":"Rescue","areaId":8,"peopleAffected":30,"message":"allocation ordering"}' >/dev/null
LOCAL_FIRST=$(python3 tests/check_json.py "$RUN_DIR/last.json" local-first)
DIST_SORTED=$(python3 tests/check_json.py "$RUN_DIR/last.json" distance-sorted)
[ "$LOCAL_FIRST" = "True" ] && ok "volunteers based in the affected area are allocated first" \
  || bad "local volunteers were not prioritised"
[ "$DIST_SORTED" = "True" ] && ok "out-of-area volunteers are ordered by shortest travel distance" \
  || bad "distance ordering violated"

# ============================================================================
head1 "Test 5 — Shortest path comes from the C++ Dijkstra implementation"

CODE=$(api GET "/api/routes?from=8&to=17" "$COOKIE_PERSON")
expect_eq 200 "$CODE" "GET /api/routes works for a signed-in user"
expect_eq "Port -> Dam" "$(json_get "$RUN_DIR/last.json" "'d[\"pathText\"]'")" \
  "the direct Port -> Dam route is returned"
expect_eq "4.5" "$(json_get "$RUN_DIR/last.json" "'str(d[\"totalKm\"])'")" \
  "the total distance is 4.5 km"

CODE=$(api GET "/api/routes?from=0&to=9999" "$COOKIE_PERSON")
expect_eq 400 "$CODE" "an unknown area id is rejected with 400"

# ============================================================================
head1 "Test 6 — Blocking a road changes the shortest path"

CODE=$(api PATCH /api/admin/roads/23 "$COOKIE_ADMIN" '{"blocked":true}')
expect_eq 200 "$CODE" "admin can block road 23 (Port - Dam)"
grep -q '^23,8,17,4.5,1$' "$RUN_DIR/data/roads_db.csv" \
  && ok "roads_db.csv records the blocked flag" || bad "blocked flag not persisted"

api GET "/api/routes?from=8&to=17" "$COOKIE_PERSON" >/dev/null
BLOCKED_KM=$(json_get "$RUN_DIR/last.json" "'str(d[\"totalKm\"])'")
USES_BLOCKED=$(json_get "$RUN_DIR/last.json" "'23 in d[\"roadIds\"]'")
[ "$BLOCKED_KM" != "4.5" ] && ok "the route avoids the blocked road ($BLOCKED_KM km detour)" \
  || bad "the route still uses the blocked road"
expect_eq "False" "$USES_BLOCKED" "the blocked road is excluded from the path"

CODE=$(api PATCH /api/admin/roads/23 "$COOKIE_ADMIN" '{"blocked":false}')
expect_eq 200 "$CODE" "admin can unblock the road"
api GET "/api/routes?from=8&to=17" "$COOKIE_PERSON" >/dev/null
expect_eq "4.5" "$(json_get "$RUN_DIR/last.json" "'str(d[\"totalKm\"])'")" \
  "unblocking restores the 4.5 km route"

# ============================================================================
head1 "Test 7 — Completing a request releases its volunteers"

api GET "/api/admin/requests/$REQ_ID" "$COOKIE_ADMIN" >/dev/null
HELD=$(json_get "$RUN_DIR/last.json" "'d[\"request\"][\"assignedVolunteers\"][0][\"id\"]'")
CODE=$(api PATCH "/api/admin/requests/$REQ_ID/complete" "$COOKIE_ADMIN" '{}')
expect_eq 200 "$CODE" "admin can complete the request"
expect_eq "Completed" "$(json_get "$RUN_DIR/last.json" "'d[\"request\"][\"status\"]'")" \
  "the request status is now Completed"
api GET /api/admin/volunteers "$COOKIE_ADMIN" >/dev/null
[ "$(json_get "$RUN_DIR/last.json" "'[v for v in d[\"volunteers\"] if v[\"id\"]==$HELD][0][\"available\"]'")" = "True" ] \
  && ok "the released volunteer is available again" \
  || bad "volunteer was not released"
grep -q "^$REQ_ID,Medical Help,2,60,.*Completed" "$RUN_DIR/data/requests_db.csv" \
  && ok "requests_db.csv stores the completed status" || bad "completion not persisted"

# ============================================================================
head1 "Test 9 — Data consistency safeguards"

CODE=$(api PATCH "/api/admin/requests/$REQ_ID/complete" "$COOKIE_ADMIN" '{}')
expect_eq 409 "$CODE" "completing an already completed request is rejected"

api POST /api/requests "$COOKIE_PERSON" \
  '{"service":"Food","areaId":18,"peopleAffected":100000,"message":"capacity stress"}' >/dev/null
api POST /api/admin/volunteers/reset "$COOKIE_ADMIN" '{}' >/dev/null
SKIPPED=$(json_get "$RUN_DIR/last.json" "'d[\"skippedAssigned\"]'")
[ "$SKIPPED" -ge 1 ] && ok "reset kept $SKIPPED volunteer(s) assigned to active requests" \
  || bad "reset released volunteers who are on active requests"

api GET /api/admin/volunteers "$COOKIE_ADMIN" >/dev/null
DOUBLED=$(python3 tests/check_json.py "$RUN_DIR/last.json" available-and-assigned)
expect_eq "0" "$DOUBLED" "no volunteer is simultaneously available and assigned"

# ============================================================================
head1 "Test 9b — Concurrent submissions cannot double-book a volunteer"

# Fire 8 requests in parallel, then verify that no volunteer appears in two
# active requests (the DataStore mutex serialises the allocation cycles).
CPIDS=""
for i in $(seq 1 8); do
  curl -s -X POST -b "$COOKIE_PERSON" -H 'Content-Type: application/json' \
    -d "{\"service\":\"Rescue\",\"areaId\":$((i % 20)),\"peopleAffected\":15,\"message\":\"parallel $i\"}" \
    "$BASE/api/requests" > /dev/null &
  CPIDS="$CPIDS $!"
done
for p in $CPIDS; do wait "$p"; done   # only the curl jobs, never the server
sleep 0.3
ACODE=$(api GET /api/admin/requests "$COOKIE_ADMIN")
expect_eq 200 "$ACODE" "the admin request list is still readable after the parallel burst"
DUPLICATE=$(python3 tests/check_json.py "$RUN_DIR/last.json" no-double-booking)
expect_eq "True" "$DUPLICATE" "no volunteer is assigned to two active requests at once"

# ============================================================================
head1 "Test 8 — Persistence across a server restart"

api POST /api/admin/volunteers "$COOKIE_ADMIN" \
  '{"name":"Restart Survivor","areaId":6,"service":"Food","capacity":33}' >/dev/null
SURVIVOR_ID=$(json_get "$RUN_DIR/last.json" "'d[\"volunteer\"][\"id\"]'")
VOLS_BEFORE=$(api GET /api/admin/volunteers "$COOKIE_ADMIN" >/dev/null; json_get "$RUN_DIR/last.json" "'len(d[\"volunteers\"])'")
REQS_BEFORE=$(api GET /api/admin/requests "$COOKIE_ADMIN" >/dev/null; json_get "$RUN_DIR/last.json" "'len(d[\"requests\"])'")

stop_server
start_server
echo "  (server restarted on $BASE)"

# Sessions live in memory only, so the administrator must sign in again.
CODE=$(api POST /api/auth/login "$COOKIE_ADMIN" '{"username":"Admin","password":"admin123"}')
expect_eq 200 "$CODE" "signing in again after the restart succeeds (sessions are in-memory)"

VOLS_AFTER=$(api GET /api/admin/volunteers "$COOKIE_ADMIN" >/dev/null; json_get "$RUN_DIR/last.json" "'len(d[\"volunteers\"])'")
REQS_AFTER=$(api GET /api/admin/requests "$COOKIE_ADMIN" >/dev/null; json_get "$RUN_DIR/last.json" "'len(d[\"requests\"])'")
expect_eq "$VOLS_BEFORE" "$VOLS_AFTER" "volunteer count survived the restart"
expect_eq "$REQS_BEFORE" "$REQS_AFTER" "request count survived the restart"
grep -q "Restart Survivor" "$RUN_DIR/data/volunteers_db.csv" \
  && ok "the volunteer added before the restart is still in the CSV" || bad "volunteer lost"
api GET "/api/admin/requests/$REQ_ID" "$COOKIE_ADMIN" >/dev/null
expect_eq "Completed" "$(json_get "$RUN_DIR/last.json" "'d[\"request\"][\"status\"]'")" \
  "the completed status also survived the restart"
CODE=$(api POST /api/auth/login "$COOKIE_ADMIN" '{"username":"Admin","password":"admin123"}')
expect_eq 200 "$CODE" "the migrated administrator password still works after restart"

# ============================================================================
printf "\n----------------------------------------------------------\n"
printf "passed: %d   failed: %d\n" "$PASS" "$FAIL"
if [ "$FAIL" -eq 0 ]; then
  printf "\033[32mALL ACCEPTANCE TESTS PASSED\033[0m\n"
else
  printf "\033[31m%d ACCEPTANCE TEST(S) FAILED\033[0m (server log: %s)\n" "$FAIL" "$SERVER_LOG"
  RUN_DIR_KEEP=1
fi
exit $([ "$FAIL" -eq 0 ] && echo 0 || echo 1)
