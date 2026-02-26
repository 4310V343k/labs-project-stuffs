#include "converter.hpp"

#include <string>
#include <stdexcept>

namespace converter {

std::string decimal_to_binary(const std::string &decimal) {
    if (decimal.empty()) return "0";
    unsigned long long num = std::stoull(decimal);
    if (num == 0) return "0";
    std::string result;
    while (num > 0) {
        result = char('0' + (num % 2)) + result;
        num /= 2;
    }
    return result;
}

std::string binary_to_decimal(const std::string &binary) {
    if (binary.empty()) return "0";
    unsigned long long num = 0;
    for (char c : binary) {
        if (c != '0' && c != '1')
            throw std::invalid_argument("Invalid binary string");
        num = num * 2 + (c - '0');
    }
    return std::to_string(num);
}

std::string binary_to_grey(const std::string &binary) {
    if (binary.empty()) return "";
    std::string grey;
    grey += binary[0];
    for (size_t i = 1; i < binary.size(); ++i) {
        grey += char('0' + ((binary[i - 1] - '0') ^ (binary[i] - '0')));
    }
    return grey;
}

std::string grey_to_binary(const std::string &grey) {
    if (grey.empty()) return "";
    std::string binary;
    binary += grey[0];
    for (size_t i = 1; i < grey.size(); ++i) {
        binary += char('0' + ((binary[i - 1] - '0') ^ (grey[i] - '0')));
    }
    return binary;
}

}