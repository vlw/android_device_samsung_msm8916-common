// SPDX-License-Identifier: Apache-2.0
// Offline comparison of an existing LOC IDL encoder with the LOC 0x35 wire schema.
#include <dlfcn.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <cstddef>
struct Request {
    uint32_t total;
    uint16_t parts, part;
    uint32_t length;
    uint8_t data[1024];
    uint8_t formatValid;
    uint32_t format;
};
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    static_assert(sizeof(Request) == 1044 && offsetof(Request, formatValid) == 1036,
                  "ARM32 LOC 0x35 request ABI");
    void* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) { std::fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
    void* cci = dlopen("libqmi_cci.so", RTLD_NOW | RTLD_LOCAL);
    using Encode = int (*)(const void*, int, uint16_t, const void*, uint32_t, void*, uint32_t, uint32_t*);
    auto encode = cci ? reinterpret_cast<Encode>(dlsym(cci, "qmi_idl_message_encode")) : nullptr;
    const void* service = dlsym(library, "loc_qmi_idl_service_object_v02");
    if (!encode || !service) { std::puts("Missing IDL symbol"); return 1; }
    for (unsigned n : {1u, 512u, 1023u, 1024u}) {
        Request request{};
        request.total = 26 * 1024 + n;
        request.parts = request.part = 27;
        request.length = n;
        request.formatValid = 1;
        for (unsigned i = 0; i < n; ++i) request.data[i] = i ^ (i >> 8);
        uint8_t output[1200]{}; uint32_t written = 0;
        int rc = encode(service, 0, 0x35, &request, sizeof(request), output, sizeof(output), &written);
        std::vector<uint8_t> expected;
        auto number = [&](unsigned type, uint64_t value, unsigned size) {
            expected.push_back(type); expected.push_back(size); expected.push_back(0);
            for (unsigned i = 0; i < size; ++i) expected.push_back(value >> (8 * i));
        };
        number(1, request.total, 4); number(2, request.parts, 2); number(3, request.part, 2);
        expected.push_back(4); expected.push_back((n + 2) & 255); expected.push_back((n + 2) >> 8);
        expected.push_back(n & 255); expected.push_back(n >> 8);
        expected.insert(expected.end(), request.data, request.data + n);
        number(0x10, 0, 4);
        if (rc || written != expected.size() || std::memcmp(output, expected.data(), written)) {
            std::printf("FAIL final-part=%u encoder=%d bytes=%u expected=%zu\n", n, rc, written, expected.size());
            for (unsigned i = 0; i < written; ++i) std::printf("%02x", output[i]);
            std::puts(""); return 1;
        }
    }
    std::puts("PASS LOC 0x35: stock IDL matches raw TLVs for final-part lengths 1/512/1023/1024");
    dlclose(cci); dlclose(library);
    return 0;
}
