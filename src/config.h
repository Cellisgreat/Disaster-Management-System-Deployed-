/* ============================================================================
 *  config.h  --  Central configuration for the Disaster Management System
 * ----------------------------------------------------------------------------
 *  Database locations, capacity limits and runtime defaults.
 *
 *  NOTE: The original project hard-coded relative paths such as "data/...".
 *        They are preserved here as defaults, but every path can now be
 *        overridden at runtime (web server: --data-dir / --public-dir) so the
 *        test-suite can run against a scratch copy of the CSV files without
 *        touching the real database.
 *
 *  The capacity limits below are the "array based storage" bounds used by the
 *  C++ data structures (fixed-size arrays, no STL containers for the core
 *  records themselves).
 * ==========================================================================*/
#ifndef DMS_CONFIG_H
#define DMS_CONFIG_H

#include <string>

namespace dms {

/* ---------------- capacity limits for the fixed-size arrays ---------------- */
const int MAX_AREAS      = 50;    /* areas in the city graph                */
const int MAX_ROADS      = 200;   /* roads (edges) in the city graph        */
const int MAX_VOLUNTEERS = 1000;  /* registered volunteers                  */
const int MAX_REQUESTS   = 2000;  /* disaster service requests              */
const int MAX_USERS      = 500;   /* login accounts                         */

/* ---------------- database file names (inside the data directory) --------- */
const char *const AREAS_DB_FILE      = "areas_db.csv";
const char *const ROADS_DB_FILE      = "roads_db.csv";
const char *const VOLUNTEERS_DB_FILE = "volunteers_db.csv";
const char *const REQUESTS_DB_FILE   = "requests_db.csv";
const char *const USERS_DB_FILE      = "users_db.csv";

/* ---------------- legacy (CLI) relative paths ----------------------------- */
/* Kept for backwards compatibility with the original console build.         */
const char *const AREAS_DB      = "data/areas_db.csv";
const char *const ROADS_DB      = "data/roads_db.csv";
const char *const VOLUNTEERS_DB = "data/volunteers_db.csv";
const char *const REQUESTS_DB   = "data/requests_db.csv";
const char *const USERS_DB      = "data/users_db.csv";

/* ---------------- session settings (web) ---------------------------------- */
const int    SESSION_TTL_SECONDS = 8 * 60 * 60;  /* 8 hours                   */
const char *const SESSION_COOKIE = "dms_session";

/* ---------------- runtime configuration ----------------------------------- */
struct RuntimeConfig {
    std::string dataDir;     /* directory holding the CSV databases          */
    std::string publicDir;   /* directory holding the frontend assets        */
    std::string host;        /* bind address (0.0.0.0 -> reachable preview)  */
    int         port;
    bool        verbose;

    RuntimeConfig()
        : dataDir("data"), publicDir("public"), host("0.0.0.0"),
          port(8080), verbose(false) {}

    std::string path(const char *file) const { return dataDir + "/" + file; }
};

}  /* namespace dms */

#endif  /* DMS_CONFIG_H */
