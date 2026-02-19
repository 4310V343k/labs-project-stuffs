#pragma once

#include <string>

namespace converter {

constexpr int BASE_MIN = 2;
constexpr int BASE_MAX = 36;

bool validate(const std::string& input, int base);

long long to_decimal(const std::string& input, int base);

std::string from_decimal(long long value, int base);

std::string convert(const std::string& input, int from_base, int to_base);

} // namespace converter
