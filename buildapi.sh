#!/bin/bash

mm || return 1
doxygen || return 1

TARGET=android
VERSION=1.0
DATE=`date +%Y.%m.%d-%k.%M|sed s/\ //`

PKG=AmlogicSetTopBox-api-$TARGET-bin-$VERSION-$DATE
TPATH=.tmp/$PKG

mkdir -p $TPATH
mkdir -p $TPATH/am_adp $TPATH/am_mw

echo Amlogic Set Top Box > $TPATH/INFO
echo Date: `date` >> $TPATH/INFO
echo Builder: `git config --get user.name` \< `git config --get user.email` \> >> $TPATH/INFO
echo Version: $VERSION >> $TPATH/INFO
echo Target: $TARGET >> $TPATH/INFO
IPADDR=`LANG=en_US ifconfig eth0 | grep inet\ addr| sed s/.*inet\ addr:// | sed s/Bcast.*//`
echo Machine: $IPADDR >> $TPATH/INFO
echo Path: `pwd` >> $TPATH/INFO
echo Branch: `cat .git/HEAD | sed s/ref:\ //` >> $TPATH/INFO
echo Commit: `git show HEAD | head -1 | grep commit | sed s/commit\ //` >> $TPATH/INFO

cp $ANDROID_PRODUCT_OUT/system/lib/libam_adp.so $TPATH || return 1
cp $ANDROID_PRODUCT_OUT/system/lib/libam_mw.so $TPATH || return 1
cp -a include doc test $TPATH || return 1
cp rule/no_src_Android.mk $TPATH/Android.mk

cd .tmp

tar czvf $PKG.tgz $PKG || return 1
cp $PKG.tgz ..

cd ..

rm -rf .tmp


