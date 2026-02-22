#pragma once
#include <string>

// Генерирует два случайных числа по size_bytes байт с помощью GMP
// и записывает их десятичное представление в указанные файлы.
// throws std::runtime_error при ошибке ввода-вывода.
void generate_and_save(const std::string &path_a, const std::string &path_b,
                       unsigned int size_bytes);

// Считывает число из первой строки файла.
// Throws std::runtime_error при ошибке ввода-вывода
// или если содержимое - не положительное целое число
std::string load_from_file(const std::string &path);
