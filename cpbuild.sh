cp libavformat/*.lib ./build/win32/share
cp libavdevice/*.lib ./build/win32/share
cp libavcodec/*.lib ./build/win32/share
cp libavutil/*.lib ./build/win32/share
cp libswresample/*.lib ./build/win32/share
cp libswscale/*.lib ./build/win32/share
cp libavfilter/*.lib ./build/win32/share
cp libpostproc/*.lib ./build/win32/share

cp libavformat/avformat-*.dll ./build/win32/share
cp libavdevice/avdevice-*.dll ./build/win32/share
cp libavcodec/avcodec-*.dll ./build/win32/share
cp libavutil/avutil-*.dll ./build/win32/share
cp libswresample/swresample-*.dll ./build/win32/share
cp libswscale/swscale-*.dll ./build/win32/share
cp libavfilter/avfilter-*.dll ./build/win32/share
cp libpostproc/*.dll ./build/win32/share

cp libavformat/avformat-*.pdb ./build/win32/share
cp libavdevice/avdevice-*.pdb ./build/win32/share
cp libavcodec/avcodec-*.pdb ./build/win32/share
cp libavutil/avutil-*.pdb ./build/win32/share
cp libswresample/swresample-*.pdb ./build/win32/share
cp libswscale/swscale-*.pdb ./build/win32/share
cp libavfilter/avfilter-*.pdb ./build/win32/share
cp libpostproc/*.pdb ./build/win32/share
 
cp libavformat/*.h ./build/include/libavformat -f -R
cp libavdevice/*.h ./build/include/libavdevice -f -R
cp libavcodec/*.h ./build/include/libavcodec -f -R
cp libavutil/*.h ./build/include/libavutil -f -R
cp libswresample/*.h ./build/include/libswresample -f -R
cp libswscale/*.h ./build/include/libswscale -f -R
cp libavfilter/*.h ./build/include/libavfilter -f -R
cp libavresample/*.h ./build/include/libavresample -f -R
cp libpostproc/*.h ./build/include/libpostproc -f -R
cp compat/*.h ./build/include/compat -f -R

cp config.h ./build/win32 -f -R
rm ./build/win32/ffconfig.h
mv ./build/win32/config.h ./build/win32/ffconfig.h
