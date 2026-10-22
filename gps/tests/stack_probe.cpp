// SPDX-License-Identifier: Apache-2.0
// Compare complete legacy GPS library sets without exchanging proprietary C++ objects.
#include <hardware/gps.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <string>
namespace {
std::atomic<unsigned> fixes{0}, sentences{0};
timespec begun{};
long elapsed() { timespec t{}; clock_gettime(CLOCK_MONOTONIC, &t); return (t.tv_sec-begun.tv_sec)*1000+(t.tv_nsec-begun.tv_nsec)/1000000; }
struct Job { void (*fn)(void*); void* arg; };
pthread_t thread(const char*, void (*fn)(void*), void* arg) {
    auto* job = new Job{fn, arg}; pthread_t id{};
    if (pthread_create(&id, nullptr, [](void* p)->void* { Job j=*static_cast<Job*>(p); delete static_cast<Job*>(p); j.fn(j.arg); return nullptr; }, job)) { delete job; return {}; }
    pthread_detach(id); return id;
}
void location(GpsLocation* l) { if (!(l->flags & GPS_LOCATION_HAS_LAT_LONG)) return; ++fixes; std::printf("FIX elapsedMs=%ld flags=%u accuracy=%.2f\n",elapsed(),l->flags,l->accuracy); }
void status(GpsStatus* s) { std::printf("STATUS elapsedMs=%ld status=%d\n",elapsed(),s->status); }
void sv(GpsSvStatus*) {} // The stock vendor extends this ABI; compare the modem's NMEA instead.
void nmea(GpsUtcTime, const char* data, int length) {
    ++sentences;
    if (length<6 || length>1024) return;
    // GSV contains satellite geometry and C/N0, with no device coordinates.
    if (!std::memcmp(data+3,"GSV",3)) {
        std::string line(data,length);
        while (!line.empty() && (line.back()=='\r'||line.back()=='\n')) line.pop_back();
        std::printf("GSV elapsedMs=%ld %s\n",elapsed(),line.c_str());
    } else if (!std::memcmp(data+3,"GSA",3)) {
        // GSA mode 1/2/3 reports solution type without coordinates.
        const char* end=data+length; const char* p=static_cast<const char*>(std::memchr(data,',',length));
        if(p && ++p<end) { p=static_cast<const char*>(std::memchr(p,',',end-p)); if(p && ++p<end) std::printf("GSA elapsedMs=%ld talker=%.3s mode=%c\n",elapsed(),data,p[0]); }
    }
}
void caps(uint32_t c) { std::printf("CAPABILITIES 0x%x\n",c); }
void wake() {}
void requestUtc() { std::puts("UTC requested"); }
void requestXtra() { std::puts("XTRA requested (no file injected by this RF probe)"); }
}
int main(int argc,char** argv) {
    if(argc<2||argc>4) return 2;
    const int seconds=argc>2?std::atoi(argv[2]):90;
    const int mode=argc>3?std::atoi(argv[3]):0;
    if(seconds<10||seconds>600||mode<0||mode>2) return 2;
    std::setvbuf(stdout,nullptr,_IOLBF,0); clock_gettime(CLOCK_MONOTONIC,&begun);
    void* lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if(!lib) {std::fprintf(stderr,"dlopen: %s\n",dlerror());return 1;}
    auto get=reinterpret_cast<const GpsInterface*(*)()>(dlsym(lib,"get_gps_interface"));
    if(!get) {std::fprintf(stderr,"No get_gps_interface\n");return 1;}
    const GpsInterface* gps=get(); if(!gps) return 1;
    GpsCallbacks cb{};cb.size=sizeof(cb);cb.location_cb=location;cb.status_cb=status;cb.sv_status_cb=sv;cb.nmea_cb=nmea;cb.set_capabilities_cb=caps;cb.acquire_wakelock_cb=cb.release_wakelock_cb=wake;cb.create_thread_cb=thread;cb.request_utc_time_cb=requestUtc;
    int rc=gps->init(&cb);std::printf("INIT %d interfaceSize=%zu\n",rc,gps->size);if(rc)return 1;
    std::ifstream maps("/proc/self/maps");std::string line;
    while(std::getline(maps,line))if(line.find("libloc_")!=std::string::npos||line.find("libgps.utils")!=std::string::npos||line.find("/hw/gps.")!=std::string::npos) std::printf("MAP %s\n",line.c_str());
    const auto* xtra=static_cast<const GpsXtraInterface*>(gps->get_extension(GPS_XTRA_INTERFACE));
    GpsXtraCallbacks xc{};xc.download_request_cb=requestXtra;xc.create_thread_cb=thread;
    if(xtra)std::printf("XTRA_INIT %d\n",xtra->init(&xc));
    rc=gps->set_position_mode(static_cast<GpsPositionMode>(mode),GPS_POSITION_RECURRENCE_PERIODIC,1000,0,0);
    std::printf("MODE %d return=%d\n",mode,rc);if(rc){gps->cleanup();return 1;}
    rc=gps->start();std::printf("START %d\n",rc);if(rc){gps->cleanup();return 1;}
    for(int i=0;i<seconds;++i){
        if(i==5){timespec utc{},ref{};clock_gettime(CLOCK_REALTIME,&utc);clock_gettime(CLOCK_BOOTTIME,&ref);std::printf("UTC_INJECT %d\n",gps->inject_time(int64_t(utc.tv_sec)*1000+utc.tv_nsec/1000000,int64_t(ref.tv_sec)*1000+ref.tv_nsec/1000000,1000));}
        sleep(1);if(i%15==14)std::printf("HEARTBEAT elapsedMs=%ld fixes=%u nmea=%u\n",elapsed(),fixes.load(),sentences.load());
    }
    std::printf("STOP %d\n",gps->stop());sleep(2);gps->cleanup();std::printf("END fixes=%u nmea=%u\n",fixes.load(),sentences.load());std::fflush(stdout);_exit(0);
}
