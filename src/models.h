/* ============================================================================
 *  models.h  --  Core record types + enum <-> CSV string mapping
 * ----------------------------------------------------------------------------
 *  These are the plain data records stored in the fixed-size arrays inside the
 *  C++ modules. They are intentionally POD-ish and free of any HTTP / JSON
 *  knowledge: the API layer is the only place that knows about web formats.
 * ==========================================================================*/
#ifndef DMS_MODELS_H
#define DMS_MODELS_H

#include <string>
#include <vector>

namespace dms {

/* ------------------------------- services -------------------------------- */
enum ServiceType { SERVICE_RESCUE = 0, SERVICE_MEDICAL = 1, SERVICE_FOOD = 2 };
const int SERVICE_COUNT = 3;

/** Canonical database spelling: "Rescue" | "Medical Help" | "Food". */
std::string serviceToString(ServiceType s);
/** Tolerant parser: accepts "medical", "Medical Help", "MEDICAL_HELP", ... */
bool parseService(const std::string &text, ServiceType &out);
/** Normalised display label used by the API/UI. */
std::string serviceDisplayName(ServiceType s);

/* ------------------------------- requests -------------------------------- */
enum RequestStatus { REQUEST_ACTIVE = 0, REQUEST_COMPLETED = 1 };

std::string requestStatusToString(RequestStatus s);
bool parseRequestStatus(const std::string &text, RequestStatus &out);

/* -------------------------------- roles ---------------------------------- */
enum UserRole { ROLE_PERSON = 0, ROLE_ADMIN = 1 };
std::string roleToString(UserRole r);
bool parseRole(const std::string &text, UserRole &out);

/* ------------------------------ city graph ------------------------------- */
struct Area {
    int         id;
    std::string name;
    Area() : id(-1) {}
};

struct Road {
    int    id;
    int    fromArea;      /* area id */
    int    toArea;        /* area id */
    double distanceKm;
    bool   blocked;       /* blocked roads are skipped by Dijkstra */
    Road() : id(-1), fromArea(-1), toArea(-1), distanceKm(0.0), blocked(false) {}
};

/* ------------------------------ volunteers ------------------------------- */
struct Volunteer {
    int         id;
    std::string name;
    int         areaId;          /* base area */
    ServiceType service;
    int         capacity;        /* how many people this volunteer can handle */
    bool        available;
    Volunteer() : id(-1), areaId(-1), service(SERVICE_RESCUE), capacity(0), available(true) {}
};

/* ------------------------------- requests -------------------------------- */
struct Request {
    int           id;
    ServiceType   service;
    int           areaId;
    int           peopleAffected;
    int           peopleCovered;
    std::vector<int> assignedVolunteerIds;
    RequestStatus status;
    std::string   message;
    /* --- NEW (web release): owner of the request. Empty for legacy rows,
     *     which simply means "created before accounts were linked".        */
    std::string   ownerUsername;
    Request()
        : id(-1), service(SERVICE_RESCUE), areaId(-1), peopleAffected(0),
          peopleCovered(0), status(REQUEST_ACTIVE) {}
};

/* -------------------------------- users ---------------------------------- */
struct User {
    std::string username;
    std::string passwordHash;   /* "pbkdf2$sha256$iter$salt$hash" or legacy hash */
    UserRole    role;
    User() : role(ROLE_PERSON) {}
};

}  /* namespace dms */

#endif  /* DMS_MODELS_H */
