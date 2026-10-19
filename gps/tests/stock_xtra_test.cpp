// SPDX-License-Identifier: Apache-2.0
// Isolated legacy HAL integration probe. Run with a matching stock library set.
#include <hardware/gps.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <ctime>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
struct Job { void (*start)(void*); void* arg; };
pthread_t thread(const char*, void (*start)(void*), void* arg) {
    auto* job = new Job{start, arg}; pthread_t id{};
    if (pthread_create(&id, nullptr, [](void* ptr)->void* {
        Job j = *static_cast<Job*>(ptr); delete static_cast<Job*>(ptr); j.start(j.arg); return nullptr;
    }, job)) { delete job; return {}; }
    pthread_detach(id); return id;
}
void location(GpsLocation*) { std::puts("Stock location callback (coordinates omitted)"); }
void status(GpsStatus* value) { std::printf("Stock status=%d\n", value->status); }
void satellites(GpsSvStatus*) {}
void nmea(GpsUtcTime, const char*, int) {}
void caps(uint32_t value) { std::printf("Stock capabilities=0x%x\n", value); }
void wake() {}
void utcRequest() { std::puts("Stock UTC requested"); }
void xtraRequest() { std::puts("Stock XTRA download requested"); }
int main(int argc, char** argv) {
    if (argc < 3 || argc > 5) return 2;
    std::vector<std::string> files;
    for (int i = 2; i < argc; ++i) {
        std::ifstream input(argv[i], std::ios::binary | std::ios::ate);
        if (!input || input.tellg() <= 0 || input.tellg() > 100000) return 1;
        input.seekg(0);
        files.emplace_back(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    }
    void* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) { std::fprintf(stderr, "%s\n", dlerror()); return 1; }
    auto get = reinterpret_cast<const GpsInterface*(*)()>(dlsym(library, "get_gps_interface"));
    if (!get) return 1;
    const GpsInterface* gps = get();
    if (!gps) return 1;
    GpsCallbacks callbacks{}; callbacks.size = sizeof(callbacks);
    callbacks.location_cb = location; callbacks.status_cb = status; callbacks.sv_status_cb = satellites;
    callbacks.nmea_cb = nmea; callbacks.set_capabilities_cb = caps;
    callbacks.acquire_wakelock_cb = callbacks.release_wakelock_cb = wake;
    callbacks.create_thread_cb = thread; callbacks.request_utc_time_cb = utcRequest;
    int rc = gps->init(&callbacks); std::printf("Stock init=%d\n", rc); if (rc) return 1;
    const auto* xtra = static_cast<const GpsXtraInterface*>(gps->get_extension(GPS_XTRA_INTERFACE));
    if (!xtra) { gps->cleanup(); return 1; }
    GpsXtraCallbacks xcb{}; xcb.download_request_cb = xtraRequest; xcb.create_thread_cb = thread;
    rc = xtra->init(&xcb); std::printf("Stock XTRA init=%d\n", rc);
    timespec now{}, ref{}; clock_gettime(CLOCK_REALTIME, &now); clock_gettime(CLOCK_BOOTTIME, &ref);
    gps->inject_time(int64_t(now.tv_sec)*1000 + now.tv_nsec/1000000,
                     int64_t(ref.tv_sec)*1000 + ref.tv_nsec/1000000, 1000);
    gps->set_position_mode(GPS_POSITION_MODE_STANDALONE, GPS_POSITION_RECURRENCE_PERIODIC, 1000, 0, 0);
    std::printf("Stock start=%d\n", gps->start());
    sleep(10);
    for (auto& data : files) {
        clock_gettime(CLOCK_REALTIME, &now); clock_gettime(CLOCK_BOOTTIME, &ref);
        std::printf("Stock active-engine UTC injection=%d\n", gps->inject_time(
                int64_t(now.tv_sec)*1000 + now.tv_nsec/1000000,
                int64_t(ref.tv_sec)*1000 + ref.tv_nsec/1000000, 1000));
        sleep(1);
        std::printf("Stock inject bytes=%zu return=%d\n", data.size(), xtra->inject_xtra_data(&data[0], data.size()));
        sleep(3);
    }
    gps->stop(); gps->cleanup(); std::puts("Stock probe finished");
    std::fflush(stdout); _exit(0);
}
