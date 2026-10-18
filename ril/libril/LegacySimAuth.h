/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <resolv.h>
#include <stdint.h>
#include <string>

inline bool legacySimAuthChallenge(int context, const std::string& base64, std::string* hex) {
    if ((context != 128 && context != 129) || base64.size() > 256) return false;
    unsigned char raw[128];
    int size = b64_pton(base64.c_str(), raw, sizeof(raw));
    if ((context == 128 && size != 16) ||
        (context == 129 && (size != 34 || raw[0] != 16 || raw[17] != 16))) return false;
    const char digits[] = "0123456789ABCDEF";
    hex->clear();
    for (int i = 0; i < size; ++i) {
        hex->push_back(digits[raw[i] >> 4]);
        hex->push_back(digits[raw[i] & 15]);
    }
    return true;
}

inline bool legacySimAuthResponse(const std::string& hex, std::string* base64) {
    if (hex.empty() || hex.size() > 256 || hex.size() % 2) return false;
    unsigned char raw[128];
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (size_t i = 0; i < hex.size(); i += 2) {
        int high = nibble(hex[i]), low = nibble(hex[i + 1]);
        if (high < 0 || low < 0) return false;
        raw[i / 2] = (high << 4) | low;
    }
    char encoded[176];
    int size = b64_ntop(raw, hex.size() / 2, encoded, sizeof(encoded));
    if (size < 0) return false;
    base64->assign(encoded, size);
    return true;
}
