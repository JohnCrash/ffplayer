这是ffmpeg播放器ffplay的一个cocos2d实现
可以独立进行编译，仅仅依赖于cocos2d。
图像将被转换为一个材质，声音部分使用的是
SDL的驱动，可以在windows,ios,android下工作。
看FFVideo.h有完整的用法说明。
你可以使用
proj.win32下的build_vc.bat编译ffmpeg windows下的动态库。
proj.ios 下的ios-build.sh用来编译ios用的静态库。
proj.android 下的ffmpeg-build.sh用来编译android用的静态库。