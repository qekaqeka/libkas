#pragma once

#ifdef __GNUC__
#define chk_add(A, B, O) __builtin_add_overflow((A), (B), (O))
#define chk_sub(A, B, O) __builtin_sub_overflow((A), (B), (O))
#define chk_mul(A, B, O) __builtin_mul_overflow((A), (B), (O))
#else
#error "Not implemented"
#endif
