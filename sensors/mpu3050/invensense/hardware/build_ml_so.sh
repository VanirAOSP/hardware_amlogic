#!/bin/sh
# add by sz.wu.zhu 20111012
echo "------------start compile ml------------"
cd $ANDROID_BUILD_TOP/hardware/amlogic/sensors/mpu3050/invensense/hardware/mlsdk

MAKE_CMD="make \
    VERBOSE=0 \
    TARGET=android \
    CROSS=$ANDROID_BUILD_TOP/prebuilt/linux-x86/toolchain/arm-eabi-4.4.3/bin/arm-eabi- \
    ANDROID_ROOT=$ANDROID_BUILD_TOP \
    KERNEL_ROOT=$ANDROID_BUILD_TOP/kernel \
    PRODUCT=$TARGET_PRODUCT \
"
eval $MAKE_CMD -f Android-shared.mk clean
eval $MAKE_CMD -f Android-shared.mk

cd ..
cp mlsdk/mllite/mpl/android/libmllite.so ./
cp mlsdk/platform/linux/libmlplatform.so ./
cp mlsdk/mldmp/mpl/android/libmpl.so ./

echo "------------end compile ml------------"
