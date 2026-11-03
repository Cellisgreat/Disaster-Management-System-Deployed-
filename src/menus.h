/* ============================================================================
 *  menus.h  --  Legacy console interface (preserved, secondary to the website)
 * ----------------------------------------------------------------------------
 *  The original interactive menus, kept so the application can still be used
 *  from a terminal (`DisasterManagementServer --cli`). They call exactly the
 *  same functions as the REST API -- the C++ modules are the single source of
 *  truth. All console I/O lives here (and in input_utils.*) so that no business
 *  module depends on cin/cout anymore.
 * ==========================================================================*/
#ifndef DMS_MENUS_H
#define DMS_MENUS_H

#include "store.h"

namespace dms {

class ConsoleApp {
public:
    explicit ConsoleApp(DataStore &store);
    /** Runs the login loop until the user chooses to exit. */
    void run();

private:
    void adminSession(const std::string &username);
    void personSession(const std::string &username);

    DataStore &store_;

    void printVolunteers() const;
    void printRequests(bool adminView) const;
    void printRoads() const;
    void printAreas() const;
    void handleShortestRoute();
    void handleAddVolunteer();
    void handleRoadState(bool blocked);
    void handleResetAvailability();
    void handleCompleteRequest();
    void handleCreateAdmin();
    void handleSubmitRequest(const std::string &username);
    void handleMyRequests(const std::string &username);
    void handleChangePassword(const std::string &username);
};

}  /* namespace dms */

#endif  /* DMS_MENUS_H */
