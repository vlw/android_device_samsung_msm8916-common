// SPDX-License-Identifier: Apache-2.0
// LOC 0x37 read-only query; explicit --inject/--inject-xtra3 and --delete-aiding mutate assistance data.
// Wire definitions: libqmi data/qmi-service-loc.json (LOC 0x35, 0x37, 0xa7).
#include <dlfcn.h>
#include <pthread.h>
#include <cstdint>
#include <cstdio>
#include <cerrno>
#include <ctime>
#include <cstring>
#include <vector>
#include <fstream>
#include <iterator>

namespace {
struct OsParams { unsigned int signalled, timedOut; pthread_mutex_t mutex; pthread_cond_t condition; };
struct Reply {
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t condition = PTHREAD_COND_INITIALIZER;
    bool received = false, valid = false;
    unsigned expected = 0x37, part = 0;
    uint32_t status = UINT32_MAX;
    uint64_t utc = 0;
    uint16_t hours = 0;
};
uint64_t little(const uint8_t* p, size_t n) {
    uint64_t v = 0;
    for (size_t i = 0; i < n; ++i) v |= uint64_t(p[i]) << (8 * i);
    return v;
}
void indication(void*, unsigned int id, void* data, unsigned int length, void* cookie) {
    auto& r = *static_cast<Reply*>(cookie);
    auto* b = static_cast<const uint8_t*>(data);
    pthread_mutex_lock(&r.mutex);
    if (id != r.expected) { pthread_mutex_unlock(&r.mutex); return; }
    for (size_t i = 0; i + 3 <= length;) {
        unsigned type = b[i]; size_t n = little(b + i + 1, 2); i += 3;
        if (n > length - i) break;
        if (type == 1 && n == 4) r.status = little(b + i, 4);
        if ((id == 0x35 || id == 0xa7) && type == 0x10 && n == 2) r.part = little(b + i, 2);
        if (id == 0x37 && type == 0x10 && n == 10) {
            r.utc = little(b + i, 8); r.hours = little(b + i + 8, 2); r.valid = true;
        }
        i += n;
    }
    r.received = true;
    pthread_cond_signal(&r.condition);
    pthread_mutex_unlock(&r.mutex);
}
}
int main(int argc, char** argv) {
    const bool xtra3 = argc == 3 && !std::strcmp(argv[1], "--inject-xtra3");
    const bool inject = xtra3 || (argc == 3 && !std::strcmp(argv[1], "--inject"));
    const bool clear = argc == 2 && !std::strcmp(argv[1], "--delete-aiding");
    if (argc != 1 && !inject && !clear) return 2;
    static_assert(sizeof(OsParams) == 16, "ARM32 CCI ABI");
    void* loc = dlopen("libloc_api_v02.so", RTLD_NOW | RTLD_LOCAL);
    void* cci = dlopen("libqmi_cci.so", RTLD_NOW | RTLD_LOCAL);
    using Init = int (*)(const void*, unsigned, decltype(&indication), void*, OsParams*, unsigned, void**);
    using Send = int (*)(void*, unsigned, const void*, unsigned, void*, unsigned, unsigned*, unsigned);
    using Release = int (*)(void*);
    auto init = cci ? reinterpret_cast<Init>(dlsym(cci, "qmi_client_init_instance")) : nullptr;
    auto send = cci ? reinterpret_cast<Send>(dlsym(cci, "qmi_client_send_raw_msg_sync")) : nullptr;
    auto release = cci ? reinterpret_cast<Release>(dlsym(cci, "qmi_client_release")) : nullptr;
    const void* service = loc ? dlsym(loc, "loc_qmi_idl_service_object_v02") : nullptr;
    if (!init || !send || !release || !service) { std::puts("LOC/CCI symbols unavailable"); return 1; }
    Reply r;
    OsParams os{};
    void* client = nullptr;
    int rc = init(service, 0xffff, indication, &r, &os, 1000, &client);
    if (rc || !client) { std::printf("LOC discovery failed: %d\n", rc); return 1; }
    auto waitIndication = [&]() {
        timespec deadline{};
        clock_gettime(CLOCK_REALTIME, &deadline); deadline.tv_sec += 3;
        pthread_mutex_lock(&r.mutex);
        while (!r.received) {
            if (pthread_cond_timedwait(&r.condition, &r.mutex, &deadline) == ETIMEDOUT) break;
        }
        bool ok = r.received;
        pthread_mutex_unlock(&r.mutex);
        return ok;
    };
    if (clear) {
        // QMI_LOC_DELETE_ASSIST_DATA 0x44: mandatory TLV 1, uint8 deleteAllFlag.
        // Standard GNSS assistance only; not factory RF/NV calibration writes.
        pthread_mutex_lock(&r.mutex); r.expected = 0x44; pthread_mutex_unlock(&r.mutex);
        const uint8_t request[] = {1, 1, 0, 1};
        uint8_t response[64]{}; unsigned size = 0;
        rc = send(client, 0x44, request, sizeof(request), response, sizeof(response), &size, 1000);
        bool responseOk = false;
        for (size_t i = 0; i + 3 <= size && i + 3 <= sizeof(response);) {
            unsigned type = response[i]; size_t n = little(response + i + 1, 2); i += 3;
            if (n > size - i || n > sizeof(response) - i) break;
            if (type == 2 && n == 4) {
                unsigned result = little(response + i, 2), error = little(response + i + 2, 2);
                std::printf("Delete aiding response: result=%u error=%u%s\n", result, error,
                            result == 0 ? " (error field ignored on SUCCESS)" : "");
                responseOk = result == 0;
            }
            i += n;
        }
        bool received = !rc && responseOk && waitIndication();
        pthread_mutex_lock(&r.mutex);
        bool ok = received && r.status == 0;
        std::printf("Delete aiding: transport=%d response=%d indication=%d status=%u\n",
                    rc, responseOk, received, r.status);
        pthread_mutex_unlock(&r.mutex);
        release(client); dlclose(cci); dlclose(loc);
        return ok ? 0 : 1;
    }
    if (inject) {
        std::ifstream input(argv[2], std::ios::binary | std::ios::ate);
        if (!input || input.tellg() <= 0 || input.tellg() > 100000) { release(client); return 1; }
        input.seekg(0);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        pthread_mutex_lock(&r.mutex); r.expected = xtra3 ? 0xa7 : 0x35; pthread_mutex_unlock(&r.mutex);
        const unsigned parts = (bytes.size() + 1023) / 1024;
        for (unsigned part = 1, offset = 0; part <= parts; ++part) {
            const unsigned count = bytes.size() - offset < 1024 ? bytes.size() - offset : 1024;
            std::vector<uint8_t> request;
            auto number = [&](unsigned type, uint64_t value, unsigned size) {
                request.push_back(type); request.push_back(size); request.push_back(0);
                for (unsigned n = 0; n < size; ++n) request.push_back(value >> (8 * n));
            };
            number(1, bytes.size(), 4); number(2, parts, 2); number(3, part, 2);
            const unsigned tlvSize = count + 2;
            request.push_back(4); request.push_back(tlvSize); request.push_back(tlvSize >> 8);
            request.push_back(count); request.push_back(count >> 8);
            request.insert(request.end(), bytes.begin() + offset, bytes.begin() + offset + count);
            if (!xtra3) number(0x10, 0, 4);
            pthread_mutex_lock(&r.mutex); r.received = false; r.status = UINT32_MAX; r.part = 0;
            pthread_mutex_unlock(&r.mutex);
            uint8_t response[64]{}; unsigned size = 0;
            rc = send(client, r.expected, request.data(), request.size(), response, sizeof(response), &size, 1000);
            bool received = !rc && waitIndication();
            std::printf("Raw injection part=%u/%u transport=%d indication=%d status=%u ackPart=%u\n",
                        part, parts, rc, received, r.status, r.part);
            if (!received || r.status || r.part != part) { release(client); return 1; }
            offset += count;
        }
        pthread_mutex_lock(&r.mutex); r.expected = 0x37; r.received = false; r.status = UINT32_MAX;
        pthread_mutex_unlock(&r.mutex);
    }
    uint8_t reply[64]{}; unsigned length = 0;
    rc = send(client, 0x37, nullptr, 0, reply, sizeof(reply), &length, 1000);
    std::printf("Validity query transport=%d reply=", rc);
    for (unsigned i = 0; i < length && i < sizeof(reply); ++i) std::printf("%02x", reply[i]);
    std::puts("");
    timespec deadline{};
    clock_gettime(CLOCK_REALTIME, &deadline); deadline.tv_sec += 3;
    pthread_mutex_lock(&r.mutex);
    while (!r.received && !rc) {
        if (pthread_cond_timedwait(&r.condition, &r.mutex, &deadline) == ETIMEDOUT) break;
    }
    std::printf("Validity indication received=%d status=%u info=%d startUtcSeconds=%llu durationHours=%u\n",
                r.received, r.status, r.valid, static_cast<unsigned long long>(r.utc), r.hours);
    bool ok = !rc && r.received && r.status == 0 && r.valid;
    pthread_mutex_unlock(&r.mutex);
    release(client); dlclose(cci); dlclose(loc);
    return ok ? 0 : 1;
}
