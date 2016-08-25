LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := CameraRtmp

LOCAL_MODULE_FILENAME := libCameraRtmp

LOCAL_SRC_FILES :=  ffcommon.c \
                    ffdec.cpp \
                    ffenc.cpp \
                    ffraw.cpp \
                    ffmpeg_jni.cpp \
                    JniHelper.cpp \
                    live.cpp \
                    android_camera.c \
                    android_demuxer.c \

LOCAL_LDLIBS := -lGLESv1_CM \
                -lGLESv2 \
                -lEGL \
                -llog \
                -lz \
                -landroid

#LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../Classes

LOCAL_WHOLE_STATIC_LIBRARIES += ffmpeg_static

include $(BUILD_SHARED_LIBRARY)

$(call import-module,ffmpeg/prebuilt/android)