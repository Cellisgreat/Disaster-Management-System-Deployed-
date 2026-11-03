#include "api_json.h"

#include <cmath>
#include <sstream>

namespace api {

using nlohmann::json;
using namespace dms;

double round2(double value) {
    return std::floor(value * 100.0 + 0.5) / 100.0;
}

json areaJson(const DataStore &store, const Area &area) {
    double x = 0.0, y = 0.0;
    store.layoutFor(area.id, x, y);

    int volunteersHere = 0, availableHere = 0;
    for (int i = 0; i < store.dm.volunteerCount(); ++i) {
        const Volunteer *v = store.dm.volunteerAt(i);
        if (!v || v->areaId != area.id) continue;
        ++volunteersHere;
        if (v->available) ++availableHere;
    }

    int openRoads = 0;
    for (int i = 0; i < store.graph.roadCount(); ++i) {
        const Road *r = store.graph.roadAt(i);
        if (!r) continue;
        if (r->fromArea != area.id && r->toArea != area.id) continue;
        if (!r->blocked) ++openRoads;
    }

    json j;
    j["id"] = area.id;
    j["name"] = area.name;
    j["x"] = round2(x);
    j["y"] = round2(y);
    j["volunteers"] = volunteersHere;
    j["availableVolunteers"] = availableHere;
    j["openRoads"] = openRoads;
    return j;
}

json roadJson(const DataStore &store, const Road &road) {
    json j;
    j["id"] = road.id;
    j["fromAreaId"] = road.fromArea;
    j["toAreaId"] = road.toArea;
    j["fromAreaName"] = store.graph.areaName(road.fromArea);
    j["toAreaName"] = store.graph.areaName(road.toArea);
    j["distanceKm"] = round2(road.distanceKm);
    j["blocked"] = road.blocked;
    j["status"] = road.blocked ? "Blocked" : "Open";
    return j;
}

json volunteerJson(const DataStore &store, const Volunteer &v) {
    json j;
    j["id"] = v.id;
    j["name"] = v.name;
    j["areaId"] = v.areaId;
    j["areaName"] = store.graph.areaName(v.areaId);
    j["service"] = serviceToString(v.service);
    j["capacity"] = v.capacity;
    j["available"] = v.available;

    const std::vector<int> active = activeRequestsFor(store, v.id);
    j["activeRequestIds"] = active;
    j["assignmentCount"] = static_cast<int>(active.size());
    if (v.available && active.empty()) j["status"] = "Available";
    else if (!active.empty()) j["status"] = "Assigned";
    else j["status"] = "Unavailable";

    /* who they are currently helping (nice detail for the admin table) */
    json areas = json::array();
    for (std::size_t i = 0; i < active.size(); ++i) {
        const Request *r = store.dm.requestById(active[i]);
        if (r) areas.push_back(store.graph.areaName(r->areaId));
    }
    j["assignedAreas"] = areas;
    return j;
}

std::vector<int> activeRequestsFor(const DataStore &store, int volunteerId) {
    std::vector<int> ids;
    for (int i = 0; i < store.dm.requestCount(); ++i) {
        const Request *r = store.dm.requestAt(i);
        if (!r || r->status != REQUEST_ACTIVE) continue;
        for (std::size_t k = 0; k < r->assignedVolunteerIds.size(); ++k) {
            if (r->assignedVolunteerIds[k] == volunteerId) { ids.push_back(r->id); break; }
        }
    }
    return ids;
}

json requestJson(const DataStore &store, const Request &r) {
    json j;
    j["id"] = r.id;
    j["service"] = serviceToString(r.service);
    j["areaId"] = r.areaId;
    j["areaName"] = store.graph.areaName(r.areaId);
    j["peopleAffected"] = r.peopleAffected;
    j["peopleCovered"] = r.peopleCovered;
    j["peopleRemaining"] = (r.peopleAffected > r.peopleCovered)
                               ? (r.peopleAffected - r.peopleCovered) : 0;
    j["progressPct"] = (r.peopleAffected > 0)
        ? round2(100.0 * static_cast<double>(r.peopleCovered) / static_cast<double>(r.peopleAffected))
        : 0.0;
    j["status"] = requestStatusToString(r.status);
    j["statusCode"] = (r.status == REQUEST_COMPLETED) ? "completed" : "active";
    j["fullyCovered"] = r.peopleCovered >= r.peopleAffected;
    j["underCovered"] = (r.status == REQUEST_ACTIVE) && (r.peopleCovered < r.peopleAffected);
    j["message"] = r.message;
    j["owner"] = r.ownerUsername;
    j["assignedVolunteerCount"] = static_cast<int>(r.assignedVolunteerIds.size());

    json vols = json::array();
    for (std::size_t i = 0; i < r.assignedVolunteerIds.size(); ++i) {
        const Volunteer *v = store.dm.volunteerById(r.assignedVolunteerIds[i]);
        json vj;
        vj["id"] = r.assignedVolunteerIds[i];
        if (v) {
            vj["name"] = v->name;
            vj["service"] = serviceToString(v->service);
            vj["capacity"] = v->capacity;
            vj["areaId"] = v->areaId;
            vj["areaName"] = store.graph.areaName(v->areaId);
            vj["available"] = v->available;
            vj["recordFound"] = true;
        } else {
            /* volunteer deleted from the database but still referenced */
            vj["name"] = "Unknown volunteer";
            vj["recordFound"] = false;
        }
        vols.push_back(vj);
    }
    j["assignedVolunteers"] = vols;
    return j;
}

json allocationJson(const AllocationEntry &e) {
    json j;
    j["volunteerId"] = e.volunteerId;
    j["name"] = e.name;
    j["areaId"] = e.areaId;
    j["areaName"] = e.areaName;
    j["service"] = serviceToString(e.service);
    j["capacity"] = e.capacity;
    j["distanceKm"] = round2(e.distanceKm);
    j["local"] = e.local;
    return j;
}

json accountJson(const User &user) {
    json j;
    j["username"] = user.username;
    j["role"] = roleToString(user.role);
    j["isAdmin"] = (user.role == ROLE_ADMIN);
    /* passwordHash is deliberately NOT serialised */
    return j;
}

json areasListJson(const DataStore &store) {
    json arr = json::array();
    for (int i = 0; i < store.graph.areaCount(); ++i) {
        const Area *a = store.graph.areaAt(i);
        if (a) arr.push_back(areaJson(store, *a));
    }
    return arr;
}

json roadsListJson(const DataStore &store) {
    json arr = json::array();
    for (int i = 0; i < store.graph.roadCount(); ++i) {
        const Road *r = store.graph.roadAt(i);
        if (r) arr.push_back(roadJson(store, *r));
    }
    return arr;
}

json graphJson(const DataStore &store) {
    json j;
    j["areas"] = areasListJson(store);
    j["roads"] = roadsListJson(store);
    json stats;
    stats["areas"] = store.graph.areaCount();
    stats["roads"] = store.graph.roadCount();
    stats["openRoads"] = store.graph.openRoadCount();
    stats["blockedRoads"] = store.graph.blockedRoadCount();
    stats["connected"] = store.graph.isConnected();
    j["stats"] = stats;
    /* canvas bounds so the frontend can scale the schematic drawing */
    double maxX = 0.0, maxY = 0.0;
    for (int i = 0; i < store.graph.areaCount(); ++i) {
        const Area *a = store.graph.areaAt(i);
        if (!a) continue;
        double x = 0.0, y = 0.0;
        store.layoutFor(a->id, x, y);
        if (x > maxX) maxX = x;
        if (y > maxY) maxY = y;
    }
    json canvas;
    canvas["width"] = (maxX > 0.0) ? (maxX + 80.0) : 1000.0;
    canvas["height"] = (maxY > 0.0) ? (maxY + 80.0) : 700.0;
    j["canvas"] = canvas;
    return j;
}

json volunteersListJson(const DataStore &store) {
    json arr = json::array();
    for (int i = 0; i < store.dm.volunteerCount(); ++i) {
        const Volunteer *v = store.dm.volunteerAt(i);
        if (v) arr.push_back(volunteerJson(store, *v));
    }
    return arr;
}

json requestsListJson(const DataStore &store, const std::vector<Request> &requests) {
    json arr = json::array();
    for (std::size_t i = 0; i < requests.size(); ++i)
        arr.push_back(requestJson(store, requests[i]));
    return arr;
}

json statsJson(const DataStore &store) {
    const SystemStats s = store.dm.computeStats();
    json j;
    j["volunteers"] = {
        {"total", s.totalVolunteers},
        {"available", s.availableVolunteers},
        {"assigned", s.assignedVolunteers},
        {"byService", {{"Rescue", s.volunteersByService[SERVICE_RESCUE]},
                       {"Medical Help", s.volunteersByService[SERVICE_MEDICAL]},
                       {"Food", s.volunteersByService[SERVICE_FOOD]}}},
        {"availableByService", {{"Rescue", s.availableByService[SERVICE_RESCUE]},
                                {"Medical Help", s.availableByService[SERVICE_MEDICAL]},
                                {"Food", s.availableByService[SERVICE_FOOD]}}}
    };
    j["requests"] = {
        {"total", s.totalRequests},
        {"active", s.activeRequests},
        {"completed", s.completedRequests},
        {"underCovered", s.underCoveredActive},
        {"unassigned", s.unassignedActive},
        {"byService", {{"Rescue", s.requestsByService[SERVICE_RESCUE]},
                       {"Medical Help", s.requestsByService[SERVICE_MEDICAL]},
                       {"Food", s.requestsByService[SERVICE_FOOD]}}}
    };
    j["people"] = {
        {"affectedTotal", s.totalPeopleAffected},
        {"coveredTotal", s.totalPeopleCovered},
        {"affectedActive", s.activePeopleAffected},
        {"coveredActive", s.activePeopleCovered},
        {"coveragePct", (s.totalPeopleAffected > 0)
             ? round2(100.0 * s.totalPeopleCovered / s.totalPeopleAffected) : 0.0}
    };
    j["city"] = {
        {"areas", store.graph.areaCount()},
        {"roads", store.graph.roadCount()},
        {"openRoads", store.graph.openRoadCount()},
        {"blockedRoads", store.graph.blockedRoadCount()}
    };
    j["accounts"] = {
        {"total", store.auth.userCount()},
        {"admins", store.auth.adminCount()}
    };
    return j;
}

json routeJson(const DataStore &store, const RouteResult &route, int fromArea, int toArea) {
    json j;
    j["found"] = route.found;
    j["from"] = {{"id", fromArea}, {"name", store.graph.areaName(fromArea)}};
    j["to"] = {{"id", toArea}, {"name", store.graph.areaName(toArea)}};
    j["totalKm"] = round2(route.totalKm);
    j["pathText"] = store.graph.describePath(route.areaIds);

    json usedRoads = json::array();
    json nodes = json::array();
    json steps = json::array();
    if (route.found) {
        for (std::size_t i = 0; i < route.areaIds.size(); ++i) {
            nodes.push_back({{"id", route.areaIds[i]},
                             {"name", store.graph.areaName(route.areaIds[i])},
                             {"cumulativeKm", round2(route.distances[i])}});
        }
        for (std::size_t i = 0; i < route.steps.size(); ++i) {
            const RouteStep &st = route.steps[i];
            usedRoads.push_back(st.roadId);
            steps.push_back({{"roadId", st.roadId},
                             {"fromAreaId", st.fromArea},
                             {"fromAreaName", store.graph.areaName(st.fromArea)},
                             {"toAreaId", st.toArea},
                             {"toAreaName", store.graph.areaName(st.toArea)},
                             {"distanceKm", round2(st.distanceKm)}});
        }
    }
    j["path"] = nodes;
    j["steps"] = steps;
    j["roadIds"] = usedRoads;
    return j;
}

}  /* namespace api */
