#pragma once

#define bitmask_test(BM, F) ((BM) & (F))
#define bitmask_set(BM, F) ((void)(*(BM) |= (F)))
#define bitmask_clear(BM, F) ((void)(*(BM) &= ~(F)))
