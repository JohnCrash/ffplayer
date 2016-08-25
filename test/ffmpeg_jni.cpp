//
// Created by john on 2016/8/2.
//

#include "ffmpeg_jni.h"

#include <jni.h>
#include <android/log.h>
#include <string.h>
#include <pthread.h>
#include "JniHelper.h"
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include "ffcommon.h"
#include "ffdec.h"
#include "ffenc.h"
#include "live.h"
#include "android_camera.h"

namespace ff {
    using namespace cocos2d;

#define TAG "AndroidDemuxer"
#define LOG(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)

#define ANDROID_DEMUXER_CLASS_NAME "com/example/wesnoth/camerartmp/AndroidDemuxer"
    static AndroidDemuxerCB_t _demuxerCB = NULL;
    struct imageFormatName_t{
        int imageFormat;
        const char * imageFormatName;
    };
#define IMAGEFORMAT_NAME(a) {a,#a}
    static imageFormatName_t _ifns[] = {
            IMAGEFORMAT_NAME(FLEX_RGBA_8888),
            IMAGEFORMAT_NAME(FLEX_RGB_888),
            IMAGEFORMAT_NAME(JPEG),
            IMAGEFORMAT_NAME(NV16),
            IMAGEFORMAT_NAME(NV21),
            IMAGEFORMAT_NAME(PRIVATE),
            IMAGEFORMAT_NAME(RAW10),
            IMAGEFORMAT_NAME(RAW12),
            IMAGEFORMAT_NAME(RAW_PRIVATE),
            IMAGEFORMAT_NAME(RAW_SENSOR),
            IMAGEFORMAT_NAME(RGB_565),
            IMAGEFORMAT_NAME(YUV_420_888),
            IMAGEFORMAT_NAME(YUV_422_888),
            IMAGEFORMAT_NAME(YUV_444_888),
            IMAGEFORMAT_NAME(YUY2),
            IMAGEFORMAT_NAME(DEPTH_POINT_CLOUD),
            IMAGEFORMAT_NAME(YV12),
            IMAGEFORMAT_NAME(DEPTH16),
    };
//    void test_function() {
//        LOG("ffmpeg camera rtmp");
//    }

    int android_getNumberOfCameras()
    {
        JniMethodInfo jmi;
        int ret = -1;
        if(JniHelper::getStaticMethodInfo(jmi,ANDROID_DEMUXER_CLASS_NAME,"getNumberOfCameras","()I")) {
            ret = jmi.env->CallStaticIntMethod(jmi.classID,jmi.methodID);
            jmi.env->DeleteLocalRef(jmi.classID);
        }
        return ret;
    }

    int android_getCameraCapabilityInteger(int n, int *pinfo,int len)
    {
        JniMethodInfo jmi;
        int ret = -1;
        if(JniHelper::getStaticMethodInfo(jmi,ANDROID_DEMUXER_CLASS_NAME,"getCameraCapabilityInteger","(I[I)I")) {
            jintArray infoObj = jmi.env->NewIntArray(1024*64);
            if(infoObj) {
                ret = jmi.env->CallStaticIntMethod(jmi.classID, jmi.methodID, n, infoObj);
                if (ret > 0 && ret <= len) {
                    //   jboolean isCopy = JNI_TRUE;
                    jint *info = jmi.env->GetIntArrayElements(infoObj, 0);
                    memcpy(pinfo, info, len);
                    jmi.env->ReleaseIntArrayElements(infoObj, info, JNI_ABORT);
                }
                jmi.env->DeleteLocalRef(infoObj);
            }
            jmi.env->DeleteLocalRef(jmi.classID);
        }
        return ret;
    }

    int android_openDemuxer(int tex,int nDevice, int w, int h, int fmt, int fps,
                            int nChannel, int sampleFmt, int sampleRate)
    {
        JniMethodInfo jmi;
        int ret = -1;
        if(JniHelper::getStaticMethodInfo(jmi,ANDROID_DEMUXER_CLASS_NAME,"openDemuxer","(IIIIIIIII)I")) {
            ret = jmi.env->CallStaticIntMethod(jmi.classID,jmi.methodID,tex,nDevice,w,h,fmt,
                                               fps,nChannel,sampleFmt,sampleRate);
            jmi.env->DeleteLocalRef(jmi.classID);
        }
        return ret;
    }

    void android_closeDemuxer()
    {
        JniMethodInfo jmi;
        if(JniHelper::getStaticMethodInfo(jmi,ANDROID_DEMUXER_CLASS_NAME,"closeDemuxer","()V")) {
            jmi.env->CallStaticVoidMethod(jmi.classID,jmi.methodID);
            jmi.env->DeleteLocalRef(jmi.classID);
        }
        _demuxerCB = NULL;
    }

    int android_autoFoucs(int bAutofocus)
    {
        JniMethodInfo jmi;
        if(JniHelper::getStaticMethodInfo(jmi,ANDROID_DEMUXER_CLASS_NAME,"autoFoucs","(Z)Z")) {
            int ret = jmi.env->CallBooleanMethod(jmi.classID,jmi.methodID,bAutofocus)?1:0;
            jmi.env->DeleteLocalRef(jmi.classID);
            return ret;
        }
        return 0;
    }

    void android_releaseBuffer(void * bufObj,unsigned char * buf)
    {
        JniMethodInfo jmi;
        if(!bufObj || !buf)return;

        if(JniHelper::getStaticMethodInfo(jmi,ANDROID_DEMUXER_CLASS_NAME,"releaseBuffer","([B)V")) {
            jbyteArray bobj = (jbyteArray)bufObj;
            jmi.env->ReleaseByteArrayElements(bobj,(jbyte*)buf,JNI_ABORT);
            jmi.env->CallStaticVoidMethod(jmi.classID,jmi.methodID,bobj);
            jmi.env->DeleteGlobalRef(bobj);
            jmi.env->DeleteLocalRef(jmi.classID);
        }
    }

    int android_setDemuxerCallback( AndroidDemuxerCB_t cb )
    {
        _demuxerCB = cb;
    }

    const char * android_ImageFormatName( int ifn )
    {
        for(int i=0;i<sizeof(_ifns)/sizeof(imageFormatName_t);i++){
            if(_ifns[i].imageFormat==ifn)
                return _ifns[i].imageFormatName;
        }
        return NULL;
    }

    int android_ImageFormat(const char * fn)
    {
        for(int i=0;i<sizeof(_ifns)/sizeof(imageFormatName_t);i++){
            if(strcmp(_ifns[i].imageFormatName,fn)==0)
                return _ifns[i].imageFormat;
        }
        return -1;
    }

    void android_updatePreivewTexture(float * pTextureMatrix)
    {
        if(!pTextureMatrix)return;

        JniMethodInfo jmi;
        if(JniHelper::getStaticMethodInfo(jmi,ANDROID_DEMUXER_CLASS_NAME,"update","([F)Z")) {
            jfloatArray jfa = jmi.env->NewFloatArray(16);
            jmi.env->CallStaticVoidMethod(jmi.classID,jmi.methodID,jfa);
            jfloat * jbuf = jmi.env->GetFloatArrayElements(jfa,0);
            memcpy(pTextureMatrix,jbuf,sizeof(float)*16);
            jmi.env->ReleaseFloatArrayElements(jfa,jbuf,JNI_ABORT);
            jmi.env->DeleteLocalRef(jmi.classID);
        }
    }

    int64_t android_getPreivewFrameCount()
    {
        JniMethodInfo jmi;
        if(JniHelper::getStaticMethodInfo(jmi,ANDROID_DEMUXER_CLASS_NAME,"getGrabFrameCount","(Z)I")) {
            int64_t ret = (int64_t)jmi.env->CallStaticLongMethod(jmi.classID,jmi.methodID);
            jmi.env->DeleteLocalRef(jmi.classID);
            return ret;
        }
        return -1;
    }

    void hello()
    {
        LOG("ff::hello()\n");
    }

    int JniHelper_GetStaticMethodInfo(JniMethodInfo *pmethodinfo,
                                            const char *className,
                                            const char *methodName,
                                            const char *paramCode)
    {
        return JniHelper::getStaticMethodInfo(*pmethodinfo,className,methodName,paramCode);
    }
}

extern "C"
{
//private static native void ratainBuffer(int type,byte [] data);
JNIEXPORT void JNICALL
Java_com_example_wesnoth_camerartmp_AndroidDemuxer_ratainBuffer(JNIEnv * env , jclass cls,
int type, jbyteArray bobj,int len,int fmt,int p0,int p1,jlong timestramp)
{
    if(ff::_demuxerCB){
        if(type==ff::android_CallbackType::VIDEO_DATA) {
            jbyteArray gobj = (jbyteArray)env -> NewGlobalRef(bobj);
            jbyte *buf = env->GetByteArrayElements(gobj, 0);
            if ( ff::_demuxerCB(type, gobj, len, (unsigned char * ) buf,fmt,p0,p1,timestramp )) {
                 ff::android_closeDemuxer();
            }
        }else if(type==ff::android_CallbackType::AUDIO_DATA){
            jbyte *buf = env->GetByteArrayElements(bobj, 0);
            if ( ff::_demuxerCB(type, bobj, len, (unsigned char * ) buf,fmt,p0,p1,timestramp )) {
                ff::android_closeDemuxer();
            }
            env->ReleaseByteArrayElements(bobj,(jbyte*)buf,JNI_ABORT);
        }else{
            if ( ff::_demuxerCB(type, bobj, 0, NULL,fmt,p0,p1,timestramp )) {
                ff::android_closeDemuxer();
            }
        }
    }
    env->DeleteLocalRef(bobj);
}

using namespace ff;

JNIEXPORT void JNICALL
Java_com_example_wesnoth_camerartmp_AndroidDemuxer_testLiveRtmpEnd(JNIEnv * env , jclass cls)
{
    ff::android_closeDemuxer();
}

int testCB(int type,void * bufObj,int size,unsigned char * buf,int fmt,int p0,int p1,int64_t timestramp)
{
    LOG("testCB[%d] length=%d buf=%p",type,size,buf);
    if(type==0)
        ff::android_releaseBuffer(bufObj,buf);
    return 0;
}

static int liveCallback(liveState *pls)
{
//	LOG("live state : %d\n", pls->state);
    if (pls->state == LIVE_ERROR){
        for (int i = 0; i < pls->nerror; i++){
            LOG(pls->errorMsg[i]);
        }
    }
    return 0;
}

JNIEXPORT void JNICALL
Java_com_example_wesnoth_camerartmp_AndroidDemuxer_testLiveRtmp(JNIEnv * env , jclass cls,int tex)
{
    AVDevice caps[8];
    char * video_name = NULL;
    char * audio_name = NULL;
    int vindex = 0;

    jniGetStaticMethodInfo = (JniGetStaticMethodInfo_t)ff::JniHelper_GetStaticMethodInfo;
    LOG("av_ff_init\n");
    av_ff_init();

/*
 * android camera enum test
 */
    LOG("ffCapDevicesList\n");
    int count = ffCapDevicesList(caps, 8);
    int vi = 0;
    LOG("ffCapDevicesList return %d\n",count);
    for (int m = 0; m < count; m++){
        if (!video_name && caps[m].type == AV_DEVICE_VIDEO){
            if (vi == vindex){
                video_name = caps[m].alternative_name;
                LOG("select : %s\n", caps[m].name);
            }
            vi++;
        }
    else if (!audio_name && caps[m].type == AV_DEVICE_AUDIO){
        audio_name = caps[m].alternative_name;
    }
    if (1){
            LOG("device name : %s \n %s\n", caps[m].name,caps[m].alternative_name);

            for (int i = 0; i < caps[m].capability_count; i++){
                if (caps[m].type == AV_DEVICE_VIDEO){
                    int min_w = caps[m].capability[i].video.min_w;
                    int min_h = caps[m].capability[i].video.min_h;
                    int min_fps = caps[m].capability[i].video.min_fps;
                    int max_w = caps[m].capability[i].video.max_w;
                    int max_h = caps[m].capability[i].video.max_h;
                    int max_fps = caps[m].capability[i].video.max_fps;
                    char * pix = caps[m].capability[i].video.pix_format;
                    char * name = caps[m].capability[i].video.codec_name;
                    LOG("min w = %d min h = %d min fps = %d "
                    "max w = %d max h = %d max fps = %d "
                    "fmt = %s\n",
                    min_w, min_h, min_fps,
                    max_w, max_h, max_fps,
                    pix);
                }
                else{
                    int min_ch = caps[m].capability[i].audio.min_ch;
                    int min_bit = caps[m].capability[i].audio.min_bit;
                    int min_rate = caps[m].capability[i].audio.min_rate;
                    int max_ch = caps[m].capability[i].audio.max_ch;
                    int max_bit = caps[m].capability[i].audio.max_bit;
                    int max_rate = caps[m].capability[i].audio.max_rate;
                    char * fmt = caps[m].capability[i].audio.sample_format;
                    char * name = caps[m].capability[i].audio.codec_name;
                    LOG("min ch = %d min bit = %d min samples = %d "
                    "max ch = %d max bit = %d max samples = %d "
                    "fmt = %s\n",
                    min_ch, min_bit, min_rate,
                    max_ch, max_bit, max_rate,
                    fmt);
                }
            }
        }
    }
/*
 * android camera live test
 */
    char fmt_name[256];
    int w, h, fps;
    w = 640;
    h = 480;
    fps = 12;
    strcpy(fmt_name, "nv21");
    //video_name = "camera_1";
    liveOnRtmp("rtmp://192.168.7.157/myapp/mystream",
                video_name,w,h,fps,fmt_name,1024*1024,
                audio_name,22050,"s16",32*1024,liveCallback);
/*
 * camera preview test
 */
#if 0
    LOG("call ff::getNumberOfCameras()\n");
    int n = ff::android_getNumberOfCameras();
    LOG("ff::getNumberOfCameras return %d\n",n);
    int * pinfo = new int[1024];
    for(int i = 0;i < n;i++){
        int count = ff::android_getCameraCapabilityInteger(i,pinfo,1024);
        LOG("Face : %s\nOrientation : %d \n",
        pinfo[0]== 1?"front":"back",
        pinfo[1]);
        //摄像头的尺寸数据
        LOG("Size:\n");
        for(int j=0;j<pinfo[2];j++){
            LOG("   %dx%d\n",pinfo[3+2*j],pinfo[3+2*j+1]);
        }
        int off1 = pinfo[2]*2+2+1;
        /*
         * 摄像头支持的格式,FLEX_RGBA_8888,FLEX_RGB_888,JPEG,NV16,YUV_420_888,YUV_422_888
         * YUV_444_888,YUY2,YV12,RGB_565,RAW10,RAW12,NV21
         */
        LOG("format: %d \n",pinfo[off1]);
        for(int k=0;k<pinfo[off1];k++){
            const char *formatName = ff::android_ImageFormatName( pinfo[k+off1+1] );
            if(formatName)
               LOG("    %s \n",formatName);
            else
              LOG("    %d \n",pinfo[k+off1+1]);
        }
        int off2 = off1+pinfo[off1]+1;
        //摄像头支持的帧
        LOG("fps: %d \n",pinfo[off2]);
        for(int s=0;s<pinfo[off2];s++){
         LOG("+%d \n",pinfo[off2+s+1]);
        }
    }
    //test preview buffer
    int fmt,sampleFmt;
    fmt = ff::NV21;
    sampleFmt = 0;
    ff::android_setDemuxerCallback(testCB);
    int ret = ff::android_openDemuxer(tex,0,640,480,fmt,-1,
                                      2,sampleFmt,44100);
    if(ret==0){
      LOG("android_openDemuxer successed,return %d\n",ret);
    }else{
      LOG("android_openDemuxer return error %d\n",ret);
    }
#endif
}

JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM *vm, void *reserved)
{

    cocos2d::JniHelper::setJavaVM(vm);

    return JNI_VERSION_1_4;
}

JNIEXPORT void JNICALL
Java_com_example_wesnoth_camerartmp_MainActivity_nativeSetContext(JNIEnv*  env, jobject thiz, jobject context, jobject assetManager)
{
    cocos2d::JniHelper::setClassLoaderFrom(context);
    //FileUtilsAndroid::setassetmanager(AAssetManager_fromJava(env, assetManager));
}

void checkGLError( const char * msg){
    int error = glGetError();
    if(error!=GL_NO_ERROR){
        LOG("%s %d",msg,error);
    }
}

JNIEXPORT void JNICALL
Java_com_example_wesnoth_camerartmp_OpenGLRenderer_updateCameraTextrue(JNIEnv*  env, jobject thiz,int tex,int w,int h,jbyteArray byuv21)
{
    jbyte *buf = env->GetByteArrayElements(byuv21, 0);
    if(buf){
        glBindTexture(GL_TEXTURE_2D, tex);
        checkGLError("updateCameraTextrue glBindTexture");
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w, h, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, buf);
        checkGLError("updateCameraTextrue glTexImage2D");
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        checkGLError("updateCameraTextrue glTexParameteri");

    }
    env->ReleaseByteArrayElements(byuv21,(jbyte*)buf,JNI_ABORT);
    env->DeleteLocalRef(byuv21);
}

} //extern "C" {