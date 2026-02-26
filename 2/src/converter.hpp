#pragma once

#include <string>

namespace converter {

std::string decimal_to_binary(const std::string &decimal);
std::string binary_to_decimal(const std::string &binary);
std::string binary_to_grey(const std::string &binary);
std::string grey_to_binary(const std::string &grey);

} // namespace converter
