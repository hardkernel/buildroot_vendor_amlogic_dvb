LOCAL_PATH := $(call my-dir)
AMLOGIC_LIBPLAYER :=y

include $(CLEAR_VARS)

LOCAL_MODULE    := libam_mw
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := am_db/am_db.c\
		   am_epg/am_epg.c\
		   am_rec/am_rec.c\
		   am_scan/am_scan.c\
		   am_sub2/am_sub.c am_sub2/dvb_sub.c \
		   am_tt2/am_tt.c \
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
		   am_si/libdvbsi/descriptors/dr_83.c\
		   am_si/libdvbsi/descriptors/dr_87.c\
		   am_si/libdvbsi/descriptors/dr_88.c\
		   am_si/libdvbsi/descriptors/dr_5d.c\
		   am_si/libdvbsi/descriptors/dr_6a.c\
		   am_si/libdvbsi/descriptors/dr_7a.c\
		   am_si/libdvbsi/descriptors/dr_7f.c\
		   am_si/libdvbsi/psi.c\
		   am_si/libdvbsi/dvbpsi.c\
		   am_si/libdvbsi/descriptor.c\
		   am_si/atsc/atsc_vct.c\
		   am_si/atsc/atsc_mgt.c\
		   am_si/atsc/atsc_rrt.c\
		   am_si/atsc/atsc_stt.c\
		   am_si/atsc/atsc_eit.c\
		   am_si/atsc/atsc_ett.c\
		   am_si/atsc/atsc_descriptor.c\
		   am_si/atsc/huffman_decode.c\
		   am_fend_ctrl/am_sec.c\
		   am_fend_ctrl/am_fend_ctrl.c\
		   am_caman/am_caman.c \
		   am_caman/ca_dummy.c \
		   am_ci/libdvben50221/asn_1.c \
           am_ci/libdvben50221/en50221_app_ai.c        \
           am_ci/libdvben50221/en50221_app_auth.c      \
           am_ci/libdvben50221/en50221_app_ca.c        \
           am_ci/libdvben50221/en50221_app_datetime.c  \
           am_ci/libdvben50221/en50221_app_dvb.c       \
           am_ci/libdvben50221/en50221_app_epg.c       \
           am_ci/libdvben50221/en50221_app_lowspeed.c  \
           am_ci/libdvben50221/en50221_app_mmi.c       \
           am_ci/libdvben50221/en50221_app_rm.c        \
           am_ci/libdvben50221/en50221_app_smartcard.c \
           am_ci/libdvben50221/en50221_app_teletext.c  \
           am_ci/libdvben50221/en50221_app_utils.c     \
           am_ci/libdvben50221/en50221_session.c       \
           am_ci/libdvben50221/en50221_stdcam.c        \
           am_ci/libdvben50221/en50221_stdcam_hlci.c   \
           am_ci/libdvben50221/en50221_stdcam_llci.c   \
           am_ci/libdvben50221/en50221_transport.c \
		   am_ci/libucsi/dvb/types.c \
		   am_ci/libdvbapi/dvbca.c \
		   am_ci/libucsi/mpeg/pmt_section.c \
		   am_ci/am_ci.c \
		   am_ci/ca_ci.c \
		   am_cc/am_cc.c \
		   am_upd/am_upd.c \
		   am_freesat/freesat.c\
                   am_closecaption/am_cc.c \
                   am_closecaption/am_cc_decoder.c \
                   am_closecaption/am_xds.c \
                   am_closecaption/am_cc_slice.c \
                   am_closecaption/am_vbi/linux_vbi/linux_vbi.c \
                   am_closecaption/am_vbi/am_vbi_api.c

LOCAL_CFLAGS+=-DANDROID -DAMLINUX -DFONT_FREETYPE -DCHIP_8226M -DLOG_LEVEL=1 #
ifeq ($(AMLOGIC_LIBPLAYER), y)
LOCAL_CFLAGS+=-DAMLOGIC_LIBPLAYER
endif

LOCAL_ARM_MODE := arm
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../include/am_adp\
		    $(LOCAL_PATH)/../include/am_mw\
		    $(LOCAL_PATH)/../include/am_mw/libdvbsi\
		    $(LOCAL_PATH)/../include/am_mw/libdvbsi/descriptors\
		    $(LOCAL_PATH)/../include/am_mw/libdvbsi/tables\
		    $(LOCAL_PATH)/../include/am_mw/atsc\
			$(LOCAL_PATH)/am_closecaption/am_vbi\
		    $(LOCAL_PATH)/../android/ndk/include\
		    packages/amlogic/LibPlayer/amadec/include\
		    packages/amlogic/LibPlayer/amcodec/include\
		    external/libzvbi/src\
		    external/sqlite/dist\
		    external/icu4c/common\
		    vendor/amlogic/frameworks/av/LibPlayer/amcodec/include\
			vendor/amlogic/frameworks/av/LibPlayer/dvbplayer/include\
			vendor/amlogic/frameworks/av/LibPlayer/amadec/include\
			external/icu/icu4c/source/common\
		    vendor/amlogic/external/libzvbi/src\
		    $(LOCAL_PATH)/am_ci

ifeq ($(AMLOGIC_LIBPLAYER), y)
LOCAL_C_INCLUDES+=packages/amlogic/LibPlayer/amffmpeg
LOCAL_C_INCLUDES+=packages/amlogic/LibPlayer/amplayer
endif

ifeq ($(strip $(BOARD_TV_USE_NEW_TVIN_PARAM)),true)
LOCAL_CFLAGS += -DCC_TV_USE_NEW_TVIN_PARAM=1
endif

ifeq ($(AMLOGIC_LIBPLAYER), y)
LOCAL_SHARED_LIBRARIES+=libicuuc libzvbi libam_adp libsqlite libamplayer liblog libdl libc libcutils
else
LOCAL_SHARED_LIBRARIES+=libicuuc libzvbi libam_adp libsqlite  liblog libdl libc libcutils
endif
LOCAL_PRELINK_MODULE := false

#LOCAL_32_BIT_ONLY := true

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE    := libam_mw
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := am_db/am_db.c\
		   am_epg/am_epg.c\
		   am_rec/am_rec.c\
		   am_scan/am_scan.c\
		   am_sub2/am_sub.c am_sub2/dvb_sub.c \
		   am_tt2/am_tt.c \
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
		   am_si/libdvbsi/descriptors/dr_83.c\
		   am_si/libdvbsi/descriptors/dr_87.c\
		   am_si/libdvbsi/descriptors/dr_88.c\
		   am_si/libdvbsi/descriptors/dr_5d.c\
		   am_si/libdvbsi/descriptors/dr_6a.c\
		   am_si/libdvbsi/descriptors/dr_7a.c\
		   am_si/libdvbsi/psi.c\
		   am_si/libdvbsi/dvbpsi.c\
		   am_si/libdvbsi/descriptor.c\
		   am_si/atsc/atsc_vct.c\
		   am_si/atsc/atsc_mgt.c\
		   am_si/atsc/atsc_rrt.c\
		   am_si/atsc/atsc_stt.c\
		   am_si/atsc/atsc_eit.c\
		   am_si/atsc/atsc_ett.c\
		   am_si/atsc/atsc_descriptor.c\
		   am_si/atsc/huffman_decode.c\
		   am_fend_ctrl/am_sec.c\
		   am_fend_ctrl/am_fend_ctrl.c\
		   am_caman/am_caman.c \
		   am_caman/ca_dummy.c \
		   am_ci/libdvben50221/asn_1.c \
           am_ci/libdvben50221/en50221_app_ai.c        \
           am_ci/libdvben50221/en50221_app_auth.c      \
           am_ci/libdvben50221/en50221_app_ca.c        \
           am_ci/libdvben50221/en50221_app_datetime.c  \
           am_ci/libdvben50221/en50221_app_dvb.c       \
           am_ci/libdvben50221/en50221_app_epg.c       \
           am_ci/libdvben50221/en50221_app_lowspeed.c  \
           am_ci/libdvben50221/en50221_app_mmi.c       \
           am_ci/libdvben50221/en50221_app_rm.c        \
           am_ci/libdvben50221/en50221_app_smartcard.c \
           am_ci/libdvben50221/en50221_app_teletext.c  \
           am_ci/libdvben50221/en50221_app_utils.c     \
           am_ci/libdvben50221/en50221_session.c       \
           am_ci/libdvben50221/en50221_stdcam.c        \
           am_ci/libdvben50221/en50221_stdcam_hlci.c   \
           am_ci/libdvben50221/en50221_stdcam_llci.c   \
           am_ci/libdvben50221/en50221_transport.c \
		   am_ci/libucsi/dvb/types.c \
		   am_ci/libdvbapi/dvbca.c \
		   am_ci/libucsi/mpeg/pmt_section.c \
		   am_ci/am_ci.c \
		   am_ci/ca_ci.c \
		   am_cc/am_cc.c \
		   am_upd/am_upd.c \
		   am_freesat/freesat.c\
                   am_closecaption/am_cc.c \
                   am_closecaption/am_cc_decoder.c \
                   am_closecaption/am_xds.c \
                   am_closecaption/am_cc_slice.c \
                   am_closecaption/am_vbi/linux_vbi/linux_vbi.c \
                   am_closecaption/am_vbi/am_vbi_api.c


LOCAL_CFLAGS+=-DANDROID -DAMLINUX -DFONT_FREETYPE -DCHIP_8226M -DLOG_LEVEL=1 #
ifeq ($(AMLOGIC_LIBPLAYER), y)
LOCAL_CFLAGS+=-DAMLOGIC_LIBPLAYER
endif

LOCAL_ARM_MODE := arm
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../include/am_adp\
		    $(LOCAL_PATH)/../include/am_mw\
		    $(LOCAL_PATH)/../include/am_mw/libdvbsi\
		    $(LOCAL_PATH)/../include/am_mw/libdvbsi/descriptors\
		    $(LOCAL_PATH)/../include/am_mw/libdvbsi/tables\
		    $(LOCAL_PATH)/../include/am_mw/atsc\
		    $(LOCAL_PATH)/am_scan/libsigdetect\
		    $(LOCAL_PATH)/../android/ndk/include\
		    packages/amlogic/LibPlayer/amadec/include\
		    packages/amlogic/LibPlayer/amcodec/include\
		    external/libzvbi/src\
		    external/sqlite/dist\
		    external/icu4c/common\
			$(LOCAL_PATH)/am_closecaption/am_vbi\
		    vendor/amlogic/frameworks/av/LibPlayer/amcodec/include\
			vendor/amlogic/frameworks/av/LibPlayer/dvbplayer/include\
			vendor/amlogic/frameworks/av/LibPlayer/amadec/include\
			external/icu/icu4c/source/common\
		    vendor/amlogic/external/libzvbi/src\
		    $(LOCAL_PATH)/am_ci

ifeq ($(AMLOGIC_LIBPLAYER), y)
LOCAL_C_INCLUDES+=packages/amlogic/LibPlayer/amffmpeg
LOCAL_C_INCLUDES+=packages/amlogic/LibPlayer/amplayer
endif


ifeq ($(AMLOGIC_LIBPLAYER), y)    
LOCAL_SHARED_LIBRARIES+=libicuuc libzvbi libam_adp libsqlite libamplayer liblog libdl libc libcutils 
else
LOCAL_SHARED_LIBRARIES+=libicuuc libzvbi libam_adp libsqlite  liblog libdl libc libcutils
endif
LOCAL_PRELINK_MODULE := false

#LOCAL_32_BIT_ONLY := true

include $(BUILD_STATIC_LIBRARY)



