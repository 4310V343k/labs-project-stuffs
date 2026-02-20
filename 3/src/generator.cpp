#include "generator.hpp"
#include "bignum.hpp"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <stdexcept>
#include <unistd.h>

#include <gmp.h>

void generate_and_save(const std::string &path_a, const std::string &path_b,
                       unsigned int size_bytes) {
    gmp_randstate_t state;
    gmp_randinit_mt(state);

    // Используем время + PID для разного зерна при каждом вызове
    unsigned long seed = static_cast<unsigned long>(std::time(nullptr))
                       ^ static_cast<unsigned long>(getpid());
    gmp_randseed_ui(state, seed);

    mpz_t n;
    mpz_init(n);

    unsigned int bits = size_bytes * 8;
    if (bits < 8) bits = 8; // минимум 1 байт

    auto write_number = [&](const std::string &path) {
        // Генерируем случайное число заданного размера и устанавливаем старший бит
        mpz_urandomb(n, state, bits);
        mpz_setbit(n, bits - 1); // гарантируем точный размер

        char *str = mpz_get_str(nullptr, 10, n);
        std::ofstream f(path);
        if (!f.is_open()) {
            std::free(str);
            mpz_clear(n);
            gmp_randclear(state);
            throw std::runtime_error("Cannot open file for writing: " + path);
        }
        f << str << "\n";
        std::free(str);
    };

    write_number(path_a);

    // Немного меняем зерно для второго числа
    gmp_randseed_ui(state, seed ^ 0xDEADBEEFUL);
    write_number(path_b);

    mpz_clear(n);
    gmp_randclear(state);
}

std::string load_from_file(const std::string &path) {
    std::ifstream f(path);
    if (!f.is_open())
        throw std::runtime_error("Не удалось открыть файл: " + path);

    std::string line;
    while (std::getline(f, line)) {
        // Обрезаем пробельные символы
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        size_t end = line.find_last_not_of(" \t\r\n");
        line = line.substr(start, end - start + 1);
        if (line.empty()) continue;

        if (!bignum_is_valid_decimal(line))
            throw std::runtime_error("Файл содержит некорректное число: " + path);
        return line;
    }
    throw std::runtime_error("Файл пуст или не содержит корректного числа: " + path);
}
