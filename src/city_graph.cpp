#include "city_graph.h"

#include <algorithm>
#include <cstdio>
#include <sstream>

#include "csv_utils.h"
#include "priority_queue.h"

namespace dms {

CityGraph::CityGraph() : areaCount_(0), roadCount_(0), adjDirty_(true), nextRoadId_(0) {
    for (int i = 0; i < MAX_AREAS; ++i) adj_[i] = 0;
}

/* ==========================================================================
 *  adjacency list management
 * ========================================================================*/
void CityGraph::addEdge(int fromArea, int roadId, int toArea, double distanceKm) {
    if (fromArea < 0 || fromArea >= MAX_AREAS) return;
    Edge *e = new Edge(roadId, toArea, distanceKm, adj_[fromArea]);
    adj_[fromArea] = e;
}

void CityGraph::clearAdjacency() {
    for (int i = 0; i < MAX_AREAS; ++i) {
        Edge *cur = adj_[i];
        while (cur) {
            Edge *next = cur->next;
            delete cur;
            cur = next;
        }
        adj_[i] = 0;
    }
    adjDirty_ = true;
}

void CityGraph::removeRoadEdges(int roadId) {
    for (int i = 0; i < MAX_AREAS; ++i) {
        Edge *cur = adj_[i];
        Edge *prev = 0;
        while (cur) {
            if (cur->roadId == roadId) {
                Edge *dead = cur;
                if (prev) prev->next = cur->next;
                else adj_[i] = cur->next;
                cur = cur->next;
                delete dead;
            } else {
                prev = cur;
                cur = cur->next;
            }
        }
    }
}

void CityGraph::rebuildAdjacency() {
    clearAdjacency();
    for (int i = 0; i < roadCount_; ++i) {
        const Road &r = roads_[i];
        if (r.fromArea < 0 || r.fromArea >= MAX_AREAS) continue;
        if (r.toArea < 0 || r.toArea >= MAX_AREAS) continue;
        addEdge(r.fromArea, r.id, r.toArea, r.distanceKm);
        addEdge(r.toArea, r.id, r.fromArea, r.distanceKm);   /* undirected road */
    }
    adjDirty_ = false;
}

void CityGraph::ensureAdjacency() {
    if (adjDirty_) rebuildAdjacency();
}

void CityGraph::refreshNextRoadId() {
    int maxId = -1;
    for (int i = 0; i < roadCount_; ++i)
        if (roads_[i].id > maxId) maxId = roads_[i].id;
    nextRoadId_ = maxId + 1;
}

void CityGraph::adjacencyFor(int areaId, std::vector<Edge> &out) const {
    out.clear();
    if (areaId < 0 || areaId >= MAX_AREAS) return;
    for (const Edge *e = adj_[areaId]; e; e = e->next) out.push_back(*e);
    /* Deterministic neighbour order keeps Dijkstra tie-breaking stable. */
    std::sort(out.begin(), out.end(), [](const Edge &a, const Edge &b) {
        if (a.distanceKm != b.distanceKm) return a.distanceKm < b.distanceKm;
        if (a.toArea != b.toArea) return a.toArea < b.toArea;
        return a.roadId < b.roadId;
    });
}

/* ==========================================================================
 *  loading / saving
 * ========================================================================*/
bool CityGraph::loadAreas(const std::string &path, std::string &error) {
    std::vector<std::vector<std::string> > rows;
    if (!readCsvFile(path, rows)) {
        error = "Could not open areas database: " + path;
        return false;
    }
    areaCount_ = 0;
    int skipped = 0;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const std::vector<std::string> &f = rows[i];
        if (f.size() < 2) { ++skipped; continue; }
        const int id = toInt(f[0], -1);
        if (i == 0 && id < 0) continue;            /* header row: skip silently */
        const std::string name = trim(f[1]);
        if (id < 0 || id >= MAX_AREAS || name.empty()) { ++skipped; continue; }
        if (areaCount_ >= MAX_AREAS) break;
        areas_[areaCount_].id = id;
        areas_[areaCount_].name = name;
        ++areaCount_;
    }
    if (skipped > 0)
        std::fprintf(stderr, "[city_graph] areas: skipped %d malformed row(s)\n", skipped);
    error.clear();
    return true;
}

bool CityGraph::loadRoads(const std::string &path, std::string &error) {
    std::vector<std::vector<std::string> > rows;
    if (!readCsvFile(path, rows)) {
        error = "Could not open roads database: " + path;
        return false;
    }
    roadCount_ = 0;
    int skipped = 0;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const std::vector<std::string> &f = rows[i];
        if (f.size() < 3) { ++skipped; continue; }
        Road r;
        r.id = toInt(f[0], -1);
        if (i == 0 && r.id < 0) continue;          /* header row: skip silently */
        r.fromArea = toInt(f[1], -1);
        r.toArea = toInt(f[2], -1);
        r.distanceKm = (f.size() > 3) ? toDouble(f[3], -1.0) : -1.0;
        r.blocked = (f.size() > 4) ? toBoolFlag(f[4], false) : false;

        if (r.id < 0 || !hasArea(r.fromArea) || !hasArea(r.toArea) ||
            r.fromArea == r.toArea || r.distanceKm <= 0.0) {
            ++skipped;
            continue;
        }
        if (roadCount_ >= MAX_ROADS) break;
        roads_[roadCount_++] = r;
    }
    refreshNextRoadId();
    adjDirty_ = true;
    ensureAdjacency();
    if (skipped > 0)
        std::fprintf(stderr, "[city_graph] roads: skipped %d malformed row(s)\n", skipped);
    error.clear();
    return true;
}

bool CityGraph::saveAreas(const std::string &path, std::string &error) const {
    std::string out;
    out += "id,name\n";
    for (int i = 0; i < areaCount_; ++i) {
        std::vector<std::string> f;
        std::ostringstream id; id << areas_[i].id;
        f.push_back(id.str());
        f.push_back(areas_[i].name);
        out += csvLine(f);
        out += '\n';
    }
    if (!writeFileAtomic(path, out)) { error = "Could not write " + path; return false; }
    error.clear();
    return true;
}

bool CityGraph::saveRoads(const std::string &path, std::string &error) const {
    std::string out;
    out += "id,from_area_id,to_area_id,distance_km,blocked\n";
    for (int i = 0; i < roadCount_; ++i) {
        const Road &r = roads_[i];
        std::ostringstream line;
        line << r.id << ',' << r.fromArea << ',' << r.toArea << ','
             << r.distanceKm << ',' << (r.blocked ? 1 : 0);
        out += line.str();
        out += '\n';
    }
    if (!writeFileAtomic(path, out)) { error = "Could not write " + path; return false; }
    error.clear();
    return true;
}

/* ==========================================================================
 *  accessors
 * ========================================================================*/
const Area *CityGraph::areaById(int id) const {
    for (int i = 0; i < areaCount_; ++i)
        if (areas_[i].id == id) return &areas_[i];
    return 0;
}

const Area *CityGraph::areaAt(int index) const {
    if (index < 0 || index >= areaCount_) return 0;
    return &areas_[index];
}

std::string CityGraph::areaName(int id) const {
    const Area *a = areaById(id);
    if (a) return a->name;
    std::ostringstream oss;
    oss << "#" << id;
    return oss.str();
}

int CityGraph::roadIndexById(int id) const {
    for (int i = 0; i < roadCount_; ++i)
        if (roads_[i].id == id) return i;
    return -1;
}

const Road *CityGraph::roadById(int id) const {
    const int idx = roadIndexById(id);
    return (idx < 0) ? 0 : &roads_[idx];
}

const Road *CityGraph::roadAt(int index) const {
    if (index < 0 || index >= roadCount_) return 0;
    return &roads_[index];
}

bool CityGraph::setRoadBlocked(int roadId, bool blocked, std::string &error) {
    const int idx = roadIndexById(roadId);
    if (idx < 0) { error = "Road not found."; return false; }
    roads_[idx].blocked = blocked;
    error.clear();
    return true;
}

int CityGraph::openRoadCount() const {
    int n = 0;
    for (int i = 0; i < roadCount_; ++i)
        if (!roads_[i].blocked) ++n;
    return n;
}

/* ==========================================================================
 *  Dijkstra  --  custom priority queue, blocked roads excluded
 * ========================================================================*/
RouteResult CityGraph::shortestPath(int fromArea, int toArea) const {
    RouteResult result;
    if (!hasArea(fromArea) || !hasArea(toArea)) return result;

    CityGraph *self = const_cast<CityGraph *>(this);
    self->ensureAdjacency();

    std::vector<double> dist(MAX_AREAS, -1.0);
    std::vector<int>    prev(MAX_AREAS, -1);
    std::vector<int>    prevRoad(MAX_AREAS, -1);
    std::vector<bool>   settled(MAX_AREAS, false);

    PriorityQueue pq;               /* <-- the custom linked-list priority queue */
    dist[fromArea] = 0.0;
    pq.push(fromArea, 0.0);

    std::vector<Edge> neighbours;
    int    node = -1;
    double key = 0.0;
    while (pq.pop(node, key)) {
        if (node < 0 || node >= MAX_AREAS) continue;
        if (settled[node]) continue;                  /* stale entry */
        settled[node] = true;
        if (node == toArea) break;                    /* early exit, optimal */

        adjacencyFor(node, neighbours);
        for (std::size_t i = 0; i < neighbours.size(); ++i) {
            const Edge &e = neighbours[i];
            const Road *r = roadById(e.roadId);
            if (!r || r->blocked) continue;           /* BLOCKED ROADS ARE SKIPPED */
            if (e.toArea < 0 || e.toArea >= MAX_AREAS) continue;
            if (settled[e.toArea]) continue;

            const double candidate = dist[node] + e.distanceKm;
            if (dist[e.toArea] < 0.0 || candidate < dist[e.toArea] - 1e-9) {
                dist[e.toArea] = candidate;
                prev[e.toArea] = node;
                prevRoad[e.toArea] = e.roadId;
                pq.push(e.toArea, candidate);
            }
        }
    }

    if (dist[toArea] < 0.0) return result;            /* unreachable */

    /* rebuild the path backwards */
    std::vector<int> path;
    for (int at = toArea; at != -1; at = prev[at]) {
        path.push_back(at);
        if (at == fromArea) break;
    }
    std::reverse(path.begin(), path.end());
    if (path.empty() || path.front() != fromArea) return result;

    result.found = true;
    result.areaIds = path;
    result.totalKm = dist[toArea];
    for (std::size_t i = 0; i < path.size(); ++i) result.distances.push_back(dist[path[i]]);
    for (std::size_t i = 1; i < path.size(); ++i) {
        RouteStep step;
        step.fromArea = path[i - 1];
        step.toArea = path[i];
        step.roadId = prevRoad[path[i]];
        const Road *r = roadById(step.roadId);
        step.distanceKm = r ? r->distanceKm : (dist[path[i]] - dist[path[i - 1]]);
        result.steps.push_back(step);
    }
    return result;
}

bool CityGraph::shortestDistance(int fromArea, int toArea, double &outKm) const {
    if (fromArea == toArea && hasArea(fromArea)) { outKm = 0.0; return true; }
    const RouteResult r = shortestPath(fromArea, toArea);
    if (!r.found) return false;
    outKm = r.totalKm;
    return true;
}

std::string CityGraph::describePath(const std::vector<int> &areaIds) const {
    if (areaIds.empty()) return "";
    std::ostringstream oss;
    for (std::size_t i = 0; i < areaIds.size(); ++i) {
        if (i) oss << " -> ";
        oss << areaName(areaIds[i]);
    }
    return oss.str();
}

bool CityGraph::isConnected() const {
    if (areaCount_ == 0) return true;
    const Area *first = areaAt(0);
    if (!first) return true;
    for (int i = 1; i < areaCount_; ++i) {
        const Area *other = areaAt(i);
        if (!other) continue;
        if (!shortestPath(first->id, other->id).found) return false;
    }
    return true;
}

}  /* namespace dms */
