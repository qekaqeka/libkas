#pragma once

#include "kas_defs.h"
#include <stddef.h>
#include <stdbool.h>

/*!
 * @brief Type for the work with parsed kallsyms
 *
 * An opaque type for the work with parsed kallsyms
 * Use kas_table_destroy to free resources occupied by this structure
 */
struct kas_table;

/*!
 * @brief Kallsyms symbol information
 *
 * @warning name field is read-only 
 */
struct kas_symbol {
    char *name; //!< Symbol name
    kas_long_t offset; //!< Kallsyms relative base symbol offset
    char type;
};

struct kas_table *kas_table_parse(struct kreader *kreader, struct kas_info *addrs);

bool kas_table_get_symbol(struct kas_table *kast, const char *symbol, struct kas_symbol *out);

void kas_table_destroy(struct kas_table *kas_table);
