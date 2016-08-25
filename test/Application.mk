APP_STL := gnustl_static
APP_CPPFLAGS := -frtti -DUSE_ZMQ -DCC_ENABLE_CHIPMUNK_INTEGRATION=1 -DCOCOS2D_DEBUG=1 -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS -std=c++11 -fsigned-char

APP_OPTIM := debug

APP_CPPFLAGS += -fexceptions
#APP_ABI := armeabi-v7a armeabi x86
APP_ABI := armeabi
#APP_ABI := armeabi
APP_PLATFORM := android-9