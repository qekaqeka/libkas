#pragma once

#include "kas_defs.h"
#include <stdbool.h>

/*!
 * @brief Search kallsyms in memory
 *
 * @param [in]  kreader Inteface to read kernel memory
 * @param [in]  kaddr_bottom    The address from which the search starts. Under some conditions kallsyms could be found under this address
 * @param [in]  kaddr_top   The address where the search stops
 * @param [out] info_out    The information about found kallsyms. They can be vaild only if the function returned true
 *
 * @warning info_out could be changed even if the function failed
 * @warning Even if the function returned true, the info_out could be invalid because of "false kallsyms"
 *
 * @return True if kallsyms were found, otherwise false. False positives are possible
 */
bool kas_search(struct kreader *kreader, kaddr_t kaddr_bottom, kaddr_t kaddr_top, struct kas_info *info_out);
