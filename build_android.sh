export TMPDIR=G:/tmp
NDK=C:/android-ndk-r9d
PLATFORM=$NDK/platforms/android-9/arch-arm/
PREBUILT=$NDK/toolchains/arm-linux-androideabi-4.8/prebuilt/windows
CPU=armeabi
OPTIMIZE_CFLAGS="-mfloat-abi=softfp -marm -mcpu=cortex-a8"
PREFIX=./android/$CPU
cpu=cortex-a8
./configure --target-os=linux \
    --prefix=$PREFIX \
    --enable-cross-compile \
    --arch=arm \
    --enable-nonfree \
    --enable-asm \
    --cc=$PREBUILT/bin/arm-linux-androideabi-gcc \
    --cross-prefix=$PREBUILT/bin/arm-linux-androideabi- \
    --nm=$PREBUILT/bin/arm-linux-androideabi-nm \
    --sysroot=$PLATFORM \
    --extra-cflags=" -O3 -fpic -DANDROID -DHAVE_SYS_UIO_H=1 $OPTIMIZE_CFLAGS " \
    --disable-shared \
    --enable-static \
    --extra-ldflags="-Wl,-rpath-link=$PLATFORM/usr/lib -L$PLATFORM/usr/lib  -nostdlib -lc -lm -ldl -llog" \
    --disable-ffmpeg \
    --disable-ffplay \
    --disable-ffprobe \
    --disable-ffserver \
    --enable-swscale \
    --enable-swresample \
    --enable-avformat \
    --enable-avcodec \
	--enable-protocol=http \
	--enable-protocol=rtmp \
    --disable-optimizations \
    --disable-debug \
    --disable-doc \
    --disable-stripping \
    --enable-yasm

make clean
make  -j4 install

#$PREBUILT/bin/arm-linux-androideabi-ar d libavcodec/libavcodec.a inverse.o

#$PREBUILT/bin/arm-linux-androideabi-ld -rpath-link=$PLATFORM/usr/lib -L$PLATFORM/usr/lib  -soname libffmpeg.so -shared -nostdlib  -z noexecstack -Bsymbolic --whole-archive --no-undefined -o $PREFIX/libffmpeg.so libavcodec/libavcodec.a libavformat/libavformat.a libavutil/libavutil.a libswscale/libswscale.a -lc -lm -lz -ldl -llog --dynamic-linker=/system/bin/linker $PREBUILT/lib/gcc/arm-linux-androideabi/4.8/libgcc.a