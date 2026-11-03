/* ============================================================================
 *  csv_utils.h  --  CSV parsing / writing helpers shared by every module
 * ----------------------------------------------------------------------------
 *  The project keeps using plain CSV files as its database. These helpers make
 *  that safe:
 *    * RFC-4180-ish parsing (quoted fields, escaped quotes, embedded commas),
 *    * field escaping on write (so user supplied messages cannot corrupt a row),
 *    * atomic file replacement (write to .tmp then rename) so a crash in the
 *      middle of a save can never leave a half written database behind,
 *    * tolerant number parsing for malformed legacy records.
 * ==========================================================================*/
#ifndef DMS_CSV_UTILS_H
#define DMS_CSV_UTILS_H

#include <string>
#include <vector>

namespace dms {

/** Split one CSV line into fields (handles "quoted, fields" and "" escapes). */
std::vector<std::string> parseCsvLine(const std::string &line);

/** Read a CSV file. Blank lines and lines starting with '#' are skipped. */
bool readCsvFile(const std::string &path, std::vector<std::vector<std::string> > &rows);

/** Escape a single field for CSV output. */
std::string csvEscape(const std::string &field);

/** Join already-escaped fields into one CSV line. */
std::string csvLine(const std::vector<std::string> &fields);

/** Atomically replace `path` with `content` (writes path.tmp then renames). */
bool writeFileAtomic(const std::string &path, const std::string &content);

/** Create every missing directory in `path` (posix + windows compatible). */
bool ensureDirectory(const std::string &path);

/** True if the file exists and can be opened for reading. */
bool fileExists(const std::string &path);

/* ---- tolerant field conversions for legacy/hand written records ---------- */
std::string trim(const std::string &s);
int    toInt(const std::string &s, int fallback = 0);
double toDouble(const std::string &s, double fallback = 0.0);
bool   toBoolFlag(const std::string &s, bool fallback = false);  /* 1/0 yes/no true/false */

}  /* namespace dms */

#endif  /* DMS_CSV_UTILS_H */
