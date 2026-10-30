/**
 * @file channel_select.c
 * @brief Maximum over the valid channels, with an explicit "none valid" answer.
 *
 * How it works
 * ------------
 * The loop walks the channels once. A channel counts only if its validity flag
 * is set. The running maximum starts at 0, so when no channel is valid the
 * result is 0 and out_any_valid tells the caller that nothing could be read.
 * The caller, not this function, decides what that means: the detector goes to
 * FAULT (REQ-SAF-007). Keeping that decision out of this function keeps the
 * selection a pure computation.
 *
 * @req REQ-FUN-005
 */
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "app/channel_select.h"
#include "common/gg_types.h"

gg_status_t channel_select_max(const gg_conc_t *concs, const bool *valid, uint8_t count,
                               gg_conc_t *out_max, bool *out_any_valid)
{
    gg_status_t status = GG_OK;
    gg_conc_t best = 0;
    bool any = false;
    uint8_t i;

    if ((concs == NULL) || (valid == NULL) || (out_max == NULL) || (out_any_valid == NULL)) {
        status = GG_ERR_NULL;
    } else if (count == 0u) {
        status = GG_ERR_RANGE;
    } else {
        for (i = 0u; i < count; i++) {
            if (valid[i] && ((!any) || (concs[i] > best))) {
                best = concs[i];
                any = true;
            }
        }
        *out_max = best;
        *out_any_valid = any;
    }
    return status;
}
