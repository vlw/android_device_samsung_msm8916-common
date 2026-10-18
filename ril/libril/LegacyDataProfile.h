/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdint.h>

// RIL <= 14 uses logical profile IDs, unlike the persistent modem-profile
// indices used by newer HALs. Android 12 sends INVALID (-1) for non-persistent
// APNs. Samsung requires logical profile 2 to request IMS PCO/P-CSCF information.
inline int legacyDataProfileId(int requestedId, uint32_t apnTypes) {
    constexpr uint32_t kIms = 0x40;
    constexpr uint32_t kDefault = 0x1;
    return (apnTypes & kIms) && !(apnTypes & kDefault) ? 2 : requestedId;
}
