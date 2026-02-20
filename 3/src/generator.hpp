#pragma once
#include <string>

// Сгенерировать два случайных числа по size_bytes байт с помощью GMP
// и записать их десятичное представление в указанные файлы.
// Бросает std::runtime_error при ошибке ввода-вывода.
void generate_and_save(const std::string &path_a, const std::string &path_b,
                       unsigned int size_bytes);

// Read a big-decimal number from a file (first non-empty line).
// Throws std::runtime_error if the file cannot be opened or the content is
// not a valid non-negative integer.
std::string load_from_file(const std::string &path);
