LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libloc_eng
LOCAL_PROPRIETARY_MODULE := true

LOCAL_MODULE_TAGS := optional

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libcutils \
    libdl \
    liblog \
    libloc_core \
    libgps.utils \
    libprocessgroup

LOCAL_SRC_FILES += \
    loc_eng.cpp \
    loc_eng_agps.cpp \
    loc_eng_xtra.cpp \
    loc_eng_ni.cpp \
    loc_eng_log.cpp \
    loc_eng_nmea.cpp \
    LocEngAdapter.cpp

# Samsung's runtime RF selector must also be restored after modem SSR.
ifeq ($(TARGET_DEVICE),a3lte)
LOCAL_SRC_FILES += samsung_gnss_rf.cpp
LOCAL_CFLAGS += -DSAMSUNG_GNSS_RF_CONFIG
endif

LOCAL_SRC_FILES += \
    loc_eng_dmn_conn.cpp \
    loc_eng_dmn_conn_handler.cpp \
    loc_eng_dmn_conn_thread_helper.c \
    loc_eng_dmn_conn_glue_msg.c \
    loc_eng_dmn_conn_glue_pipe.c

LOCAL_CFLAGS += \
     -fno-short-enums \
     -D_ANDROID_ \
     -Wno-unused-parameter

LOCAL_C_INCLUDES:= \
    $(TARGET_OUT_HEADERS)/gps.utils \
    $(TARGET_OUT_HEADERS)/libloc_core \
    $(LOCAL_PATH) \
    $(TARGET_OUT_HEADERS)/libflp

LOCAL_HEADER_LIBRARIES := libgps.utils_headers libloc_core_headers

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libloc_eng_headers
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)
include $(BUILD_HEADER_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := gps.$(TARGET_BOARD_PLATFORM)
LOCAL_PROPRIETARY_MODULE := true

LOCAL_MODULE_TAGS := optional

## Libs
LOCAL_SHARED_LIBRARIES := \
    libutils \
    libcutils \
    liblog \
    libloc_eng \
    libloc_core \
    libgps.utils \
    libdl \
    libprocessgroup

LOCAL_SRC_FILES += \
    loc.cpp \
    gnss_sv.cpp \
    gps.c

LOCAL_CFLAGS += \
    -fno-short-enums \
     -D_ANDROID_ \
     -Wno-unused-parameter

## Includes
LOCAL_C_INCLUDES:= \
    $(TARGET_OUT_HEADERS)/gps.utils \
    $(TARGET_OUT_HEADERS)/libloc_core \
    $(TARGET_OUT_HEADERS)/libflp

LOCAL_MODULE_RELATIVE_PATH := hw

LOCAL_HEADER_LIBRARIES := libgps.utils_headers libloc_core_headers

include $(BUILD_SHARED_LIBRARY)

# Optional regression probe; never added to PRODUCT_PACKAGES.
include $(CLEAR_VARS)
LOCAL_MODULE := a3_gnss_nmea_test
LOCAL_UNINSTALLABLE_MODULE := true
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := ../../tests/nmea_boundary_test.cpp
LOCAL_CFLAGS := -fno-short-enums -D_ANDROID_ -Wall -Wextra -Werror -Wno-unused-parameter -Wno-mismatched-tags
LOCAL_C_INCLUDES := $(LOCAL_PATH) $(TARGET_OUT_HEADERS)/libflp
LOCAL_HEADER_LIBRARIES := libhardware_headers libgps.utils_headers libloc_core_headers
LOCAL_SHARED_LIBRARIES := libloc_eng libloc_core libgps.utils libhidlbase liblog libutils
include $(BUILD_EXECUTABLE)
