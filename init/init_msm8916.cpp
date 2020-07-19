/*
   Copyright (c) 2016, The CyanogenMod Project

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
   ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
   BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
   BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
   OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
   IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <android-base/file.h>
#include <android-base/strings.h>

#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <android-base/properties.h>

#include "vendor_init.h"
#include "property_service.h"

#define SIMSLOT_FILE "/proc/simslot_count"
#define SERIAL_NUMBER_FILE "/efs/FactoryApp/serial_no"
#define BT_ADDR_FILE "/efs/bluetooth/bt_addr"

#include "init_msm8916.h"

using android::base::GetProperty;
using android::base::ReadFileToString;
using android::base::Trim;
using android::init::property_set;

static void init_alarm_boot_properties()
{
    char const *boot_reason_file = "/proc/sys/kernel/boot_reason";
    std::string boot_reason;
    std::string tmp = GetProperty("ro.boot.alarmboot","");

    if (ReadFileToString(boot_reason_file, &boot_reason)) {
        /*
         * Setup ro.alarm_boot value to true when it is RTC triggered boot up
         * For existing PMIC chips, the following mapping applies
         * for the value of boot_reason:
         *
         * 0 -> unknown
         * 1 -> hard reset
         * 2 -> sudden momentary power loss (SMPL)
         * 3 -> real time clock (RTC)
         * 4 -> DC charger inserted
         * 5 -> USB charger insertd
         * 6 -> PON1 pin toggled (for secondary PMICs)
         * 7 -> CBLPWR_N pin toggled (for external power supply)
         * 8 -> KPDPWR_N pin toggled (power key pressed)
         */
        if (Trim(boot_reason) == "3" || tmp == "true")
            property_set("ro.alarm_boot", "true");
        else
            property_set("ro.alarm_boot", "false");
    }
}

// copied from build/tools/releasetools/ota_from_target_files.py
// but with "." at the end and empty entry
std::vector<std::string> ro_product_props_default_source_order = {
    "",
    "product.",
    "product_services.",
    "odm.",
    "vendor.",
    "system.",
};

void property_override(char const prop[], char const value[], bool add = true)
{
    auto pi = (prop_info *) __system_property_find(prop);

    if (pi != nullptr) {
        __system_property_update(pi, value, strlen(value));
    } else if (add) {
        __system_property_add(prop, strlen(prop), value, strlen(value));
    }
}

void set_cdma_properties(const char *operator_alpha, const char *operator_numeric, const char * network)
{
	/* Dynamic CDMA Properties */
	property_set("ro.cdma.home.operator.alpha", operator_alpha);
	property_set("ro.cdma.home.operator.numeric", operator_numeric);
	property_set("ro.telephony.default_network", network);

	/* Static CDMA Properties */
	property_set("ril.subscription.types", "NV,RUIM");
	property_set("ro.telephony.default_cdma_sub", "0");
	property_set("ro.telephony.get_imsi_from_sim", "true");
	property_set("ro.telephony.ril.config", "newDriverCallU,newDialCode");
	property_set("telephony.lteOnCdmaDevice", "1");
}

void set_dsds_properties()
{
	property_set("ro.multisim.simslotcount", "2");
	property_set("ro.telephony.ril.config", "simactivation");
	property_set("persist.radio.multisim.config", "dsds");
}

void set_gsm_properties()
{
	property_set("telephony.lteOnCdmaDevice", "0");
    property_set("telephony.lteOnGsmDevice", "0");
	property_set("ro.telephony.default_network", "9");
}

void set_lte_properties()
{
	property_set("persist.radio.lte_vrte_ltd", "1");
	property_set("telephony.lteOnCdmaDevice", "0");
	property_set("telephony.lteOnGsmDevice", "1");
	property_set("ro.telephony.default_network", "10");
}

void set_wifi_properties()
{
	property_set("ro.carrier", "wifi-only");
	property_set("ro.radio.noril", "1");
}

void set_ro_product_prop(char const prop[], char const value[])
{
    for (const auto &source : ro_product_props_default_source_order) {
        auto prop_name = "ro.product." + source + prop;
        property_override(prop_name.c_str(), value, false);
    }
}

void set_fingerprint()
{
	std::string fingerprint = GetProperty("ro.build.fingerprint", "");

	if ((strlen(fingerprint.c_str()) > 1) && (strlen(fingerprint.c_str()) <= PROP_VALUE_MAX))
		return;

	char new_fingerprint[PROP_VALUE_MAX+1];

	std::string build_id = GetProperty("ro.build.id","");
	std::string build_tags = GetProperty("ro.build.tags","");
	std::string build_type = GetProperty("ro.build.type","");
	std::string device = GetProperty("ro.product.device","");
	std::string incremental_version = GetProperty("ro.build.version.incremental","");
	std::string release_version = GetProperty("ro.build.version.release","");

	snprintf(new_fingerprint, PROP_VALUE_MAX, "samsung/%s/%s:%s/%s/%s:%s/%s",
		device.c_str(), device.c_str(), release_version.c_str(), build_id.c_str(),
		incremental_version.c_str(), build_type.c_str(), build_tags.c_str());

    set_ro_product_prop("fingerprint", new_fingerprint);
}

void set_target_properties(const char *device, const char *model)
{
    set_ro_product_prop("device", device);
    set_ro_product_prop("model", model);
    set_ro_product_prop("name", device);
    
    // Init a dummy BT MAC address, will be overwritten later
    property_set("ro.boot.btmacaddr", "00:00:00:00:00:00");
    init_alarm_boot_properties();

	property_set("ro.ril.telephony.mqanelements", "6");

	/* check and/or set fingerprint */
	set_fingerprint();

	/* check for multi-sim devices */
    std::string sim_count;
    if (ReadFileToString(SIMSLOT_FILE, &sim_count)) {
        if (Trim(sim_count) == "2")
            set_dsds_properties();
    }

	char const *serial_number_file = SERIAL_NUMBER_FILE;
	std::string serial_number;

	char const *bt_addr_file = BT_ADDR_FILE;
	std::string bt_address;

	if (ReadFileToString(serial_number_file, &serial_number)) {
		serial_number = Trim(serial_number);
		property_override("ro.serialno", serial_number.c_str());
	}

	if (ReadFileToString(bt_addr_file, &bt_address)) {
		bt_address = Trim(bt_address);
		property_override("persist.service.bdroid.bdaddr", bt_address.c_str());
        property_override("ro.boot.btmacaddr", bt_address.c_str());
	}

}
