#include "api_routes.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <sstream>
#include <vector>

#include "api_json.h"
#include "csv_utils.h"
#include "json.hpp"

namespace api {

using nlohmann::json;
using namespace dms;

/* ==========================================================================
 *  Login throttle
 * ========================================================================*/
LoginThrottle::LoginThrottle() {}

bool LoginThrottle::blocked(const std::string &key) {
    std::lock_guard<std::mutex> guard(mutex_);
    std::map<std::string, Entry>::iterator it = entries_.find(key);
    if (it == entries_.end()) return false;
    if (std::time(0) - it->second.firstFailure > WINDOW_SECONDS) {
        entries_.erase(it);
        return false;
    }
    return it->second.failures >= MAX_FAILURES;
}

void LoginThrottle::recordFailure(const std::string &key) {
    std::lock_guard<std::mutex> guard(mutex_);
    Entry &e = entries_[key];
    if (e.failures == 0) e.firstFailure = std::time(0);
    ++e.failures;
}

void LoginThrottle::reset(const std::string &key) {
    std::lock_guard<std::mutex> guard(mutex_);
    entries_.erase(key);
}

/* ==========================================================================
 *  helpers (file-local)
 * ========================================================================*/
namespace {

const std::time_t SERVER_START = std::time(0);

void sendJson(httplib::Response &res, const json &payload, int status = 200) {
    res.status = status;
    res.set_content(payload.dump(), "application/json; charset=utf-8");
}

void sendError(httplib::Response &res, int status, const std::string &message,
               const std::string &code) {
    json j;
    j["error"] = message;
    j["code"] = code;
    sendJson(res, j, status);
}

std::string cookieValue(const httplib::Request &req, const std::string &name) {
    if (!req.has_header("Cookie")) return "";
    const std::string cookies = req.get_header_value("Cookie");
    std::string::size_type pos = 0;
    while (pos < cookies.size()) {
        std::string::size_type end = cookies.find(';', pos);
        if (end == std::string::npos) end = cookies.size();
        std::string pair = cookies.substr(pos, end - pos);
        const std::string::size_type eq = pair.find('=');
        if (eq != std::string::npos) {
            const std::string key = trim(pair.substr(0, eq));
            if (key == name) return trim(pair.substr(eq + 1));
        }
        pos = end + 1;
    }
    return "";
}

void setSessionCookie(httplib::Response &res, const std::string &token) {
    std::ostringstream oss;
    oss << SESSION_COOKIE << '=' << token
        << "; Path=/; HttpOnly; SameSite=Strict; Max-Age=" << SESSION_TTL_SECONDS;
    res.set_header("Set-Cookie", oss.str());
}

void clearSessionCookie(httplib::Response &res) {
    std::ostringstream oss;
    oss << SESSION_COOKIE << "=deleted; Path=/; HttpOnly; SameSite=Strict; Max-Age=0";
    res.set_header("Set-Cookie", oss.str());
}

/** Looks the session up without writing a response. */
bool lookupSession(DataStore &store, const httplib::Request &req, Session &out) {
    const std::string token = cookieValue(req, SESSION_COOKIE);
    if (token.empty()) return false;
    if (!store.sessions.lookup(token, out)) return false;
    /* the account must still exist */
    if (!store.auth.findUser(out.username)) {
        store.sessions.destroy(token);
        return false;
    }
    return true;
}

bool requireSession(DataStore &store, const httplib::Request &req, httplib::Response &res,
                    Session &out) {
    if (lookupSession(store, req, out)) return true;
    sendError(res, 401, "Authentication required. Please sign in.", "unauthorized");
    return false;
}

bool requireAdmin(DataStore &store, const httplib::Request &req, httplib::Response &res,
                  Session &out) {
    if (!requireSession(store, req, res, out)) return false;
    if (out.role != ROLE_ADMIN) {
        sendError(res, 403, "Administrator privileges are required for this operation.",
                  "forbidden");
        return false;
    }
    return true;
}

bool parseBody(const httplib::Request &req, httplib::Response &res, json &out) {
    if (req.body.empty()) {
        sendError(res, 400, "Request body is required.", "empty_body");
        return false;
    }
    if (req.has_header("Content-Type")) {
        const std::string ct = req.get_header_value("Content-Type");
        if (ct.find("application/json") == std::string::npos) {
            sendError(res, 415, "Content-Type must be application/json.", "unsupported_media_type");
            return false;
        }
    }
    try {
        out = json::parse(req.body);
    } catch (const std::exception &) {
        sendError(res, 400, "Malformed JSON body.", "invalid_json");
        return false;
    }
    if (!out.is_object()) {
        sendError(res, 400, "Request body must be a JSON object.", "invalid_json");
        return false;
    }
    return true;
}

/** Cheap CSRF guard: reject mutating requests whose Origin is another site. */
bool sameOrigin(const httplib::Request &req) {
    if (!req.has_header("Origin")) return true;         /* same-origin fetch may omit it */
    const std::string origin = req.get_header_value("Origin");
    if (origin == "null") return false;
    const std::string host = req.get_header_value("Host");
    if (host.empty()) return true;
    const std::string::size_type scheme = origin.find("://");
    if (scheme == std::string::npos) return false;
    const std::string originHost = origin.substr(scheme + 3);
    return originHost == host;
}

bool guardMutation(const httplib::Request &req, httplib::Response &res) {
    if (sameOrigin(req)) return true;
    sendError(res, 403, "Cross-site request blocked.", "csrf");
    return false;
}

std::string clientKey(const httplib::Request &req, const std::string &username) {
    return req.remote_addr + "|" + username;
}

bool readInt(const json &body, const char *field, int &out) {
    if (!body.contains(field)) return false;
    const json &v = body[field];
    if (v.is_number_integer()) { out = v.get<int>(); return true; }
    if (v.is_number()) { out = static_cast<int>(v.get<double>()); return true; }
    if (v.is_string()) {
        const std::string t = trim(v.get<std::string>());
        if (t.empty()) return false;
        char *end = 0;
        const long parsed = std::strtol(t.c_str(), &end, 10);
        if (end == t.c_str() || *end != '\0') return false;
        out = static_cast<int>(parsed);
        return true;
    }
    return false;
}

bool readString(const json &body, const char *field, std::string &out) {
    if (!body.contains(field)) return false;
    const json &v = body[field];
    if (v.is_string()) { out = v.get<std::string>(); return true; }
    if (v.is_number()) { out = v.dump(); return true; }
    return false;
}

bool readBool(const json &body, const char *field, bool &out) {
    if (!body.contains(field)) return false;
    const json &v = body[field];
    if (v.is_boolean()) { out = v.get<bool>(); return true; }
    if (v.is_number_integer()) { out = v.get<int>() != 0; return true; }
    if (v.is_string()) { out = toBoolFlag(v.get<std::string>(), false); return true; }
    return false;
}

json userPayload(const Session &s) {
    json j;
    j["username"] = s.username;
    j["role"] = roleToString(s.role);
    j["isAdmin"] = (s.role == ROLE_ADMIN);
    return j;
}

json sessionResponse(const Session &s) {
    json j;
    j["authenticated"] = true;
    j["user"] = userPayload(s);
    j["redirect"] = (s.role == ROLE_ADMIN) ? "/admin.html" : "/person.html";
    return j;
}

json requestArray(DataStore &store, const std::vector<Request> &requests) {
    json arr = json::array();
    for (std::size_t i = 0; i < requests.size(); ++i)
        arr.push_back(requestJson(store, requests[i]));
    /* newest first */
    std::sort(arr.begin(), arr.end(), [](const json &a, const json &b) {
        return a["id"].get<int>() > b["id"].get<int>();
    });
    return arr;
}

json allRequestsSorted(DataStore &store) {
    std::vector<Request> all;
    for (int i = 0; i < store.dm.requestCount(); ++i) {
        const Request *r = store.dm.requestAt(i);
        if (r) all.push_back(*r);
    }
    return requestArray(store, all);
}

}  /* anonymous namespace */

/* ==========================================================================
 *  Public API routes
 * ========================================================================*/
void registerApiRoutes(httplib::Server &server, DataStore &store, LoginThrottle &throttle) {

    /* ------------------------------------------------------------------ meta */
    server.Get("/api/health", [&store](const httplib::Request &, httplib::Response &res) {
        json j;
        j["status"] = "ok";
        j["uptimeSeconds"] = static_cast<int>(std::time(0) - SERVER_START);
        j["areas"] = store.graph.areaCount();
        j["roads"] = store.graph.roadCount();
        j["volunteers"] = store.dm.volunteerCount();
        j["requests"] = store.dm.requestCount();
        sendJson(res, j);
    });

    server.Get("/api/meta", [](const httplib::Request &, httplib::Response &res) {
        json j;
        j["name"] = "CivicShield Disaster Management System";
        j["version"] = "2.0.0-web";
        j["backend"] = "C++17 (cpp-httplib)";
        j["services"] = json::array({"Rescue", "Medical Help", "Food"});
        j["roles"] = json::array({"ADMIN", "PERSON"});
        j["disclaimer"] =
            "Educational demonstration system for a college capstone project. "
            "It is not a certified emergency dispatch service. In a real emergency "
            "contact your local emergency number immediately.";
        j["demoNotice"] =
            "Demo administrator credentials are documented in the project README. "
            "Change the demo password from the Account settings page.";
        sendJson(res, j);
    });

    /* public aggregate counters for the landing page */
    server.Get("/api/public/summary", [&store](const httplib::Request &, httplib::Response &res) {
        std::lock_guard<std::recursive_mutex> guard(store.mutex);
        const SystemStats s = store.dm.computeStats();
        json j;
        j["volunteers"] = s.totalVolunteers;
        j["availableVolunteers"] = s.availableVolunteers;
        j["activeRequests"] = s.activeRequests;
        j["completedRequests"] = s.completedRequests;
        j["peopleCovered"] = s.totalPeopleCovered;
        j["peopleAffected"] = s.totalPeopleAffected;
        j["areas"] = store.graph.areaCount();
        j["roads"] = store.graph.roadCount();
        j["services"] = json::array({"Rescue", "Medical Help", "Food"});
        sendJson(res, j);
    });

    /* ------------------------------------------------------------------ auth */
    server.Post("/api/auth/login", [&store, &throttle](const httplib::Request &req,
                                                      httplib::Response &res) {
        if (!guardMutation(req, res)) return;
        json body;
        if (!parseBody(req, res, body)) return;

        std::string username, password;
        if (!readString(body, "username", username) || !readString(body, "password", password)) {
            sendError(res, 400, "Username and password are required.", "invalid_input");
            return;
        }

        const std::string key = clientKey(req, username);
        if (throttle.blocked(key)) {
            sendError(res, 429, "Too many failed attempts. Please wait a few minutes.",
                      "rate_limited");
            return;
        }

        std::lock_guard<std::recursive_mutex> guard(store.mutex);
        AuthResult result = store.auth.login(username, password);
        if (!result.ok) {
            throttle.recordFailure(key);
            sendError(res, 401, result.error, result.code);
            return;
        }
        throttle.reset(key);

        Session session;
        session.username = result.username;
        session.role = result.role;
        const std::string token = store.sessions.create(result.username, result.role);
        setSessionCookie(res, token);

        json j = sessionResponse(session);
        if (result.migrated) {
            j["notice"] = "Your password hash was upgraded to PBKDF2-HMAC-SHA256.";
        }
        sendJson(res, j);
    });

    server.Post("/api/auth/logout", [&store](const httplib::Request &req, httplib::Response &res) {
        if (!guardMutation(req, res)) return;
        const std::string token = cookieValue(req, SESSION_COOKIE);
        if (!token.empty()) store.sessions.destroy(token);
        clearSessionCookie(res);
        json j;
        j["ok"] = true;
        j["message"] = "Signed out.";
        sendJson(res, j);
    });

    server.Get("/api/auth/me", [&store](const httplib::Request &req, httplib::Response &res) {
        Session session;
        if (!lookupSession(store, req, session)) {
            json j;
            j["authenticated"] = false;
            sendJson(res, j);
            return;
        }
        sendJson(res, sessionResponse(session));
    });

    server.Post("/api/auth/register", [&store](const httplib::Request &req, httplib::Response &res) {
        if (!guardMutation(req, res)) return;
        json body;
        if (!parseBody(req, res, body)) return;

        std::string username, password;
        if (!readString(body, "username", username) || !readString(body, "password", password)) {
            sendError(res, 400, "Username and password are required.", "invalid_input");
            return;
        }
        bool wantsAdmin = false;
        readBool(body, "admin", wantsAdmin);

        Session caller;
        const bool callerIsAdmin = lookupSession(store, req, caller) && caller.role == ROLE_ADMIN;

        std::lock_guard<std::recursive_mutex> guard(store.mutex);
        OpResult result = store.auth.registerUser(username, password, wantsAdmin, callerIsAdmin);
        if (!result.ok) {
            const int status = (result.code == "forbidden") ? 403
                             : (result.code == "username_taken") ? 409
                             : (result.code == "storage") ? 500 : 400;
            sendError(res, status, result.error, result.code);
            return;
        }

        json j;
        j["message"] = "Account created successfully.";
        const User *created = store.auth.findUser(username);
        if (created) j["user"] = accountJson(*created);

        /* Self-registration signs the new citizen in; admin-created accounts do
         * not disturb the administrator's own session. */
        if (!callerIsAdmin) {
            const std::string token =
                store.sessions.create(created->username, created->role);
            setSessionCookie(res, token);
            j["authenticated"] = true;
            j["redirect"] = "/person.html";
            Session createdSession;
            createdSession.username = created->username;
            createdSession.role = created->role;
            j["user"] = userPayload(createdSession);
        } else {
            j["authenticated"] = false;
        }
        sendJson(res, j, 201);
    });

    server.Post("/api/auth/password", [&store](const httplib::Request &req,
                                              httplib::Response &res) {
        if (!guardMutation(req, res)) return;
        Session session;
        if (!requireSession(store, req, res, session)) return;

        json body;
        if (!parseBody(req, res, body)) return;
        std::string currentPassword, newPassword;
        if (!readString(body, "currentPassword", currentPassword) ||
            !readString(body, "newPassword", newPassword)) {
            sendError(res, 400, "Current and new password are required.", "invalid_input");
            return;
        }

        std::lock_guard<std::recursive_mutex> guard(store.mutex);
        OpResult result = store.auth.changePassword(session.username, currentPassword, newPassword);
        if (!result.ok) {
            const int status = (result.code == "invalid_credentials") ? 401
                             : (result.code == "storage") ? 500 : 400;
            sendError(res, status, result.error, result.code);
            return;
        }

        /* Invalidate every existing session, then issue a fresh one. */
        store.sessions.destroyForUser(session.username);
        const std::string token = store.sessions.create(session.username, session.role);
        setSessionCookie(res, token);

        json j;
        j["ok"] = true;
        j["message"] = "Password updated. Other sessions have been signed out.";
        sendJson(res, j);
    });

    /* ------------------------------------------------------------ city graph */
    server.Get("/api/areas", [&store](const httplib::Request &, httplib::Response &res) {
        std::lock_guard<std::recursive_mutex> guard(store.mutex);
        json j;
        j["areas"] = areasListJson(store);
        sendJson(res, j);
    });

    server.Get("/api/roads", [&store](const httplib::Request &, httplib::Response &res) {
        std::lock_guard<std::recursive_mutex> guard(store.mutex);
        json j;
        j["roads"] = roadsListJson(store);
        sendJson(res, j);
    });

    server.Get("/api/graph", [&store](const httplib::Request &, httplib::Response &res) {
        std::lock_guard<std::recursive_mutex> guard(store.mutex);
        sendJson(res, graphJson(store));
    });

    server.Get("/api/routes", [&store](const httplib::Request &req, httplib::Response &res) {
        Session session;
        if (!requireSession(store, req, res, session)) return;

        if (!req.has_param("from") || !req.has_param("to")) {
            sendError(res, 400, "Query parameters 'from' and 'to' are required.", "invalid_input");
            return;
        }
        const int fromArea = toInt(req.get_param_value("from"), -1);
        const int toArea = toInt(req.get_param_value("to"), -1);

        std::lock_guard<std::recursive_mutex> guard(store.mutex);
        if (!store.graph.hasArea(fromArea) || !store.graph.hasArea(toArea)) {
            sendError(res, 400, "Unknown area id. Use /api/areas to list valid ids.",
                      "invalid_area");
            return;
        }

        const RouteResult route = store.graph.shortestPath(fromArea, toArea);
        json j = routeJson(store, route, fromArea, toArea);
        if (!route.found) {
            j["message"] = "No open route connects these areas. Some roads may be blocked.";
        }
        sendJson(res, j);
    });

    /* -------------------------------------------------------------- requests */
    server.Post("/api/requests", [&store](const httplib::Request &req, httplib::Response &res) {
        if (!guardMutation(req, res)) return;
        Session session;
        if (!requireSession(store, req, res, session)) return;

        json body;
        if (!parseBody(req, res, body)) return;

        std::string serviceText, message;
        int areaId = -1, peopleAffected = 0;
        if (!readString(body, "service", serviceText) || !readInt(body, "areaId", areaId) ||
            !readInt(body, "peopleAffected", peopleAffected)) {
            sendError(res, 400, "service, areaId and peopleAffected are required.",
                      "invalid_input");
            return;
        }
        readString(body, "message", message);

        ServiceType service;
        if (!parseService(serviceText, service)) {
            sendError(res, 400, "Unknown service. Use Rescue, Medical Help or Food.",
                      "invalid_service");
            return;
        }

        std::lock_guard<std::recursive_mutex> guard(store.mutex);
        RequestOutcome outcome = store.dm.submitRequest(service, areaId, peopleAffected, message,
                                                       session.username, store.graph);
        if (!outcome.result.ok) {
            const int status = (outcome.result.code == "storage") ? 500 : 400;
            sendError(res, status, outcome.result.error, outcome.result.code);
            return;
        }

        json j;
        j["message"] = outcome.peopleRemaining > 0
            ? "Request recorded. Some affected people still need assistance."
            : "Request recorded and volunteers dispatched.";
        j["request"] = requestJson(store, outcome.request);
        json allocations = json::array();
        for (std::size_t i = 0; i < outcome.allocations.size(); ++i)
            allocations.push_back(allocationJson(outcome.allocations[i]));
        j["allocations"] = allocations;
        j["peopleRemaining"] = outcome.peopleRemaining;
        sendJson(res, j, 201);
    });

    server.Get("/api/requests/my", [&store](const httplib::Request &req, httplib::Response &res) {
        Session session;
        if (!requireSession(store, req, res, session)) return;
        std::lock_guard<std::recursive_mutex> guard(store.mutex);
        json j;
        j["requests"] = requestArray(store, store.dm.requestsForOwner(session.username));
        sendJson(res, j);
    });

    server.Get(R"(/api/requests/(\d+))", [&store](const httplib::Request &req,
                                                  httplib::Response &res) {
        Session session;
        if (!requireSession(store, req, res, session)) return;
        const int id = toInt(req.matches[1].str(), -1);

        std::lock_guard<std::recursive_mutex> guard(store.mutex);
        const Request *r = store.dm.requestById(id);
        if (!r) { sendError(res, 404, "Request not found.", "not_found"); return; }
        if (session.role != ROLE_ADMIN && r->ownerUsername != session.username) {
            sendError(res, 403, "You can only view your own requests.", "forbidden");
            return;
        }
        json j;
        j["request"] = requestJson(store, *r);
        sendJson(res, j);
    });

    /* ------------------------------------------------------- admin: volunteers */
    server.Get("/api/admin/volunteers", [&store](const httplib::Request &req,
                                                 httplib::Response &res) {
        Session session;
        if (!requireAdmin(store, req, res, session)) return;
        std::lock_guard<std::recursive_mutex> guard(store.mutex);
        json j;
        j["volunteers"] = volunteersListJson(store);
        sendJson(res, j);
    });

    server.Post("/api/admin/volunteers", [&store](const httplib::Request &req,
                                                  httplib::Response &res) {
        if (!guardMutation(req, res)) return;
        Session session;
        if (!requireAdmin(store, req, res, session)) return;

        json body;
        if (!parseBody(req, res, body)) return;

        std::string name, serviceText;
        int areaId = -1, capacity = 0;
        if (!readString(body, "name", name) || !readInt(body, "areaId", areaId) ||
            !readInt(body, "capacity", capacity) || !readString(body, "service", serviceText)) {
            sendError(res, 400, "name, areaId, service and capacity are required.",
                      "invalid_input");
            return;
        }
        ServiceType service;
        if (!parseService(serviceText, service)) {
            sendError(res, 400, "Unknown service. Use Rescue, Medical Help or Food.",
                      "invalid_service");
            return;
        }

        std::lock_guard<std::recursive_mutex> guard(store.mutex);
        int newId = -1;
        OpResult result = store.dm.addVolunteer(name, areaId, service, capacity, store.graph, &newId);
        if (!result.ok) {
            const int status = (result.code == "storage") ? 500
                             : (result.code == "invalid_area") ? 400 : 400;
            sendError(res, status, result.error, result.code);
            return;
        }
        const Volunteer *v = store.dm.volunteerById(newId);
        json j;
        j["message"] = "Volunteer registered and saved to volunteers_db.csv.";
        if (v) j["volunteer"] = volunteerJson(store, *v);
        sendJson(res, j, 201);
    });

    server.Post("/api/admin/volunteers/reset", [&store](const httplib::Request &req,
                                                       httplib::Response &res) {
        if (!guardMutation(req, res)) return;
        Session session;
        if (!requireAdmin(store, req, res, session)) return;

        std::lock_guard<std::recursive_mutex> guard(store.mutex);
        DisasterManagement::ResetOutcome outcome = store.dm.resetVolunteerAvailability();
        if (!outcome.result.ok) {
            sendError(res, 500, outcome.result.error, outcome.result.code);
            return;
        }
        json j;
        j["reactivated"] = outcome.reactivated;
        j["skippedAssigned"] = outcome.skippedAssigned;
        j["message"] = outcome.skippedAssigned > 0
            ? "Availability reset. " + std::to_string(outcome.skippedAssigned) +
              " volunteer(s) kept assigned to active requests."
            : "All volunteers not on active requests are now available.";
        sendJson(res, j);
    });

    /* --------------------------------------------------------- admin: requests */
    server.Get("/api/admin/requests", [&store](const httplib::Request &req,
                                               httplib::Response &res) {
        Session session;
        if (!requireAdmin(store, req, res, session)) return;
        std::lock_guard<std::recursive_mutex> guard(store.mutex);
        json j;
        j["requests"] = allRequestsSorted(store);
        sendJson(res, j);
    });

    server.Get(R"(/api/admin/requests/(\d+))", [&store](const httplib::Request &req,
                                                        httplib::Response &res) {
        Session session;
        if (!requireAdmin(store, req, res, session)) return;
        const int id = toInt(req.matches[1].str(), -1);
        std::lock_guard<std::recursive_mutex> guard(store.mutex);
        const Request *r = store.dm.requestById(id);
        if (!r) { sendError(res, 404, "Request not found.", "not_found"); return; }
        json j;
        j["request"] = requestJson(store, *r);
        sendJson(res, j);
    });

    server.Patch(R"(/api/admin/requests/(\d+)/complete)", [&store](const httplib::Request &req,
                                                                  httplib::Response &res) {
        if (!guardMutation(req, res)) return;
        Session session;
        if (!requireAdmin(store, req, res, session)) return;
        const int id = toInt(req.matches[1].str(), -1);

        std::lock_guard<std::recursive_mutex> guard(store.mutex);
        const Request *before = store.dm.requestById(id);
        if (!before) { sendError(res, 404, "Request not found.", "not_found"); return; }

        std::vector<int> assigned = before->assignedVolunteerIds;
        OpResult result = store.dm.completeRequest(id);
        if (!result.ok) {
            const int status = (result.code == "not_found") ? 404
                             : (result.code == "already_completed") ? 409
                             : (result.code == "storage") ? 500 : 400;
            sendError(res, status, result.error, result.code);
            return;
        }

        json released = json::array();
        for (std::size_t i = 0; i < assigned.size(); ++i) {
            const Volunteer *v = store.dm.volunteerById(assigned[i]);
            if (!v) continue;
            json vj;
            vj["id"] = v->id;
            vj["name"] = v->name;
            vj["available"] = v->available;
            released.push_back(vj);
        }

        json j;
        j["message"] = "Request marked as completed. Assigned volunteers were released.";
        j["releasedVolunteers"] = released;
        const Request *after = store.dm.requestById(id);
        if (after) j["request"] = requestJson(store, *after);
        sendJson(res, j);
    });

    /* ------------------------------------------------------------- admin: roads */
    server.Patch(R"(/api/admin/roads/(\d+))", [&store](const httplib::Request &req,
                                                       httplib::Response &res) {
        if (!guardMutation(req, res)) return;
        Session session;
        if (!requireAdmin(store, req, res, session)) return;
        const int roadId = toInt(req.matches[1].str(), -1);

        json body;
        if (!parseBody(req, res, body)) return;

        std::lock_guard<std::recursive_mutex> guard(store.mutex);
        const Road *existing = store.graph.roadById(roadId);
        if (!existing) { sendError(res, 404, "Road not found.", "not_found"); return; }

        bool blocked = !existing->blocked;      /* default: toggle */
        readBool(body, "blocked", blocked);

        const bool previous = existing->blocked;
        std::string error;
        if (!store.graph.setRoadBlocked(roadId, blocked, error)) {
            sendError(res, 404, error, "not_found");
            return;
        }
        if (!store.graph.saveRoads(store.roadsPath(), error)) {
            store.graph.setRoadBlocked(roadId, previous, error);   /* rollback */
            sendError(res, 500, "Could not save roads database.", "storage");
            return;
        }

        const Road *updated = store.graph.roadById(roadId);
        json j;
        j["message"] = blocked ? "Road blocked. Dijkstra will avoid it." : "Road re-opened.";
        if (updated) j["road"] = roadJson(store, *updated);
        json stats;
        stats["openRoads"] = store.graph.openRoadCount();
        stats["blockedRoads"] = store.graph.blockedRoadCount();
        stats["connected"] = store.graph.isConnected();
        j["cityStats"] = stats;
        sendJson(res, j);
    });

    /* ---------------------------------------------------------- admin: accounts */
    server.Get("/api/admin/accounts", [&store](const httplib::Request &req,
                                               httplib::Response &res) {
        Session session;
        if (!requireAdmin(store, req, res, session)) return;
        std::lock_guard<std::recursive_mutex> guard(store.mutex);
        json arr = json::array();
        for (int i = 0; i < store.auth.userCount(); ++i) {
            const User *u = store.auth.userAt(i);
            if (u) arr.push_back(accountJson(*u));
        }
        json j;
        j["accounts"] = arr;
        j["admins"] = store.auth.adminCount();
        sendJson(res, j);
    });

    server.Post("/api/admin/accounts", [&store](const httplib::Request &req,
                                                httplib::Response &res) {
        if (!guardMutation(req, res)) return;
        Session session;
        if (!requireAdmin(store, req, res, session)) return;

        json body;
        if (!parseBody(req, res, body)) return;
        std::string username, password, roleText = "ADMIN";
        if (!readString(body, "username", username) || !readString(body, "password", password)) {
            sendError(res, 400, "username and password are required.", "invalid_input");
            return;
        }
        readString(body, "role", roleText);
        UserRole role;
        if (!parseRole(roleText, role)) {
            sendError(res, 400, "Unknown role. Use ADMIN or PERSON.", "invalid_role");
            return;
        }

        std::lock_guard<std::recursive_mutex> guard(store.mutex);
        OpResult result = store.auth.registerUser(username, password, role == ROLE_ADMIN, true);
        if (!result.ok) {
            const int status = (result.code == "username_taken") ? 409
                             : (result.code == "storage") ? 500 : 400;
            sendError(res, status, result.error, result.code);
            return;
        }
        const User *created = store.auth.findUser(username);
        json j;
        j["message"] = "Account created.";
        if (created) j["user"] = accountJson(*created);
        sendJson(res, j, 201);
    });

    /* --------------------------------------------------------------- dashboard */
    server.Get("/api/admin/dashboard", [&store](const httplib::Request &req,
                                                httplib::Response &res) {
        Session session;
        if (!requireAdmin(store, req, res, session)) return;

        std::lock_guard<std::recursive_mutex> guard(store.mutex);
        const SystemStats s = store.dm.computeStats();

        json j;
        j["generatedAt"] = static_cast<long long>(std::time(0));
        j["stats"] = statsJson(store);
        j["me"] = userPayload(session);

        /* recent requests (newest 6) */
        std::vector<Request> all;
        for (int i = 0; i < store.dm.requestCount(); ++i) {
            const Request *r = store.dm.requestAt(i);
            if (r) all.push_back(*r);
        }
        std::sort(all.begin(), all.end(), [](const Request &a, const Request &b) {
            return a.id > b.id;
        });
        std::vector<Request> recent(all.begin(), all.begin() + std::min<std::size_t>(6, all.size()));
        j["recentRequests"] = requestArray(store, recent);

        /* volunteer availability overview */
        json overview;
        overview["total"] = s.totalVolunteers;
        overview["available"] = s.availableVolunteers;
        overview["assigned"] = s.assignedVolunteers;
        j["volunteerOverview"] = overview;

        /* service wise distribution */
        json distribution = json::array();
        const char *labels[SERVICE_COUNT] = {"Rescue", "Medical Help", "Food"};
        for (int i = 0; i < SERVICE_COUNT; ++i) {
            json row;
            row["service"] = labels[i];
            row["volunteers"] = s.volunteersByService[i];
            row["available"] = s.availableByService[i];
            row["requests"] = s.requestsByService[i];
            distribution.push_back(row);
        }
        j["serviceDistribution"] = distribution;

        /* alerts that need an administrator's attention */
        json alerts = json::array();
        for (int i = 0; i < store.dm.requestCount(); ++i) {
            const Request *r = store.dm.requestAt(i);
            if (!r || r->status != REQUEST_ACTIVE) continue;
            if (r->peopleCovered >= r->peopleAffected) continue;
            json alert;
            alert["severity"] = r->assignedVolunteerIds.empty() ? "critical" : "warning";
            alert["title"] = std::string(r->assignedVolunteerIds.empty()
                                             ? "No volunteers assigned"
                                             : "Assistance still required");
            alert["message"] = std::string(serviceToString(r->service)) + " request #" +
                               std::to_string(r->id) + " in " + store.graph.areaName(r->areaId) +
                               " covers " + std::to_string(r->peopleCovered) + " of " +
                               std::to_string(r->peopleAffected) + " people (" +
                               std::to_string(r->peopleAffected - r->peopleCovered) +
                               " still waiting).";
            alert["requestId"] = r->id;
            alerts.push_back(alert);
        }
        if (store.graph.blockedRoadCount() > 0) {
            json alert;
            alert["severity"] = "info";
            alert["title"] = "Road network partially blocked";
            alert["message"] = std::to_string(store.graph.blockedRoadCount()) +
                               " road(s) are currently blocked; routing avoids them.";
            alerts.push_back(alert);
        }
        if (!store.graph.isConnected()) {
            json alert;
            alert["severity"] = "critical";
            alert["title"] = "City network is disconnected";
            alert["message"] = "Some areas can no longer be reached using open roads.";
            alerts.push_back(alert);
        }
        for (int i = 0; i < SERVICE_COUNT; ++i) {
            if (s.availableByService[i] == 0 && s.volunteersByService[i] > 0) {
                json alert;
                alert["severity"] = "warning";
                alert["title"] = std::string("No available ") + labels[i] + " volunteers";
                alert["message"] = std::string("All ") + labels[i] +
                                   " volunteers are currently assigned to active requests.";
                alerts.push_back(alert);
            }
        }
        j["alerts"] = alerts;

        /* quick reference lists for the UI */
        json areas = json::array();
        for (int i = 0; i < store.graph.areaCount(); ++i) {
            const Area *a = store.graph.areaAt(i);
            if (a) areas.push_back({{"id", a->id}, {"name", a->name}});
        }
        j["areas"] = areas;
        sendJson(res, j);
    });
}

/* ==========================================================================
 *  Static frontend
 * ========================================================================*/
void configureStaticServing(httplib::Server &server, const std::string &publicDir) {
    server.set_file_extension_and_mimetype_mapping("js", "application/javascript");
    server.set_file_extension_and_mimetype_mapping("mjs", "application/javascript");
    server.set_file_extension_and_mimetype_mapping("css", "text/css");
    server.set_file_extension_and_mimetype_mapping("svg", "image/svg+xml");
    server.set_file_extension_and_mimetype_mapping("json", "application/json");
    server.set_file_extension_and_mimetype_mapping("webmanifest", "application/manifest+json");
    server.set_file_extension_and_mimetype_mapping("ico", "image/x-icon");

    server.set_default_headers({
        {"X-Content-Type-Options", "nosniff"},
        {"Referrer-Policy", "no-referrer"}
    });

    if (!server.set_mount_point("/", publicDir)) {
        std::fprintf(stderr,
                     "[http] WARNING: could not mount frontend directory '%s'. "
                     "Check the --public-dir option.\n", publicDir.c_str());
    }

    server.set_error_handler([](const httplib::Request &req, httplib::Response &res) {
        /* cpp-httplib invokes this for every status >= 400. When a handler
         * already produced a body (our JSON errors), leave it untouched. */
        if (!res.body.empty()) return;
        if (req.path.rfind("/api/", 0) == 0) {
            json j;
            j["error"] = "Endpoint not found.";
            j["code"] = "not_found";
            res.status = 404;
            res.set_content(j.dump(), "application/json; charset=utf-8");
            return;
        }
        if (res.status == 404) {
            res.set_content(
                "<!doctype html><html><head><meta charset='utf-8'>"
                "<title>404 - CivicShield</title></head>"
                "<body style='font-family:system-ui;background:#0f172a;color:#e2e8f0;"
                "display:flex;align-items:center;justify-content:center;height:100vh;margin:0'>"
                "<div style='text-align:center'><h1 style='font-size:48px;margin:0'>404</h1>"
                "<p style='color:#94a3b8'>That page does not exist.</p>"
                "<a href='/' style='color:#38bdf8'>Return to the home page</a></div>"
                "</body></html>",
                "text/html; charset=utf-8");
        }
    });
}

}  /* namespace api */
