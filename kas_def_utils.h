#pragma once

#include <stdint.h>
#include "kas_defs.h"

#define max(A, B) ((A) > (B) ? (A) : (B))
#define min(A, B) ((A) < (B) ? (A) : (B))
#define sizeof_el(X) sizeof(X[0])
#define countof(X) (sizeof(X) / sizeof_el(X))

/*!
 * @brief Calculates the ceiling of A / B
 *
 * @warning A and B must be of integer types
 * @warning Side-effects unfriendly macro
 */
#define div_ceil(A, B) ((A) / (B) + ((A) % (B) == 0 ? 0 : 1))

/*!
 * @brief Calculates the nearest number to A, which bigger or equal A and divides by B
 *
 * @warning A and B must be of integer types
 * @warning Side-effects unfriendly macro
 */
#define align_div_ceil(A, B) (div_ceil((A), (B)) * (B))

/*!
 * @brief Calculates the nearest number to A, which smaller or equal A and divides by B
 *
 * @warning A and B must be of integer types
 * @warning Side-effects unfriendly macro
 */
#define align_div_floor(A, B) (((A) / (B)) * (B))

/*!
 * @brief Checks if addr is under certain aligment
 */
#define is_aligned(ADDR, ALIG) ((intptr_t)(ADDR) % (ALIG) == 0)

/*!
 * @brief Offset the pointer
 *
 * @warning Side-effects unfriendly macro
 *
 * @return A offseted pointer
 */
#define offptr(P, O) (typeof(P))((char*)(P) + (O))

/*!
 * @brief Reads the kernel memory by kreader
 *
 * @param [in]  kreader An interface to read the kernel memory
 * @param [in]  kaddr   A kernel address
 * @param [in]  klen    How many bytes are needed to be read
 * @param [in]  ubuff   A buffer in the userspace memory, where the contents of kernel memory will be placed
 *
 * @return The number of read bytes or -1 on failure
 */
static inline ssize_t kreader_read(struct kreader *kreader, kaddr_t kaddr, size_t klen, void *ubuff) {
    return kreader->func(kreader->private_arg, kaddr, klen, ubuff);
}

/*!
 * @brief Read variable from the kernel memory
 *
 * @param [in]  kreader An interface to read kernel memory
 * @param [in]  kaddr   A kernel address
 * @param [out] var A variable in the userspace memory, where the content of the kernel memory will be placed.
 *
 * The size or the read is derived from the var parameter type
 *
 * @return True if all the memory for the var was read, otherwise false
 */
#define kreader_read_var(kreader, kaddr, var) ( kreader_read((kreader), kaddr, sizeof(*(var)), (var)) == sizeof(*var) )
