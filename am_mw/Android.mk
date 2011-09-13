LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := libam_mw
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := am_db/am_db.c\
		   am_epg/am_epg.c\
		   am_font/am_font.c am_font/freetype.c\
		   am_rec/am_rec.c\
		   am_scan/am_scan.c\
		   am_subtitle/am_mw_subtitle.c am_subtitle/dvb_sub.c am_subtitle/sub_memory.c\
		   am_teletext/am_mw_teletext.c am_teletext/VTCommon.c am_teletext/VTDrawer.c am_teletext/VTTeletext.c\
		   am_teletext/VTCharacterSet.c am_teletext/VTDecoder.c am_teletext/VTMosaicGraphics.c am_teletext/VTTopText.c\
		   am_si/am_si.c\
		   am_si/libdvbsi/tables/bat.c\
		   am_si/libdvbsi/tables/sdt.c\
		   am_si/libdvbsi/tables/pat.c\
		   am_si/libdvbsi/tables/cat.c\
		   am_si/libdvbsi/tables/pmt.c\
		   am_si/libdvbsi/tables/tot.c\
		   am_si/libdvbsi/tables/eit.c\
		   am_si/libdvbsi/tables/nit.c\
		   am_si/libdvbsi/demux.c\
		   am_si/libdvbsi/descriptors/dr_0f.c\
		   am_si/libdvbsi/descriptors/dr_44.c\
		   am_si/libdvbsi/descriptors/dr_0a.c\
		   am_si/libdvbsi/descriptors/dr_47.c\
		   am_si/libdvbsi/descriptors/dr_03.c\
		   am_si/libdvbsi/descriptors/dr_5a.c\
		   am_si/libdvbsi/descriptors/dr_05.c\
		   am_si/libdvbsi/descriptors/dr_48.c\
		   am_si/libdvbsi/descriptors/dr_69.c\
		   am_si/libdvbsi/descriptors/dr_02.c\
		   am_si/libdvbsi/descriptors/dr_4d.c\
		   am_si/libdvbsi/descriptors/dr_58.c\
		   am_si/libdvbsi/descriptors/dr_56.c\
		   am_si/libdvbsi/descriptors/dr_4e.c\
		   am_si/libdvbsi/descriptors/dr_4a.c\
		   am_si/libdvbsi/descriptors/dr_45.c\
		   am_si/libdvbsi/descriptors/dr_41.c\
		   am_si/libdvbsi/descriptors/dr_43.c\
		   am_si/libdvbsi/descriptors/dr_0e.c\
		   am_si/libdvbsi/descriptors/dr_04.c\
		   am_si/libdvbsi/descriptors/dr_59.c\
		   am_si/libdvbsi/descriptors/dr_0c.c\
		   am_si/libdvbsi/descriptors/dr_54.c\
		   am_si/libdvbsi/descriptors/dr_09.c\
		   am_si/libdvbsi/descriptors/dr_52.c\
		   am_si/libdvbsi/descriptors/dr_40.c\
		   am_si/libdvbsi/descriptors/dr_55.c\
		   am_si/libdvbsi/descriptors/dr_08.c\
		   am_si/libdvbsi/descriptors/dr_0b.c\
		   am_si/libdvbsi/descriptors/dr_42.c\
		   am_si/libdvbsi/descriptors/dr_07.c\
		   am_si/libdvbsi/descriptors/dr_0d.c\
		   am_si/libdvbsi/descriptors/dr_06.c\
		   am_si/libdvbsi/psi.c\
		   am_si/libdvbsi/dvbpsi.c\
		   am_si/libdvbsi/descriptor.c\
		   am_si/atsc/atsc_cvct.c\
		   am_si/atsc/atsc_tvct.c\
		   am_si/atsc/atsc_mgt.c\
		   am_si/atsc/atsc_rrt.c\
		   am_si/atsc/atsc_stt.c\
		   am_si/atsc/atsc_descriptor.c\
		   am_si/atsc/huffman_decode.c



LOCAL_CFLAGS+=-DANDROID -DAMLINUX -DFONT_FREETYPE -DCHIP_8226M
LOCAL_ARM_MODE := arm
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../include/am_adp\
		    $(LOCAL_PATH)/../include/am_mw\
		    $(LOCAL_PATH)/../include/am_mw/libdvbsi\
		    $(LOCAL_PATH)/../include/am_mw/libdvbsi/descriptors\
		    $(LOCAL_PATH)/../include/am_mw/libdvbsi/tables\
		    $(LOCAL_PATH)/../include/am_mw/atsc\
		    $(LOCAL_PATH)/../android/ndk/include\
		    $(LOCAL_PATH)/../android/ex_include

LOCAL_STATIC_LIBRARIES += libfreetype libiconv
LOCAL_SHARED_LIBRARIES += libam_adp libsqlite libamplayer liblog libc

LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)

include $(LOCAL_PATH)/../android/ex_lib/Android.mk

