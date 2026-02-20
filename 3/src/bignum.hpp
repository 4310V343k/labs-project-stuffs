#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// Big unsigned integer stored as a little-endian vector of 32-bit limbs
// in base 2^32.  limbs[0] is the least significant 32-bit word.
using BigNum = std::vector<uint32_t>;

// ── Conversion ──────────────────────────────────────────────────────────────
BigNum      bignum_from_decimal(const std::string &s);
std::string bignum_to_decimal(const BigNum &a);

// ── Predicates ──────────────────────────────────────────────────────────────
bool bignum_is_zero(const BigNum &a);
bool bignum_is_valid_decimal(const std::string &s);  // digits only, no leading zeros

// ── Comparison ──────────────────────────────────────────────────────────────
// Returns -1, 0, or 1
int bignum_cmp(const BigNum &a, const BigNum &b);

// ── Arithmetic ──────────────────────────────────────────────────────────────
BigNum bignum_add(const BigNum &a, const BigNum &b);
BigNum bignum_sub(const BigNum &a, const BigNum &b); // requires a >= b
BigNum bignum_mul(const BigNum &a, const BigNum &b);

// Returns {quotient, remainder}; throws std::invalid_argument if b == 0
std::pair<BigNum, BigNum> bignum_divmod(const BigNum &a, const BigNum &b);

// exp must be 1, 2, or 3; throws std::invalid_argument otherwise
BigNum bignum_pow(const BigNum &base, int exp);

// Integer square root (floor)
BigNum bignum_isqrt(const BigNum &a);

// ── Number-theory ───────────────────────────────────────────────────────────
// Trial-division primality test up to isqrt(a).  Blocking — may be slow.
bool bignum_is_prime(const BigNum &a);

// ── Casting-out-nines ────────────────────────────────────────────────────────
// Sum of decimal digits mod 9 (returns 9 instead of 0 for non-zero numbers)
int  bignum_digit_root(const BigNum &a);
bool bignum_verify_add(const BigNum &a, const BigNum &b, const BigNum &sum);
