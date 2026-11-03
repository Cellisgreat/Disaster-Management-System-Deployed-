#include "sessions.h"

#include <algorithm>
#include <vector>

#include "crypto.h"

namespace dms {

SessionStore::SessionStore(int ttlSeconds) : ttl_(ttlSeconds) {}

std::string SessionStore::create(const std::string &username, UserRole role) {
    const std::string token = randomHex(32);      /* 256 bit opaque token */
    Session s;
    s.username = username;
    s.role = role;
    s.createdAt = std::time(0);
    s.expiresAt = s.createdAt + static_cast<std::time_t>(ttl_);

    std::lock_guard<std::mutex> guard(mutex_);
    sessions_[token] = s;
    return token;
}

bool SessionStore::lookup(const std::string &token, Session &out) {
    if (token.empty()) return false;
    std::lock_guard<std::mutex> guard(mutex_);
    std::map<std::string, Session>::iterator it = sessions_.find(token);
    if (it == sessions_.end()) return false;
    if (it->second.expired(std::time(0))) {
        sessions_.erase(it);
        return false;
    }
    out = it->second;
    return true;
}

void SessionStore::destroy(const std::string &token) {
    std::lock_guard<std::mutex> guard(mutex_);
    sessions_.erase(token);
}

int SessionStore::destroyForUser(const std::string &username) {
    std::lock_guard<std::mutex> guard(mutex_);
    int removed = 0;
    std::map<std::string, Session>::iterator it = sessions_.begin();
    while (it != sessions_.end()) {
        if (it->second.username == username) {
            sessions_.erase(it++);
            ++removed;
        } else {
            ++it;
        }
    }
    return removed;
}

int SessionStore::purgeExpired() {
    std::lock_guard<std::mutex> guard(mutex_);
    const std::time_t now = std::time(0);
    int removed = 0;
    std::map<std::string, Session>::iterator it = sessions_.begin();
    while (it != sessions_.end()) {
        if (it->second.expired(now)) {
            sessions_.erase(it++);
            ++removed;
        } else {
            ++it;
        }
    }
    return removed;
}

std::size_t SessionStore::size() {
    std::lock_guard<std::mutex> guard(mutex_);
    return sessions_.size();
}

}  /* namespace dms */
