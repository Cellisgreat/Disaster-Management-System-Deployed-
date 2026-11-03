#include "input_utils.h"

#include <cctype>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>

#include "csv_utils.h"

namespace dms {

std::string readLine(const std::string &prompt) {
    std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) return "";
    return trim(line);
}

int readIntInRange(const std::string &prompt, int minValue, int maxValue) {
    while (true) {
        const std::string text = readLine(prompt);
        if (text.empty()) {
            std::cout << "  ! Please enter a number.\n";
            continue;
        }
        char *end = 0;
        const long value = std::strtol(text.c_str(), &end, 10);
        if (end == text.c_str() || *end != '\0') {
            std::cout << "  ! '" << text << "' is not a valid number.\n";
            continue;
        }
        if (value < minValue || value > maxValue) {
            std::cout << "  ! Value must be between " << minValue << " and " << maxValue << ".\n";
            continue;
        }
        return static_cast<int>(value);
    }
}

double readPositiveDouble(const std::string &prompt) {
    while (true) {
        const std::string text = readLine(prompt);
        char *end = 0;
        const double value = std::strtod(text.c_str(), &end);
        if (end == text.c_str() || *end != '\0' || value <= 0.0) {
            std::cout << "  ! Please enter a positive number.\n";
            continue;
        }
        return value;
    }
}

bool readConfirmation(const std::string &prompt) {
    const std::string answer = readLine(prompt + " [y/N]: ");
    if (answer.empty()) return false;
    const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(answer[0])));
    return c == 'y';
}

void printRule(char character, int width) {
    std::cout << std::string(static_cast<std::size_t>(width), character) << '\n';
}

void printTitle(const std::string &title) {
    std::cout << '\n';
    printRule('=');
    std::cout << title << '\n';
    printRule('=');
}

void pauseConsole() {
    std::cout << "\nPress Enter to continue...";
    std::string discard;
    std::getline(std::cin, discard);
}

}  /* namespace dms */
