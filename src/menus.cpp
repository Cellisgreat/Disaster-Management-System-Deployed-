#include "menus.h"

#include <iomanip>
#include <iostream>
#include <vector>

#include "input_utils.h"

namespace dms {

namespace {

std::string serviceOf(const Volunteer &v) { return serviceToString(v.service); }

}  /* anonymous namespace */

ConsoleApp::ConsoleApp(DataStore &store) : store_(store) {}

/* ==========================================================================
 *  entry point: login loop
 * ========================================================================*/
void ConsoleApp::run() {
    printTitle("CIVICSHIELD  //  DISASTER MANAGEMENT SYSTEM (console mode)");
    std::cout << "Educational demonstration build. Type 'exit' at the username prompt to quit.\n";

    while (true) {
        std::cout << '\n';
        const std::string username = readLine("Username: ");
        if (username.empty()) continue;
        if (username == "exit" || username == "quit") {
            std::cout << "Goodbye.\n";
            return;
        }
        const std::string password = readLine("Password: ");

        std::lock_guard<std::recursive_mutex> guard(store_.mutex);
        const AuthResult result = store_.auth.login(username, password);
        if (!result.ok) {
            std::cout << "  ! " << result.error << '\n';
            continue;
        }
        std::cout << "  Welcome, " << result.username << " (" << roleToString(result.role) << ")\n";
        if (result.role == ROLE_ADMIN) adminSession(result.username);
        else personSession(result.username);
    }
}

/* ==========================================================================
 *  printing helpers
 * ========================================================================*/
void ConsoleApp::printAreas() const {
    std::cout << "\n  Areas:\n";
    for (int i = 0; i < store_.graph.areaCount(); ++i) {
        const Area *a = store_.graph.areaAt(i);
        if (!a) continue;
        std::cout << "    [" << std::setw(2) << a->id << "] " << a->name << '\n';
    }
}

void ConsoleApp::printVolunteers() const {
    std::cout << "\n  " << std::left << std::setw(4) << "ID" << std::setw(22) << "Name"
              << std::setw(22) << "Base area" << std::setw(15) << "Service"
              << std::setw(10) << "Capacity" << "Status\n";
    printRule('-', 90);
    for (int i = 0; i < store_.dm.volunteerCount(); ++i) {
        const Volunteer *v = store_.dm.volunteerAt(i);
        if (!v) continue;
        std::cout << "  " << std::left << std::setw(4) << v->id << std::setw(22) << v->name
                  << std::setw(22) << store_.graph.areaName(v->areaId)
                  << std::setw(15) << serviceOf(*v) << std::setw(10) << v->capacity
                  << (v->available ? "Available" : "Assigned") << '\n';
    }
}

void ConsoleApp::printRequests(bool adminView) const {
    if (store_.dm.requestCount() == 0) {
        std::cout << "\n  No requests recorded yet.\n";
        return;
    }
    std::cout << "\n  " << std::left << std::setw(4) << "ID" << std::setw(15) << "Service"
              << std::setw(20) << "Area" << std::setw(10) << "Affected" << std::setw(10)
              << "Covered" << std::setw(10) << "Vols" << std::setw(12) << "Status";
    if (adminView) std::cout << "Owner";
    std::cout << '\n';
    printRule('-', adminView ? 100 : 90);
    for (int i = 0; i < store_.dm.requestCount(); ++i) {
        const Request *r = store_.dm.requestAt(i);
        if (!r) continue;
        std::cout << "  " << std::left << std::setw(4) << r->id
                  << std::setw(15) << serviceToString(r->service)
                  << std::setw(20) << store_.graph.areaName(r->areaId)
                  << std::setw(10) << r->peopleAffected
                  << std::setw(10) << r->peopleCovered
                  << std::setw(10) << r->assignedVolunteerIds.size()
                  << std::setw(12) << requestStatusToString(r->status);
        if (adminView) std::cout << (r->ownerUsername.empty() ? "-" : r->ownerUsername);
        std::cout << '\n';
        if (!r->message.empty()) std::cout << "       message: " << r->message << '\n';
    }
}

void ConsoleApp::printRoads() const {
    std::cout << "\n  " << std::left << std::setw(6) << "ID" << std::setw(24) << "From"
              << std::setw(24) << "To" << std::setw(10) << "Km" << "Status\n";
    printRule('-', 78);
    for (int i = 0; i < store_.graph.roadCount(); ++i) {
        const Road *r = store_.graph.roadAt(i);
        if (!r) continue;
        std::cout << "  " << std::left << std::setw(6) << r->id
                  << std::setw(24) << store_.graph.areaName(r->fromArea)
                  << std::setw(24) << store_.graph.areaName(r->toArea)
                  << std::setw(10) << r->distanceKm
                  << (r->blocked ? "BLOCKED" : "open") << '\n';
    }
}

/* ==========================================================================
 *  shared actions
 * ========================================================================*/
void ConsoleApp::handleShortestRoute() {
    printAreas();
    const int from = readIntInRange("  Source area id: ", 0, 999);
    const int to = readIntInRange("  Destination area id: ", 0, 999);

    std::lock_guard<std::recursive_mutex> guard(store_.mutex);
    if (!store_.graph.hasArea(from) || !store_.graph.hasArea(to)) {
        std::cout << "  ! Unknown area id.\n";
        return;
    }
    const RouteResult route = store_.graph.shortestPath(from, to);
    if (!route.found) {
        std::cout << "  ! No open route between those areas.\n";
        return;
    }
    std::cout << "\n  Shortest path : " << store_.graph.describePath(route.areaIds) << '\n';
    std::cout << "  Total distance: " << route.totalKm << " km\n";
}

void ConsoleApp::handleAddVolunteer() {
    const std::string name = readLine("  Volunteer name: ");
    printAreas();
    const int areaId = readIntInRange("  Base area id: ", 0, 999);

    std::cout << "  Service: 0=Rescue 1=Medical Help 2=Food\n";
    const int serviceChoice = readIntInRange("  Service: ", 0, 2);
    const int capacity = readIntInRange("  People capacity: ", 1, 100000);

    std::lock_guard<std::recursive_mutex> guard(store_.mutex);
    int newId = -1;
    const OpResult result = store_.dm.addVolunteer(name, areaId, static_cast<ServiceType>(serviceChoice),
                                                   capacity, store_.graph, &newId);
    if (!result.ok) {
        std::cout << "  ! " << result.error << '\n';
        return;
    }
    std::cout << "  Volunteer added with ID " << newId << " and saved to CSV.\n";
}

void ConsoleApp::handleRoadState(bool blocked) {
    printRoads();
    const int roadId = readIntInRange("  Road id: ", 0, 9999);

    std::lock_guard<std::recursive_mutex> guard(store_.mutex);
    const Road *road = store_.graph.roadById(roadId);
    if (!road) { std::cout << "  ! Road not found.\n"; return; }
    if (road->blocked == blocked) {
        std::cout << "  ! Road is already " << (blocked ? "blocked" : "open") << ".\n";
        return;
    }
    if (!readConfirmation(std::string("  ") + (blocked ? "Block" : "Unblock") + " this road?"))
        return;

    std::string error;
    store_.graph.setRoadBlocked(roadId, blocked, error);
    if (!store_.graph.saveRoads(store_.roadsPath(), error)) {
        store_.graph.setRoadBlocked(roadId, !blocked, error);
        std::cout << "  ! Could not save roads database.\n";
        return;
    }
    std::cout << "  Road " << roadId << (blocked ? " blocked." : " re-opened.") << '\n';
}

void ConsoleApp::handleResetAvailability() {
    std::lock_guard<std::recursive_mutex> guard(store_.mutex);
    const DisasterManagement::ResetOutcome outcome = store_.dm.resetVolunteerAvailability();
    std::cout << "  Reactivated " << outcome.reactivated << " volunteer(s).\n";
    if (outcome.skippedAssigned > 0)
        std::cout << "  Kept " << outcome.skippedAssigned
                  << " volunteer(s) assigned to active requests.\n";
}

void ConsoleApp::handleCompleteRequest() {
    printRequests(false);
    const int id = readIntInRange("  Request id to complete: ", 1, 999999);
    if (!readConfirmation("  Mark this request completed and release its volunteers?")) return;

    std::lock_guard<std::recursive_mutex> guard(store_.mutex);
    const OpResult result = store_.dm.completeRequest(id);
    if (!result.ok) { std::cout << "  ! " << result.error << '\n'; return; }
    std::cout << "  Request " << id << " completed; volunteers released.\n";
}

void ConsoleApp::handleCreateAdmin() {
    const std::string username = readLine("  New admin username: ");
    const std::string password = readLine("  New admin password: ");

    std::lock_guard<std::recursive_mutex> guard(store_.mutex);
    const OpResult result = store_.auth.registerUser(username, password, true, true);
    if (!result.ok) { std::cout << "  ! " << result.error << '\n'; return; }
    std::cout << "  Administrator account created.\n";
}

void ConsoleApp::handleChangePassword(const std::string &username) {
    const std::string current = readLine("  Current password: ");
    const std::string next = readLine("  New password: ");
    const std::string confirm = readLine("  Confirm new password: ");
    if (next != confirm) { std::cout << "  ! Passwords do not match.\n"; return; }

    std::lock_guard<std::recursive_mutex> guard(store_.mutex);
    const OpResult result = store_.auth.changePassword(username, current, next);
    if (!result.ok) { std::cout << "  ! " << result.error << '\n'; return; }
    std::cout << "  Password updated.\n";
}

void ConsoleApp::handleSubmitRequest(const std::string &username) {
    std::cout << "  Service: 0=Rescue 1=Medical Help 2=Food\n";
    const int serviceChoice = readIntInRange("  Service: ", 0, 2);
    printAreas();
    const int areaId = readIntInRange("  Affected area id: ", 0, 999);
    const int people = readIntInRange("  Number of affected people: ", 1, 100000);
    const std::string message = readLine("  Additional message (optional): ");

    std::lock_guard<std::recursive_mutex> guard(store_.mutex);
    const RequestOutcome outcome =
        store_.dm.submitRequest(static_cast<ServiceType>(serviceChoice), areaId, people, message,
                                username, store_.graph);
    if (!outcome.result.ok) {
        std::cout << "  ! " << outcome.result.error << '\n';
        return;
    }
    std::cout << "\n  Request #" << outcome.request.id << " recorded.\n";
    std::cout << "  People covered: " << outcome.request.peopleCovered << " / "
              << outcome.request.peopleAffected << '\n';
    std::cout << "  Volunteers assigned: " << outcome.allocations.size() << '\n';
    for (std::size_t i = 0; i < outcome.allocations.size(); ++i) {
        const AllocationEntry &e = outcome.allocations[i];
        std::cout << "    - " << e.name << " (" << e.areaName << ", capacity " << e.capacity;
        if (!e.local) std::cout << ", " << e.distanceKm << " km away";
        std::cout << ")\n";
    }
    if (outcome.peopleRemaining > 0)
        std::cout << "  ! " << outcome.peopleRemaining
                  << " people still need assistance -- no further volunteers available.\n";
}

void ConsoleApp::handleMyRequests(const std::string &username) {
    const std::vector<Request> mine = store_.dm.requestsForOwner(username);
    if (mine.empty()) { std::cout << "\n  You have not submitted any requests.\n"; return; }
    std::cout << "\n  " << std::left << std::setw(4) << "ID" << std::setw(15) << "Service"
              << std::setw(20) << "Area" << std::setw(12) << "Covered" << "Status\n";
    printRule('-', 70);
    for (std::size_t i = 0; i < mine.size(); ++i) {
        std::cout << "  " << std::left << std::setw(4) << mine[i].id
                  << std::setw(15) << serviceToString(mine[i].service)
                  << std::setw(20) << store_.graph.areaName(mine[i].areaId)
                  << std::setw(12) << (std::to_string(mine[i].peopleCovered) + "/" +
                                       std::to_string(mine[i].peopleAffected))
                  << requestStatusToString(mine[i].status) << '\n';
    }
}

/* ==========================================================================
 *  menus
 * ========================================================================*/
void ConsoleApp::adminSession(const std::string &username) {
    while (true) {
        printTitle("ADMIN CONTROL PANEL");
        std::cout << "  1. View volunteers            2. View disaster requests\n"
                     "  3. Find shortest route        4. Add volunteer\n"
                     "  5. View city roads            6. Block a road\n"
                     "  7. Unblock a road             8. Reset volunteer availability\n"
                     "  9. Complete a request        10. Create admin account\n"
                     " 11. Change my password         0. Log out\n";
        const int choice = readIntInRange("  Choice: ", 0, 11);

        std::lock_guard<std::recursive_mutex> guard(store_.mutex);
        switch (choice) {
            case 1: printVolunteers(); break;
            case 2: printRequests(true); break;
            case 3: handleShortestRoute(); break;
            case 4: handleAddVolunteer(); break;
            case 5: printRoads(); break;
            case 6: handleRoadState(true); break;
            case 7: handleRoadState(false); break;
            case 8: handleResetAvailability(); break;
            case 9: handleCompleteRequest(); break;
            case 10: handleCreateAdmin(); break;
            case 11: handleChangePassword(username); break;
            case 0: return;
            default: break;
        }
        pauseConsole();
    }
}

void ConsoleApp::personSession(const std::string &username) {
    while (true) {
        printTitle("CITIZEN SERVICES");
        std::cout << "  1. Submit disaster request   2. Find shortest route\n"
                     "  3. My requests               4. Change my password\n"
                     "  0. Log out\n";
        const int choice = readIntInRange("  Choice: ", 0, 4);

        std::lock_guard<std::recursive_mutex> guard(store_.mutex);
        switch (choice) {
            case 1: handleSubmitRequest(username); break;
            case 2: handleShortestRoute(); break;
            case 3: handleMyRequests(username); break;
            case 4: handleChangePassword(username); break;
            case 0: return;
            default: break;
        }
        pauseConsole();
    }
}

}  /* namespace dms */
