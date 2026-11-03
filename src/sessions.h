/* ============================================================================
 *  sessions.h  --  In-memory, thread-safe session store for the web layer
 * ----------------------------------------------------------------------------
 *  Sessions are opaque 256-bit random tokens delivered as an HttpOnly cookie.
 *  The store is intentionally server side: the browser only ever holds the
 *  token, never a role, so a user cannot escalate privileges by editing it.
 *
 *  (Sessions live in memory only -- restarting the server logs everybody out,
 *   which is normal for an educational deployment and keeps the CSV databases
 *   as the single source of truth for data.)
 * ==========================================================================*/
#ifndef DMS_SESSIONS_H
#define DMS_SESSIONS_H

#include <ctime>
#include <map>
#include <mutex>
#include <string>

#include "models.h"

namespace dms {

struct Session {
    std::string username;
    UserRole    role;
    std::time_t createdAt;
    std::time_t expiresAt;
    Session() : role(ROLE_PERSON), createdAt(0), expiresAt(0) {}
    bool expired(std::time_t now) const { return expiresAt <= now; }
};

class SessionStore {
public:
    explicit SessionStore(int ttlSeconds);

    /** Create a session for an authenticated account and return its token. */
    std::string create(const std::string &username, UserRole role);

    /** Look up a token. Returns false when unknown or expired. */
    bool lookup(const std::string &token, Session &out);

    /** Drop one session (logout). */
    void destroy(const std::string &token);

    /** Drop every session for an account (e.g. after a password change). */
    int destroyForUser(const std::string &username);

    /** Remove expired sessions; returns how many were removed. */
    int purgeExpired();

    std::size_t size();

private:
    int                              ttl_;
    std::map<std::string, Session>   sessions_;
    std::mutex                       mutex_;
};

}  /* namespace dms */

#endif  /* DMS_SESSIONS_H */
