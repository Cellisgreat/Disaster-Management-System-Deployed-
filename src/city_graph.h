/* ============================================================================
 *  city_graph.h  --  City network: areas, roads and Dijkstra's algorithm
 * ----------------------------------------------------------------------------
 *  ORIGINAL DATA STRUCTURES PRESERVED:
 *    * adjacency list built from the road array (linked edge nodes),
 *    * array based storage for areas and roads (MAX_AREAS / MAX_ROADS),
 *    * Dijkstra's shortest path implemented with the custom, linked-list based
 *      PriorityQueue from priority_queue.h.
 *
 *  Because this release runs in a web server (many threads), the graph keeps a
 *  std::mutex that the API layer locks around the service functions below via
 *  the DataStore-owned lock. The algorithms themselves are unchanged.
 * ==========================================================================*/
#ifndef DMS_CITY_GRAPH_H
#define DMS_CITY_GRAPH_H

#include <string>
#include <vector>

#include "config.h"
#include "models.h"

namespace dms {

/* ------------------------------ result types ----------------------------- */
struct RouteStep {
    int    fromArea;
    int    toArea;
    int    roadId;
    double distanceKm;
    RouteStep() : fromArea(-1), toArea(-1), roadId(-1), distanceKm(0.0) {}
};

struct RouteResult {
    bool                   found;
    std::vector<int>       areaIds;    /* full node sequence, source .. dest */
    std::vector<RouteStep> steps;      /* road ids along the path            */
    std::vector<double>    distances;  /* cumulative distance at each node   */
    double                 totalKm;
    RouteResult() : found(false), totalKm(0.0) {}
};

/* -------------------------------- graph ---------------------------------- */
class CityGraph {
public:
    CityGraph();

    /* ---- loading / saving ------------------------------------------------ */
    bool loadAreas(const std::string &path, std::string &error);
    bool loadRoads(const std::string &path, std::string &error);
    bool saveAreas(const std::string &path, std::string &error) const;
    bool saveRoads(const std::string &path, std::string &error) const;

    /* ---- area access ----------------------------------------------------- */
    int         areaCount() const { return areaCount_; }
    const Area *areaById(int id) const;          /* 0 when unknown */
    const Area *areaAt(int index) const;         /* positional     */
    /** Area name, or "#<id>" when unknown (never throws). */
    std::string areaName(int id) const;
    bool        hasArea(int id) const { return areaById(id) != 0; }

    /* ---- road access ----------------------------------------------------- */
    int         roadCount() const { return roadCount_; }
    const Road *roadById(int id) const;
    const Road *roadAt(int index) const;
    int         roadIndexById(int id) const;

    bool setRoadBlocked(int roadId, bool blocked, std::string &error);

    /* ---- algorithm ------------------------------------------------------- */
    /**
     * Dijkstra shortest path between two areas.
     * Blocked roads are excluded. Uses the custom PriorityQueue internally.
     * Returns a RouteResult with found=false when no open path exists.
     */
    RouteResult shortestPath(int fromArea, int toArea) const;

    /** Shortest distance between two areas (used by volunteer allocation). */
    bool shortestDistance(int fromArea, int toArea, double &outKm) const;

    /**
     * Explanation of a route for API/CLI: e.g. "Port -> Bus Terminal -> Dam".
     * Returns "" when the areas are unknown.
     */
    std::string describePath(const std::vector<int> &areaIds) const;

    /* ---- statistics ------------------------------------------------------ */
    int  openRoadCount() const;
    int  blockedRoadCount() const { return roadCount_ - openRoadCount(); }
    /** True when every area can be reached from area 0 using open roads. */
    bool isConnected() const;

private:
    /* adjacency list node (original linked-list style edge storage) */
    struct Edge {
        int    roadId;
        int    toArea;
        double distanceKm;
        Edge  *next;
        Edge(int r, int t, double d, Edge *n) : roadId(r), toArea(t), distanceKm(d), next(n) {}
    };

    void addEdge(int fromArea, int roadId, int toArea, double distanceKm);
    void clearAdjacency();
    void rebuildAdjacency();
    /** Remove the adjacency entries belonging to one road. */
    void removeRoadEdges(int roadId);
    void adjacencyFor(int areaId, std::vector<Edge> &out) const;

    Area  areas_[MAX_AREAS];
    int   areaCount_;

    Road  roads_[MAX_ROADS];
    int   roadCount_;

    Edge *adj_[MAX_AREAS];   /* adjacency list heads */
    bool  adjDirty_;
    int   nextRoadId_;

    void ensureAdjacency();
    void refreshNextRoadId();
};

}  /* namespace dms */

#endif  /* DMS_CITY_GRAPH_H */
