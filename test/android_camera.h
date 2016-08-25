#ifndef __ANDROID_CAMERA_H__
#define __ANDROID_CAMERA_H__

#include <stdint.h>
#include <jni.h>
#include "libavutil/attributes.h"

enum android_ImageFormat {
    FLEX_RGBA_8888 = 42,
    FLEX_RGB_888 = 41,
    JPEG = 256,
    NV16 = 16,
    NV21 = 17,
    PRIVATE = 34,
    RAW10 = 37,
    RAW12 = 38,
    RAW_PRIVATE = 36,
    RAW_SENSOR = 32,
    RGB_565 = 4,
    UNKNOWN = 0,
    YUV_420_888 = 35,
    YUV_422_888 = 39,
    YUV_444_888 = 40,
    YUY2 = 20,
    DEPTH_POINT_CLOUD = 257,
    YV12 = 842094169,
    DEPTH16 = 1144402265,
};

enum android_CallbackType{
    VIDEO_DATA = 0,
    AUDIO_DATA = 1,
    AUTO_FOUCS = 2,
    TEXTURE_AVAILABLE = 3,
    CAMERA_OPEN = 4,
    CAMERA_CLOSE = 5
};


struct JniMethodInfo_t
{
    JNIEnv *    env;
    jclass      classID;
    jmethodID   methodID;
};

typedef int (*JniGetStaticMethodInfo_t)(struct JniMethodInfo_t *pmethodinfo,
        const char *className,
        const char *methodName,
        const char *paramCode);

extern JniGetStaticMethodInfo_t jniGetStaticMethodInfo;

const char * android_ImageFormatName( int ifn );

int android_ImageFormat(const char * fn);

typedef int (*AndroidDemuxerCB_t)(int type,void * bufObj,int size,unsigned char * buf,
                                  int fmt,int w,int h,int64_t timestramp); //fmt,ch,rate

int android_getNumberOfCameras();

int android_getCameraCapabilityInteger(int n, int *pinfo,int len);

int android_openDemuxer(int tex,int nDevice, int w, int h, int fmt, int fps,
                        int nChannel, int sampleFmt, int sampleRate);
int android_getDemuxerInfo(int *pw,int *ph,int *pfmt,int *pfps,int *pch,int *psampleFmt,int *psampleRate);

void android_updatePreivewTexture(float * pTextureMatrix);

int64_t android_getPreivewFrameCount();

void android_closeDemuxer();

int android_isClosed();

int android_autoFoucs(int b);

void android_releaseBuffer(void * bufObj, unsigned char * buf);

int android_setDemuxerCallback( AndroidDemuxerCB_t cb );

#endif