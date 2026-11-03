/* ============================================================================
 *  input_utils.h  --  Robust console input helpers (legacy CLI only)
 * ----------------------------------------------------------------------------
 *  Kept from the original console application. The web server never uses these
 *  (HTTP handlers receive plain parameters), but preserving them keeps the
 *  original interface working exactly as before.
 * ==========================================================================*/
#ifndef DMS_INPUT_UTILS_H
#define DMS_INPUT_UTILS_H

#include <string>

namespace dms {

/** Read one trimmed line, prompting with `prompt`. */
std::string readLine(const std::string &prompt);

/** Read an integer inside [minValue, maxValue]; repeats until valid. */
int readIntInRange(const std::string &prompt, int minValue, int maxValue);

/** Read a positive double (used for distances). */
double readPositiveDouble(const std::string &prompt);

/** Ask a yes/no question; returns true for 'y'/'yes'. */
bool readConfirmation(const std::string &prompt);

/** Print a horizontal rule of the given character. */
void printRule(char character = '-', int width = 64);

/** Print a section title. */
void printTitle(const std::string &title);

/** Wait for the user to press Enter. */
void pauseConsole();

}  /* namespace dms */

#endif  /* DMS_INPUT_UTILS_H */
