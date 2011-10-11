BASE=.

include $(BASE)/rule/def.mk

SUBDIRS=doc script include am_adp am_mw am_app test


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

