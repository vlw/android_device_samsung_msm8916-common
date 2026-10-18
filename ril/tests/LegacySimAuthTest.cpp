/* SPDX-License-Identifier: Apache-2.0 */
#include "../libril/LegacySimAuth.h"
#include <cassert>
#include <cstdio>
int main() {
    std::string aka(34, '\0'); aka[0] = aka[17] = 16;
    char encoded[64];
    assert(b64_ntop(reinterpret_cast<const unsigned char*>(aka.data()), aka.size(), encoded,
                    sizeof(encoded)) > 0);
    std::string result;
    assert(legacySimAuthChallenge(129, encoded, &result) && result.size() == 68);
    assert(result.substr(0, 2) == "10" && result.substr(34, 2) == "10");
    assert(!legacySimAuthChallenge(129, "bad", &result));
    assert(!legacySimAuthChallenge(129, "AAAAAAAAAAAAAAAAAAAAAA==", &result));
    assert(!legacySimAuthChallenge(127, encoded, &result));
    assert(legacySimAuthChallenge(128, "AAAAAAAAAAAAAAAAAAAAAA==", &result));
    assert(result == std::string(32, '0'));
    assert(legacySimAuthResponse("DB0401020304", &result) && result == "2wQBAgME");
    assert(legacySimAuthResponse("dc021234", &result) && result == "3AISNA==");
    assert(!legacySimAuthResponse("DB0", &result));
    assert(!legacySimAuthResponse("not-hex", &result));
    assert(!legacySimAuthResponse("", &result));
    std::puts("PASS legacy SIM/AKA wire encoding, malformed input and AUTS response");
}
