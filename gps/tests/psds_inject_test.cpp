// SPDX-License-Identifier: Apache-2.0
// Optional integration probe: inject one already downloaded binary via the real HAL.
#include <android/hardware/gnss/1.0/IGnss.h>
#include <fstream>
#include <iterator>
#include <cstdio>
#include <string>
#include <cstring>
#include <ctime>

int main(int argc, char** argv) {
    const bool timeAid = argc == 3 && !std::strcmp(argv[1], "--inject-time");
    if (argc != 2 && !timeAid) {
        std::fprintf(stderr, "Usage: a3_gnss_psds_test [--inject-time] downloaded-xtra-file\n");
        return 2;
    }
    std::ifstream input(argv[timeAid ? 2 : 1], std::ios::binary | std::ios::ate);
    if (!input || input.tellg() <= 0 || input.tellg() > 1000000) {
        std::fprintf(stderr, "Missing, empty or oversized assistance file\n");
        return 1;
    }
    input.seekg(0);
    std::string data((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (data.empty() || static_cast<unsigned char>(data[0]) != 1) {
        std::fprintf(stderr, "Not the expected stock XTRA3 binary envelope\n");
        return 1;
    }
    auto gnss = android::hardware::gnss::V1_0::IGnss::getService();
    if (!gnss) return 1;
    if (timeAid) {
        timespec utc{}, elapsed{};
        if (clock_gettime(CLOCK_REALTIME, &utc) || clock_gettime(CLOCK_BOOTTIME, &elapsed)) return 1;
        const int64_t now = int64_t(utc.tv_sec) * 1000 + utc.tv_nsec / 1000000;
        const int64_t reference = int64_t(elapsed.tv_sec) * 1000 + elapsed.tv_nsec / 1000000;
        auto timeResult = gnss->injectTime(now, reference, 1000);
        if (!timeResult.isOk() || !static_cast<bool>(timeResult)) return 1;
        std::printf("Time aiding queued: utcMs=%lld uncertaintyMs=1000\n", static_cast<long long>(now));
    }
    auto extension = gnss->getExtensionXtra();
    if (!extension.isOk()) return 1;
    android::sp<android::hardware::gnss::V1_0::IGnssXtra> xtra = extension;
    if (!xtra) return 1;
    android::hardware::hidl_string bytes;
    bytes.setToExternal(data.data(), data.size());
    auto result = xtra->injectXtraData(bytes);
    bool accepted = result.isOk() && static_cast<bool>(result);
    std::printf("HAL XTRA queue acceptance: bytes=%zu accepted=%d; check modem injection log\n",
                data.size(), accepted);
    return accepted ? 0 : 1;
}
