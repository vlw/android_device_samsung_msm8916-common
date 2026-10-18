/* SPDX-License-Identifier: Apache-2.0 */
#include "../libril/LegacyDataProfile.h"
#include <cassert>
#include <cstdio>
int main() {
    assert(legacyDataProfileId(-1, 0x40) == 2); // Android 12 non-persistent IMS
    assert(legacyDataProfileId(0, 0x40) == 2);  // legacy APN database default
    assert(legacyDataProfileId(2, 0x40) == 2);
    assert(legacyDataProfileId(-1, 0x1) == -1); // Internet unchanged
    assert(legacyDataProfileId(3, 0x8) == 3);   // DUN unchanged
    assert(legacyDataProfileId(-1, 0x200) == -1); // emergency unchanged
    assert(legacyDataProfileId(-1, 0x41) == -1); // shared Internet+IMS unchanged
    assert(legacyDataProfileId(7, 0) == 7);
    std::puts("PASS legacy IMS profile translation and non-IMS isolation");
}
