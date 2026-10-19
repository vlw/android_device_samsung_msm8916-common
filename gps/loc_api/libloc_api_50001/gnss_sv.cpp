// SPDX-License-Identifier: BSD-3-Clause
#include "gnss_sv.h"
#include <algorithm>

void loc_convert_sv_status(const HaxxSvStatus& source, GnssSvStatus& target) {
    target = {};
    target.size = sizeof(target);
    target.num_svs = std::max(0, std::min(source.num_svs, std::min<int>(GPS_MAX_SVS, GNSS_MAX_SVS)));
    for (int i = 0; i < target.num_svs; ++i) {
        const auto& input = source.sv_list[i];
        auto& output = target.gnss_sv_list[i];
        output.size = sizeof(output);
        output.svid = input.prn;
        output.c_n0_dbhz = input.snr;
        output.elevation = input.elevation;
        output.azimuth = input.azimuth;
        if (input.prn >= 1 && input.prn <= 32) {
            output.constellation = GNSS_CONSTELLATION_GPS;
            const uint32_t bit = uint32_t{1} << (input.prn - 1);
            if (source.ephemeris_mask & bit) output.flags |= GNSS_SV_FLAGS_HAS_EPHEMERIS_DATA;
            if (source.almanac_mask & bit) output.flags |= GNSS_SV_FLAGS_HAS_ALMANAC_DATA;
            if (source.gps_used_in_fix_mask & bit) output.flags |= GNSS_SV_FLAGS_USED_IN_FIX;
        } else if (input.prn >= 65 && input.prn <= 88) {
            output.constellation = GNSS_CONSTELLATION_GLONASS;
            output.svid = input.prn - 64;
            if (source.glo_used_in_fix_mask & (uint32_t{1} << (output.svid - 1))) {
                output.flags |= GNSS_SV_FLAGS_USED_IN_FIX;
            }
        } else if (input.prn >= 201 && input.prn <= 237) {
            output.constellation = GNSS_CONSTELLATION_BEIDOU;
            output.svid = input.prn - 200;
            if (source.bds_used_in_fix_mask & (uint64_t{1} << (output.svid - 1))) {
                output.flags |= GNSS_SV_FLAGS_USED_IN_FIX;
            }
        } else if (input.prn >= 33 && input.prn <= 64) {
            output.constellation = GNSS_CONSTELLATION_SBAS;
            output.svid = input.prn + 87;
        } else if (input.prn >= 193 && input.prn <= 200) {
            output.constellation = GNSS_CONSTELLATION_QZSS;
        } else {
            output.constellation = GNSS_CONSTELLATION_UNKNOWN;
        }
        // Only GPS has ephemeris/almanac masks in this binary ABI.
    }
}

void loc_convert_legacy_sv_status(const HaxxSvStatus& source, GpsSvStatus& target) {
    target = {};
    target.size = sizeof(target);
    target.num_svs = std::max(0, std::min(source.num_svs, GPS_MAX_SVS));
    std::copy_n(source.sv_list, target.num_svs, target.sv_list);
    target.ephemeris_mask = source.ephemeris_mask;
    target.almanac_mask = source.almanac_mask;
    target.used_in_fix_mask = source.gps_used_in_fix_mask;
}
