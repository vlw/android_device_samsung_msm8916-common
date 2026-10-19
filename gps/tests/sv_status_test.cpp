// SPDX-License-Identifier: BSD-3-Clause
#include "gnss_sv.h"
#include <cstdio>
#include <cstring>
#include <climits>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main() {
    HaxxSvStatus input{};
    const int prns[] = {1, 32, 65, 88, 201, 235, 33, 64, 193, 200, 0, 238};
    input.num_svs = sizeof(prns) / sizeof(prns[0]);
    input.gps_used_in_fix_mask = 1u << 31;
    input.glo_used_in_fix_mask = (1u << 23) | 1;
    input.bds_used_in_fix_mask = (uint64_t{1} << 34) | 1;
    input.ephemeris_mask = 1;
    input.almanac_mask = 1u << 31;
    for (int i = 0; i < input.num_svs; ++i) {
        input.sv_list[i] = {sizeof(GpsSvInfo), prns[i], 12.25f + i, 23.5f + i, 130.75f + i};
    }
    GnssSvStatus output;
    loc_convert_sv_status(input, output);
    CHECK(output.size == sizeof(output) && output.num_svs == input.num_svs);
    for (int i = 0; i < input.num_svs; ++i) {
        CHECK(output.gnss_sv_list[i].size == sizeof(GnssSvInfo));
        CHECK(output.gnss_sv_list[i].c_n0_dbhz == input.sv_list[i].snr);
        CHECK(output.gnss_sv_list[i].elevation == input.sv_list[i].elevation);
        CHECK(output.gnss_sv_list[i].azimuth == input.sv_list[i].azimuth);
    }
    CHECK(output.gnss_sv_list[0].flags == GNSS_SV_FLAGS_HAS_EPHEMERIS_DATA);
    CHECK(output.gnss_sv_list[1].flags == (GNSS_SV_FLAGS_HAS_ALMANAC_DATA | GNSS_SV_FLAGS_USED_IN_FIX));
    for (int i = 2; i <= 5; ++i) CHECK(output.gnss_sv_list[i].flags == GNSS_SV_FLAGS_USED_IN_FIX);
    CHECK(output.gnss_sv_list[2].constellation == GNSS_CONSTELLATION_GLONASS);
    CHECK(output.gnss_sv_list[2].svid == 1 && output.gnss_sv_list[3].svid == 24);
    CHECK(output.gnss_sv_list[4].constellation == GNSS_CONSTELLATION_BEIDOU);
    CHECK(output.gnss_sv_list[4].svid == 1 && output.gnss_sv_list[5].svid == 35);
    CHECK(output.gnss_sv_list[6].constellation == GNSS_CONSTELLATION_SBAS);
    CHECK(output.gnss_sv_list[6].svid == 120 && output.gnss_sv_list[7].svid == 151);
    CHECK(output.gnss_sv_list[8].constellation == GNSS_CONSTELLATION_QZSS && output.gnss_sv_list[8].svid == 193);
    CHECK(output.gnss_sv_list[10].constellation == GNSS_CONSTELLATION_UNKNOWN);
    CHECK(output.gnss_sv_list[11].constellation == GNSS_CONSTELLATION_UNKNOWN);
    GpsSvStatus legacy;
    loc_convert_legacy_sv_status(input, legacy);
    CHECK(legacy.size == sizeof(legacy) && legacy.num_svs == input.num_svs);
    CHECK(legacy.used_in_fix_mask == input.gps_used_in_fix_mask);
    CHECK(std::memcmp(legacy.sv_list, input.sv_list, input.num_svs * sizeof(GpsSvInfo)) == 0);
    input.num_svs = INT_MAX;
    loc_convert_sv_status(input, output);
    CHECK(output.num_svs == GPS_MAX_SVS);
    input.num_svs = -1;
    loc_convert_sv_status(input, output);
    CHECK(output.num_svs == 0);
    std::puts("PASS constellation IDs, 32/64-bit used masks, GPS ephemeris/almanac, exact C/N0, legacy ABI and count bounds");
}
