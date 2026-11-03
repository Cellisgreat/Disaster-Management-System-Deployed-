#include "csv_utils.h"

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace dms {

std::string trim(const std::string &s) {
    std::size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::vector<std::string> parseCsvLine(const std::string &line) {
    std::vector<std::string> out;
    std::string field;
    bool inQuotes = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    field += '"';
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                field += c;
            }
        } else {
            if (c == '"') {
                inQuotes = true;
            } else if (c == ',') {
                out.push_back(field);
                field.clear();
            } else if (c == '\r') {
                /* ignore CR so CRLF files parse identically */
            } else {
                field += c;
            }
        }
    }
    out.push_back(field);
    return out;
}

bool readCsvFile(const std::string &path, std::vector<std::vector<std::string> > &rows) {
    rows.clear();
    std::ifstream in(path.c_str());
    if (!in.is_open()) return false;

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
        const std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        rows.push_back(parseCsvLine(line));
    }
    return true;
}

std::string csvEscape(const std::string &field) {
    bool needsQuotes = false;
    for (std::size_t i = 0; i < field.size(); ++i) {
        const char c = field[i];
        if (c == ',' || c == '"' || c == '\n' || c == '\r') { needsQuotes = true; break; }
    }
    if (!needsQuotes) return field;

    std::string out = "\"";
    for (std::size_t i = 0; i < field.size(); ++i) {
        if (field[i] == '"') out += "\"\"";
        else if (field[i] == '\n' || field[i] == '\r') out += ' ';  /* keep rows single-line */
        else out += field[i];
    }
    out += '"';
    return out;
}

std::string csvLine(const std::vector<std::string> &fields) {
    std::string out;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (i) out += ',';
        out += csvEscape(fields[i]);
    }
    return out;
}

bool ensureDirectory(const std::string &path) {
    if (path.empty()) return false;
    std::string current;
    for (std::size_t i = 0; i <= path.size(); ++i) {
        const bool atEnd = (i == path.size());
        const char c = atEnd ? '/' : path[i];
        if (c == '/' || c == '\\') {
            if (!current.empty() && current != "." && current != "..") {
#ifdef _WIN32
                _mkdir(current.c_str());
#else
                mkdir(current.c_str(), 0775);
#endif
            }
        }
        if (!atEnd) current += path[i];
    }
    return true;
}

bool writeFileAtomic(const std::string &path, const std::string &content) {
    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp.c_str(), std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        out.flush();
        if (!out.good()) { out.close(); std::remove(tmp.c_str()); return false; }
    }
    std::remove(path.c_str());          /* windows rename() refuses existing targets */
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        std::remove(tmp.c_str());
        return false;
    }
    return true;
}

bool fileExists(const std::string &path) {
    std::ifstream in(path.c_str());
    return in.is_open();
}

int toInt(const std::string &s, int fallback) {
    const std::string t = trim(s);
    if (t.empty()) return fallback;
    char *end = 0;
    errno = 0;
    const long v = std::strtol(t.c_str(), &end, 10);
    if (errno != 0 || end == t.c_str()) return fallback;
    return static_cast<int>(v);
}

double toDouble(const std::string &s, double fallback) {
    const std::string t = trim(s);
    if (t.empty()) return fallback;
    char *end = 0;
    errno = 0;
    const double v = std::strtod(t.c_str(), &end);
    if (errno != 0 || end == t.c_str()) return fallback;
    return v;
}

bool toBoolFlag(const std::string &s, bool fallback) {
    const std::string t = trim(s);
    if (t.empty()) return fallback;
    for (std::size_t i = 0; i < t.size(); ++i) {
        if (std::isalpha(static_cast<unsigned char>(t[i]))) {
            std::string low = t;
            for (std::size_t j = 0; j < low.size(); ++j)
                low[j] = static_cast<char>(std::tolower(static_cast<unsigned char>(low[j])));
            if (low == "true" || low == "yes" || low == "y" || low == "available" || low == "open")
                return true;
            if (low == "false" || low == "no" || low == "n" || low == "assigned" || low == "blocked")
                return false;
            return fallback;
        }
    }
    return toInt(t, fallback ? 1 : 0) != 0;
}

}  /* namespace dms */
