// SPDX-License-Identifier: Apache-2.0
// Optional test adapter for the complete SM-A300F stock library set. Not a product HAL.
#include <hardware/gps.h>
#include <dlfcn.h>
#include <pthread.h>
#include <cstdlib>
#include <cstring>
#include <string>
namespace {
pthread_once_t once = PTHREAD_ONCE_INIT;
const GpsInterface* realGps;
GpsInterface wrapped{};
GpsCallbacks caller{}, forwarded{};
void nmea(GpsUtcTime timestamp, const char* data, int length) {
    if (!caller.nmea_cb || !data || length <= 0 || length > 4096) return;
    // The stock buffer is length-delimited; Android 12 HIDL also requires data[length]==0.
    const std::string terminated(data, static_cast<size_t>(length));
    caller.nmea_cb(timestamp, terminated.c_str(), length);
}
int init(GpsCallbacks* callbacks) {
    if (!callbacks) return -1;
    caller = {}; std::memcpy(&caller, callbacks, callbacks->size < sizeof(caller) ? callbacks->size : sizeof(caller));
    forwarded = caller; forwarded.nmea_cb = nmea;
    return realGps->init(&forwarded);
}
void load() {
    void* library = dlopen("/data/local/tmp/gnss-rf-compare/stock/lib/hw/gps.default.so", RTLD_NOW | RTLD_LOCAL);
    if (!library) return;
    auto get = reinterpret_cast<const GpsInterface*(*)()>(dlsym(library, "get_gps_interface"));
    if (!get || !(realGps=get()) || realGps->size < sizeof(GpsInterface)) { realGps=nullptr; return; }
    wrapped = *realGps; wrapped.init = init;
}
const GpsInterface* get(gps_device_t*) { pthread_once(&once, load); return realGps ? &wrapped : nullptr; }
int close(hw_device_t* device) { std::free(device); return 0; }
int open(const hw_module_t* module, const char*, hw_device_t** output) {
    auto* device=static_cast<gps_device_t*>(std::calloc(1,sizeof(gps_device_t)));
    if (!device) return -1;
    device->common.tag=HARDWARE_DEVICE_TAG;device->common.module=const_cast<hw_module_t*>(module);
    device->common.close=close;device->get_gps_interface=get;*output=&device->common;return 0;
}
hw_module_methods_t methods{open};
}
extern "C" {
hw_module_t HAL_MODULE_INFO_SYM = {
    HARDWARE_MODULE_TAG, 1, 0, GPS_HARDWARE_MODULE_ID,
    "SM-A300F stock GPS diagnostic adapter", "Samsung/device test adapter",
    &methods, nullptr, {0}
};
}
