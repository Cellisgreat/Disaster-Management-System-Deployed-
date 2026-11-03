#include "models.h"

#include <cctype>
#include "csv_utils.h"

namespace dms {

static std::string lower(const std::string &s) {
    std::string out = s;
    for (std::size_t i = 0; i < out.size(); ++i)
        out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(out[i])));
    return out;
}

std::string serviceToString(ServiceType s) {
    switch (s) {
        case SERVICE_MEDICAL: return "Medical Help";
        case SERVICE_FOOD:    return "Food";
        case SERVICE_RESCUE:
        default:              return "Rescue";
    }
}

std::string serviceDisplayName(ServiceType s) { return serviceToString(s); }

bool parseService(const std::string &text, ServiceType &out) {
    std::string key = lower(trim(text));
    std::string compact;
    for (std::size_t i = 0; i < key.size(); ++i)
        if (std::isalnum(static_cast<unsigned char>(key[i]))) compact += key[i];

    if (compact == "rescue" || compact == "rescuehelp") { out = SERVICE_RESCUE; return true; }
    if (compact == "medical" || compact == "medicalhelp" || compact == "medic") {
        out = SERVICE_MEDICAL; return true;
    }
    if (compact == "food" || compact == "foodhelp" || compact == "foodsupply") {
        out = SERVICE_FOOD; return true;
    }
    return false;
}

std::string requestStatusToString(RequestStatus s) {
    return (s == REQUEST_COMPLETED) ? "Completed" : "Active";
}

bool parseRequestStatus(const std::string &text, RequestStatus &out) {
    const std::string key = lower(trim(text));
    if (key.empty()) { out = REQUEST_ACTIVE; return true; }
    if (key == "completed" || key == "complete" || key == "closed" || key == "done" || key == "1") {
        out = REQUEST_COMPLETED; return true;
    }
    if (key == "active" || key == "open" || key == "pending" || key == "0") {
        out = REQUEST_ACTIVE; return true;
    }
    return false;
}

std::string roleToString(UserRole r) { return (r == ROLE_ADMIN) ? "ADMIN" : "PERSON"; }

bool parseRole(const std::string &text, UserRole &out) {
    const std::string key = lower(trim(text));
    if (key == "admin" || key == "administrator") { out = ROLE_ADMIN; return true; }
    if (key == "person" || key == "user" || key == "citizen" || key == "" ) {
        out = ROLE_PERSON; return true;
    }
    return false;
}

}  /* namespace dms */
