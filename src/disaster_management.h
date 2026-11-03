/* ============================================================================
 *  disaster_management.h  --  Volunteers, requests and the allocation engine
 * ----------------------------------------------------------------------------
 *  The heart of the project. All console I/O has been removed: each operation
 *  takes parameters, mutates the in-memory arrays, persists to CSV and returns
 *  a structured result, so the exact same C++ logic serves the website and the
 *  legacy CLI.
 *
 *  THE ALLOCATION ALGORITHM (unchanged in behaviour)
 *  ------------------------------------------------
 *   1. The affected area and the required service are taken from the request.
 *   2. Available volunteers of that service who are ALREADY IN the affected
 *      area are allocated first.
 *   3. If more help is needed, available volunteers of that service in other
 *      areas are located; Dijkstra's algorithm on the city graph gives their
 *      shortest travel distance and they are allocated nearest-first.
 *   4. Allocation stops once the affected population is covered, or when no
 *      suitable reachable volunteer remains.
 *   5. Every allocated volunteer becomes unavailable.
 *   6. The request record is stored and all CSVs are rewritten.
 *
 *  Safeguards (new in the web release)
 *  -----------------------------------
 *   * Volatile in-memory structures are only touched under the DataStore lock
 *     (see store.h), so two simultaneous requests can never double-book one
 *     volunteer.
 *   * "Reset availability" refuses to free a volunteer who is still assigned
 *     to an ACTIVE request -- it reports how many were skipped instead.
 *   * Completing a request releases its volunteers exactly once (idempotent
 *     second call is rejected).
 * ==========================================================================*/
#ifndef DMS_DISASTER_MANAGEMENT_H
#define DMS_DISASTER_MANAGEMENT_H

#include <string>
#include <vector>

#include "authentication.h"      /* OpResult */
#include "city_graph.h"
#include "config.h"
#include "models.h"

namespace dms {

/* --------------------------- allocation details --------------------------- */
struct AllocationEntry {
    int         volunteerId;
    std::string name;
    int         areaId;
    std::string areaName;
    ServiceType service;
    int         capacity;
    double      distanceKm;   /* shortest travel distance to the affected area */
    bool        local;        /* true when already inside the affected area    */
    AllocationEntry()
        : volunteerId(-1), areaId(-1), service(SERVICE_RESCUE), capacity(0),
          distanceKm(0.0), local(false) {}
};

struct RequestOutcome {
    OpResult                     result;
    Request                      request;
    std::vector<AllocationEntry> allocations;
    int                          peopleRemaining;
    RequestOutcome() : peopleRemaining(0) {}
};

/* ------------------------------ statistics ------------------------------- */
struct SystemStats {
    /* volunteers */
    int totalVolunteers;
    int availableVolunteers;
    int assignedVolunteers;
    int volunteersByService[SERVICE_COUNT];
    int availableByService[SERVICE_COUNT];
    /* requests */
    int totalRequests;
    int activeRequests;
    int completedRequests;
    int requestsByService[SERVICE_COUNT];
    /* people */
    int totalPeopleAffected;
    int totalPeopleCovered;
    int activePeopleAffected;
    int activePeopleCovered;
    /* quality of service */
    int underCoveredActive;    /* active requests where covered < affected */
    int unassignedActive;      /* active requests with no volunteer at all  */

    SystemStats();
};

/* ------------------------------- manager --------------------------------- */
class DisasterManagement {
public:
    DisasterManagement();

    /* ---- persistence ----------------------------------------------------- */
    bool loadVolunteers(const std::string &path, std::string &error);
    bool loadRequests(const std::string &path, std::string &error);
    bool saveVolunteers(const std::string &path, std::string &error) const;
    bool saveRequests(const std::string &path, std::string &error) const;

    void setPaths(const std::string &volunteers, const std::string &requests) {
        volunteersPath_ = volunteers;
        requestsPath_ = requests;
    }
    const std::string &volunteersPath() const { return volunteersPath_; }
    const std::string &requestsPath() const { return requestsPath_; }

    /* ---- volunteers ------------------------------------------------------ */
    int             volunteerCount() const { return volunteerCount_; }
    const Volunteer *volunteerAt(int index) const;
    const Volunteer *volunteerById(int id) const;
    int             volunteerIndexOf(int id) const;

    /** Add a volunteer; validates inputs, appends to the array, persists. */
    OpResult addVolunteer(const std::string &name, int areaId, ServiceType service, int capacity,
                          const CityGraph &graph, int *outNewId = 0);

    /** Availability reset honouring active-request assignments. */
    struct ResetOutcome {
        OpResult result;
        int      reactivated;      /* volunteers made available            */
        int      skippedAssigned;  /* left assigned: still on active work  */
        ResetOutcome() : reactivated(0), skippedAssigned(0) {}
    };
    ResetOutcome resetVolunteerAvailability();

    /* ---- requests -------------------------------------------------------- */
    int             requestCount() const { return requestCount_; }
    const Request  *requestAt(int index) const;
    const Request  *requestById(int id) const;
    int             requestIndexOf(int id) const;

    /** Requests owned by a username (case-insensitive); empty = nobody. */
    std::vector<Request> requestsForOwner(const std::string &username) const;

    /**
     * Submit a disaster request and run the allocation algorithm.
     * `ownerUsername` links the request to the authenticated citizen account.
     */
    RequestOutcome submitRequest(ServiceType service, int areaId, int peopleAffected,
                                 const std::string &message, const std::string &ownerUsername,
                                 const CityGraph &graph);

    /** Mark an active request completed and release its volunteers. */
    OpResult completeRequest(int requestId);

    /* ---- analytics ------------------------------------------------------- */
    SystemStats computeStats() const;
    bool        isVolunteerBusy(int volunteerId) const;  /* on an ACTIVE request */

private:
    int  nextVolunteerId() const;
    int  nextRequestId() const;
    void rebuildAssignmentIndex();   /* volunteer id -> active request count */
    void markAssignment(int volunteerId, bool assigned);

    std::string volunteersPath_;
    std::string requestsPath_;

    Volunteer volunteers_[MAX_VOLUNTEERS];
    int       volunteerCount_;

    Request   requests_[MAX_REQUESTS];
    int       requestCount_;

    /** How many ACTIVE requests currently hold each volunteer (by id). */
    std::vector<int> assignmentCount_;
};

}  /* namespace dms */

#endif  /* DMS_DISASTER_MANAGEMENT_H */
