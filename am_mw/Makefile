BASE=..

MODULE_DEPEND=-L$(TARGET_BUILDDIR)/am_adp -lam_adp


include $(BASE)/rule/def.mk
ifeq ($(TARGET),android)
SUBDIRS=am_cfg am_si am_img am_db am_font am_scan am_epg am_rec am_sub am_tt
else
SUBDIRS=am_cfg am_si am_img am_db am_font am_scan am_epg am_gui am_rec am_sub am_tt
endif

LIB_TARGET=am_mw

ifeq ($(TARGET),android)
am_mw_OBJS=am_cfg/am_cfg am_si/am_si am_img/am_img  am_font/am_font am_scan/am_scan am_epg/am_epg  am_db/am_db am_rec/am_rec am_sub/am_sub am_tt/am_tt
else
am_mw_OBJS=am_cfg/am_cfg am_si/am_si am_img/am_img  am_font/am_font am_scan/am_scan am_epg/am_epg am_gui/am_gui am_db/am_db am_rec/am_rec am_sub/am_sub am_tt/am_tt
endif

include $(BASE)/rule/rule.mk
