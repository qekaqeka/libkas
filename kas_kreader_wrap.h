#pragma once

#include <stdbool.h>
#include "kas_defs.h"

/*!
 * @brief Makes the kreader buffered
 *
 * @param[in, out]  kreader  A kreader which needs to be buffered. Unchaged if failed to enable bufferisation
 * @param[in]   buff    A buffer which will be used. Pass NULL to use automaticaly allocated buffer
 * @param[in]   capacity    The size of the passed buffer. Ignored if the the passed buffer is NULL
 *
 * @return True if the kreader made buffered. Otherwise false and the kreader is unchanged
 */
bool kas_kreader_buffered_wrap(struct kreader *kreader, void *buff, size_t capacity);

/*!
 * @brief Restore the original kreader from the buffered one
 *
 * @param[in, out]  kreader A buffered kreader
 */
void kas_kreader_buffered_unwrap(struct kreader *kreader);
