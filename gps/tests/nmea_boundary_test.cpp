// SPDX-License-Identifier: Apache-2.0
#include <loc_eng_msg.h>
#include <hidl/HidlSupport.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <string>
namespace {
std::string expected;
bool delivered;
void capture(GpsUtcTime, const char* data, int length) {
    android::hardware::hidl_string value;
    value.setToExternal(data, length); // Reproduce the real Android 12 contract check.
    delivered = static_cast<size_t>(length) == expected.size() &&
                !std::memcmp(value.c_str(), expected.data(), expected.size());
}
bool check(loc_eng_data_s_type& owner, const char* data, int length) {
    expected.assign(data, length); delivered = false;
    LocEngReportNmea message(&owner, data, length);
    if (message.mNmea[length] != '\0') return false;
    message.proc();
    return delivered;
}
}
int main() {
    loc_eng_data_s_type owner{}; owner.nmea_cb = capture;
    const long page = sysconf(_SC_PAGESIZE);
    if (page <= 0) return 1;
    auto* mapping=static_cast<char*>(mmap(nullptr, page*2, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0));
    if (mapping == MAP_FAILED) return 1;
    if (mprotect(mapping+page, page, PROT_NONE)) { munmap(mapping,page*2); return 1; }
    char* boundary=mapping+page-3; std::memcpy(boundary,"ABC",3);
    // There is no readable source terminator. strlcpy would cross into PROT_NONE.
    bool ok=check(owner,boundary,3);
    const char embedded[]={'A','\0','B','C'};
    ok=ok && check(owner,embedded,sizeof(embedded));
    const char sentence[]="$GPGSA,A,1,,,,,,,,,,,,,,,*1E\r\n";
    ok=ok && check(owner,sentence,sizeof(sentence)-1);
    munmap(mapping,page*2);
    std::puts(ok ? "PASS NMEA guard-page source, embedded NUL, and actual HIDL boundary" : "FAIL NMEA boundary");
    return ok ? 0 : 1;
}
