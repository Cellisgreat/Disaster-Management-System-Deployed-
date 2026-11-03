/* ============================================================================
 *  api_json.h  --  Model -> JSON serialisation for the REST API
 * ----------------------------------------------------------------------------
 *  Keeping serialisation in one place guarantees that every endpoint (and the
 *  frontend) sees records in exactly the same shape, and that sensitive fields
 *  (password hashes) never leak: they simply are not serialised anywhere.
 * ==========================================================================*/
#ifndef DMS_API_JSON_H
#define DMS_API_JSON_H

#include <string>
#include <vector>

#include "json.hpp"
#include "store.h"

namespace api {

using nlohmann::json;

/* ---- primitives --------------------------------------------------------- */
json areaJson(const dms::DataStore &store, const dms::Area &area);
json roadJson(const dms::DataStore &store, const dms::Road &road);
json volunteerJson(const dms::DataStore &store, const dms::Volunteer &volunteer);
json requestJson(const dms::DataStore &store, const dms::Request &request);
json allocationJson(const dms::AllocationEntry &entry);
json accountJson(const dms::User &user);

/* ---- aggregates --------------------------------------------------------- */
json areasListJson(const dms::DataStore &store);
json roadsListJson(const dms::DataStore &store);
json graphJson(const dms::DataStore &store);
json volunteersListJson(const dms::DataStore &store);
json requestsListJson(const dms::DataStore &store, const std::vector<dms::Request> &requests);
json statsJson(const dms::DataStore &store);
json routeJson(const dms::DataStore &store, const dms::RouteResult &route, int fromArea, int toArea);
/** Active request ids a volunteer is currently attached to. */
std::vector<int> activeRequestsFor(const dms::DataStore &store, int volunteerId);

/** Round money-free doubles nicely for the UI (2 decimals). */
double round2(double value);

}  /* namespace api */

#endif  /* DMS_API_JSON_H */
