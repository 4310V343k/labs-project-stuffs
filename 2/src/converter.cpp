#include "converter.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

namespace converter {

void check_raise_base(int base) {
    if (base < BASE_MIN || base > BASE_MAX) {
        throw std::invalid_argument(
            "Система исчисления должна быть в [" + std::to_string(BASE_MIN) + ", " +
            std::to_string(BASE_MAX) + "], получено " + std::to_string(base));
    }
}

int char_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'Z') return 10 + (c - 'A');
    return -1;
}

bool validate(const std::string& input, int base) {
    if (base < BASE_MIN || base > BASE_MAX) return false;
    if (input.empty()) return false;

    std::size_t start = 0;
    if (input[0] == '-') {
        if (input.size() == 1) return false;
        start = 1;
    }

    for (std::size_t i = start; i < input.size(); ++i) {
        int v = char_value(input[i]);
        if (v < 0 || v >= base) return false;
    }
    return true;
}

long long to_decimal(const std::string& input, int base) {
    check_raise_base(base);
    if (!validate(input, base)) {
        throw std::invalid_argument(
            "Неверные цифры для СИ " +
            std::to_string(base) + ": \"" + input + "\"");
    }

    bool negative = (input[0] == '-');
    long long result = 0;
    for (std::size_t i = (negative ? 1 : 0); i < input.size(); ++i) {
        result = result * base + char_value(input[i]);
    }
    return negative ? -result : result;
}

std::string from_decimal(long long value, int base) {
    check_raise_base(base);

    if (value == 0) return "0";

    bool negative = (value < 0);
    unsigned long long magnitude =
        negative ? static_cast<unsigned long long>(-(value + 1)) + 1
                 : static_cast<unsigned long long>(value);

    const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    std::string result;
    while (magnitude > 0) {
        result += digits[magnitude % static_cast<unsigned long long>(base)];
        magnitude /= static_cast<unsigned long long>(base);
    }
    if (negative) result += '-';
    std::reverse(result.begin(), result.end());
    return result;
}

std::string convert(const std::string& input, int from_base, int to_base) {
    check_raise_base(from_base);
    check_raise_base(to_base);

    if (input.empty()) {
        throw std::invalid_argument("Входная строка пуста.");
    }

    if (from_base == to_base) {
        if (!validate(input, from_base)) {
            throw std::invalid_argument(
                "Неверные цифры для СИ " + std::to_string(from_base));
        }
        std::string out = input;
        std::transform(out.begin(), out.end(), out.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return out;
    }

    long long decimal = to_decimal(input, from_base);
    return from_decimal(decimal, to_base);
}

} // namespace converter
