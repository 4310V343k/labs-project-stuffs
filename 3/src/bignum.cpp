#include "bignum.hpp"

#include <algorithm>
#include <unordered_map>
#include <stdexcept>
#include <string>


/**
 * СПОСОБ ХРАНЕНИЯ ЧИСЛА:
 * 
 * BigNum представляет собой неотрицательное целое число в системе счисления
 * с основанием 2^32.
 * 
 * Структура данных:
 * - BigNum является вектором uint32_t элементов (называются "limbs" или "слова")
 * - Каждое слово хранит 32-битное значение (от 0 до 2^32-1)
 * - Число хранится в формате little-endian (младшие разряды в начале вектора)
 *   - Так индекс лимба - его степень в числе. Во всех функциях, кроме вывода,
 *     это удобно и читается естественно
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
 * - Старшие нули (trailing zeros) после вычислений удаляются функцией normalize()
 * - Число 0 представляется как вектор с одним элементом [0]
 * - Система счисления 2^32, а не 2^64 позволяет эффективно использовать
 *   64-битные операции (прямо как мой процессор) для 32-битных чисел (спасибо организации ЭВМ)
 */


// Убирает ведущие нули
static void normalize(BigNum &a) {
    while (a.size() > 1 && a.back() == 0)
        a.pop_back();
}

static BigNum zero_bn() { return {0}; }
static BigNum one_bn()  { return {1}; }

// Преобразование

BigNum bignum_from_decimal(const std::string &s) {
    if (s.empty() || s == "0") return zero_bn();
    BigNum result = {0};
    for (char c : s) {
        // result = result * 10 + digit
        uint64_t carry = static_cast<uint64_t>(c - '0');
        for (auto &limb : result) {
            uint64_t cur = static_cast<uint64_t>(limb) * 10 + carry;
            limb  = static_cast<uint32_t>(cur);        // младшие 32 бита
            carry = cur >> 32;                          // перенос
        }
        if (carry) result.push_back(static_cast<uint32_t>(carry));
    }
    normalize(result);
    return result;
}

// Умножает десятичную строку на множитель и прибавляет слагаемое (на месте).
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

// Количество десятичных цифр в BigNum (оценка сверху)
static size_t decimal_digits_estimate(const BigNum &a) {
    // 32 * log10(2) ≈ 9.6329
    return a.size() * 9633 / 1000 + 1;
}

using Pow10Cache = std::unordered_map<size_t, BigNum>;

// 10^k с кэшем: каждое значение вычисляется не более одного раза за вызов to_decimal
static const BigNum &bignum_pow10_cached(size_t k, Pow10Cache &cache) {
    auto it = cache.find(k);
    if (it != cache.end()) return it->second;

    BigNum result;
    if (k == 0)      result = {1};
    else if (k == 1) result = {10};
    else {
        const BigNum &half = bignum_pow10_cached(k / 2, cache);
        result = bignum_mul(half, half);
        if (k % 2 == 1)
            result = bignum_mul(result, BigNum{10});
    }
    return cache.emplace(k, std::move(result)).first->second;
}

// Divide-and-conquer конвертация. Примерно в 10 раз быстрее наивной для 10-чисел с ~200000 цифр
// Быстрее базового варианта за счёт того, что разделение лимбов значительно быстрее
// операций над отдельными десятичными символами
// При ~300000 цифр: ~12 уровней рекурсии
// Порог: при малых числах использование divmod становится дороже посимвольной конвертации
static constexpr size_t DC_THRESHOLD_LIMBS = 32; // ~300 десятичных цифр

static std::string to_decimal_dc(const BigNum &a, Pow10Cache &cache) {
    // Базовый случай: наивная конвертация
    if (a.size() <= DC_THRESHOLD_LIMBS) {
        if (bignum_is_zero(a)) return "0";
        std::string result = "0";
        for (int i = static_cast<int>(a.size()) - 1; i >= 0; --i)
            decimal_mul_add(result, 0x100000000ULL, a[i]);
        return result;
    }

    // Разбиваем N = hi * 10^k + lo, где k ≈ D/2 (половина десятичных цифр)
    size_t k = decimal_digits_estimate(a) / 2;

    const BigNum &mid = bignum_pow10_cached(k, cache);
    auto [hi, lo] = bignum_divmod(a, mid);

    std::string hi_str = to_decimal_dc(hi, cache);
    std::string lo_str = to_decimal_dc(lo, cache);

    // lo < 10^k, поэтому lo_str имеет не более k цифр;
    // дополняем нулями слева до ровно k символов
    if (lo_str.size() < k)
        lo_str.insert(0, k - lo_str.size(), '0');

    return hi_str + lo_str;
}

// Нужна для создания кэша степеней только один раз
std::string bignum_to_decimal(const BigNum &a) {
    if (bignum_is_zero(a)) return "0";
    Pow10Cache cache;
    return to_decimal_dc(a, cache);
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
    // Нет ведущих нулей (конечно, если 0 - не всё число)
    if (s.size() > 1 && s[0] == '0') return false;
    return true;
}

// Сравнение

int bignum_cmp(const BigNum &a, const BigNum &b) {
    // Сравниваем эффективную длину (игнорируем ведущие нули)
    size_t sa = a.size(), sb = b.size();
    // normalize требует копирования,
    // обойдёмся без него
    while (sa > 1 && a[sa - 1] == 0) --sa;
    while (sb > 1 && b[sb - 1] == 0) --sb;
    if (sa != sb) return (sa < sb) ? -1 : 1;
    // Если длина оинаковая, сравниваем по лимбам от старшего
    for (int i = static_cast<int>(sa) - 1; i >= 0; --i) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return  1;
    }
    // Все лимбы одинаковые, значит число тоже
    return 0;
}

// Сложение

BigNum bignum_add(const BigNum &a, const BigNum &b) {
    // Результат может быть на 1 слово длиннее максимального из входных чисел,
    // если есть перенос из старшего разряда
    size_t n = std::max(a.size(), b.size());
    BigNum result(n + 1, 0);
    uint64_t carry = 0;
    for (size_t i = 0; i < n; ++i) {
        uint64_t av = (i < a.size()) ? a[i] : 0;
        uint64_t bv = (i < b.size()) ? b[i] : 0;
        uint64_t sum = av + bv + carry;
        // Младшие 32 (на самом деле 1 разряд) бита результата записываем в число
        // Старшие 32 бита переносим на след итерацию
        result[i] = static_cast<uint32_t>(sum);
        carry = sum >> 32;
    }
    // Записываем перенос в старший разряд
    result[n] = static_cast<uint32_t>(carry);
    normalize(result);
    return result;
}

// Умножение в столбик (спасибо организации ЭВМ, снова)

BigNum bignum_mul(const BigNum &a, const BigNum &b) {
    if (bignum_is_zero(a) || bignum_is_zero(b)) return zero_bn();
    size_t na = a.size(), nb = b.size();
    // Результат будет максимум na + nb лимбов
    BigNum result(na + nb, 0);
    // Умножаем каждое слово a на каждое слово b
    for (size_t i = 0; i < na; ++i) {
        // Тут почти как сложение, только произведение, и an раз
        uint64_t carry = 0;
        for (size_t j = 0; j < nb; ++j) {
            uint64_t cur = static_cast<uint64_t>(a[i]) * b[j]
                         + result[i + j] // Промежуточный результат с прошлой итерации i
                         + carry;
            // Младшие 32 бита (на самом деле 1 разряд) результата записываем в число
            // Старшие 32 бита переносим на след итерацию
            result[i + j] = static_cast<uint32_t>(cur);
            carry = cur >> 32;
        }
        // Записываем перенос в следующий старший разряд
        result[i + nb] += static_cast<uint32_t>(carry);
    }
    normalize(result);
    return result;
}

// Деление
// Тут уже использовал сложный алгоритм, т.к. на деление ещё завязан корень и
// конвертация в строку и, соответственно, почти все другие операции
// Реализован Алгоритм D Кнута (длинное деление для больших чисел)
// Много математики, но комментарии объясняют только код, остальное есть в https://habr.com/ru/articles/974048/

std::pair<BigNum, BigNum> bignum_divmod(const BigNum &a, const BigNum &b) {
    if (bignum_is_zero(b))
        throw std::invalid_argument("Ошибка: деление на ноль");

    int cmp = bignum_cmp(a, b);
    // тривиальные случаи
    if (cmp < 0) return {zero_bn(), a};
    if (cmp == 0) return {one_bn(), zero_bn()};

    // Копируем числа для нормализации и изменения на месте
    // Нормализация: домножать на нек. число (2), пока делитель не больше половины разряда (2^31)
    // то есть имеет старший бит старшего лимба равный 1
    BigNum u = a, v = b;
    normalize(u); normalize(v);

    size_t n = v.size();
    size_t m = u.size() - n; // тогда частное q имеет не более m+1 слов

    // Ищем нужный сдвиг
    uint32_t msv = v.back();
    int shift = 0;
    while ((msv & (1u << 31)) == 0) { msv <<= 1; ++shift; }

    // И сдвигаем каждый лимб u и v влево на shift бит, сохраняя перенос между словами
    u.push_back(0);
    if (shift > 0) {
        for (int i = static_cast<int>(u.size()) - 1; i > 0; --i)
            u[i] = (u[i] << shift) | (u[i-1] >> (32 - shift));
        u[0] <<= shift;

        for (int i = static_cast<int>(v.size()) - 1; i > 0; --i)
            v[i] = (v[i] << shift) | (v[i - 1] >> (32 - shift));
        v[0] <<= shift;
    }
    // Нормализация завершена

    BigNum q(m + 1, 0); // частное
    uint64_t vn1 = v[n - 1]; // старший лимб делителя. Гарантированно >= 2^31
    uint64_t vn2 = (n >= 2) ? v[n - 2] : 0; // второй по старшинству лимб делителя (или 0, если его нет)

    // От старшего к младшему лимбу делимого, вычисляем по одному слову частного q[j]
    for (int j = static_cast<int>(m); j >= 0; --j) {
        // Берём окно делимого - два старших разряда и ещё один для проверки
        uint64_t u_hi = static_cast<uint64_t>(u[j + n]);
        uint64_t u_lo = static_cast<uint64_t>(u[j + n - 1]);
        uint64_t u_lo2 = (n >= 2) ? static_cast<uint64_t>(u[j + n - 2]) : 0;

        uint64_t qhat, rhat;
        // Оцениваем qhat сверху
        if (u_hi >= vn1) { // делимое больше делителя
            qhat = 0xFFFFFFFFULL; // максимум 2^32-1
            rhat = u_hi - vn1 + u_lo; // приблизительно
        } else { // делимое меньше делителя, можно оценить qhat через обычное деление
            uint64_t num = (u_hi << 32) | u_lo;
            qhat = num / vn1; // а чо придумывать велосипед
            rhat = num % vn1; // деление и остаток, кстати, это одна операция внутри, а не две раздельные
        }

        // Уточняем qhat, т.к. при делении мы игнорировали младшие разряды
        // Пока восстановленное делимое больше реального, уменьшаем qhat
        // После этого он *всё ещё* может быть на единицу больше, чем нужно, но не больше
        while (qhat * vn2 > ((rhat << 32) | u_lo2)) {
            --qhat;
            rhat += vn1;
            if (rhat > 0xFFFFFFFFULL) break;
        }

        // Вычитание столбиком (в задании вычитания нет, но пришлось сделать!!! везде обман!!!)
        // u[j..j+n] - qhat * v[0..n-1], при этом умножаем прямо в цикле, без отдельной переменной
        // в каждом j делаем u[j+i] - qhat * v[i] - borrow (с переносом с младших разрядов)
        int64_t borrow = 0;
        for (size_t i = 0; i < n; ++i) {
            uint64_t p = qhat * v[i];
            int64_t t = static_cast<int64_t>(u[j + i]) // из числа
                      - static_cast<int64_t>(p & 0xFFFFFFFFULL) // вычитаем произведение (старшая часть пойдёт в перенос)
                      - borrow; // и перенос из младших разрядов
            // младший разряд записываем, старший разряд переносим
            u[j + i] = static_cast<uint32_t>(t);
            borrow = static_cast<int64_t>(p >> 32) - (t >> 32); // -1 или 0
        }
        // Под конец вычитаем перенос из старшего разряда
        int64_t t = static_cast<int64_t>(u[j + n]) - borrow;
        u[j + n] = static_cast<uint32_t>(t);

        // Записываем qhat в частное
        q[j] = static_cast<uint32_t>(qhat);

        // Если qhat всё же был на единицу больше, чем нужно, то
        // результат вычитания будет отрицательным, и нам нужно добавить делитель обратно
        if (t < 0) {
            --q[j];
            // обычное сложение, прямо как bignum_add
            uint64_t carry = 0;
            for (size_t i = 0; i < n; ++i) {
                uint64_t s = static_cast<uint64_t>(u[j + i]) + v[i] + carry;
                u[j + i] = static_cast<uint32_t>(s);
                carry = s >> 32;
            }
            u[j + n] += static_cast<uint32_t>(carry);
        }
    }

    // В u остался остаток (ха!). 
    // Сдвигаем его вправо на shift (делим на то, на что умножали в начале. В частном же умножения сократились сами)
    BigNum rem(n);
    for (size_t i = 0; i < n; ++i) rem[i] = u[i];
    if (shift > 0) {
        for (size_t i = 0; i < n - 1; ++i)
            rem[i] = (rem[i] >> shift) | (rem[i + 1] << (32 - shift));
        rem[n - 1] >>= shift;
    }

    normalize(q);
    normalize(rem);
    return {q, rem}; // фух
}

// Возведение в степень (exp из {1, 2, 3})

BigNum bignum_pow(const BigNum &base, int exp) {
    if (exp < 1 || exp > 3)
        throw std::invalid_argument("Ошибка: степень должна быть 1, 2 или 3");
    BigNum result = base;
    for (int i = 1; i < exp; ++i)
        result = bignum_mul(result, base); // как сложно
    return result;
}

// Целочисленный корень (метод Ньютона)

BigNum bignum_isqrt(const BigNum &a) {
    if (bignum_is_zero(a)) return zero_bn();

    // Начальное приближение: 2^(ceil(bits/2))
    // Считаем bits
    size_t bits = (a.size() - 1) * 32;
    uint32_t top = a.back();
    while (top >>= 1) ++bits;
    ++bits;

    // С округлением вверх
    size_t half_bits = (bits + 1) / 2;
    // 2^n === 1 << n
    BigNum x(half_bits / 32 + 1, 0);
    // В старшем бите нужного лимба
    x[half_bits / 32] = (1u << (half_bits % 32));

    // Итерация Ньютона: x_new = (x + a/x) / 2
    while (true) {
        // Вычисляем a/x, спасибо крутому делению
        auto [q, _r] = bignum_divmod(a, x);
        BigNum sum = bignum_add(x, q);
        // sum / 2
        uint64_t carry = 0;
        for (int i = static_cast<int>(sum.size()) - 1; i >= 0; --i) {
            uint64_t cur = (carry << 32) | sum[i];
            sum[i] = static_cast<uint32_t>(cur >> 1);
            carry = cur & 1;
        }
        normalize(sum);
        
        if (bignum_cmp(sum, x) >= 0) break; // сошлось
        x = sum;
    }
    return x;
}

// Проверка простоты (деление перебором)

bool bignum_is_prime(const BigNum &a) {
    if (bignum_cmp(a, one_bn()) <= 0) return false; // 0 и 1 не простые
    if (bignum_cmp(a, BigNum{3}) <= 0) return true; // 2 и 3 простые
    if ((a[0] & 1) == 0) return false; // чётные числа не простые

    BigNum limit = bignum_isqrt(a); // простых множителей выше квадратного корня быть не может
    BigNum i = bignum_from_decimal("3");
    BigNum two = {2};

    // Перебор делителей от 3 до sqrt(a) с шагом 2
    // Можно сравнивать только текущий лимб, но я уже устал, босс
    while (bignum_cmp(i, limit) <= 0) {
        // если делится без остатка - не простое
        auto [_q, rem] = bignum_divmod(a, i);
        if (bignum_is_zero(rem)) return false;

        i = bignum_add(i, two); // на 2 не делится, уже проверили
    }
    return true;
}

// Проверка через исключение девяток

int bignum_digit_root_mod_9(const BigNum &a) {
    // 2^32 ≡ 4 (mod 9), поэтому
    // N ≡ a[0]*4^0 + a[1]*4^1 + a[2]*4^2 + ... (mod 9)
    // Степени 4 по mod 9 циклически: 1, 4, 7, 1, 4, 7, ...  (период 3)
    static const int pow4mod9[3] = {1, 4, 7};
    uint64_t sum = 0;
    for (size_t i = 0; i < a.size(); ++i){
        sum += static_cast<uint64_t>(a[i]) * pow4mod9[i % 3];
        sum %= 9;
    }
    return static_cast<int>(sum % 9);
}
