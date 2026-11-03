/* ============================================================================
 *  main.cpp  --  Program entry point
 * ----------------------------------------------------------------------------
 *  Two modes, one binary:
 *
 *    DisasterManagementServer                 -> web server (primary interface)
 *    DisasterManagementServer --cli           -> original console application
 *
 *  Options
 *    --port <n>         HTTP port                     (default 8080)
 *    --host <addr>      bind address                 (default 0.0.0.0)
 *    --data-dir <path>  folder containing the CSVs   (default data)
 *    --public-dir <p>   folder with the frontend     (default public)
 *    --cli              run the legacy console menus
 *    --check            load the databases, print a report, exit (CI/smoke test)
 *    --verbose          log every HTTP request
 *    --help             show this message
 * ==========================================================================*/
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "api_routes.h"
#include "menus.h"
#include "store.h"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

void printUsage() {
    std::printf(
        "CivicShield Disaster Management System\n"
        "\n"
        "Usage: DisasterManagementServer [options]\n"
        "\n"
        "  --port <n>          HTTP port to listen on           (default 8080)\n"
        "  --host <addr>       Address to bind                 (default 0.0.0.0)\n"
        "  --data-dir <path>   Directory with the CSV files    (default data)\n"
        "  --public-dir <path> Directory with the web frontend (default public)\n"
        "  --cli               Run the legacy console interface instead of the web UI\n"
        "  --check             Load databases, print a report and exit\n"
        "  --verbose           Log every HTTP request\n"
        "  --help              Show this help\n"
        "\n"
        "Examples:\n"
        "  DisasterManagementServer\n"
        "  DisasterManagementServer --port 9000 --verbose\n"
        "  DisasterManagementServer --cli\n");
}

bool parseArgs(int argc, char *argv[], dms::RuntimeConfig &cfg, bool &runCli, bool &checkOnly) {
    runCli = false;
    checkOnly = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") { printUsage(); return false; }
        else if (arg == "--cli") runCli = true;
        else if (arg == "--check") checkOnly = true;
        else if (arg == "--verbose") cfg.verbose = true;
        else if (arg == "--port" && i + 1 < argc) cfg.port = std::atoi(argv[++i]);
        else if (arg == "--host" && i + 1 < argc) cfg.host = argv[++i];
        else if (arg == "--data-dir" && i + 1 < argc) cfg.dataDir = argv[++i];
        else if (arg == "--public-dir" && i + 1 < argc) cfg.publicDir = argv[++i];
        else {
            std::fprintf(stderr, "Unknown or incomplete option: %s\n\n", arg.c_str());
            printUsage();
            return false;
        }
    }
    if (cfg.port <= 0 || cfg.port > 65535) {
        std::fprintf(stderr, "Invalid port.\n");
        return false;
    }
    return true;
}

}  /* namespace */

int main(int argc, char *argv[]) {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);   /* keep the console banner readable */
#endif

    dms::RuntimeConfig cfg;

    /* Cloud platforms provide PORT; DMS_DATA_DIR can point at a mounted volume.
       Command-line options below intentionally take precedence over environment. */
    const char *envPort = std::getenv("PORT");
    if (envPort && *envPort) cfg.port = std::atoi(envPort);
    const char *envDataDir = std::getenv("DMS_DATA_DIR");
    if (envDataDir && *envDataDir) cfg.dataDir = envDataDir;
    const char *envHost = std::getenv("HOST");
    if (envHost && *envHost) cfg.host = envHost;

    bool runCli = false, checkOnly = false;
    if (!parseArgs(argc, argv, cfg, runCli, checkOnly)) return 1;

    dms::DataStore store(cfg);
    std::string error;
    if (!store.initialize(error)) {
        std::fprintf(stderr, "Startup failed: %s\n", error.c_str());
        return 1;
    }

    if (checkOnly) {
        const dms::SystemStats stats = store.dm.computeStats();
        std::printf("check: areas=%d roads=%d blocked=%d connected=%s\n",
                    store.graph.areaCount(), store.graph.roadCount(),
                    store.graph.blockedRoadCount(), store.graph.isConnected() ? "yes" : "no");
        std::printf("check: volunteers=%d available=%d assigned=%d\n", stats.totalVolunteers,
                    stats.availableVolunteers, stats.assignedVolunteers);
        std::printf("check: requests=%d active=%d completed=%d accounts=%d\n",
                    stats.totalRequests, stats.activeRequests, stats.completedRequests,
                    store.auth.userCount());
        return 0;
    }

    if (runCli) {
        dms::ConsoleApp app(store);
        app.run();
        return 0;
    }

    /* ------------------------------- web server --------------------------- */
    httplib::Server server;
    api::LoginThrottle throttle;
    api::registerApiRoutes(server, store, throttle);
    api::configureStaticServing(server, cfg.publicDir);

    if (cfg.verbose) {
        server.set_logger([](const httplib::Request &req, const httplib::Response &res) {
            std::printf("[http] %s %s -> %d\n", req.method.c_str(), req.path.c_str(), res.status);
            std::fflush(stdout);
        });
    }

    std::printf("\n");
    std::printf("  CivicShield Disaster Management System  (C++ backend)\n");
    std::printf("  ----------------------------------------------------------\n");
    std::printf("  Frontend : http://localhost:%d/\n", cfg.port);
    std::printf("  API      : http://localhost:%d/api/health\n", cfg.port);
    std::printf("  Data     : %s\n", cfg.dataDir.c_str());
    std::printf("  Assets   : %s\n", cfg.publicDir.c_str());
    std::printf("  Demo     : Admin / admin123  (change it from Account settings)\n");
    std::printf("  Note     : educational demonstration system, not a real dispatch service.\n");
    std::printf("  Press Ctrl+C to stop.\n\n");
    std::fflush(stdout);

    if (!server.listen(cfg.host.c_str(), cfg.port)) {
        std::fprintf(stderr,
                     "Could not start the HTTP server on %s:%d "
                     "(is the port already in use?)\n", cfg.host.c_str(), cfg.port);
        return 1;
    }
    return 0;
}
