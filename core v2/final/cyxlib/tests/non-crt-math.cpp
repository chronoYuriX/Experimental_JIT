#include <stdint.h>
#include <string.h>

#define NCM_API __attribute__((used, noinline))
#ifndef __NCM_NO_PACKING
namespace NCM {
#endif

// ### consts ##########################################################################################################
const float  INF       = [](uint32_t r) { float f; memcpy(&f, &r, sizeof(f)); return f; }(0x7F800000U);
const float  NAN       = [](uint32_t r) { float f; memcpy(&f, &r, sizeof(f)); return f; }(0x7F8FFFFFU);
const double INF_D     = [](uint64_t r) { double d; memcpy(&d, &r, sizeof(d)); return d; }(0x7FF0000000000000);
const double NAN_D     = [](uint64_t r) { double d; memcpy(&d, &r, sizeof(d)); return d; }(0x7FF8000000000000);
const float  PI        = 3.14159265358979323846f;
const double PI_D      = 3.14159265358979323846;
const float  LN2       = 0.69314718055994530942f;
const double LN2_D     = 0.69314718055994530942;
const float  LOG10E    = 0.43429448190325182765f;
const double LOG10E_D  = 0.43429448190325182765;
const float  MATH_E    = 2.71828182845904523536f;
const float  MATH_E_D  = 2.71828182845904523536;


const float  EXP_MAX   = 86.f;
const double EXP_PAX_D = 700.;
#ifdef __NCM_FAST
	const float  ERR       = 1e-2f;
	const double ERR_D     = 1e-4;
	const int32_t MAX_ITER = 4, MAX_ITER_D = 8;
#elif defined(__NCM_HIGH_QUALITY)
	const float  ERR       = 1e-6f;
	const double ERR_D     = 1e-12;
	const int32_t MAX_ITER = 12, MAX_ITER_D = 24;
#else
	const float  ERR       = 1e-4f;
	const double ERR_D     = 1e-8;
	const int32_t MAX_ITER = 8, MAX_ITER_D = 16;
#endif

// ### basic functions #################################################################################################
bool isnan(float f) {
    uint32_t i; memcpy(&i, &f, 4);
    return (((i >> 23) & 0xFF) == 0xFF) && ((i & 0x7FFFFF) != 0);
}
bool isinf(float f) {
    uint32_t i; memcpy(&i, &f, 4);
    return (((i >> 23) & 0xFF) == 0xFF) && ((i & 0x7FFFFF) == 0);
}
bool isnan(double d) {
    uint64_t i; memcpy(&i, &d, 8);
    return (((i >> 52) & 0x7FF) == 0x7FF) && ((i & 0xFFFFFFFFFFFFFULL) != 0);
}
bool isinf(double d) {
    uint64_t i; memcpy(&i, &d, 8);
    return (((i >> 52) & 0x7FF) == 0x7FF) && ((i & 0xFFFFFFFFFFFFFULL) == 0);
}
float fsplit(float d, float* int_part) {
    *int_part = (int64_t)d;
    return d - *(float*)int_part;
}
double fsplit(double d, double* int_part) {
    *int_part = (int64_t)d;
    return d - *(double*)int_part;
}
float fabs(float f) { return (f > 0.f) ? f : -f; }
double fabs(double d) { return (d > 0.) ? d : -d; }

// ### math functions ##################################################################################################
float fact32(int32_t n) {
    float result = 1.f;
    for (int32_t i = 1; i < n; ++i) result *= i;
    return result;
}
float normalize2PI(float x) {
    while (x > PI) x -= 2.f * PI;
    while (x < -PI) x += 2.f * PI;
    return x;
}

float sin(float x) {
    x = normalize2PI(x);
    float sum = 0.f, term = x; int32_t n = 0;
    while (fabs(term) > ERR && n++ < MAX_ITER) {
        sum += term;
        term = -term * x * x / float((2 * n) * (2 * n + 1));
    }
    return sum;
}
float cos(float x) {
    x = normalize2PI(x);
    float sum = 0.f, term = 1.f; int32_t n = 0;
    while (fabs(term) > ERR && n++ < MAX_ITER) {
        sum += term;
        term = -term * x * x / float((2 * n - 1) * (2 * n));
    }
    return sum;
}
float tan(float x) {
    float s = sin(x), c = cos(x);
    if (fabs(c) < ERR) return (s >= 0.f) ? INF : -INF;
    return s / c;
}
float cot(float x) {
    float s = sin(x), c = cos(x);
    if (fabs(s) < ERR) {
        if (fabs(c) < ERR) return NAN;  // 0.f / 0.f = NAN
        return (c > 0.f) ? INF : -INF;
    } if (fabs(c) < ERR) return 0.0f;  // cot(pi / 2) = 0
    return c / s;
}
float csc(float x) {
	float s = sin(x);
	if (fabs(s) < ERR) return (s > 0.f) ? INF : -INF;
	return 1.f / s;
}
float sec(float x) {
	float c = cos(c);
	if (fabs(c) < ERR) return (c > 0.f) ? INF : -INF;
	return 1.f / c;
}

float ln(float x) {
    if (x <= 0.f) return -INF;
    int32_t k = 0;
    while (x >= 2.f) { x *= .5f; k++; }
    while (x < 1.f)  { x *= 2.f; k--; } // x in [1, 2)
    float y = (x - 1.f) / (x + 1.f); // ln((1 + y) / (1 - y))
    float y2 = y * y, sum = 0.f, term = y;
    int32_t n = 0;
    while (fabs(term) > ERR && n++ < MAX_ITER) {
        sum += term;
        term *= y2 * (2.f * (float)n - 1.f) / (2.f * (float)n + 1.f);
    }
    return 2.f * sum + (float)k * LN2;
}
float log10(float x) { return ln(x) * LOG10E; }
float log(float base, float mantissa) { return ln(mantissa) / ln(base); }

float ipow(float base, int64_t exponent) {
    if (exponent == 0) return 1.f;
    float result = 1.f;
    if (exponent < 0) { base = 1.f / base; exponent = -exponent; }
    while (exponent > 0) {
        if (exponent & 1) result *= base;
        base *= base;
        exponent >>= 1;
    }
    return result;
}
float exp(float x) {
    if (x == 0.f) return 1.f;
    if (x > EXP_MAX) return INF;
    if (x < -EXP_MAX) return 0.f;
    int64_t n = (int64_t)x; // e ^ x = e ^ (n + r) = (e ^ n) * (e ^ r), |r| < 1.f
    float r = x - (float)n, sum = 1.f, term = 1.f;
    for (int32_t i = 1; i < MAX_ITER; i++) {
        term *= r / i;
		sum += term;
        if (fabs(term) < ERR) break;
    }
    return ipow(MATH_E, n) * sum;
}
float pow(float base, float exponent) {
    if (base == 0.f) return (exponent <= 0.f) ? NAN : 0.f; // 0.f ^ 0.f or 0.f ^ negative
    if (base == 1.f || exponent == 0.f) return 1.f;
    if (exponent == 1.f) return base;
    if (base < 0.f) {
        if (exponent != (int64_t)exponent) return NAN;
        float result = exp(exponent * ln(-base));
        return ((int64_t)exponent % 2 == 0) ? result : -result;
    }
    return (exponent == (int64_t)exponent) ? ipow(base, (int64_t)exponent) : exp(exponent * ln(base));
}


#ifndef __NCM_NO_PACKING
}
#endif

