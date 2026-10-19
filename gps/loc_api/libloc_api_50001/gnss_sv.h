// SPDX-License-Identifier: BSD-3-Clause
#pragma once
#include <gps_extended.h>

// Keep the proprietary HaxxSvStatus ABI unchanged at the loc_api boundary.
void loc_convert_sv_status(const HaxxSvStatus& source, GnssSvStatus& target);
void loc_convert_legacy_sv_status(const HaxxSvStatus& source, GpsSvStatus& target);
