#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// Подробности имплементации в bignum.cpp
using BigNum = std::vector<uint32_t>;

// -- Конверсия ---------------------------------------------------------------
BigNum      bignum_from_decimal(const std::string &s);
std::string bignum_to_decimal(const BigNum &a);

// -- Предикаты ---------------------------------------------------------------
bool bignum_is_zero(const BigNum &a);
bool bignum_is_valid_decimal(const std::string &s);  // только цифры, нет ведущих нулей

// -- Сравнение ----------------------------------------------------------------
// Возвращает -1, 0, или 1
int bignum_cmp(const BigNum &a, const BigNum &b);

// -- Арифметика ---------------------------------------------------------------
BigNum bignum_add(const BigNum &a, const BigNum &b);
BigNum bignum_mul(const BigNum &a, const BigNum &b);

// Возвращает {частное, остаток}; throws std::invalid_argument if b == 0
std::pair<BigNum, BigNum> bignum_divmod(const BigNum &a, const BigNum &b);

// экспонента должна быть 1, 2, или 3; throws std::invalid_argument в ином случае
BigNum bignum_pow(const BigNum &base, int exp);

// Целочисленный корень с округлением вниз
BigNum bignum_isqrt(const BigNum &a);

// -- Теория чисел --------------------------------------------------------------
// Проверяет все числа до квадратного корня, что может быть довольно медленно
bool bignum_is_prime(const BigNum &a);

// -- Исключение девяток --------------------------------------------------------
// Сумма десятичных чисел % 9, но [1; 9] вместо [0; 8], чтобы отличать число 0 от 9*n % 9
int  bignum_digit_root_mod_9(const BigNum &a);
