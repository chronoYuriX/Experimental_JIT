#include <stdint.h>
#include <memory.h>

#ifndef __NCM_NO_PACKING
namespace NCM {
#endif

//const float INF = std::bit_cast<float>(0x7F800000U), NAN = std::bit_cast<float>(0x7FC00000);
const float INF = [](uint32_t r) { float f; memcpy(&f, &r, sizeof(f)); return f; }(0x7F800000U);
const float NAN = [](uint32_t r) { float f; memcpy(&f, &r, sizeof(f)); return f; }(0x7F800000U);

bool isnan(float f) {
    uint32_t i = *(uint32_t*)&f;
    return ((i >> 23) & 0xFF == 0xFF) && (i & 0x7FFFFF != 0);
}

bool isinf(float f) {
    uint32_t i = *(uint32_t*)&f;
    return ((i >> 23) & 0xFF == 0xFF) && (i & 0x7FFFFF == 0);
}

double split_double(double d, double* int_part) {
    *int_part = (int64_t)d;
    return d - *(double*)int_part;
}

float absf(float f) { return (f > 0) ? f : -f; }

// --- 辅助常量 ---
static const float PI = 3.14159265358979323846f;
static const float TWO_PI = 6.28318530717958647692f;
static const float HALF_PI = 1.57079632679489661923f;
static const float INV_PI = 0.31830988618379067154f;

// --- 辅助函数：将角度归约到 [-PI, PI] ---
static float reduce_to_pi(float x) {
    // 使用整数除法归约，避免精度损失累积
    float n = (float)(int)(x * INV_PI + (x >= 0 ? 0.5f : -0.5f));
    return x - n * PI;
}

// --- 辅助函数：多项式计算（Horner法）---
static float poly_eval(float x, const float* coeffs, int n) {
    float result = coeffs[n - 1];
    for (int i = n - 2; i >= 0; --i)
        result = result * x + coeffs[i];
    return result;
}

// --- sinf：使用泰勒展开/切比雪夫逼近，范围 [-PI/2, PI/2] ---
float sinf(float f) {
    // 先归约到 [-PI, PI]
    f = reduce_to_pi(f);
    // 进一步归约到 [-PI/2, PI/2]，利用 sin(-x) = -sin(x), sin(PI-x) = sin(x)
    int sign = 1;
    if (f < -HALF_PI) {
        f += PI;
        sign = -1;
    } else if (f > HALF_PI) {
        f = PI - f;
    }
    // 奇函数，取绝对值，最后补符号
    float x = (f < 0) ? -f : f;
    if (f < 0) sign = -sign;

    // 切比雪夫逼近 sin(x) ≈ x * (1 - x2 * (1/6 - x2 * (1/120 - x2/5040)))
    float x2 = x * x;
    float result = x * (1.0f - x2 * (1.0f / 6.0f - x2 * (1.0f / 120.0f - x2 / 5040.0f)));
    return sign * result;
}

// --- cosf：cos(x) = sin(PI/2 - x) ---
float cosf(float f) {
    return sinf(HALF_PI - f);
}

// --- tanf：tan(x) = sin(x) / cos(x) ---
float tanf(float f) {
    float s = sinf(f);
    float c = cosf(f);
    if (c == 0.0f) return INF;
    return s / c;
}

// --- cotf：cot(x) = 1/tan(x) ---
float cotf(float f) {
    float t = tanf(f);
    if (t == 0.0f) return INF;
    return 1.0f / t;
}

// --- cscf：csc(x) = 1/sin(x) ---
float cscf(float f) {
    float s = sinf(f);
    if (s == 0.0f) return INF;
    return 1.0f / s;
}

// --- secf：sec(x) = 1/cos(x) ---
float secf(float f) {
    float c = cosf(f);
    if (c == 0.0f) return INF;
    return 1.0f / c;
}

// --- 辅助：求自然对数，使用浮点数分解 + 多项式逼近 ---
float logf(float f) {
    if (f <= 0.0f) return NAN;
    if (isinf(f)) return INF;
    if (f == 1.0f) return 0.0f;

    // 将 f 分解为 m * 2^e，其中 m ∈ [1, 2)
    uint32_t bits = *(uint32_t*)&f;
    int e = ((bits >> 23) & 0xFF) - 127;  // 指数部分
    uint32_t mant_bits = (bits & 0x7FFFFF) | 0x3F800000;  // 设置指数为 0（即 2^0）
    float m = *(float*)&mant_bits;  // m ∈ [1, 2)

    // 令 x = (m - 1) / (m + 1)，则 ln(m) ≈ 2 * (x + x3/3 + x?/5 + ...)
    float x = (m - 1.0f) / (m + 1.0f);
    float x2 = x * x;
    float ln_m = 2.0f * x * (1.0f + x2 * (1.0f / 3.0f + x2 * (1.0f / 5.0f + x2 * (1.0f / 7.0f + x2 / 9.0f))));

    // ln(f) = ln(m) + e * ln(2)
    static const float LN2 = 0.69314718055994530942f;
    return ln_m + e * LN2;
}

// --- logf2：以任意底数的对数 ---
float logf2(float base, float mantissa) {
    if (base <= 0.0f || base == 1.0f || mantissa <= 0.0f) return NAN;
    return logf(mantissa) / logf(base);
}

// --- 双曲函数：使用指数定义 ---
// 辅助：exp(x)，使用泰勒展开
static float expf_custom(float x) {
    if (isinf(x)) return (x > 0) ? INF : 0.0f;
    // 将 x 分解为整数部分和小数部分
    float xi = (float)(int)x;
    float xf = x - xi;
    // e^xf 使用泰勒展开
    float result = 1.0f + xf * (1.0f + xf * (0.5f + xf * (1.0f / 6.0f + xf * (1.0f / 24.0f + xf / 120.0f))));
    // e^xi = 2^(xi / ln2)，用整数幂
    // 实际上 e^xi = (e)^xi，我们用 2^(xi/log2(e)) 不好，直接用累乘
    float e_pow_int = 1.0f;
    float e_val = 2.71828182845904523536f;
    if (xi >= 0) {
        for (int i = 0; i < (int)xi; ++i) e_pow_int *= e_val;
    } else {
        for (int i = 0; i < -(int)xi; ++i) e_pow_int /= e_val;
    }
    return result * e_pow_int;
}

float sinhf(float f) {
    float e_x = expf_custom(f);
    float e_nx = 1.0f / e_x;
    return (e_x - e_nx) * 0.5f;
}

float coshf(float f) {
    float e_x = expf_custom(f);
    float e_nx = 1.0f / e_x;
    return (e_x + e_nx) * 0.5f;
}

float tanhf(float f) {
    float sh = sinhf(f);
    float ch = coshf(f);
    if (ch == 0.0f) return (sh > 0) ? INF : -INF;
    return sh / ch;
}

float cothf(float f) {
    float th = tanhf(f);
    if (th == 0.0f) return INF;
    return 1.0f / th;
}

float cschf(float f) {
    float sh = sinhf(f);
    if (sh == 0.0f) return INF;
    return 1.0f / sh;
}

float sechf(float f) {
    float ch = coshf(f);
    if (ch == 0.0f) return INF;
    return 1.0f / ch;
}

// --- 反三角函数：使用牛顿法或级数展开 ---
// 辅助：sqrtf，使用牛顿迭代
static float sqrtf_custom(float x) {
    if (x <= 0.0f) return (x == 0.0f) ? 0.0f : NAN;
    if (isinf(x)) return INF;
    // 初始猜测：用浮点技巧
    uint32_t bits = *(uint32_t*)&x;
    bits = (bits >> 1) + 0x1FC00000;  // 近似 sqrt
    float guess = *(float*)&bits;
    // 牛顿迭代
    for (int i = 0; i < 3; ++i)
        guess = (guess + x / guess) * 0.5f;
    return guess;
}

float atanf(float f);
// asinf：asin(x) = arctan(x / sqrt(1 - x2))
float asinf(float f) {
    if (f < -1.0f || f > 1.0f) return NAN;
    if (f == 1.0f) return HALF_PI;
    if (f == -1.0f) return -HALF_PI;
    return atanf(f / sqrtf_custom(1.0f - f * f));
}

// acosf：acos(x) = PI/2 - asin(x)
float acosf(float f) {
    return HALF_PI - asinf(f);
}

// atanf：使用切比雪夫逼近
float atanf(float f) {
    if (isinf(f)) return (f > 0) ? HALF_PI : -HALF_PI;
    // 利用 atan(-x) = -atan(x)
    int sign = 1;
    if (f < 0) { f = -f; sign = -1; }
    // 对于大值，使用 atan(x) = PI/2 - atan(1/x)
    int complement = 0;
    if (f > 1.0f) { f = 1.0f / f; complement = 1; }
    // 切比雪夫逼近 atan(x) ≈ x * (1 - x2 * (1/3 - x2 * (1/5 - x2/7)))
    float x2 = f * f;
    float result = f * (1.0f - x2 * (1.0f / 3.0f - x2 * (1.0f / 5.0f - x2 / 7.0f)));
    if (complement) result = HALF_PI - result;
    return sign * result;
}

// acotf：acot(x) = PI/2 - atan(x)
float acotf(float f) {
    return HALF_PI - atanf(f);
}

// acscf：acsc(x) = asin(1/x)
float acscf(float f) {
    if (f == 0.0f) return NAN;
    return asinf(1.0f / f);
}

// asecf：asec(x) = acos(1/x)
float asecf(float f) {
    if (f == 0.0f) return NAN;
    return acosf(1.0f / f);
}

// --- powf：x^y = e^(y * ln(x)) ---
float powf(float base, float exponent) {
    if (base < 0 && exponent != (int)exponent) return NAN;  // 负数的小数次幂无定义
    if (base == 0.0f && exponent <= 0) return NAN;
    if (base == 0.0f) return 0.0f;
    if (exponent == 0.0f) return 1.0f;
    if (base < 0) {
        // 负数的整数次幂
        int e = (int)exponent;
        float pos_pow = expf_custom(exponent * logf(-base));
        return (e % 2 == 0) ? pos_pow : -pos_pow;
    }
    return expf_custom(exponent * logf(base));
}

// --- facf：阶乘（Gamma函数近似，只支持整数）---
float facf(float f) {
    if (f < 0.0f) return NAN;
    int n = (int)f;
    if (f != (float)n) return NAN;  // 非整数暂不支持
    float result = 1.0f;
    for (int i = 2; i <= n; ++i)
        result *= i;
    return result;
}

#ifndef __NCM_NO_PACKING
}
#endif
