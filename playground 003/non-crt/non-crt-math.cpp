#include <stdint.h>

#ifndef __NCM_NO_PACKING
namespace NCM {
#endif

const float NAN = 0.f / 0.f, INF = 1.f / 0.f;
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
float sinf(float f) {
	return 0.f;
}
float cosf(float f) {
	return 0.f;
}
float tanf(float f) {
	return 0.f;
}
float cotf(float f) {
	return 0.f;
}
float cscf(float f) {
	return 0.f;
}
float secf(float f) {
	return 0.f;
}
float logf(float f) {
	return 0.f;
}
float logf2(float base, float mantissa) {
	return 0.f;
}
float sinhf(float f) {
	return 0.f;
}
float coshf(float f) {
	return 0.f;
}
float tanhf(float f) {
	return 0.f;
}
float cothf(float f) {
	return 0.f;
}
float cschf(float f) {
	return 0.f;
}
float sechf(float f) {
	return 0.f;
}
float asinf(float f) {
	return 0.f;
}
float acosf(float f) {
	return 0.f;
}
float atanf(float f) {
	return 0.f;
}
float acotf(float f) {
	return 0.f;
}
float acscf(float f) {
	return 0.f;
}
float asecf(float f) {
	return 0.f;
}
float powf(float base, float exponent) {
	return 0.f;
}
float facf(float f) {
	return 0.f;
}

#ifndef __NCM_NO_PACKING
}
#endif
