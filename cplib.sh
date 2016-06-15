cp libavformat/*.lib ../LJSrcNew/cocos2d-x-2.2.2/external/ffmpeg/prebuilt/win32/share
cp libavdevice/*.lib ../LJSrcNew/cocos2d-x-2.2.2/external/ffmpeg/prebuilt/win32/share
cp libavcodec/*.lib ../LJSrcNew/cocos2d-x-2.2.2/external/ffmpeg/prebuilt/win32/share
cp libavutil/*.lib ../LJSrcNew/cocos2d-x-2.2.2/external/ffmpeg/prebuilt/win32/share
cp libswresample/*.lib ../LJSrcNew/cocos2d-x-2.2.2/external/ffmpeg/prebuilt/win32/share
cp libswscale/*.lib ../LJSrcNew/cocos2d-x-2.2.2/external/ffmpeg/prebuilt/win32/share
cp libavfilter/*.lib ../LJSrcNew/cocos2d-x-2.2.2/external/ffmpeg/prebuilt/win32/share

cp libavformat/avformat-*.dll ../LJSrcNew/cocos2d-x-2.2.2/external/ffmpeg/prebuilt/win32/share
cp libavdevice/avdevice-*.dll ../LJSrcNew/cocos2d-x-2.2.2/external/ffmpeg/prebuilt/win32/share
cp libavcodec/avcodec-*.dll ../LJSrcNew/cocos2d-x-2.2.2/external/ffmpeg/prebuilt/win32/share
cp libavutil/avutil-*.dll ../LJSrcNew/cocos2d-x-2.2.2/external/ffmpeg/prebuilt/win32/share
cp libswresample/swresample-*.dll ../LJSrcNew/cocos2d-x-2.2.2/external/ffmpeg/prebuilt/win32/share
cp libswscale/swscale-*.dll ../LJSrcNew/cocos2d-x-2.2.2/external/ffmpeg/prebuilt/win32/share
cp libavfilter/avfilter-*.dll ../LJSrcNew/cocos2d-x-2.2.2/external/ffmpeg/prebuilt/win32/share

cp libavformat/avformat-*.dll ../LJSrcNew/cocos2d-x-2.2.2/projects/ljshell/proj.win32/Debug.win32
cp libavdevice/avdevice-*.dll ../LJSrcNew/cocos2d-x-2.2.2/projects/ljshell/proj.win32/Debug.win32
cp libavcodec/avcodec-*.dll ../LJSrcNew/cocos2d-x-2.2.2/projects/ljshell/proj.win32/Debug.win32
cp libavutil/avutil-*.dll ../LJSrcNew/cocos2d-x-2.2.2/projects/ljshell/proj.win32/Debug.win32
cp libswresample/swresample-*.dll ../LJSrcNew/cocos2d-x-2.2.2/projects/ljshell/proj.win32/Debug.win32
cp libswscale/swscale-*.dll ../LJSrcNew/cocos2d-x-2.2.2/projects/ljshell/proj.win32/Debug.win32
cp libavfilter/avfilter-*.dll ../LJSrcNew/cocos2d-x-2.2.2/projects/ljshell/proj.win32/Debug.win32

cp libavformat/avformat-*.pdb ../LJSrcNew/cocos2d-x-2.2.2/projects/ljshell/proj.win32/Debug.win32
cp libavdevice/avdevice-*.pdb ../LJSrcNew/cocos2d-x-2.2.2/projects/ljshell/proj.win32/Debug.win32
cp libavcodec/avcodec-*.pdb ../LJSrcNew/cocos2d-x-2.2.2/projects/ljshell/proj.win32/Debug.win32
cp libavutil/avutil-*.pdb ../LJSrcNew/cocos2d-x-2.2.2/projects/ljshell/proj.win32/Debug.win32
cp libswresample/swresample-*.pdb ../LJSrcNew/cocos2d-x-2.2.2/projects/ljshell/proj.win32/Debug.win32
cp libswscale/swscale-*.pdb ../LJSrcNew/cocos2d-x-2.2.2/projects/ljshell/proj.win32/Debug.win32
cp libavfilter/avfilter-*.pdb ../LJSrcNew/cocos2d-x-2.2.2/projects/ljshell/proj.win32/Debug.win32
 
cp libavformat/*.h ../LJSrcNew/cocos2d-x-2.2.2/external/ffmpeg/include/libavformat -f -R
cp libavdevice/*.h ../LJSrcNew/cocos2d-x-2.2.2/external/ffmpeg/include/libavdevice -f -R
cp libavcodec/*.h ../LJSrcNew/cocos2d-x-2.2.2/external/ffmpeg/include/libavcodec -f -R
cp libavutil/*.h ../LJSrcNew/cocos2d-x-2.2.2/external/ffmpeg/include/libavutil -f -R
cp libswresample/*.h ../LJSrcNew/cocos2d-x-2.2.2/external/ffmpeg/include/libswresample -f -R
cp libswscale/*.h ../LJSrcNew/cocos2d-x-2.2.2/external/ffmpeg/include/libswscale -f -R
cp libavfilter/*.h ../LJSrcNew/cocos2d-x-2.2.2/external/ffmpeg/include/libavfilter -f -R
cp libavresample/*.h ../LJSrcNew/cocos2d-x-2.2.2/external/ffmpeg/include/libavresample -f -R
cp libpostproc/*.h ../LJSrcNew/cocos2d-x-2.2.2/external/ffmpeg/include/libpostproc -f -R
cp compat/*.h ../LJSrcNew/cocos2d-x-2.2.2/external/ffmpeg/include/compat -f -R

cp config.h ../LJSrcNew/cocos2d-x-2.2.2/external/ffmpeg/prebuilt/win32 -f -R
rm ../LJSrcNew/cocos2d-x-2.2.2/external/ffmpeg/prebuilt/win32/ffconfig.h
mv ../LJSrcNew/cocos2d-x-2.2.2/external/ffmpeg/prebuilt/win32/config.h ../LJSrcNew/cocos2d-x-2.2.2/external/ffmpeg/prebuilt/win32/ffconfig.h
