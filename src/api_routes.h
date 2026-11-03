/* ============================================================================
 *  api_routes.h  --  REST API + static asset wiring for cpp-httplib
 * ----------------------------------------------------------------------------
 *  Every route validates input, authenticates the caller, authorises the role
 *  and then delegates to the C++ business modules. Errors are always JSON:
 *
 *      { "error": "human readable message", "code": "machine_code" }
 * ==========================================================================*/
#ifndef DMS_API_ROUTES_H
#define DMS_API_ROUTES_H

#include <ctime>
#include <map>
#include <mutex>
#include <string>

#include "httplib.h"
#include "store.h"

namespace api {

/** Very small in-memory login throttle (per username + IP). */
class LoginThrottle {
public:
    LoginThrottle();
    bool blocked(const std::string &key);
    void recordFailure(const std::string &key);
    void reset(const std::string &key);

private:
    struct Entry {
        int       failures;
        std::time_t firstFailure;
        Entry() : failures(0), firstFailure(0) {}
    };
    static const int    MAX_FAILURES = 8;
    static const int    WINDOW_SECONDS = 300;
    std::map<std::string, Entry> entries_;
    std::mutex mutex_;
};

/** Registers every REST route under /api on the server. */
void registerApiRoutes(httplib::Server &server, dms::DataStore &store, LoginThrottle &throttle);

/** Serves the frontend from `publicDir` and installs the JSON 404 handler. */
void configureStaticServing(httplib::Server &server, const std::string &publicDir);

}  /* namespace api */

#endif  /* DMS_API_ROUTES_H */
