#!/bin/bash

if test "$1" = "config" || test "$1" = ""; then
	export PKG_CONFIG_PATH=../x264/build/win32
	"./configure --toolchain=msvc --disable-static --enable-shared --enable-protocol=http \
	--enable-protocol=rtmp \
	--enable-gpl \
	--enable-libx264"
fi

if test "$1" = "make" || test "$1" = ""; then
	make
fi

if test "$1" = "install" || test "$1" = ""; then
	mkdir build  > /dev/null
	mkdir build/include > /dev/null
	mkdir build/win32 > /dev/null
	mkdir build/win32/share > /dev/null
	
	cp libavformat/*.lib ./build/win32/share -f
	cp libavdevice/*.lib ./build/win32/share -f
	cp libavcodec/*.lib ./build/win32/share -f
	cp libavutil/*.lib ./build/win32/share -f
	cp libswresample/*.lib ./build/win32/share -f
	cp libswscale/*.lib ./build/win32/share -f
	cp libavfilter/*.lib ./build/win32/share -f
	cp libpostproc/*.lib ./build/win32/share -f

	cp libavformat/avformat-*.dll ./build/win32/share -f
	cp libavdevice/avdevice-*.dll ./build/win32/share -f
	cp libavcodec/avcodec-*.dll ./build/win32/share -f
	cp libavutil/avutil-*.dll ./build/win32/share -f
	cp libswresample/swresample-*.dll ./build/win32/share -f
	cp libswscale/swscale-*.dll ./build/win32/share -f
	cp libavfilter/avfilter-*.dll ./build/win32/share -f
	cp libpostproc/*.dll ./build/win32/share -f
	
	cp libavformat/avformat-*.pdb ./build/win32/share -f
	cp libavdevice/avdevice-*.pdb ./build/win32/share -f
	cp libavcodec/avcodec-*.pdb ./build/win32/share -f
	cp libavutil/avutil-*.pdb ./build/win32/share -f
	cp libswresample/swresample-*.pdb ./build/win32/share -f
	cp libswscale/swscale-*.pdb ./build/win32/share -f
	cp libavfilter/avfilter-*.pdb ./build/win32/share -f
	cp libpostproc/*.pdb ./build/win32/share -f
	
	cp *.exe ./build/win32/share -f
	
	cp config.h ./build/win32 -f
	rm ./build/win32/ffconfig.h > /dev/null
	mv ./build/win32/config.h ./build/win32/ffconfig.h
	
	rm temp.tar -f > /dev/null
	find libavformat libavcodec libavdevice  libavutil libswresample libswscale libavfilter libpostproc -iname "*.h" -exec tar -rvf temp.tar {} \;
	tar xvf temp.tar -C build/include > /dev/null
	rm temp.tar -f > /dev/null
	

fi
