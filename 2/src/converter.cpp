#include "converter.hpp"

#include <string>
#include <stdexcept>

namespace converter {

std::string decimal_to_binary(const std::string &decimal) {
    if (decimal.empty()) return "";
    unsigned long long num;
    try {
        num = std::stoull(decimal);
    }
    catch (const std::exception &e) {
        throw std::invalid_argument("Некорректное десятичное число");
    }
    if (num == 0) return "0";
    std::string result;
    while (num > 0) {
        result = char('0' + (num % 2)) + result;
        num /= 2;
    }
    return result;
}

std::string binary_to_decimal(const std::string &binary) {
    if (binary.empty()) return "";
    unsigned long long num = 0;
    for (char c : binary) {
        if (c != '0' && c != '1')
            throw std::invalid_argument("Строка не бинарная");
        num = num * 2 + (c - '0');
    }
    return std::to_string(num);
}

std::string binary_to_grey(const std::string &binary) {
    if (binary.empty()) return "";
    std::string grey;
    
    if (binary[0] != '0' && binary[0] != '1')
        throw std::invalid_argument("Строка не бинарная");
    grey += binary[0];
    for (size_t i = 1; i < binary.size(); ++i) {
        if (binary[i] != '0' && binary[i] != '1')
            throw std::invalid_argument("Строка не бинарная");
        grey += char('0' + ((binary[i - 1] - '0') ^ (binary[i] - '0')));
    }
    return grey;
}

std::string grey_to_binary(const std::string &grey) {
    if (grey.empty()) return "";
    std::string binary;
    
    if (grey[0] != '0' && grey[0] != '1')
        throw std::invalid_argument("Строка не бинарная");
    binary += grey[0];
    for (size_t i = 1; i < grey.size(); ++i) {
        if (grey[i] != '0' && grey[i] != '1')
            throw std::invalid_argument("Строка не бинарная");
        binary += char('0' + ((binary[i - 1] - '0') ^ (grey[i] - '0')));
    }
    return binary;
}

}