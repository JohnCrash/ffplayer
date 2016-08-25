#include "android_camera.h"
#include <android/log.h>

#define TAG "AndroidDemuxer"
#define LOG(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)

#define ANDROID_DEMUXER_CLASS_NAME "org/ffmpeg/device/AndroidDemuxer"

JniGetStaticMethodInfo_t jniGetStaticMethodInfo = NULL;

static AndroidDemuxerCB_t _demuxerCB = NULL;

typedef struct imageFormatName{
    int imageFormat;
    const char * imageFormatName;
} imageFormatName_t;
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

/*
 * 输入android图像格式标识，返回格式名称
 */
const char * android_ImageFormatName( int ifn )
{
    int i;
    for(i=0;i<sizeof(_ifns)/sizeof(imageFormatName_t);i++){
        if(_ifns[i].imageFormat==ifn)
            return _ifns[i].imageFormatName;
    }
    return NULL;
}

/*
 * 输入格式名称返回格式标识
 */
int android_ImageFormat(const char * fn)
{
    int i;
    for(i=0;i<sizeof(_ifns)/sizeof(imageFormatName_t);i++){
        if(strcmp(_ifns[i].imageFormatName,fn)==0)
            return _ifns[i].imageFormat;
    }
    return -1;
}

/*
 * 取得Android摄像机数量
 */
int android_getNumberOfCameras()
{
    struct JniMethodInfo_t jmi;
    int ret = -1;
    if(jniGetStaticMethodInfo(&jmi,ANDROID_DEMUXER_CLASS_NAME,"getNumberOfCameras","()I")) {
        ret = (*jmi.env)->CallStaticIntMethod(jmi.env,jmi.classID,jmi.methodID);
        (*jmi.env)->DeleteLocalRef(jmi.env,jmi.classID);
    }
    return ret;
}

/*
 * 取得Android摄像机信息
 */
int android_getCameraCapabilityInteger(int n, int *pinfo,int len)
{
    struct JniMethodInfo_t jmi;
    int ret = -1;
    if(jniGetStaticMethodInfo(&jmi,ANDROID_DEMUXER_CLASS_NAME,"getCameraCapabilityInteger","(I[I)I")) {
        jintArray infoObj = (*jmi.env)->NewIntArray(jmi.env,1024*64);
        if(infoObj) {
            ret = (*jmi.env)->CallStaticIntMethod(jmi.env,jmi.classID, jmi.methodID, n, infoObj);
            if (ret > 0 && ret <= len) {
                //   jboolean isCopy = JNI_TRUE;
                jint *info = (*jmi.env)->GetIntArrayElements(jmi.env,infoObj, 0);
                memcpy(pinfo, info, len);
                (*jmi.env)->ReleaseIntArrayElements(jmi.env,infoObj, info, JNI_ABORT);
            }
            (*jmi.env)->DeleteLocalRef(jmi.env,infoObj);
        }
        (*jmi.env)->DeleteLocalRef(jmi.env,jmi.classID);
    }
    return ret;
}

/*
 * 打开android摄像头和录音设备，tex是一个OES材质
 *         glGenTextures(1,textures,0);
 *         glBindTexture(GL_TEXTURE_EXTERNAL_OES, textures[0]);
 *         glTexParameterf(GL_TEXTURE_EXTERNAL_OES,
 *                 GL_TEXTURE_MIN_FILTER,
 *                 GL_LINEAR);
 *         glTexParameterf(GL_TEXTURE_EXTERNAL_OES,
 *                 GL_TEXTURE_MAG_FILTER, GL_LINEAR);
 *         // Clamp to edge is only option.
 *         glTexParameteri(GL_TEXTURE_EXTERNAL_OES,
 *                 GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
 *         glTexParameteri(GL_TEXTURE_EXTERNAL_OES,
 *                 GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
 * 注意打开android权限
 *     <uses-permission android:name="android.permission.INTERNET"/>
 *     <uses-permission android:name="android.permission.ACCESS_NETWORK_STATE"/>
 *     <uses-permission android:name="android.permission.CAMERA" />
 *     <uses-feature android:name="android.hardware.camera" />
 *     <uses-permission android:name="android.permission.RECORD_AUDIO" />
 *     <uses-permission android:name="android.permission.WRITE_EXTERNAL_STORAGE" />
 *
 * nDevice 设备序号
 * w,h 请求的尺寸，fmt请求的图像的格式，fps请求的帧率。注意请求不一定被满足。
 * nChannel 录音频道数，sampleFmt 采样格式，sampleRate采样率
 * 成功返回0，失败返回<0
 *
 */
int android_openDemuxer(int tex,int nDevice, int w, int h, int fmt, int fps,
                        int nChannel, int sampleFmt, int sampleRate)
{
    struct JniMethodInfo_t jmi;
    int ret = -1;
    if(jniGetStaticMethodInfo(&jmi,ANDROID_DEMUXER_CLASS_NAME,"openDemuxer","(IIIIIIIII)I")) {
        ret = (*jmi.env)->CallStaticIntMethod(jmi.env,jmi.classID,jmi.methodID,tex,nDevice,w,h,fmt,
                                           fps,nChannel,sampleFmt,sampleRate);
        (*jmi.env)->DeleteLocalRef(jmi.env,jmi.classID);
    }
    return ret;
}

/*
 * 如果使用预视材质，需要在OpenGL渲染线程中调用这个更新函数，用来向材质中装入图像
 */
void android_updatePreivewTexture(float * pTextureMatrix)
{
    if(!pTextureMatrix)return;

    struct JniMethodInfo_t jmi;
    if(jniGetStaticMethodInfo(&jmi,ANDROID_DEMUXER_CLASS_NAME,"update","([F)Z")) {
        jfloatArray jfa = (*jmi.env)->NewFloatArray(jmi.env,16);
        (*jmi.env)->CallStaticVoidMethod(jmi.env,jmi.classID,jmi.methodID,jfa);
        jfloat * jbuf = (*jmi.env)->GetFloatArrayElements(jmi.env,jfa,0);
        memcpy(pTextureMatrix,jbuf,sizeof(float)*16);
        (*jmi.env)->ReleaseFloatArrayElements(jmi.env,jfa,jbuf,JNI_ABORT);
        (*jmi.env)->DeleteLocalRef(jmi.env,jmi.classID);
    }
}

/*
 * 当更新好一个视频材质时，将会增加预视帧数
 */
int64_t android_getPreivewFrameCount()
{
    struct JniMethodInfo_t jmi;
    if(jniGetStaticMethodInfo(&jmi,ANDROID_DEMUXER_CLASS_NAME,"getGrabFrameCount","(Z)I")) {
        int64_t ret = (int64_t)(*jmi.env)->CallStaticLongMethod(jmi.env,jmi.classID,jmi.methodID);
        (*jmi.env)->DeleteLocalRef(jmi.env,jmi.classID);
        return ret;
    }
    return -1;
}

/*
 * 关闭当前的android摄像头和录音设备
 */
void android_closeDemuxer()
{
    struct JniMethodInfo_t jmi;
    if(jniGetStaticMethodInfo(&jmi,ANDROID_DEMUXER_CLASS_NAME,"closeDemuxer","()V")) {
        (*jmi.env)->CallStaticVoidMethod(jmi.env,jmi.classID,jmi.methodID);
        (*jmi.env)->DeleteLocalRef(jmi.env,jmi.classID);
    }
    _demuxerCB = NULL;
}

/*
 * 打开或者关闭自动对焦
 */
int android_autoFoucs(int bAutofocus)
{
    struct JniMethodInfo_t jmi;
    if(jniGetStaticMethodInfo(&jmi,ANDROID_DEMUXER_CLASS_NAME,"autoFoucs","(Z)Z")) {
        int ret = (*jmi.env)->CallBooleanMethod(jmi.env,jmi.classID,jmi.methodID,bAutofocus)?1:0;
        (*jmi.env)->DeleteLocalRef(jmi.env,jmi.classID);
        return ret;
    }
    return 0;
}

/*
 * 归还缓冲区
 */
void android_releaseBuffer(void * bufObj, unsigned char * buf)
{
    struct JniMethodInfo_t jmi;
    if(!bufObj || !buf)return;

    if(jniGetStaticMethodInfo(&jmi,ANDROID_DEMUXER_CLASS_NAME,"releaseBuffer","([B)V")) {
        jbyteArray bobj = (jbyteArray)bufObj;
        (*jmi.env)->ReleaseByteArrayElements(jmi.env,bobj,(jbyte*)buf,JNI_ABORT);
        (*jmi.env)->CallStaticVoidMethod(jmi.env,jmi.classID,jmi.methodID,bobj);
        (*jmi.env)->DeleteGlobalRef(jmi.env,bobj);
        (*jmi.env)->DeleteLocalRef(jmi.env,jmi.classID);
    }
}

int android_setDemuxerCallback( AndroidDemuxerCB_t cb )
{
    _demuxerCB = cb;
}

int android_isClosed()
{
    struct JniMethodInfo_t jmi;

    if(jniGetStaticMethodInfo(&jmi,ANDROID_DEMUXER_CLASS_NAME,"isClosed","()Z")) {
        jboolean b = (*jmi.env)->CallStaticBooleanMethod(jmi.env,jmi.classID,jmi.methodID);
        (*jmi.env)->DeleteLocalRef(jmi.env,jmi.classID);
        return b ? 1 : 0;
    }
    return -1;
}

/*
 * 当设备打开后获取最终的参数
 */
int android_getDemuxerInfo(int *pw,int *ph,int *pfmt,int *pfps,
                           int *pch,int *psampleFmt,int *psampleRate)
{
    struct JniMethodInfo_t jmi;

    if(jniGetStaticMethodInfo(&jmi,ANDROID_DEMUXER_CLASS_NAME,"getDemuxerInfo","([I)Z")) {
        jintArray pinfo = (*jmi.env)->NewIntArray(jmi.env,7);
        jboolean b = (*jmi.env)->CallStaticBooleanMethod(jmi.env,jmi.classID,jmi.methodID,pinfo);
        jint* pbuf = (*jmi.env)->GetIntArrayElements(jmi.env,pinfo,0);
        if(pw)*pw = pbuf[0];
        if(ph)*ph = pbuf[1];
        if(pfmt)*pfmt = pbuf[2];
        if(pfps)*pfps = pbuf[3];
        if(pch)*pch = pbuf[4];
        if(psampleFmt)*psampleFmt = pbuf[5];
        if(psampleRate)*psampleRate = pbuf[6];
        (*jmi.env)->ReleaseIntArrayElements(jmi.env,pinfo,pbuf,JNI_ABORT);
        (*jmi.env)->DeleteLocalRef(jmi.env,pinfo);
        (*jmi.env)->DeleteLocalRef(jmi.env,jmi.classID);
        return b ? 0 : -1;
    }
    return -2;
}

/*
 * 图像数据和音频数据传入
 */
JNIEXPORT void JNICALL
Java_org_ffmpeg_device_AndroidDemuxer_ratainBuffer(JNIEnv * env , jclass cls,
int type, jbyteArray bobj,int len,int fmt,int p0,int p1,jlong timestramp)
{
    if(type==VIDEO_DATA) {
            /*
             * 为了避免memcpy操作，这里为缓冲区创建一个全局引用。确保缓存区在编码器中不会被释放
             * 当使用完成后调用android_releaseBuffer归还缓冲区。
             */
            jbyteArray gobj = (jbyteArray)(*env)->NewGlobalRef(env, bobj);
            jbyte *buf = (*env)->GetByteArrayElements(env, gobj, 0);
            if(_demuxerCB){
                if(_demuxerCB(type, gobj, len, (unsigned char * ) buf,fmt,p0,p1,timestramp )) {
                    android_releaseBuffer(gobj,buf);
                    android_closeDemuxer();
                }
            }else{
                //没有回调直接归还缓冲区
                android_releaseBuffer(gobj,buf);
            }
    }else if(type==AUDIO_DATA){
        jbyte *buf = (*env)->GetByteArrayElements(env,bobj, 0);
        if ( _demuxerCB(type, bobj, len, (unsigned char * ) buf,fmt,p0,p1,timestramp )) {
            android_closeDemuxer();
        }
        (*env)->ReleaseByteArrayElements(env,bobj,(jbyte*)buf,JNI_ABORT);
    }else{
        if ( _demuxerCB(type, bobj, 0, NULL,fmt,p0,p1,timestramp )) {
            android_closeDemuxer();
        }
    }
    (*env)->DeleteLocalRef(env,bobj);
}