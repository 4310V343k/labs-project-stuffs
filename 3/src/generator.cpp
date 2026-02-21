#include "generator.hpp"
#include "bignum.hpp"

#include <cstdlib>
#include <ctime>
#include <fstream>
#include <functional>
#include <memory>
#include <stdexcept>
#include <thread>

#include <gmp.h>

// RAII-обёртки
struct GmpRand {
    gmp_randstate_t state;
    GmpRand()  { gmp_randinit_mt(state); }
    ~GmpRand() { gmp_randclear(state); }
    GmpRand(const GmpRand&)            = delete;
    GmpRand& operator=(const GmpRand&) = delete;
};

struct Mpz {
    mpz_t val;
    Mpz()  { mpz_init(val); }
    ~Mpz() { mpz_clear(val); }
    Mpz(const Mpz&)            = delete;
    Mpz& operator=(const Mpz&) = delete;
};

void generate_and_save(const std::string &path_a, const std::string &path_b,
                       unsigned int size_bytes) {
    GmpRand rng;

    // Используем время + хэш thread id для разного зерна при каждом вызове
    unsigned long seed = static_cast<unsigned long>(std::time(nullptr))
                       ^ static_cast<unsigned long>(
                             std::hash<std::thread::id>{}(
                                 std::this_thread::get_id()));
    gmp_randseed_ui(rng.state, seed);

    Mpz n;

    unsigned int bits = size_bytes * 8;
    if (bits < 8) bits = 8; // минимум 1 байт

    auto write_number = [&](const std::string &path) {
        // Генерируем случайное число заданного размера и устанавливаем старший бит
        mpz_urandomb(n.val, rng.state, bits);
        mpz_setbit(n.val, bits - 1); // гарантируем точный размер

        std::unique_ptr<char, decltype(&std::free)> str{
            mpz_get_str(nullptr, 10, n.val), std::free};

        std::ofstream f(path);
        if (!f.is_open())
            throw std::runtime_error("Cannot open file for writing: " + path);

        f << str.get() << "\n";
    };

    write_number(path_a);

    // Немного меняем зерно для второго числа
    gmp_randseed_ui(rng.state, seed ^ 0xDEADBEEFUL);
    write_number(path_b);
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
