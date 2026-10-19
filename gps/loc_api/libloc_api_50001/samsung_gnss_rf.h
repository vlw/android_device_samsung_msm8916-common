// SPDX-License-Identifier: BSD-3-Clause
#pragma once
#include <cstddef>
#include <cstdint>

namespace samsung_gnss {
// SECGPS v1 message 9: one mandatory TLV 1 containing the RF selector.
// Preserve only the SM-A300FU stock default. This is not a gain/voltage API.
constexpr uint8_t kStockRfRequest[] = {0x01, 0x01, 0x00, 0x01};
int parseRfReply(const uint8_t* bytes, size_t length);
int discoverRfService();
int applyStockRfConfig();
}
