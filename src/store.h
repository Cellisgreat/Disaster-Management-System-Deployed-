/* ============================================================================
 *  store.h  --  DataStore: the single owner of all in-memory state
 * ----------------------------------------------------------------------------
 *  One DataStore instance exists per process (created in main). It owns:
 *      * the CityGraph          (areas, roads, adjacency list, Dijkstra)
 *      * the DisasterManagement (volunteers, requests, allocation engine)
 *      * the Authentication     (accounts, roles, password hashes)
 *      * the SessionStore       (web sessions)
 *      * the schematic map layout (optional data/area_layout.csv)
 *
 *  CONCURRENCY
 *  -----------
 *  cpp-httplib serves requests on several threads. All business state is
 *  guarded by one recursive mutex, and every mutating handler takes it for the
 *  whole read-modify-write-validate-persist cycle. That is what prevents two
 *  simultaneous submissions from double-booking the same volunteer.
 * ==========================================================================*/
#ifndef DMS_STORE_H
#define DMS_STORE_H

#include <mutex>
#include <string>
#include <vector>

#include "authentication.h"
#include "city_graph.h"
#include "config.h"
#include "disaster_management.h"
#include "sessions.h"

namespace dms {

struct LayoutPoint {
    int    areaId;
    double x;
    double y;
    LayoutPoint() : areaId(-1), x(0), y(0) {}
};

class DataStore {
public:
    explicit DataStore(const RuntimeConfig &config);

    /** Load every CSV database; creates defaults when files are missing. */
    bool initialize(std::string &error);

    /* ---- mutable state (always accessed under `mutex`) ------------------- */
    RuntimeConfig      cfg;
    CityGraph          graph;
    DisasterManagement dm;
    Authentication     auth;
    SessionStore       sessions;
    std::recursive_mutex mutex;

    /* ---- layout ---------------------------------------------------------- */
    const std::vector<LayoutPoint> &layout() const { return layout_; }
    /** Schematic coordinates for an area (falls back to a ring layout). */
    void layoutFor(int areaId, double &x, double &y) const;

    /* ---- convenience reload --------------------------------------------- */
    bool reloadGraph(std::string &error);

    /** Absolute-ish path helpers used by the API layer. */
    std::string areasPath() const { return cfg.path(AREAS_DB_FILE); }
    std::string roadsPath() const { return cfg.path(ROADS_DB_FILE); }
    std::string volunteersPath() const { return cfg.path(VOLUNTEERS_DB_FILE); }
    std::string requestsPath() const { return cfg.path(REQUESTS_DB_FILE); }
    std::string usersPath() const { return cfg.path(USERS_DB_FILE); }

private:
    void loadLayout();
    std::vector<LayoutPoint> layout_;
};

}  /* namespace dms */

#endif  /* DMS_STORE_H */
