#include "disaster_management.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>

#include "csv_utils.h"

namespace dms {

/* ==========================================================================
 *  small helpers
 * ========================================================================*/
namespace {

std::string joinIds(const std::vector<int> &ids, const char *sep) {
    std::ostringstream oss;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i) oss << sep;
        oss << ids[i];
    }
    return oss.str();
}

/** Parses "1;4;7" / "1,4,7" / "1 4 7" (legacy tolerant). */
std::vector<int> splitIds(const std::string &text) {
    std::vector<int> out;
    std::string token;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        const char c = (i == text.size()) ? ';' : text[i];
        if (c == ';' || c == ',' || c == '|' || c == ' ' || c == '\t') {
            const std::string t = trim(token);
            if (!t.empty()) {
                const int id = toInt(t, -1);
                if (id >= 0) out.push_back(id);
            }
            token.clear();
        } else {
            token += c;
        }
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

std::string cleanedText(const std::string &s, std::size_t maxLen) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == '\n' || c == '\r') out += ' ';
        else if (c == '\t') out += ' ';
        else if (c < 0x20) continue;                 /* drop control characters */
        else out += static_cast<char>(c);
    }
    out = trim(out);
    while (out.size() > maxLen) out.erase(out.size() - 1);
    return out;
}

}  /* anonymous namespace */

/* ==========================================================================
 *  SystemStats
 * ========================================================================*/
SystemStats::SystemStats()
    : totalVolunteers(0), availableVolunteers(0), assignedVolunteers(0),
      totalRequests(0), activeRequests(0), completedRequests(0),
      totalPeopleAffected(0), totalPeopleCovered(0), activePeopleAffected(0),
      activePeopleCovered(0), underCoveredActive(0), unassignedActive(0) {
    for (int i = 0; i < SERVICE_COUNT; ++i) {
        volunteersByService[i] = 0;
        availableByService[i] = 0;
        requestsByService[i] = 0;
    }
}

/* ==========================================================================
 *  construction / persistence
 * ========================================================================*/
DisasterManagement::DisasterManagement() : volunteerCount_(0), requestCount_(0) {}

bool DisasterManagement::loadVolunteers(const std::string &path, std::string &error) {
    volunteersPath_ = path;
    volunteerCount_ = 0;
    std::vector<std::vector<std::string> > rows;
    if (!readCsvFile(path, rows)) {
        std::fprintf(stderr, "[dms] volunteers database %s not found; starting empty\n",
                     path.c_str());
        error.clear();
        return true;
    }
    int skipped = 0;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const std::vector<std::string> &f = rows[i];
        if (f.size() < 5) { ++skipped; continue; }
        Volunteer v;
        v.id = toInt(f[0], -1);
        if (i == 0 && v.id < 0) continue;          /* header row: skip silently */
        v.name = cleanedText(f[1], 60);
        v.areaId = toInt(f[2], -1);
        if (!parseService(f[3], v.service)) { ++skipped; continue; }
        v.capacity = toInt(f[4], 0);
        v.available = (f.size() > 5) ? toBoolFlag(f[5], true) : true;
        if (v.id < 0 || v.name.empty() || v.capacity <= 0) { ++skipped; continue; }
        if (volunteerCount_ >= MAX_VOLUNTEERS) break;
        volunteers_[volunteerCount_++] = v;
    }
    if (skipped > 0)
        std::fprintf(stderr, "[dms] skipped %d malformed volunteer row(s)\n", skipped);
    rebuildAssignmentIndex();
    error.clear();
    return true;
}

bool DisasterManagement::loadRequests(const std::string &path, std::string &error) {
    requestsPath_ = path;
    requestCount_ = 0;
    std::vector<std::vector<std::string> > rows;
    if (!readCsvFile(path, rows)) {
        std::fprintf(stderr, "[dms] requests database %s not found; starting empty\n",
                     path.c_str());
        error.clear();
        rebuildAssignmentIndex();
        return true;
    }
    int skipped = 0;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const std::vector<std::string> &f = rows[i];
        if (f.size() < 5) { ++skipped; continue; }
        Request r;
        r.id = toInt(f[0], -1);
        if (i == 0 && r.id < 0) continue;          /* header row: skip silently */
        if (!parseService(f[1], r.service)) { ++skipped; continue; }
        r.areaId = toInt(f[2], -1);
        r.peopleAffected = toInt(f[3], 0);
        r.peopleCovered = toInt(f[4], 0);
        if (f.size() > 5) r.assignedVolunteerIds = splitIds(f[5]);
        if (f.size() > 6) parseRequestStatus(f[6], r.status);
        if (f.size() > 7) r.message = f[7];
        /* NEW field; absent in legacy rows -> empty owner (admin-only view). */
        if (f.size() > 8) r.ownerUsername = trim(f[8]);

        if (r.id < 0) { ++skipped; continue; }
        if (r.peopleAffected < 0) r.peopleAffected = 0;
        if (r.peopleCovered < 0) r.peopleCovered = 0;
        if (requestCount_ >= MAX_REQUESTS) break;
        requests_[requestCount_++] = r;
    }
    if (skipped > 0)
        std::fprintf(stderr, "[dms] skipped %d malformed request row(s)\n", skipped);
    rebuildAssignmentIndex();
    /* Repair availability flags: a volunteer listed on an ACTIVE request must
     * not be marked available (protects against hand-edited CSVs). */
    for (int i = 0; i < volunteerCount_; ++i) {
        if (isVolunteerBusy(volunteers_[i].id)) volunteers_[i].available = false;
    }
    error.clear();
    return true;
}

bool DisasterManagement::saveVolunteers(const std::string &path, std::string &error) const {
    std::string out;
    out += "id,name,area_id,service,capacity,available\n";
    for (int i = 0; i < volunteerCount_; ++i) {
        const Volunteer &v = volunteers_[i];
        std::ostringstream line;
        line << v.id << ',';
        line << csvEscape(v.name) << ',';
        line << v.areaId << ',';
        line << serviceToString(v.service) << ',';
        line << v.capacity << ',';
        line << (v.available ? 1 : 0);
        out += line.str();
        out += '\n';
    }
    if (!writeFileAtomic(path, out)) { error = "Could not write volunteers database."; return false; }
    error.clear();
    return true;
}

bool DisasterManagement::saveRequests(const std::string &path, std::string &error) const {
    std::string out;
    out += "id,service,area_id,people_affected,people_covered,assigned_volunteers,"
           "status,message,owner_username\n";
    for (int i = 0; i < requestCount_; ++i) {
        const Request &r = requests_[i];
        std::ostringstream line;
        line << r.id << ',';
        line << serviceToString(r.service) << ',';
        line << r.areaId << ',';
        line << r.peopleAffected << ',';
        line << r.peopleCovered << ',';
        line << joinIds(r.assignedVolunteerIds, ";") << ',';
        line << requestStatusToString(r.status) << ',';
        line << csvEscape(r.message) << ',';
        line << csvEscape(r.ownerUsername);
        out += line.str();
        out += '\n';
    }
    if (!writeFileAtomic(path, out)) { error = "Could not write requests database."; return false; }
    error.clear();
    return true;
}

/* ==========================================================================
 *  volunteers
 * ========================================================================*/
const Volunteer *DisasterManagement::volunteerAt(int index) const {
    if (index < 0 || index >= volunteerCount_) return 0;
    return &volunteers_[index];
}

int DisasterManagement::volunteerIndexOf(int id) const {
    for (int i = 0; i < volunteerCount_; ++i)
        if (volunteers_[i].id == id) return i;
    return -1;
}

const Volunteer *DisasterManagement::volunteerById(int id) const {
    const int idx = volunteerIndexOf(id);
    return (idx < 0) ? 0 : &volunteers_[idx];
}

int DisasterManagement::nextVolunteerId() const {
    int maxId = 0;
    for (int i = 0; i < volunteerCount_; ++i)
        if (volunteers_[i].id > maxId) maxId = volunteers_[i].id;
    return maxId + 1;
}

OpResult DisasterManagement::addVolunteer(const std::string &name, int areaId,
                                          ServiceType service, int capacity,
                                          const CityGraph &graph, int *outNewId) {
    const std::string cleanName = cleanedText(name, 60);
    if (cleanName.size() < 2)
        return OpResult::failure("Volunteer name must be at least 2 characters.", "invalid_name");
    if (cleanName.size() > 60)
        return OpResult::failure("Volunteer name is too long.", "invalid_name");
    if (!graph.hasArea(areaId))
        return OpResult::failure("Unknown base area.", "invalid_area");
    if (capacity < 1 || capacity > 100000)
        return OpResult::failure("Capacity must be between 1 and 100000 people.", "invalid_capacity");
    if (service < SERVICE_RESCUE || service > SERVICE_FOOD)
        return OpResult::failure("Unknown service type.", "invalid_service");
    if (volunteerCount_ >= MAX_VOLUNTEERS)
        return OpResult::failure("Volunteer capacity of the system has been reached.", "capacity");

    Volunteer v;
    v.id = nextVolunteerId();
    v.name = cleanName;
    v.areaId = areaId;
    v.service = service;
    v.capacity = capacity;
    v.available = true;
    volunteers_[volunteerCount_++] = v;

    std::string error;
    if (!saveVolunteers(volunteersPath_, error)) {
        --volunteerCount_;                        /* rollback */
        return OpResult::failure(error, "storage");
    }
    if (outNewId) *outNewId = v.id;
    return OpResult::success();
}

DisasterManagement::ResetOutcome DisasterManagement::resetVolunteerAvailability() {
    ResetOutcome outcome;
    bool changed = false;
    for (int i = 0; i < volunteerCount_; ++i) {
        if (isVolunteerBusy(volunteers_[i].id)) {
            /* Assigned to an ACTIVE request -> never released by a bulk reset. */
            if (volunteers_[i].available) { volunteers_[i].available = false; changed = true; }
            ++outcome.skippedAssigned;
            continue;
        }
        if (!volunteers_[i].available) {
            volunteers_[i].available = true;
            ++outcome.reactivated;
            changed = true;
        }
    }
    if (changed) {
        std::string error;
        if (!saveVolunteers(volunteersPath_, error))
            outcome.result = OpResult::failure(error, "storage");
        else
            outcome.result = OpResult::success();
    } else {
        outcome.result = OpResult::success();
    }
    return outcome;
}

/* ==========================================================================
 *  assignment bookkeeping (prevents double booking)
 * ========================================================================*/
void DisasterManagement::rebuildAssignmentIndex() {
    assignmentCount_.assign(1, 0);
    for (int i = 0; i < requestCount_; ++i) {
        const Request &r = requests_[i];
        if (r.status != REQUEST_ACTIVE) continue;
        for (std::size_t j = 0; j < r.assignedVolunteerIds.size(); ++j)
            markAssignment(r.assignedVolunteerIds[j], true);
    }
}

void DisasterManagement::markAssignment(int volunteerId, bool assigned) {
    if (volunteerId < 0) return;
    const std::size_t need = static_cast<std::size_t>(volunteerId) + 1;
    if (assignmentCount_.size() < need) assignmentCount_.resize(need, 0);
    if (assigned) ++assignmentCount_[static_cast<std::size_t>(volunteerId)];
    else if (assignmentCount_[static_cast<std::size_t>(volunteerId)] > 0)
        --assignmentCount_[static_cast<std::size_t>(volunteerId)];
}

bool DisasterManagement::isVolunteerBusy(int volunteerId) const {
    if (volunteerId < 0) return false;
    const std::size_t idx = static_cast<std::size_t>(volunteerId);
    return idx < assignmentCount_.size() && assignmentCount_[idx] > 0;
}

/* ==========================================================================
 *  requests
 * ========================================================================*/
const Request *DisasterManagement::requestAt(int index) const {
    if (index < 0 || index >= requestCount_) return 0;
    return &requests_[index];
}

int DisasterManagement::requestIndexOf(int id) const {
    for (int i = 0; i < requestCount_; ++i)
        if (requests_[i].id == id) return i;
    return -1;
}

const Request *DisasterManagement::requestById(int id) const {
    const int idx = requestIndexOf(id);
    return (idx < 0) ? 0 : &requests_[idx];
}

int DisasterManagement::nextRequestId() const {
    int maxId = 0;
    for (int i = 0; i < requestCount_; ++i)
        if (requests_[i].id > maxId) maxId = requests_[i].id;
    return maxId + 1;
}

std::vector<Request> DisasterManagement::requestsForOwner(const std::string &username) const {
    std::vector<Request> out;
    std::string key = trim(username);
    for (std::size_t i = 0; i < key.size(); ++i)
        key[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(key[i])));
    if (key.empty()) return out;

    for (int i = 0; i < requestCount_; ++i) {
        std::string owner = requests_[i].ownerUsername;
        for (std::size_t j = 0; j < owner.size(); ++j)
            owner[j] = static_cast<char>(std::tolower(static_cast<unsigned char>(owner[j])));
        if (owner == key) out.push_back(requests_[i]);
    }
    return out;
}

/* --------------------------------------------------------------------------
 *  The allocation algorithm
 * ------------------------------------------------------------------------*/
RequestOutcome DisasterManagement::submitRequest(ServiceType service, int areaId,
                                                 int peopleAffected,
                                                 const std::string &message,
                                                 const std::string &ownerUsername,
                                                 const CityGraph &graph) {
    RequestOutcome outcome;

    if (!graph.hasArea(areaId)) {
        outcome.result = OpResult::failure("Unknown affected area.", "invalid_area");
        return outcome;
    }
    if (peopleAffected < 1 || peopleAffected > 100000) {
        outcome.result = OpResult::failure("Number of affected people must be between 1 and 100000.",
                                          "invalid_people");
        return outcome;
    }
    if (service < SERVICE_RESCUE || service > SERVICE_FOOD) {
        outcome.result = OpResult::failure("Unknown service type.", "invalid_service");
        return outcome;
    }
    if (requestCount_ >= MAX_REQUESTS) {
        outcome.result = OpResult::failure("Request capacity of the system has been reached.",
                                          "capacity");
        return outcome;
    }

    /* ---------------- step 1 & 2: candidates (local first) ---------------- */
    struct Candidate {
        int    index;
        double distance;
        bool   local;
    };
    std::vector<Candidate> candidates;

    for (int i = 0; i < volunteerCount_; ++i) {
        const Volunteer &v = volunteers_[i];
        if (v.service != service) continue;          /* wrong specialisation */
        if (!v.available) continue;                  /* already dispatched    */
        if (isVolunteerBusy(v.id)) continue;         /* safety net            */

        Candidate c;
        c.index = i;
        c.local = (v.areaId == areaId);
        c.distance = 0.0;
        if (!c.local) {
            double km = 0.0;
            /* step 4: shortest travel distance through the city graph */
            if (!graph.shortestDistance(v.areaId, areaId, km)) continue;  /* unreachable */
            c.distance = km;
        }
        candidates.push_back(c);
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate &a, const Candidate &b) {
        if (a.local != b.local) return a.local;                 /* local volunteers first */
        if (a.distance != b.distance) return a.distance < b.distance; /* then nearest      */
        return a.index < b.index;
    });

    /* ---------------- step 3: allocate until covered ---------------------- */
    int remaining = peopleAffected;
    std::vector<int> assignedIds;

    for (std::size_t i = 0; i < candidates.size() && remaining > 0; ++i) {
        Volunteer &v = volunteers_[candidates[i].index];
        const int covered = std::min(v.capacity, remaining);
        remaining -= covered;
        v.available = false;                        /* step 5 */
        assignedIds.push_back(v.id);

        AllocationEntry entry;
        entry.volunteerId = v.id;
        entry.name = v.name;
        entry.areaId = v.areaId;
        entry.areaName = graph.areaName(v.areaId);
        entry.service = v.service;
        entry.capacity = v.capacity;
        entry.distanceKm = candidates[i].distance;
        entry.local = candidates[i].local;
        outcome.allocations.push_back(entry);
    }

    /* ---------------- step 6: record + persist --------------------------- */
    Request r;
    r.id = nextRequestId();
    r.service = service;
    r.areaId = areaId;
    r.peopleAffected = peopleAffected;
    r.peopleCovered = peopleAffected - remaining;
    r.assignedVolunteerIds = assignedIds;
    r.status = REQUEST_ACTIVE;
    r.message = cleanedText(message, 500);
    r.ownerUsername = trim(ownerUsername);

    requests_[requestCount_++] = r;
    rebuildAssignmentIndex();

    std::string error;
    if (!saveRequests(requestsPath_, error) || !saveVolunteers(volunteersPath_, error)) {
        /* full rollback so memory never diverges from disk */
        --requestCount_;
        rebuildAssignmentIndex();
        for (int i = 0; i < volunteerCount_; ++i)
            volunteers_[i].available = !isVolunteerBusy(volunteers_[i].id);
        outcome.allocations.clear();
        outcome.result = OpResult::failure(error.empty() ? "Could not save data." : error,
                                          "storage");
        return outcome;
    }

    outcome.result = OpResult::success();
    outcome.request = r;
    outcome.peopleRemaining = remaining;
    return outcome;
}

OpResult DisasterManagement::completeRequest(int requestId) {
    const int idx = requestIndexOf(requestId);
    if (idx < 0) return OpResult::failure("Request not found.", "not_found");
    if (requests_[idx].status == REQUEST_COMPLETED)
        return OpResult::failure("This request has already been completed.", "already_completed");

    const std::vector<int> assigned = requests_[idx].assignedVolunteerIds;

    requests_[idx].status = REQUEST_COMPLETED;
    rebuildAssignmentIndex();   /* this request no longer counts as active */

    bool volunteerChanged = false;
    for (std::size_t i = 0; i < assigned.size(); ++i) {
        const int vIndex = volunteerIndexOf(assigned[i]);
        if (vIndex < 0) continue;
        const bool stillBusy = isVolunteerBusy(assigned[i]);
        if (!stillBusy && !volunteers_[vIndex].available) {
            volunteers_[vIndex].available = true;   /* released back to the pool */
            volunteerChanged = true;
        }
    }

    std::string error;
    const bool savedRequests = saveRequests(requestsPath_, error);
    bool savedVolunteers = true;
    if (volunteerChanged) savedVolunteers = saveVolunteers(volunteersPath_, error);

    if (!savedRequests || !savedVolunteers) {
        /* roll back */
        requests_[idx].status = REQUEST_ACTIVE;
        rebuildAssignmentIndex();
        for (int i = 0; i < volunteerCount_; ++i)
            volunteers_[i].available = !isVolunteerBusy(volunteers_[i].id);
        return OpResult::failure(error.empty() ? "Could not save data." : error, "storage");
    }
    return OpResult::success();
}

/* ==========================================================================
 *  analytics
 * ========================================================================*/
SystemStats DisasterManagement::computeStats() const {
    SystemStats s;

    for (int i = 0; i < volunteerCount_; ++i) {
        const Volunteer &v = volunteers_[i];
        ++s.totalVolunteers;
        int svc = static_cast<int>(v.service);
        if (svc >= 0 && svc < SERVICE_COUNT) ++s.volunteersByService[svc];
        if (v.available) {
            ++s.availableVolunteers;
            if (svc >= 0 && svc < SERVICE_COUNT) ++s.availableByService[svc];
        } else {
            ++s.assignedVolunteers;
        }
    }

    for (int i = 0; i < requestCount_; ++i) {
        const Request &r = requests_[i];
        ++s.totalRequests;
        int svc = static_cast<int>(r.service);
        if (svc >= 0 && svc < SERVICE_COUNT) ++s.requestsByService[svc];
        s.totalPeopleAffected += r.peopleAffected;
        s.totalPeopleCovered += r.peopleCovered;

        if (r.status == REQUEST_ACTIVE) {
            ++s.activeRequests;
            s.activePeopleAffected += r.peopleAffected;
            s.activePeopleCovered += r.peopleCovered;
            if (r.peopleCovered < r.peopleAffected) ++s.underCoveredActive;
            if (r.assignedVolunteerIds.empty()) ++s.unassignedActive;
        } else {
            ++s.completedRequests;
        }
    }
    return s;
}

}  /* namespace dms */
