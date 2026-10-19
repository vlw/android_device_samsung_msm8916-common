// SPDX-License-Identifier: BSD-3-Clause
#include "samsung_gnss_rf.h"
#include <cerrno>
#include <cstring>
#include <dlfcn.h>
#include <pthread.h>
#include <log/log.h>

namespace samsung_gnss {
namespace {
// Discovery metadata from the stock SECGPS IDL object. No IDL codec tables
// are needed: requests/replies use CCI's raw TLV API, not a synthetic codec.
struct ServiceObject {
    uint32_t libraryVersion, idlVersion, serviceId, maxMessageLength;
    uint16_t counts[3];
    const void* messageTables[3];
    const void* typeTable;
    uint32_t minorVersion;
    const void* parent;
};
const ServiceObject service = {2, 1, 0xe8, 0x210c, {}, {}, nullptr, 0, nullptr};
struct OsParams {
    unsigned int signalled, timedOut;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
};
using Init = int (*)(const void*, unsigned int, void*, void*, OsParams*, unsigned int, void**);
using Send = int (*)(void*, unsigned int, const void*, unsigned int, void*, unsigned int,
                    unsigned int*, unsigned int);
using Release = int (*)(void*);
using GetField = int (*)(const void*, uint32_t*);

int transact(unsigned int message) {
    static_assert(sizeof(ServiceObject) == 48, "Stock QMI service metadata is ARM32");
    static_assert(sizeof(OsParams) == 16, "CCI OS signal ABI is ARM32 bionic");
    void* library = dlopen("libqmi_cci.so", RTLD_NOW | RTLD_LOCAL);
    if (!library) return -ENOSYS;
    auto init = reinterpret_cast<Init>(dlsym(library, "qmi_client_init_instance"));
    auto send = reinterpret_cast<Send>(dlsym(library, "qmi_client_send_raw_msg_sync"));
    auto release = reinterpret_cast<Release>(dlsym(library, "qmi_client_release"));
    auto getId = reinterpret_cast<GetField>(dlsym(library, "qmi_idl_get_service_id"));
    auto getVersion = reinterpret_cast<GetField>(dlsym(library, "qmi_idl_get_idl_version"));
    uint32_t id = 0, version = 0;
    int result = -EPROTO;
    void* client = nullptr;
    OsParams signal{};
    if (init && send && release && getId && getVersion &&
            !getId(&service, &id) && !getVersion(&service, &version) && id == 0xe8 && version == 1) {
        // Bounded discovery, including when this modem does not have SECGPS.
        int rc = init(&service, 0xffff, nullptr, nullptr, &signal, 1000, &client);
        if (rc || !client) {
            ALOGW("Samsung GNSS: SECGPS discovery failed: %d", rc);
            result = -ENODEV;
        } else if (!message) {
            result = 0;
        } else {
            uint8_t reply[64]{};
            unsigned int received = 0;
            rc = send(client, message, kStockRfRequest, sizeof(kStockRfRequest), reply, sizeof(reply),
                      &received, 1000);
            result = rc ? -EIO : received <= sizeof(reply) ? parseRfReply(reply, received) : -EPROTO;
            ALOGI("Samsung GNSS: %s: transport=%d result=%d",
                  message == 8 ? "stock XTRA enable" : "stock RF selector 1", rc, result);
        }
    }
    if (client && release) release(client);
    dlclose(library);
    return result;
}
}

int parseRfReply(const uint8_t* bytes, size_t length) {
    if (!bytes) return -EPROTO;
    bool seen = false;
    int result = -EPROTO;
    for (size_t offset = 0; offset < length;) {
        if (length - offset < 3) return -EPROTO;
        uint8_t type = bytes[offset];
        size_t size = bytes[offset + 1] | (size_t{bytes[offset + 2]} << 8);
        offset += 3;
        if (size > length - offset) return -EPROTO;
        if (type == 1) {
            if (seen || size != 1) return -EPROTO;
            seen = true;
            // Modem handler returns 0 after accepted PDSM parameters, 0xff on error.
            result = bytes[offset] == 0 ? 0 : -EIO;
        }
        offset += size;
    }
    return seen ? result : -EPROTO;
}

int discoverRfService() { return transact(0); }
int applyStockRfConfig() { return transact(9); }
int enableStockXtra() { return transact(8); }
}
