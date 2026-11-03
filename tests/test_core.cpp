/* ============================================================================
 *  tests/test_core.cpp  --  Self-contained test suite for the C++ core
 * ----------------------------------------------------------------------------
 *  Exercises the data structures and business rules directly (no HTTP), so a
 *  failure points straight at the algorithm rather than at the web layer.
 *
 *  Build & run (from the project root):
 *      g++ -std=c++17 -Ithird_party -Isrc tests/test_core.cpp src/csv_utils.cpp \
 *          src/models.cpp src/crypto.cpp src/priority_queue.cpp src/city_graph.cpp \
 *          src/disaster_management.cpp src/authentication.cpp -o build/test_core
 *      ./build/test_core data
 *
 *  argv[1] is the data directory (defaults to "data").
 * ==========================================================================*/
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "authentication.h"
#include "city_graph.h"
#include "crypto.h"
#include "csv_utils.h"
#include "disaster_management.h"
#include "priority_queue.h"

using namespace dms;

/* --------------------------------------------------------------- harness -- */
static int g_passed = 0;
static int g_failed = 0;
static std::string g_section = "";

static void section(const std::string &name) {
    g_section = name;
    std::printf("\n\033[1m== %s ==\033[0m\n", name.c_str());
}

static void check(bool condition, const std::string &what) {
    if (condition) {
        ++g_passed;
        std::printf("  \033[32mPASS\033[0m %s\n", what.c_str());
    } else {
        ++g_failed;
        std::printf("  \033[31mFAIL\033[0m %s\n", what.c_str());
    }
}

static void checkNear(double actual, double expected, double tolerance, const std::string &what) {
    const bool ok = std::fabs(actual - expected) <= tolerance;
    check(ok, what + " (got " + std::to_string(actual) + ", expected " +
                  std::to_string(expected) + ")");
}

/* ----------------------------------------------------- scratch filesystem -- */
static std::string scratchDir() {
    const char *tmp = std::getenv("TMPDIR");
    std::string base = tmp ? tmp : "/tmp";
    std::string pattern = base + "/dms_test_XXXXXX";
    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back('\0');
#ifdef _WIN32
    /* Windows: fall back to a deterministic folder next to the tests */
    const std::string dir = "_test_scratch";
    ensureDirectory(dir);
    return dir;
#else
    char *result = mkdtemp(&buffer[0]);
    if (!result) return "_test_scratch";
    return std::string(result);
#endif
}

static void writeText(const std::string &path, const std::string &content) {
    std::ofstream out(path.c_str(), std::ios::binary | std::ios::trunc);
    out << content;
}

/* ==========================================================================
 *  1. Priority queue (custom linked-list implementation)
 * ========================================================================*/
static void testPriorityQueue() {
    section("Custom priority queue (sorted singly linked list)");

    PriorityQueue pq;
    check(pq.empty(), "a fresh queue is empty");
    check(pq.size() == 0, "a fresh queue has size 0");

    pq.push(10, 5.0);
    pq.push(20, 1.0);
    pq.push(30, 3.0);
    pq.push(40, 1.0);
    check(pq.size() == 4, "size grows with each push");

    int node = -1;
    double key = 0.0;
    check(pq.pop(node, key) && node == 20, "minimum key is extracted first");
    checkNear(key, 1.0, 1e-9, "extracted key is 1.0");
    check(pq.pop(node, key) && node == 40, "equal keys keep insertion (FIFO) order");
    check(pq.pop(node, key) && node == 30, "next smallest follows");
    check(pq.pop(node, key) && node == 10, "largest key is extracted last");
    check(!pq.pop(node, key), "pop on an empty queue returns false");
    check(pq.empty(), "queue is empty again");
}

/* ==========================================================================
 *  2. CSV parsing / writing
 * ========================================================================*/
static void testCsv() {
    section("CSV parsing and escaping");

    const std::vector<std::string> f = parseCsvLine("1,\"Name, with comma\",\"say \"\"hi\"\"\",5");
    check(f.size() == 4, "quoted fields with commas are kept whole");
    check(f.size() > 1 && f[1] == "Name, with comma", "embedded comma preserved");
    check(f.size() > 2 && f[2] == "say \"hi\"", "escaped quotes are unescaped");
    check(f.size() > 3 && f[3] == "5", "plain numeric field parsed");

    const std::string escaped = csvEscape("Flood, \"urgent\"\nmessage");
    check(!escaped.empty() && escaped[0] == '"' && escaped[escaped.size() - 1] == '"' &&
          escaped.find("\"\"urgent\"\"") != std::string::npos,
          "fields needing quotes are quoted and inner quotes are doubled");
    const std::vector<std::string> round = parseCsvLine(escaped);
    check(round.size() == 1, "escaped field round-trips as one field");
    check(round[0].find("Flood, \"urgent\"") == 0, "content survives the round-trip");

    check(toInt("42") == 42 && toInt("", -3) == -3 && toInt("abc", 7) == 7,
          "toInt is tolerant of missing/garbage values");
    check(toBoolFlag("1", false) && !toBoolFlag("0", true) && toBoolFlag("yes", false) &&
          !toBoolFlag("blocked", true), "boolean flags accept 1/0 and yes/no/blocked");
}

/* ==========================================================================
 *  3. Password hashing / crypto
 * ========================================================================*/
static void testCrypto() {
    section("Password hashing (PBKDF2) and legacy migration");

    check(sha256Hex("abc") ==
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "SHA-256 matches the FIPS test vector for 'abc'");

    const std::string stored = hashPassword("correct horse", 1000);
    check(stored.rfind("pbkdf2$sha256$", 0) == 0, "new hashes use the pbkdf2$sha256 format");
    check(verifyPassword("correct horse", stored), "correct password verifies");
    check(!verifyPassword("wrong horse", stored), "wrong password is rejected");
    check(stored.find("correct horse") == std::string::npos, "the plaintext is not stored");

    const std::string legacy = legacyEducationalHash("admin123");
    check(isLegacyHash(legacy), "the original educational hash is recognised as legacy");
    check(verifyPassword("admin123", legacy), "a legacy account can still authenticate");
    check(!verifyPassword("admin124", legacy), "legacy verification rejects a bad password");

    check(randomHex(16).size() == 32, "session tokens are 32 hex characters per 16 bytes");
    check(randomHex(16) != randomHex(16), "two tokens differ");
    check(constantTimeEquals("abc", "abc") && !constantTimeEquals("abc", "abd"),
          "constant-time compare behaves like ==");
}

/* ==========================================================================
 *  4. City graph and Dijkstra
 * ========================================================================*/
static void testGraph(const std::string &dataDir) {
    section("City graph, adjacency list and Dijkstra");

    CityGraph graph;
    std::string error;
    check(graph.loadAreas(dataDir + "/areas_db.csv", error), "areas_db.csv loads");
    check(graph.loadRoads(dataDir + "/roads_db.csv", error), "roads_db.csv loads");
    check(graph.areaCount() == 20, "20 areas are loaded");
    check(graph.roadCount() == 38, "38 roads are loaded");
    check(graph.areaName(0) == "Hospital" && graph.areaName(19) == "East Dam",
          "area ids map to the documented names");

    /* Port (8) -> Dam (17) is a direct 4.5 km road (id 23) */
    RouteResult direct = graph.shortestPath(8, 17);
    check(direct.found, "a route exists from Port to Dam");
    checkNear(direct.totalKm, 4.5, 1e-9, "direct Port->Dam distance is 4.5 km");
    check(direct.steps.size() == 1 && direct.steps[0].roadId == 23,
          "the direct road (id 23) is used");
    check(graph.describePath(direct.areaIds) == "Port -> Dam", "path description is correct");

    /* same area */
    const RouteResult same = graph.shortestPath(6, 6);
    check(same.found && same.totalKm == 0.0, "a route from an area to itself is 0 km");

    /* unknown areas */
    const RouteResult bad = graph.shortestPath(4, 999);
    check(!bad.found, "an unknown area id yields no route (no crash)");

    /* blocked roads are excluded */
    check(graph.setRoadBlocked(23, true, error), "road 23 can be blocked");
    const RouteResult blocked = graph.shortestPath(8, 17);
    check(blocked.found, "an alternative route still exists with road 23 blocked");
    check(blocked.totalKm > 4.5, "the alternative route is longer");
    bool usesBlocked = false;
    for (std::size_t i = 0; i < blocked.steps.size(); ++i)
        if (blocked.steps[i].roadId == 23) usesBlocked = true;
    check(!usesBlocked, "the blocked road is never used by Dijkstra");

    graph.setRoadBlocked(23, false, error);
    const RouteResult reopened = graph.shortestPath(8, 17);
    checkNear(reopened.totalKm, 4.5, 1e-9, "unblocking restores the direct route");

    /* disconnecting the destination */
    graph.setRoadBlocked(23, true, error);   /* 8-17 */
    graph.setRoadBlocked(36, true, error);   /* 16-17 */
    graph.setRoadBlocked(37, true, error);   /* 16-19 */
    graph.setRoadBlocked(35, true, error);   /* 15-19 */
    graph.setRoadBlocked(27, true, error);   /* 10-17 */
    graph.setRoadBlocked(33, true, error);   /* 14-15 */
    const RouteResult isolated = graph.shortestPath(8, 17);
    check(!isolated.found, "Dam becomes unreachable when all its roads are blocked");
    graph.setRoadBlocked(23, false, error);
    graph.setRoadBlocked(36, false, error);
    graph.setRoadBlocked(37, false, error);
    graph.setRoadBlocked(35, false, error);
    graph.setRoadBlocked(27, false, error);
    graph.setRoadBlocked(33, false, error);

    /* transitively correct distances (Dijkstra sanity check) */
    double km = 0.0;
    check(graph.shortestDistance(0, 19, km) && km > 0.0, "shortestDistance works helper");
    const RouteResult longRoute = graph.shortestPath(0, 19);
    check(longRoute.found, "Hospital -> East Dam is reachable");
    double sum = 0.0;
    for (std::size_t i = 0; i < longRoute.steps.size(); ++i) sum += longRoute.steps[i].distanceKm;
    checkNear(sum, longRoute.totalKm, 1e-6, "segment distances add up to the reported total");
}

/* ==========================================================================
 *  5. Volunteer allocation engine
 * ========================================================================*/
static void testAllocation() {
    section("Volunteer allocation engine (local first, then nearest)");

    const std::string dir = scratchDir();
    ensureDirectory(dir);

    /* a small deterministic city: 0..5 with known distances */
    writeText(dir + "/areas.csv",
              "id,name\n0,Alpha\n1,Bravo\n2,Charlie\n3,Delta\n4,Echo\n5,Foxtrot\n");
    writeText(dir + "/roads.csv",
              "id,from_area_id,to_area_id,distance_km,blocked\n"
              "0,0,2,3.0,0\n"     /* Alpha  -- Charlie : 3.0 */
              "1,5,2,2.2,0\n"     /* Foxtrot -- Charlie : 2.2 */
              "2,0,5,9.0,0\n"
              "3,0,3,4.0,0\n");   /* Delta linked through Alpha only (7.0 km to Charlie) */
    /* volunteers */
    writeText(dir + "/volunteers.csv",
              "id,name,area_id,service,capacity,available\n"
              "1,Local Medic,2,Medical Help,10,1\n"       /* local, 0 km          */
              "2,Alpha Medic,0,Medical Help,20,1\n"       /* 3.0 km away          */
              "3,Foxtrot Medic,5,Medical Help,20,1\n"     /* 2.2 km - nearer      */
              "4,Local Cook,2,Food,100,1\n"               /* wrong service        */
              "5,Faraway Medic,3,Medical Help,50,1\n");   /* 7.0 km via Alpha     */
    writeText(dir + "/requests.csv",
              "id,service,area_id,people_affected,people_covered,assigned_volunteers,"
              "status,message,owner_username\n");

    CityGraph graph;
    std::string error;
    graph.loadAreas(dir + "/areas.csv", error);
    graph.loadRoads(dir + "/roads.csv", error);
    check(graph.areaCount() == 6 && graph.roadCount() == 4, "scratch graph loaded");

    DisasterManagement dm;
    dm.setPaths(dir + "/volunteers.csv", dir + "/requests.csv");
    dm.loadVolunteers(dir + "/volunteers.csv", error);
    dm.loadRequests(dir + "/requests.csv", error);
    check(dm.volunteerCount() == 5, "five volunteers loaded");

    /* ---- allocation: 25 people need medical help in area 2 -------------- */
    RequestOutcome out = dm.submitRequest(SERVICE_MEDICAL, 2, 25, "test message", "citizen.test", graph);
    check(out.result.ok, "the request is accepted");
    check(out.allocations.size() == 2, "two volunteers are enough to cover 25 people");
    if (out.allocations.size() == 2) {
        check(out.allocations[0].volunteerId == 1 && out.allocations[0].local,
              "the volunteer already in the affected area is chosen first");
        check(out.allocations[1].volunteerId == 3,
              "the second choice is the NEAREST volunteer (2.2 km), not the farther one");
        checkNear(out.allocations[1].distanceKm, 2.2, 1e-9, "its travel distance is 2.2 km");
        bool pickedFar = false;
        for (std::size_t i = 0; i < out.allocations.size(); ++i)
            if (out.allocations[i].volunteerId == 5) pickedFar = true;
        check(!pickedFar, "the farther volunteer (7.0 km) was NOT chosen");
    }
    check(out.request.peopleCovered == 25, "coverage is capped at the affected headcount");
    check(out.peopleRemaining == 0, "no one is left uncovered");
    check(out.request.assignedVolunteerIds.size() == 2, "assignments recorded on the request");
    check(out.request.ownerUsername == "citizen.test", "the request is linked to its owner");

    const Volunteer *v1 = dm.volunteerById(1);
    const Volunteer *v3 = dm.volunteerById(3);
    const Volunteer *v2 = dm.volunteerById(2);
    check(v1 && !v1->available, "allocated volunteer 1 becomes unavailable");
    check(v3 && !v3->available, "allocated volunteer 3 becomes unavailable");
    check(v2 && v2->available, "an unallocated volunteer stays available");

    /* ---- double booking protection -------------------------------------- */
    RequestOutcome second = dm.submitRequest(SERVICE_MEDICAL, 2, 5, "second", "citizen.test", graph);
    check(second.result.ok, "a second request is accepted");
    bool reused = false;
    for (std::size_t i = 0; i < second.allocations.size(); ++i) {
        if (second.allocations[i].volunteerId == 1 || second.allocations[i].volunteerId == 3)
            reused = true;
    }
    check(!reused, "a volunteer on an active request is never assigned twice");

    /* ---- reset protection ----------------------------------------------- */
    DisasterManagement::ResetOutcome reset = dm.resetVolunteerAvailability();
    check(reset.result.ok, "availability reset succeeds");
    check(reset.skippedAssigned >= 2, "volunteers on active requests are NOT released");
    const Volunteer *v1after = dm.volunteerById(1);
    check(v1after && !v1after->available, "volunteer 1 stays assigned after a bulk reset");

    /* ---- completion releases volunteers --------------------------------- */
    const std::vector<int> heldByFirst = out.request.assignedVolunteerIds;
    OpResult done = dm.completeRequest(out.request.id);
    check(done.ok, "the request can be completed");
    for (std::size_t i = 0; i < heldByFirst.size(); ++i) {
        const Volunteer *v = dm.volunteerById(heldByFirst[i]);
        if (!v) continue;
        const bool stillUsedBySecond = dm.requestById(second.request.id) &&
            std::find(dm.requestById(second.request.id)->assignedVolunteerIds.begin(),
                      dm.requestById(second.request.id)->assignedVolunteerIds.end(),
                      v->id) != dm.requestById(second.request.id)->assignedVolunteerIds.end();
        if (!stillUsedBySecond)
            check(v->available, "volunteer released after completion: " + v->name);
    }
    check(!dm.completeRequest(out.request.id).ok, "completing twice is rejected (idempotency)");

    /* ---- insufficient capacity ------------------------------------------ */
    RequestOutcome huge = dm.submitRequest(SERVICE_RESCUE, 2, 500, "no rescue crew",
                                           "citizen.test", graph);
    check(huge.result.ok, "a request can be recorded even with no suitable volunteer");
    check(huge.allocations.empty(), "no volunteer of that service exists: nothing allocated");
    check(huge.peopleRemaining == 500, "the full headcount remains uncovered");
    check(huge.request.peopleCovered == 0, "coverage stays at zero");

    /* ---- validation ----------------------------------------------------- */
    check(!dm.submitRequest(SERVICE_FOOD, 999, 10, "", "x", graph).result.ok,
          "an unknown area is rejected");
    check(!dm.submitRequest(SERVICE_FOOD, 2, 0, "", "x", graph).result.ok,
          "a zero headcount is rejected");
    check(!dm.addVolunteer("A", 2, SERVICE_FOOD, 10, graph).ok,
          "a one-character volunteer name is rejected");
    int newId = -1;
    check(dm.addVolunteer("New Rescuer", 4, SERVICE_RESCUE, 30, graph, &newId).ok,
          "a valid volunteer is added");
    check(newId > 0, "the new volunteer receives an id");

    /* ---- persistence round-trip ---------------------------------------- */
    DisasterManagement reloaded;
    reloaded.setPaths(dir + "/volunteers.csv", dir + "/requests.csv");
    reloaded.loadVolunteers(dir + "/volunteers.csv", error);
    reloaded.loadRequests(dir + "/requests.csv", error);
    check(reloaded.requestCount() == dm.requestCount(), "requests survive a reload from CSV");
    check(reloaded.volunteerCount() == dm.volunteerCount(), "volunteers survive a reload from CSV");
    const Request *persisted = reloaded.requestById(out.request.id);
    check(persisted && persisted->status == REQUEST_COMPLETED, "completed status is persisted");
    check(persisted && persisted->ownerUsername == "citizen.test", "the owner column persists");

    /* ---- owner scoping -------------------------------------------------- */
    check(dm.requestsForOwner("citizen.test").size() == 3, "owner sees only their own requests");
    check(dm.requestsForOwner("somebody.else").empty(), "another account sees nothing");
    check(dm.requestsForOwner("").empty(), "an empty owner matches nobody");

    /* ---- statistics ----------------------------------------------------- */
    const SystemStats stats = dm.computeStats();
    check(stats.totalVolunteers == stats.availableVolunteers + stats.assignedVolunteers,
          "available + assigned equals the total volunteer count");
    check(stats.totalRequests == dm.requestCount(), "total requests matches the record count");
    check(stats.activeRequests + stats.completedRequests == stats.totalRequests,
          "active + completed equals the total request count");
    check(stats.underCoveredActive >= 1, "under-covered active requests are counted");
}

/* ==========================================================================
 *  6. Authentication and role rules
 * ========================================================================*/
static void testAuthentication() {
    section("Authentication, roles and legacy hash migration");

    const std::string dir = scratchDir();
    ensureDirectory(dir);
    const std::string usersPath = dir + "/users.csv";

    writeText(usersPath,
              "username,password_hash,role\n"
              "Admin," + legacyEducationalHash("admin123") + ",ADMIN\n"
              "someone," + legacyEducationalHash("secret1") + ",PERSON\n");

    Authentication auth(usersPath);
    std::string error;
    check(auth.load(error), "users_db.csv loads");
    check(auth.userCount() == 2, "two accounts loaded (header row skipped)");
    check(auth.adminCount() == 1, "one administrator");

    /* legacy login + migration */
    AuthResult admin = auth.login("Admin", "admin123");
    check(admin.ok && admin.role == ROLE_ADMIN, "the demo admin can sign in");
    check(admin.migrated, "the legacy hash is upgraded on first login");
    Authentication reloaded(usersPath);
    reloaded.load(error);
    const User *migrated = reloaded.findUser("Admin");
    check(migrated && !isLegacyHash(migrated->passwordHash),
          "users_db.csv now stores a PBKDF2 hash");
    check(reloaded.login("Admin", "admin123").ok, "the migrated account still signs in");

    /* bad credentials */
    AuthResult bad = auth.login("Admin", "wrong");
    check(!bad.ok, "a wrong password is rejected");
    check(auth.login("ghost", "admin123").code == "invalid_credentials",
          "an unknown user does not leak which part was wrong");

    /* self registration cannot create admins */
    OpResult escalate = auth.registerUser("attacker", "password1", true, false);
    check(!escalate.ok && escalate.code == "forbidden",
          "self-registration cannot request an ADMIN role");
    OpResult person = auth.registerUser("citizen1", "password1", false, false);
    check(person.ok, "a citizen account can be created");
    check(auth.findUser("citizen1")->role == ROLE_PERSON, "self-registered accounts are PERSON");
    check(!auth.registerUser("citizen1", "password1", false, false).ok,
          "duplicate usernames are rejected");
    check(!auth.registerUser("ab", "password1", false, false).ok, "short usernames are rejected");
    check(!auth.registerUser("validname", "12345", false, false).ok, "short passwords are rejected");

    /* an administrator may create another administrator */
    OpResult promoted = auth.registerUser("ops.admin", "password9", true, true);
    check(promoted.ok && auth.findUser("ops.admin")->role == ROLE_ADMIN,
          "an administrator can create another administrator");
    check(auth.adminCount() == 2, "admin count increased");

    /* password change */
    check(!auth.changePassword("citizen1", "wrongpass", "newpass1").ok,
          "changing a password requires the current one");
    check(auth.changePassword("citizen1", "password1", "newpass1").ok, "password change succeeds");
    check(auth.login("citizen1", "newpass1").ok, "the new password works");
    check(!auth.login("citizen1", "password1").ok, "the old password no longer works");
}

/* ==========================================================================
 *  7. Backward compatibility with the original CSV layout
 * ========================================================================*/
static void testBackwardCompatibility() {
    section("Backward compatibility of the original CSV files");

    const std::string dir = scratchDir();
    ensureDirectory(dir);

    /* the ORIGINAL 8-column request layout (no owner column) */
    writeText(dir + "/requests_legacy.csv",
              "id,service,area_id,people_affected,people_covered,assigned_volunteers,"
              "status,message\n"
              "1,Rescue,8,60,60,1;5,Active,Storm surge\n"
              "2,Medical Help,6,80,80,8,Completed,\"Triage, city centre\"\n");
    writeText(dir + "/volunteers_legacy.csv",
              "id,name,area_id,service,capacity,available\n"
              "1,Ravi,8,Rescue,40,0\n"
              "8,Doc,6,Medical Help,50,1\n");

    DisasterManagement dm;
    std::string error;
    dm.setPaths(dir + "/volunteers_legacy.csv", dir + "/requests_legacy.csv");
    dm.loadVolunteers(dir + "/volunteers_legacy.csv", error);
    dm.loadRequests(dir + "/requests_legacy.csv", error);
    check(dm.requestCount() == 2, "legacy request rows load");
    const Request *r1 = dm.requestById(1);
    check(r1 && r1->ownerUsername.empty(), "legacy requests simply have no owner");
    check(r1 && r1->assignedVolunteerIds.size() == 2, "semicolon separated assignments parse");
    const Request *r2 = dm.requestById(2);
    check(r2 && r2->message == "Triage, city centre", "quoted messages with commas parse");

    /* availability repair: volunteer 1 is assigned to an ACTIVE request */
    const Volunteer *v1 = dm.volunteerById(1);
    check(v1 && !v1->available, "a volunteer on an active request is forced unavailable on load");

    /* saving writes the NEW layout, and reloading keeps everything */
    dm.saveRequests(dir + "/requests_legacy.csv", error);
    std::vector<std::vector<std::string> > rows;
    readCsvFile(dir + "/requests_legacy.csv", rows);
    check(rows.size() == 3, "header + two rows written");
    check(rows[0].size() == 9 && rows[0][8] == "owner_username",
          "the new owner_username column is appended last");

    DisasterManagement again;
    again.setPaths(dir + "/volunteers_legacy.csv", dir + "/requests_legacy.csv");
    again.loadRequests(dir + "/requests_legacy.csv", error);
    check(again.requestCount() == 2, "the upgraded file still loads");
}

/* ==========================================================================
 *  main
 * ========================================================================*/
int main(int argc, char *argv[]) {
    const std::string dataDir = (argc > 1) ? argv[1] : "data";

    std::printf("CivicShield core test suite\n");
    std::printf("data directory: %s\n", dataDir.c_str());

    testPriorityQueue();
    testCsv();
    testCrypto();
    testGraph(dataDir);
    testAllocation();
    testAuthentication();
    testBackwardCompatibility();

    std::printf("\n----------------------------------------------------------\n");
    std::printf("passed: %d   failed: %d\n", g_passed, g_failed);
    if (g_failed == 0) std::printf("\033[32mALL CORE TESTS PASSED\033[0m\n");
    else std::printf("\033[31m%d TEST(S) FAILED\033[0m\n", g_failed);
    return g_failed == 0 ? 0 : 1;
}
