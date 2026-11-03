#include "store.h"

#include <cmath>
#include <cstdio>

#include "csv_utils.h"

namespace dms {

DataStore::DataStore(const RuntimeConfig &config)
    : cfg(config),
      auth(config.path(USERS_DB_FILE)),
      sessions(SESSION_TTL_SECONDS) {}

bool DataStore::initialize(std::string &error) {
    if (!ensureDirectory(cfg.dataDir)) {
        error = "Could not create data directory: " + cfg.dataDir;
        return false;
    }

    if (!graph.loadAreas(areasPath(), error)) return false;
    if (!graph.loadRoads(roadsPath(), error)) return false;

    if (!auth.load(error)) return false;
    if (!auth.seedDefaultAdminIfEmpty(error)) return false;

    dm.setPaths(volunteersPath(), requestsPath());
    if (!dm.loadVolunteers(volunteersPath(), error)) return false;
    if (!dm.loadRequests(requestsPath(), error)) return false;

    loadLayout();

    std::printf("[store] loaded %d areas, %d roads (%d blocked), %d volunteers, "
                "%d requests, %d accounts\n",
                graph.areaCount(), graph.roadCount(), graph.blockedRoadCount(),
                dm.volunteerCount(), dm.requestCount(), auth.userCount());
    error.clear();
    return true;
}

bool DataStore::reloadGraph(std::string &error) {
    if (!graph.loadAreas(areasPath(), error)) return false;
    if (!graph.loadRoads(roadsPath(), error)) return false;
    loadLayout();
    return true;
}

void DataStore::loadLayout() {
    layout_.clear();
    std::vector<std::vector<std::string> > rows;
    if (readCsvFile(cfg.path("area_layout.csv"), rows)) {
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const std::vector<std::string> &f = rows[i];
            if (f.size() < 3) continue;
            LayoutPoint p;
            p.areaId = toInt(f[0], -1);
            p.x = toDouble(f[1], 0.0);
            p.y = toDouble(f[2], 0.0);
            if (p.areaId >= 0 && graph.hasArea(p.areaId)) layout_.push_back(p);
        }
    }
    if (layout_.empty())
        std::fprintf(stderr, "[store] area_layout.csv missing: using generated ring layout\n");
}

void DataStore::layoutFor(int areaId, double &x, double &y) const {
    for (std::size_t i = 0; i < layout_.size(); ++i) {
        if (layout_[i].areaId == areaId) { x = layout_[i].x; y = layout_[i].y; return; }
    }
    /* deterministic fallback: evenly spaced ring, stable per area id */
    int index = 0;
    for (int i = 0; i < graph.areaCount(); ++i) {
        const Area *a = graph.areaAt(i);
        if (a && a->id == areaId) { index = i; break; }
    }
    const int total = graph.areaCount() > 0 ? graph.areaCount() : 1;
    const double angle = (2.0 * 3.14159265358979 * index) / total;
    x = 500.0 + 380.0 * std::cos(angle);
    y = 350.0 + 260.0 * std::sin(angle);
}

}  /* namespace dms */
