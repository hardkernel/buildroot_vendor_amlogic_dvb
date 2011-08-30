BASE=.

include $(BASE)/rule/def.mk

SUBDIRS=doc script include am_adp am_mw am_app test

ifeq ($(TARGET),android)
  SUBDIRS+=android
endif

include $(BASE)/rule/rule.mk

PHONY+=doxygen
doxygen:
	doxygen
	#cd doc/doxygen/latex; sed -f $(ROOTDIR)/rule/latex_conv.sed refman.tex > new_refman.tex; mv new_refman.tex refman.tex; make

DATE:=$(shell date +%Y.%m.%d-%k.%M|sed s/\ //)

PHONY+=dist
DIST_NAME:=AmlogicSetTopBox-$(VERSION)-$(DATE)
DIST_DIRS:=android config doc Doxyfile include rule script am_adp am_mw am_app test Makefile
dist: distclean info
	mkdir -p disttmp/$(DIST_NAME)
	mv build/INFO disttmp/$(DIST_NAME)/
	cp $(DIST_DIRS) disttmp/$(DIST_NAME)/ -a
	cp rule/README.dist disttmp/$(DIST_NAME)/README
	cd disttmp; tar czvf $(DIST_NAME).tgz $(DIST_NAME); cp $(DIST_NAME).tgz ..
	rm -rf disttmp

PHONY+=api-src-dist
LINUX_DIST_NAME:=AmlogicSetTopBox-api-src-$(VERSION)-$(DATE)
LINUX_DIST_DIRS:=config doc Doxyfile include rule script am_adp am_mw am_app test
api-src-dist: distclean doxygen info
	mkdir -p disttmp/$(LINUX_DIST_NAME)
	mv build/INFO disttmp/$(LINUX_DIST_NAME)/
	cp $(LINUX_DIST_DIRS) disttmp/$(LINUX_DIST_NAME)/ -a
ifeq ($(TARGET),android)
	mkdir -p disttmp/$(LINUX_DIST_NAME)/android
	cp android/ndk android/ex_lib android/ex_include android/armelf.x android/armelf.xsc disttmp/$(LINUX_DIST_NAME)/android -a
endif
	sed 's/SUBDIRS\(.*\)android/SUBDIRS\1/' Makefile > disttmp/$(LINUX_DIST_NAME)/Makefile
	cp rule/README.api-src-dist disttmp/$(LINUX_DIST_NAME)/README
	cd disttmp; tar czvf $(LINUX_DIST_NAME).tgz $(LINUX_DIST_NAME); cp $(LINUX_DIST_NAME).tgz ..
	rm -rf disttmp

PHONY+=android-src-dist
ANDROID_DIST_NAME:=AmlogicSetTopBox-android-src-$(VERSION)-$(DATE)
ANDROID_DIST_DIRS:=config Makefile rule script android
android-src-dist: distclean info
	mkdir -p disttmp/$(ANDROID_DIST_NAME)
	mv build/INFO disttmp/$(ANDROID_DIST_NAME)/
	cp $(ANDROID_DIST_DIRS) disttmp/$(ANDROID_DIST_NAME)/ -a
	sed s/PREBUILDDIRS.*// android/am_adp/jni/Makefile > disttmp/$(ANDROID_DIST_NAME)/android/am_adp/jni/Makefile
	sed s/PREBUILDDIRS.*// android/am_mw/jni/Makefile > disttmp/$(ANDROID_DIST_NAME)/android/am_mw/jni/Makefile
	echo "APIDIR?=" >> disttmp/$(ANDROID_DIST_NAME)/config/env_android.mk
	sed 's/SUBDIRS\(.*\)android/SUBDIRS=android/' Makefile > disttmp/$(ANDROID_DIST_NAME)/Makefile
	cp rule/README.android-src-dist disttmp/$(ANDROID_DIST_NAME)/README
	cd disttmp; tar czvf $(ANDROID_DIST_NAME).tgz $(ANDROID_DIST_NAME); cp $(ANDROID_DIST_NAME).tgz ..
	rm -rf disttmp

PHONY+=api-bin-dist
SDK_NAME:=AmlogicSetTopBox-api-$(TARGET)-bin-$(VERSION)-$(DATE)
SDK_OBJS:=config include rule doc test
api-bin-dist: all doxygen info
	mkdir -p disttmp/$(SDK_NAME)/build/$(TARGET)/am_adp disttmp/$(SDK_NAME)/build/$(TARGET)/am_mw
	mv build/INFO disttmp/$(SDK_NAME)/
	cp $(SDK_OBJS) disttmp/$(SDK_NAME)/ -a
	cp build/$(TARGET)/am_adp/libam_adp.so disttmp/$(SDK_NAME)/build/$(TARGET)/am_adp
	cp build/$(TARGET)/am_mw/libam_mw.so disttmp/$(SDK_NAME)/build/$(TARGET)/am_mw
	cp rule/README.api-bin-dist disttmp/$(SDK_NAME)/README
	sed 's/SUBDIRS\(.*\)/SUBDIRS=test/' Makefile > disttmp/$(SDK_NAME)/Makefile
ifeq ($(TARGET),android)
	mkdir -p disttmp/$(SDK_NAME)/android
	cp android/ndk android/ex_lib android/ex_include android/armelf.x android/armelf.xsc disttmp/$(SDK_NAME)/android -a
endif
	cd disttmp; tar czvf $(SDK_NAME).tgz $(SDK_NAME); cp $(SDK_NAME).tgz ..
	rm -rf disttmp

PHONY+=apk-dist
APK_NAME:=AmlogicSetTopBox-apk-$(VERSION)-$(DATE)
apk-dist: all info
	mkdir -p disttmp/$(APK_NAME)
	mv build/INFO disttmp/$(APK_NAME)/
	cp $(ANDROID_TOPDIR)/out/target/product/$(TARGET_PRODUCT)/system/app/dvbdemotest.apk disttmp/$(APK_NAME)/;\
	cp rule/README.apk-dist disttmp/$(APK_NAME)/README
	cd disttmp; tar czvf $(APK_NAME).tgz $(APK_NAME); cp $(APK_NAME).tgz ..
	rm -rf disttmp

PHONY+=info
info:
	echo Amlogic Set Top Box > build/INFO
	echo Date: `date` >> build/INFO
	echo Builder: `git config --get user.name` \< `git config --get user.email` \> >> build/INFO
	echo Version: $(VERSION) >> build/INFO
	echo Target: $(TARGET) >> build/INFO
	IPADDR=`ifconfig eth0 | grep inet\ 地址| sed s/.*inet\ 地址:// | sed s/广播.*//`;\
	if [ x$IPADDR = x ]; then\
		IPADDR=`ifconfig eth0 | grep inet\ 地址| sed s/.*inet\ addr:// | sed s/Bcast.*//`;\
	fi;\
	echo Machine: $$IPADDR >> build/INFO
	echo Path: `pwd` >> build/INFO
	echo Branch: `cat .git/HEAD | sed s/ref:\ //` >> build/INFO
	echo Commit: `git show HEAD | head -1 | grep commit | sed s/commit\ //` >> build/INFO

