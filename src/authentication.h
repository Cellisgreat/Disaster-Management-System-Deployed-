/* ============================================================================
 *  authentication.h  --  Accounts, roles and password verification
 * ----------------------------------------------------------------------------
 *  Refactored from the original console implementation: instead of prompting
 *  with cin/cout and looping until success, every operation now returns a
 *  structured result so it can be called from the HTTP layer *and* from the
 *  console menu.
 *
 *  Security notes
 *  --------------
 *  * Passwords are stored with PBKDF2-HMAC-SHA256 (see crypto.h).
 *  * Accounts still carrying the old educational hash are migrated
 *    automatically the first time they authenticate successfully.
 *  * Only ADMIN accounts may create further ADMIN accounts; self-registration
 *    always yields the PERSON role.
 * ==========================================================================*/
#ifndef DMS_AUTHENTICATION_H
#define DMS_AUTHENTICATION_H

#include <string>

#include "config.h"
#include "models.h"

namespace dms {

/** Generic structured outcome used by the business layer. */
struct OpResult {
    bool        ok;
    std::string error;    /* human readable message when !ok */
    std::string code;     /* machine friendly error code        */
    OpResult() : ok(false) {}
    static OpResult success() { OpResult r; r.ok = true; return r; }
    static OpResult failure(const std::string &msg, const std::string &code = "error") {
        OpResult r; r.ok = false; r.error = msg; r.code = code; return r;
    }
};

struct AuthResult {
    bool        ok;
    std::string error;
    std::string code;
    std::string username;
    UserRole    role;
    bool        migrated;   /* legacy hash upgraded during this login */
    AuthResult() : ok(false), role(ROLE_PERSON), migrated(false) {}
};

class Authentication {
public:
    explicit Authentication(const std::string &usersDbPath);

    /* ---- persistence ----------------------------------------------------- */
    bool load(std::string &error);
    bool save(std::string &error) const;
    /** Creates the demo administrator if the users database is empty. */
    bool seedDefaultAdminIfEmpty(std::string &error);

    /* ---- queries --------------------------------------------------------- */
    int         userCount() const { return userCount_; }
    const User *userAt(int index) const;
    const User *findUser(const std::string &username) const;   /* case-insensitive */
    bool        exists(const std::string &username) const;
    int         adminCount() const;

    /* ---- operations ------------------------------------------------------ */
    /**
     * Register a new account. `requestedAdmin` is honoured only when
     * `requestedByIsAdmin` is true -- the API layer passes the verified session
     * role, so role escalation cannot be triggered from the browser.
     */
    OpResult registerUser(const std::string &username, const std::string &password,
                          bool requestedAdmin, bool requestedByIsAdmin);

    AuthResult login(const std::string &username, const std::string &password);

    OpResult changePassword(const std::string &username, const std::string &oldPassword,
                            const std::string &newPassword, bool skipOldPassword = false);

    /* ---- validation helpers (also used by the API for friendly messages) -- */
    static bool isValidUsername(const std::string &username, std::string &reason);
    static bool isValidPassword(const std::string &password, std::string &reason);

private:
    std::string path_;
    User        users_[MAX_USERS];
    int         userCount_;
};

}  /* namespace dms */

#endif  /* DMS_AUTHENTICATION_H */
