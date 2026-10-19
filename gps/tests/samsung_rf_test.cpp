// SPDX-License-Identifier: BSD-3-Clause
#include "samsung_gnss_rf.h"
#include <cerrno>
#include <cstdio>
#include <cstring>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(int argc, char** argv) {
    using namespace samsung_gnss;
    if (argc == 2 && !std::strcmp(argv[1], "--discover")) {
        int result = discoverRfService();
        std::printf("SECGPS v1 discovery (no requests sent): %d\n", result);
        return result ? 1 : 0;
    }
    if (argc == 2 && !std::strcmp(argv[1], "--apply-stock")) {
        int result = applyStockRfConfig();
        std::printf("Stock RF selector 1 / modem acknowledgement: %d\n", result);
        return result ? 1 : 0;
    }
    CHECK(argc == 1);
    const uint8_t accepted[] = {1, 1, 0, 0};
    const uint8_t failed[] = {1, 1, 0, 0xff};
    const uint8_t truncated[] = {1, 2, 0, 0};
    const uint8_t duplicate[] = {1, 1, 0, 0, 1, 1, 0, 0};
    const uint8_t unknown[] = {2, 1, 0, 0};
    const uint8_t malformed[] = {1, 1, 0, 0, 2};
    const uint8_t expectedRequest[] = {1, 1, 0, 1};
    CHECK(!std::memcmp(expectedRequest, kStockRfRequest, sizeof(expectedRequest)));
    CHECK(parseRfReply(accepted, sizeof(accepted)) == 0);
    CHECK(parseRfReply(failed, sizeof(failed)) == -EIO);
    CHECK(parseRfReply(truncated, sizeof(truncated)) == -EPROTO);
    CHECK(parseRfReply(duplicate, sizeof(duplicate)) == -EPROTO);
    CHECK(parseRfReply(unknown, sizeof(unknown)) == -EPROTO);
    CHECK(parseRfReply(malformed, sizeof(malformed)) == -EPROTO);
    CHECK(parseRfReply(nullptr, 0) == -EPROTO);
    CHECK(parseRfReply(accepted, 0) == -EPROTO);
    std::puts("PASS stock request TLV and modem success/error/truncation/duplicate/missing-response handling; no RF requests sent");
}
