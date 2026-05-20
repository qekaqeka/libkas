#pragma once

#include "kas_defs.h"

#ifndef KALLSYMS_SYM_MAX_LEN
#define KALLSYMS_SYM_MAX_LEN 512
#endif

#ifndef KALLSYMS_LABELS_ALIGN
#define KALLSYMS_LABELS_ALIGN 8
#endif

#define KALLSYMS_TOKEN_TABLE_SIZE 256

#ifndef KAS_ALIGN_FILLER
#define KAS_ALIGN_FILLER ((kas_byte_t)0)
#endif
