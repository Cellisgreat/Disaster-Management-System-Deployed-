#include "authentication.h"

#include <cctype>
#include <cstdio>
#include <sstream>

#include "crypto.h"
#include "csv_utils.h"

namespace dms {

namespace {

std::string lower(const std::string &s) {
    std::string out = s;
    for (std::size_t i = 0; i < out.size(); ++i)
        out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(out[i])));
    return out;
}

std::string normalizeUsername(const std::string &s) { return trim(s); }

}  /* anonymous namespace */

Authentication::Authentication(const std::string &usersDbPath)
    : path_(usersDbPath), userCount_(0) {}

bool Authentication::load(std::string &error) {
    std::vector<std::vector<std::string> > rows;
    userCount_ = 0;
    if (!readCsvFile(path_, rows)) {
        /* A missing users file is not fatal: the demo admin gets seeded later. */
        std::fprintf(stderr, "[auth] users database %s not found; starting empty\n",
                     path_.c_str());
        error.clear();
        return true;
    }
    int skipped = 0;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const std::vector<std::string> &f = rows[i];
        if (f.size() < 3) { ++skipped; continue; }
        User u;
        u.username = trim(f[0]);
        if (i == 0 && (u.username == "username" || u.username.empty())) continue; /* header */
        u.passwordHash = trim(f[1]);
        if (!parseRole(f[2], u.role)) { ++skipped; continue; }
        if (u.username.empty() || u.passwordHash.empty()) { ++skipped; continue; }
        if (userCount_ >= MAX_USERS) break;
        users_[userCount_++] = u;
    }
    if (skipped > 0)
        std::fprintf(stderr, "[auth] skipped %d malformed user row(s)\n", skipped);
    error.clear();
    return true;
}

bool Authentication::save(std::string &error) const {
    std::string out;
    out += "username,password_hash,role\n";
    for (int i = 0; i < userCount_; ++i) {
        std::vector<std::string> f;
        f.push_back(users_[i].username);
        f.push_back(users_[i].passwordHash);
        f.push_back(roleToString(users_[i].role));
        out += csvLine(f);
        out += '\n';
    }
    if (!writeFileAtomic(path_, out)) {
        error = "Could not write users database.";
        return false;
    }
    error.clear();
    return true;
}

bool Authentication::seedDefaultAdminIfEmpty(std::string &error) {
    if (userCount_ > 0) { error.clear(); return true; }
    User admin;
    admin.username = "Admin";
    admin.passwordHash = hashPassword("admin123");
    admin.role = ROLE_ADMIN;
    users_[userCount_++] = admin;
    std::fprintf(stderr,
                 "[auth] users database was empty: seeded demo administrator "
                 "'Admin' / 'admin123'\n");
    return save(error);
}

const User *Authentication::userAt(int index) const {
    if (index < 0 || index >= userCount_) return 0;
    return &users_[index];
}

const User *Authentication::findUser(const std::string &username) const {
    const std::string key = lower(normalizeUsername(username));
    for (int i = 0; i < userCount_; ++i)
        if (lower(users_[i].username) == key) return &users_[i];
    return 0;
}

bool Authentication::exists(const std::string &username) const {
    return findUser(username) != 0;
}

int Authentication::adminCount() const {
    int n = 0;
    for (int i = 0; i < userCount_; ++i)
        if (users_[i].role == ROLE_ADMIN) ++n;
    return n;
}

bool Authentication::isValidUsername(const std::string &username, std::string &reason) {
    const std::string u = trim(username);
    if (u.size() < 3 || u.size() > 24) {
        reason = "Username must be between 3 and 24 characters.";
        return false;
    }
    for (std::size_t i = 0; i < u.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(u[i]);
        if (!std::isalnum(c) && c != '_' && c != '.' && c != '-') {
            reason = "Username may only contain letters, digits, '_', '.' and '-'.";
            return false;
        }
    }
    reason.clear();
    return true;
}

bool Authentication::isValidPassword(const std::string &password, std::string &reason) {
    if (password.size() < 6) {
        reason = "Password must be at least 6 characters long.";
        return false;
    }
    if (password.size() > 128) {
        reason = "Password must be at most 128 characters long.";
        return false;
    }
    reason.clear();
    return true;
}

OpResult Authentication::registerUser(const std::string &username, const std::string &password,
                                      bool requestedAdmin, bool requestedByIsAdmin) {
    const std::string name = normalizeUsername(username);
    std::string reason;
    if (!isValidUsername(name, reason)) return OpResult::failure(reason, "invalid_username");
    if (!isValidPassword(password, reason)) return OpResult::failure(reason, "invalid_password");
    if (exists(name)) return OpResult::failure("That username is already taken.", "username_taken");
    if (userCount_ >= MAX_USERS)
        return OpResult::failure("Account capacity reached.", "capacity");

    if (requestedAdmin && !requestedByIsAdmin)
        return OpResult::failure("Only an administrator can create administrator accounts.",
                                 "forbidden");

    User u;
    u.username = name;
    u.passwordHash = hashPassword(password);
    u.role = (requestedAdmin && requestedByIsAdmin) ? ROLE_ADMIN : ROLE_PERSON;
    users_[userCount_++] = u;

    std::string error;
    if (!save(error)) {
        --userCount_;                       /* roll back the in-memory change */
        return OpResult::failure(error, "storage");
    }
    return OpResult::success();
}

AuthResult Authentication::login(const std::string &username, const std::string &password) {
    AuthResult result;
    const std::string name = normalizeUsername(username);
    if (name.empty() || password.empty()) {
        result.error = "Username and password are required.";
        result.code = "invalid_input";
        return result;
    }

    for (int i = 0; i < userCount_; ++i) {
        if (lower(users_[i].username) != lower(name)) continue;

        if (!verifyPassword(password, users_[i].passwordHash)) {
            result.error = "Incorrect username or password.";
            result.code = "invalid_credentials";
            return result;
        }

        /* Successful authentication: transparently upgrade legacy hashes. */
        if (isLegacyHash(users_[i].passwordHash)) {
            users_[i].passwordHash = hashPassword(password);
            std::string saveError;
            if (save(saveError)) {
                result.migrated = true;
                std::fprintf(stderr, "[auth] migrated legacy password hash for '%s'\n",
                             users_[i].username.c_str());
            } else {
                std::fprintf(stderr, "[auth] could not persist hash migration: %s\n",
                             saveError.c_str());
            }
        }

        result.ok = true;
        result.username = users_[i].username;
        result.role = users_[i].role;
        return result;
    }

    result.error = "Incorrect username or password.";
    result.code = "invalid_credentials";
    return result;
}

OpResult Authentication::changePassword(const std::string &username, const std::string &oldPassword,
                                        const std::string &newPassword, bool skipOldPassword) {
    const std::string name = normalizeUsername(username);
    int index = -1;
    for (int i = 0; i < userCount_; ++i)
        if (lower(users_[i].username) == lower(name)) { index = i; break; }
    if (index < 0) return OpResult::failure("Account not found.", "not_found");

    if (!skipOldPassword && !verifyPassword(oldPassword, users_[index].passwordHash))
        return OpResult::failure("Current password is incorrect.", "invalid_credentials");

    std::string reason;
    if (!isValidPassword(newPassword, reason)) return OpResult::failure(reason, "invalid_password");
    if (verifyPassword(newPassword, users_[index].passwordHash))
        return OpResult::failure("The new password must differ from the current one.",
                                 "same_password");

    users_[index].passwordHash = hashPassword(newPassword);
    std::string error;
    if (!save(error)) return OpResult::failure(error, "storage");
    return OpResult::success();
}

}  /* namespace dms */
