/**
 * СПОСОБ ХРАНЕНИЯ ЧИСЛА:
 * 
 * BigNum представляет собой неотрицательное целое число в системе счисления
 * с основанием 2^32 (основание = 4294967296).
 * 
 * Структура данных:
 * - BigNum является вектором uint32_t элементов (называются "limbs" или "слова")
 * - Каждое слово хранит 32-битное значение (от 0 до 2^32-1)
 * - Число хранится в формате little-endian (младшие разряды в начале вектора)
 * 
 * Представление:
 * Если BigNum = [a0, a1, a2, ..., an], то число равно:
 *   число = a0 + a1*2^32 + a2*2^64 + ... + an*2^(32*n)
 * 
 * Пример:
 * - Число 5000000000 (больше чем 2^32-1 = 4294967295)
 * - Хранится как [705032704, 1] (в little-endian)
 * - Проверка: 705032704 + 1*2^32 = 705032704 + 4294967296 = 5000000000
 * 
 * Особенности:
 * - Старшие нули (trailing zeros) удаляются функцией normalize()
 * - Число 0 представляется как вектор с одним элементом [0]
 * - После операций может потребоваться нормализация
 * - Система счисления 2^32 позволяет эффективно использовать 64-битные операции
 *   при умножении двух 32-битных чисел (спасибо организации ЭВМ)
 */
#include "bignum.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <string>



// Внутренние вспомогательные функции

static void normalize(BigNum &a) {
    while (a.size() > 1 && a.back() == 0)
        a.pop_back();
}

static BigNum zero_bn() { return {0}; }
static BigNum one_bn()  { return {1}; }

// Делит десятичную строку на делитель; возвращает остаток.
// Используется bignum_from_decimal для перевода 10-й системы в 2^32.
static uint32_t decimal_divmod_inplace(std::string &s, uint64_t divisor) {
    uint64_t remainder = 0;
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        remainder = remainder * 10 + static_cast<uint64_t>(c - '0');
        result.push_back(static_cast<char>('0' + remainder / divisor));
        remainder %= divisor;
    }
    // Удаляем ведущие нули
    size_t first = result.find_first_not_of('0');
    s = (first == std::string::npos) ? "0" : result.substr(first);
    return static_cast<uint32_t>(remainder);
}

// Умножить десятичную строку на множитель и прибавить слагаемое (на месте).
// Используется bignum_to_decimal для перевода из 2^32 в десятичную.
static void decimal_mul_add(std::string &s, uint64_t factor, uint64_t addend) {
    uint64_t carry = addend;
    for (int i = static_cast<int>(s.size()) - 1; i >= 0; --i) {
        uint64_t v = static_cast<uint64_t>(s[i] - '0') * factor + carry;
        s[i] = static_cast<char>('0' + v % 10);
        carry = v / 10;
    }
    while (carry) {
        s.insert(s.begin(), static_cast<char>('0' + carry % 10));
        carry /= 10;
    }
}

// Преобразование

BigNum bignum_from_decimal(const std::string &s) {
    if (s.empty() || s == "0") return zero_bn();

    BigNum result;
    std::string tmp = s;
    // Удаляем ведущие нули
    size_t first = tmp.find_first_not_of('0');
    tmp = (first == std::string::npos) ? "0" : tmp.substr(first);

    while (tmp != "0") {
        result.push_back(decimal_divmod_inplace(tmp, 0x100000000ULL));
    }
    if (result.empty()) result.push_back(0);
    return result; // already little-endian
}

std::string bignum_to_decimal(const BigNum &a) {
    if (bignum_is_zero(a)) return "0";

    // Сначала обрабатываем старшее слово, затем умножаем-добавляем остальные.
    std::string result = "0";
    for (int i = static_cast<int>(a.size()) - 1; i >= 0; --i) {
        decimal_mul_add(result, 0x100000000ULL, a[i]);
    }
    return result;
}

// Предикаты

bool bignum_is_zero(const BigNum &a) {
    for (auto limb : a)
        if (limb != 0) return false;
    return true;
}

bool bignum_is_valid_decimal(const std::string &s) {
    if (s.empty()) return false;
    for (char c : s)
        if (c < '0' || c > '9') return false;
    // Нет ведущих нулей (кроме самой строки "0")
    if (s.size() > 1 && s[0] == '0') return false;
    return true;
}

// Сравнение

int bignum_cmp(const BigNum &a, const BigNum &b) {
    // Сравниваем эффективную длину (игнорируем хвостовые нули)
    size_t sa = a.size(), sb = b.size();
    while (sa > 1 && a[sa - 1] == 0) --sa;
    while (sb > 1 && b[sb - 1] == 0) --sb;
    if (sa != sb) return (sa < sb) ? -1 : 1;
    for (int i = static_cast<int>(sa) - 1; i >= 0; --i) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return  1;
    }
    return 0;
}

// Сложение

BigNum bignum_add(const BigNum &a, const BigNum &b) {
    size_t n = std::max(a.size(), b.size());
    BigNum result(n + 1, 0);
    uint64_t carry = 0;
    for (size_t i = 0; i < n; ++i) {
        uint64_t av = (i < a.size()) ? a[i] : 0;
        uint64_t bv = (i < b.size()) ? b[i] : 0;
        uint64_t sum = av + bv + carry;
        result[i] = static_cast<uint32_t>(sum);
        carry = sum >> 32;
    }
    result[n] = static_cast<uint32_t>(carry);
    normalize(result);
    return result;
}

// Вычитание (требуется a >= b)

BigNum bignum_sub(const BigNum &a, const BigNum &b) {
    BigNum result(a.size(), 0);
    int64_t borrow = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        int64_t av = static_cast<int64_t>(a[i]);
        int64_t bv = (i < b.size()) ? static_cast<int64_t>(b[i]) : 0;
        int64_t diff = av - bv - borrow;
        if (diff < 0) {
            diff += static_cast<int64_t>(1) << 32;
            borrow = 1;
        } else {
            borrow = 0;
        }
        result[i] = static_cast<uint32_t>(diff);
    }
    normalize(result);
    return result;
}

// Умножение (метод в столбик, O(n^2))

BigNum bignum_mul(const BigNum &a, const BigNum &b) {
    if (bignum_is_zero(a) || bignum_is_zero(b)) return zero_bn();
    size_t na = a.size(), nb = b.size();
    BigNum result(na + nb, 0);
    for (size_t i = 0; i < na; ++i) {
        uint64_t carry = 0;
        for (size_t j = 0; j < nb; ++j) {
            uint64_t cur = static_cast<uint64_t>(a[i]) * b[j]
                         + result[i + j]
                         + carry;
            result[i + j] = static_cast<uint32_t>(cur);
            carry = cur >> 32;
        }
        result[i + nb] += static_cast<uint32_t>(carry);
    }
    normalize(result);
    return result;
}

// Деление: алгоритм D Кнута (длинное деление для больших чисел)

std::pair<BigNum, BigNum> bignum_divmod(const BigNum &a, const BigNum &b) {
    if (bignum_is_zero(b))
        throw std::invalid_argument("Ошибка: деление на ноль");

    int cmp = bignum_cmp(a, b);
    if (cmp < 0) return {zero_bn(), a};
    if (cmp == 0) return {one_bn(), zero_bn()};

    // Нормализуем b так, чтобы старшее слово было >= 2^31 (сдвигаем оба)
    BigNum u = a, v = b;
    normalize(u); normalize(v);

    size_t n = v.size();
    size_t m = u.size() - n; // частное имеет не более m+1 слов

    // Ищем сдвиг d, чтобы v[n-1]*2^d >= 2^31
    uint32_t msv = v.back();
    int shift = 0;
    while ((msv & 0x80000000u) == 0) { msv <<= 1; ++shift; }

    // Сдвигаем u и v влево на shift бит
    if (shift > 0) {
        u.push_back(0);
        for (int i = static_cast<int>(u.size()) - 1; i > 0; --i)
            u[i] = (u[i] << shift) | (u[i-1] >> (32 - shift));
        u[0] <<= shift;

        uint32_t prev = 0;
        for (size_t i = 0; i < v.size(); ++i) {
            uint32_t next = v[i] >> (32 - shift);
            v[i] = (v[i] << shift) | prev;
            prev = next;
        }
        // prev должен быть 0, так как v уже нормализован
    }
    // Убеждаемся, что u достаточно длинный
    while (u.size() < n + m + 1) u.push_back(0);

    BigNum q(m + 1, 0);
    uint64_t vn1 = v[n - 1];
    uint64_t vn2 = (n >= 2) ? v[n - 2] : 0;

    for (int j = static_cast<int>(m); j >= 0; --j) {
        uint64_t u_hi = static_cast<uint64_t>(u[j + n]);
        uint64_t u_lo = static_cast<uint64_t>(u[j + n - 1]);
        uint64_t u_lo2 = (n >= 2) ? static_cast<uint64_t>(u[j + n - 2]) : 0;

        uint64_t qhat, rhat;
        if (u_hi >= vn1) {
            qhat = 0xFFFFFFFFULL;
            rhat = u_hi - vn1 + u_lo; // approximate
        } else {
            uint64_t num = (u_hi << 32) | u_lo;
            qhat = num / vn1;
            rhat = num % vn1;
        }

        // Уточняем qhat
        while (qhat > 0xFFFFFFFFULL || qhat * vn2 > ((rhat << 32) | u_lo2)) {
            --qhat;
            rhat += vn1;
            if (rhat > 0xFFFFFFFFULL) break;
        }

        // Умножаем и вычитаем
        int64_t borrow = 0;
        for (size_t i = 0; i < n; ++i) {
            uint64_t p = qhat * v[i];
            int64_t t = static_cast<int64_t>(u[j + i])
                      - static_cast<int64_t>(p & 0xFFFFFFFFULL)
                      - borrow;
            u[j + i] = static_cast<uint32_t>(t);
            borrow = static_cast<int64_t>(p >> 32) - (t >> 32);
        }
        int64_t t = static_cast<int64_t>(u[j + n]) - borrow;
        u[j + n] = static_cast<uint32_t>(t);

        q[j] = static_cast<uint32_t>(qhat);

        if (t < 0) {
            // Добавляем обратно
            --q[j];
            uint64_t carry2 = 0;
            for (size_t i = 0; i < n; ++i) {
                uint64_t s = static_cast<uint64_t>(u[j + i]) + v[i] + carry2;
                u[j + i] = static_cast<uint32_t>(s);
                carry2 = s >> 32;
            }
            u[j + n] += static_cast<uint32_t>(carry2);
        }
    }

    // Остаток в u[0..n-1]; сдвигаем вправо на shift
    BigNum rem(n);
    for (size_t i = 0; i < n; ++i) rem[i] = u[i];
    if (shift > 0) {
        for (size_t i = 0; i < n - 1; ++i)
            rem[i] = (rem[i] >> shift) | (rem[i + 1] << (32 - shift));
        rem[n - 1] >>= shift;
    }

    normalize(q);
    normalize(rem);
    return {q, rem};
}

// Возведение в степень (exp из {1, 2, 3})

BigNum bignum_pow(const BigNum &base, int exp) {
    if (exp < 1 || exp > 3)
        throw std::invalid_argument("Ошибка: степень должна быть 1, 2 или 3");
    BigNum result = base;
    for (int i = 1; i < exp; ++i)
        result = bignum_mul(result, base);
    return result;
}

// Целый корень квадратный (метод Ньютона)

BigNum bignum_isqrt(const BigNum &a) {
    if (bignum_is_zero(a)) return zero_bn();

    // Начальное приближение: 2^(ceil(bits/2))
    size_t bits = (a.size() - 1) * 32;
    uint32_t top = a.back();
    while (top >>= 1) ++bits;
    ++bits;

    size_t half_bits = (bits + 1) / 2;
    BigNum x(half_bits / 32 + 1, 0);
    x[half_bits / 32] = (1u << (half_bits % 32));
    normalize(x);

    // Итерация Ньютона: x_new = (x + a/x) / 2
    while (true) {
        auto [q, _r] = bignum_divmod(a, x);
        BigNum sum = bignum_add(x, q);
        // sum / 2
        BigNum x_new(sum.size(), 0);
        uint64_t carry = 0;
        for (int i = static_cast<int>(sum.size()) - 1; i >= 0; --i) {
            uint64_t cur = (carry << 32) | sum[i];
            x_new[i] = static_cast<uint32_t>(cur / 2);
            carry = cur % 2;
        }
        normalize(x_new);

        if (bignum_cmp(x_new, x) >= 0) break; // сошлось
        x = x_new;
    }
    return x;
}

// Проверка простоты (деление перебором)

bool bignum_is_prime(const BigNum &a) {
    // Малые случаи обрабатываем через перевод в десятичную строку
    std::string dec = bignum_to_decimal(a);

    // 0 и 1 не простые
    if (dec == "0" || dec == "1") return false;
    // 2 и 3 простые
    if (dec == "2" || dec == "3") return true;
    // Четные числа
    if ((a[0] & 1) == 0) return false;

    BigNum limit = bignum_isqrt(a);
    BigNum i = bignum_from_decimal("3");
    BigNum two = {2};

    // Перебор делителей от 3 до sqrt(a) с шагом 2
    while (bignum_cmp(i, limit) <= 0) {
        auto [_q, rem] = bignum_divmod(a, i);
        if (bignum_is_zero(rem)) return false;
        i = bignum_add(i, two);
    }
    return true;
}

// Проверка через исключение девяток

int bignum_digit_root(const BigNum &a) {
    std::string dec = bignum_to_decimal(a);
    int sum = 0;
    for (char c : dec) sum += (c - '0');
    sum %= 9;
    return sum; // 0 означает кратность 9 (или само число 0)
}

bool bignum_verify_add(const BigNum &a, const BigNum &b, const BigNum &sum) {
    int ra = bignum_digit_root(a);
    int rb = bignum_digit_root(b);
    int rs = bignum_digit_root(sum);
    return (ra + rb) % 9 == rs % 9;
}
