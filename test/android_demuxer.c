//
// Created by john on 2016/8/11.
//
#ifdef __ANDROID__
#include "android_demuxer.h"

#include "libavutil/parseutils.h"
#include "libavutil/pixdesc.h"
#include "libavutil/opt.h"
#include "libavformat/internal.h"
#include "libavformat/riff.h"
#include "libavdevice/avdevice.h"
#include "libavcodec/raw.h"

#include "android_camera.h"

#define MAX_GRAB_BUFFER_SIZE (32*1024*1024)

static AVFormatContext *_avctx = NULL;

static int debug = 0;
#define DEBUG(format,...) if(debug)av_log(_avctx,AV_LOG_VERBOSE,format, ##__VA_ARGS__)

static int parse_device_name(AVFormatContext *avctx)
{
    struct android_camera_ctx *ctx = avctx->priv_data;
    char **device_name = ctx->device_name;
    char *name = av_strdup(avctx->filename);
    char *tmp = name;
    int ret = 1;
    char *type;

    while ((type = strtok(tmp, "="))) {
        char *token = strtok(NULL, ":");
        tmp = NULL;

        if(!strcmp(type, "video")) {
            device_name[0] = token;
        } else if (!strcmp(type, "audio")) {
            device_name[1] = token;
        } else {
            device_name[0] = NULL;
            device_name[1] = NULL;
            break;
        }
    }

    if (!device_name[0] && !device_name[1]) {
        ret = 0;
    } else {
        if (device_name[0])
            device_name[0] = av_strdup(device_name[0]);
        if (device_name[1])
            device_name[1] = av_strdup(device_name[1]);
    }

    av_free(name);
    return ret;
}

/*
 * 枚举android摄像头信息，并通过AV_LOG_INFO传回
 */
static int
android_cycle_devices(AVFormatContext *avctx,enum androidDeviceType devtype)
{
    struct android_camera_ctx *ctx = avctx->priv_data;
    const char *device_name = ctx->device_name[devtype];
    char name[128];
    int i,n;

    if(devtype==VideoDevice){
        n = android_getNumberOfCameras();
        for(i=0;i<n;i++) {
            snprintf(name,128,"camera_%d",i);
            av_log(avctx, AV_LOG_INFO, " \"%s\"\n", name);
            av_log(avctx, AV_LOG_INFO, "    Alternative name \"%s\"\n", name);
        }
    }
    else if(devtype==AudioDevice){
        strcpy(name,"microphone");
        av_log(avctx, AV_LOG_INFO, " \"%s\"\n",name);
        av_log(avctx, AV_LOG_INFO, "    Alternative name \"%s\"\n",name);
    }
    else{
        return AVERROR(EIO);
    }
    return 0;
}

/*
 * 枚举摄像头信息，通过AV_LOG_INFO传回
 */
#define MAXCAPS 32*1024

static int parse_device_id(const char *device_name)
{
    if(device_name) {
        int len = strlen(device_name);
        char c = device_name[len-1];
        if(c >= '0' && c <= '9')
            return c-'0';
    }
    return -1;
}

static const char * android_2pixfmt_name(int fmt){
    switch(fmt){
        case NV21:return "nv21";
        case NV16:return "nv16";
        case YUV_420_888:return "yuv420p";
        case YUV_422_888:return "yuv422p";
        case YUV_444_888:return "yuv444p";
        case YV12:return "nv12";
        case YUY2:return "yuyv422";
        case FLEX_RGBA_8888:return "gbrap";
        case FLEX_RGB_888:return "gbrp";
        default:return "none";
    }
}

static int
android_list_device_options(AVFormatContext *avctx,enum androidDeviceType devtype)
{
    struct android_camera_ctx *ctx = avctx->priv_data;
    const char *device_name = ctx->device_name[devtype];
    int deviceId;
    int n,i,off1,off2,s,fps,k;
    int * pinfo;
    int j,w,h;
    const char *formatName;
    double min_fps,max_fps;

    if(devtype==VideoDevice){
        deviceId = parse_device_id(device_name);
        if(deviceId<0){
            av_log(avctx, AV_LOG_ERROR, "android_list_device_options device name:%s\n", deviceId);
            return AVERROR(EIO);
        }
        n = android_getNumberOfCameras();
        pinfo = malloc(MAXCAPS*sizeof(int));
        for(i=0;i<n;i++) {
            if(i==deviceId) {
                int ncount = android_getCameraCapabilityInteger(deviceId, pinfo, MAXCAPS);
                if (ncount > 0) {
                    av_log(avctx, AV_LOG_INFO, "face : %s\n", pinfo[0]==1?"front":"back");
                    av_log(avctx, AV_LOG_INFO, "orientation : %d\n", pinfo[1]);
                    off1 = pinfo[2]*2+2+1;
                    off2 = off1+pinfo[off1]+1;
                    min_fps = 100;
                    max_fps = 0;
                    /*
                     * 摄像头支持的帧率
                     */
                    for(s=0;s<pinfo[off2];s++){
                        fps =  pinfo[off2+s+1];
                        av_log(avctx, AV_LOG_INFO,"   framerate: %d \n",fps);
                        if( min_fps > fps )
                            min_fps = fps;
                        if( max_fps < fps )
                            max_fps = fps;
                    }
                    /*
                     * 摄像头支持的格式,FLEX_RGBA_8888,FLEX_RGB_888,JPEG,NV16,YUV_420_888,YUV_422_888
                     * YUV_444_888,YUY2,YV12,RGB_565,RAW10,RAW12,NV21
                     */
                    for(k=0;k<pinfo[off1];k++){
                        formatName = android_2pixfmt_name( pinfo[k+off1+1] );
                        /*
                         * 格式和尺寸的组合
                         */
                        for(j=0;j<pinfo[2];j++){
                            w = pinfo[3+2*j];
                            h = pinfo[3+2*j+1];
                            if(formatName)
                                av_log(avctx, AV_LOG_INFO,"  pixel_format=%s\n",formatName);
                            else
                                av_log(avctx, AV_LOG_INFO,"  pixel_format=UNKNOW\n");
                            av_log(avctx, AV_LOG_INFO, "  min s=%ldx%ld fps=%g max s=%ldx%ld fps=%g\n",
                                   w,h,min_fps,w,h,max_fps);
                        }
                    }
                } else {
                    av_log(avctx, AV_LOG_ERROR,
                           "android_getCameraCapabilityInteger(%d) return %d\n", i, ncount);
                    return AVERROR(EIO);
                }
            }
        }
        free(pinfo);
    }
    else if(devtype==AudioDevice){
        /*
         * 固定的参数
         */
        av_log(avctx, AV_LOG_INFO, "  min ch=%lu bits=%lu rate=%6lu max ch=%lu bits=%lu rate=%6lu\n",
               1,8,22050,2,16,44100);
    }
    else{
        return AVERROR(EIO);
    }
    return 0;
}

static void android_buffer_release(void *opaque, uint8_t *data)
{
	DEBUG("android_buffer_release");
    android_releaseBuffer(opaque,data);
	DEBUG("android_buffer_release return");
}
/*
 * 俘获回调
 */
static int android_grab_buffer(int type,void * bufObj,int buf_size,unsigned char * buf,
                               int fmt,int w,int h,int64_t timestramp)
{
    struct android_camera_ctx *ctx;
    AVPacketList **ppktl, *pktl_next;

	DEBUG("android_grab_buffer %d ,size=%d",type,buf_size);
    if(type!=VIDEO_DATA && type!=AUDIO_DATA){
        return 0;
    }
    if(!_avctx){
        if(type==VIDEO_DATA)
            android_releaseBuffer(bufObj,buf);
        av_log(_avctx, AV_LOG_VERBOSE, "android_grab_buffer _avctx=NULL'.\n");
        return 0;
    }

    ctx = _avctx->priv_data;

    /*
     * 这里需要进行线程同步，标准生产消费模型
     */
    DEBUG("android_grab_buffer : grab lock");
    pthread_mutex_lock(&ctx->mutex);
    /*
     * 这里必须考虑俘获缓冲区大小，如果读帧数据太慢导致缓冲区积累需要丢弃
     * 这只是简单的控制俘获队列中的缓存区大小
     */
    if( ctx->bufsize > MAX_GRAB_BUFFER_SIZE ){
        if(type==VIDEO_DATA)
            android_releaseBuffer(bufObj,buf);
        pthread_cond_signal(&ctx->cond); //别免空队列无限等待
        pthread_mutex_unlock(&ctx->mutex);
		DEBUG("android_grab_buffer : real-time buffer too full,frame dropped!");
        return 0;
    }

    pktl_next = av_mallocz(sizeof(AVPacketList));
    if(!pktl_next){
        pthread_cond_signal(&ctx->cond); //别免空队列无限等待
        pthread_mutex_unlock(&ctx->mutex);
		DEBUG("android_grab_buffer return : -2");
        return -2;
    }
    pktl_next->pkt.pts = timestramp;
    if(type==VIDEO_DATA){
        /*
         * 这里为了避免避免使用memcpy，采用引用计数法管理缓冲区
         * buf 最后使用android_releaseBuffer归还缓冲区
         */
        pktl_next->pkt.stream_index = ctx->stream_index[VideoDevice];

        /*
         * 创建一个引用计数缓冲区
         */
        pktl_next->pkt.buf = av_buffer_create(buf,buf_size,
                                              android_buffer_release,
                                              bufObj,AV_BUFFER_FLAG_READONLY);
        pktl_next->pkt.data = buf;
        pktl_next->pkt.size = buf_size;
    }else if(type==AUDIO_DATA){
        /*
         * 创建一个新的packet
         */
        if(av_new_packet(&pktl_next->pkt, buf_size) < 0){
            av_free(pktl_next);
            pthread_cond_signal(&ctx->cond); //别免空队列无限等待
            pthread_mutex_unlock(&ctx->mutex);
			DEBUG("android_grab_buffer return : -3");
            return -3;
        }

        pktl_next->pkt.stream_index = ctx->stream_index[AudioDevice];

        memcpy(pktl_next->pkt.data, buf, buf_size);
    }
    /*
     * 加入列表
     */
    for(ppktl = &ctx->pktl ; *ppktl ; ppktl = &(*ppktl)->next);
    *ppktl = pktl_next;

    ctx->bufsize += buf_size;
    pthread_cond_signal(&ctx->cond);
    pthread_mutex_unlock(&ctx->mutex);

	DEBUG("android_grab_buffer return : 0");
    return 0;
}

static enum AVCodecID waveform_codec_id(enum AVSampleFormat sample_fmt)
{
    switch (sample_fmt) {
        case AV_SAMPLE_FMT_U8:  return AV_CODEC_ID_PCM_U8;
        case AV_SAMPLE_FMT_S16: return AV_CODEC_ID_PCM_S16LE;
        case AV_SAMPLE_FMT_S32: return AV_CODEC_ID_PCM_S32LE;
        default:                return AV_CODEC_ID_NONE; /* Should never happen. */
    }
}

static enum AVSampleFormat waveform_format(int width)
{
    switch(width){
        case 8:return AV_SAMPLE_FMT_U8;
        case 16:return AV_SAMPLE_FMT_S16;
        case 32:return AV_SAMPLE_FMT_S32;
    }
    return AV_SAMPLE_FMT_NONE;
}

static enum AVPixelFormat android_pixfmt(int fmt)
{
    switch(fmt){
        case NV21:return AV_PIX_FMT_NV21;
        case NV16:return AV_PIX_FMT_NV16;
        case YUV_420_888:return AV_PIX_FMT_YUV420P;
        case YUV_422_888:return AV_PIX_FMT_YUV422P;
        case YUV_444_888:return AV_PIX_FMT_YUV444P;
        case YV12:return AV_PIX_FMT_NV12;
        case YUY2:return AV_PIX_FMT_YUYV422;
        case FLEX_RGBA_8888:return AV_PIX_FMT_GBRAP;
        case FLEX_RGB_888:return AV_PIX_FMT_GBRP;
        default:return AV_PIX_FMT_NONE;
    }
}

static unsigned int android_pixfmt_tag(int fmt)
{
    switch(fmt){
        case NV21:return MKTAG('N', 'V', '2', '1');
        case NV16:return MKTAG('N', 'V', '1', '6');
        case YUV_420_888:return MKTAG('Y', 'V', '1', '2');
        case YUV_422_888:return MKTAG('Y', '4', '2', 'B');
        case YUV_444_888:return MKTAG('4', '4', '4', 'P');
        case YV12:return MKTAG('N', 'V', '1', '2');
        case YUY2:return MKTAG('y', 'u', 'v', '2');
        case FLEX_RGBA_8888:return 0;
        case FLEX_RGB_888:return MKTAG('G', '3', 00 ,  8 );
        default:return 0;
    }
}

static enum android_ImageFormat android_pixfmt2av(enum AVPixelFormat fmt)
{
    switch(fmt){
        case AV_PIX_FMT_NV21:return NV21;
        case AV_PIX_FMT_NV16:return NV16;
        //case AV_PIX_FMT_YUV420P:return YUV_420_888;
        case AV_PIX_FMT_YUV422P:return YUV_422_888;
        case AV_PIX_FMT_YUV444P:return YUV_444_888;
        case AV_PIX_FMT_YUV420P:return YV12;//宽度16位对齐的YUV420P,stride = ALIGN(width, 16)
        case AV_PIX_FMT_YUYV422:return YUY2;
        case AV_PIX_FMT_GBRAP:return FLEX_RGBA_8888;
        case AV_PIX_FMT_GBRP:return FLEX_RGB_888;
        default:return -1;
    }
}

static int android_pixfmt_prebits(int fmt)
{
    switch(fmt){
        case NV21:12;
        case NV16:16;
        case YUV_420_888:12;
        case YUV_422_888:16;
        case YUV_444_888:24;
        case YV12:return 12;
        case YUY2:return 16;
        case FLEX_RGBA_8888:return 32;
        case FLEX_RGB_888:return 24;
        default:return 0;
    }
}

/*
 * 加入设备
 */
static int add_device(AVFormatContext *avctx,enum androidDeviceType devtype)
{
    int ret = AVERROR(EIO);
    AVCodecParameters *par;
    AVStream *st;

    struct android_camera_ctx *ctx = avctx->priv_data;
    st = avformat_new_stream(avctx, NULL);
    if (!st) {
        ret = AVERROR(ENOMEM);
        return ret;
    }
    st->id = devtype;

    ctx->stream_index[devtype] = st->index;
    par = st->codecpar;
    if(devtype==VideoDevice){
        AVRational time_base;
        int width,height;
        int fmt,fps;
		DEBUG("android_getDemuxerInfo : w:%d,h:%d,fmt:%d,fps:%d");
        if( android_getDemuxerInfo(&width,&height,&fmt,&fps,NULL,NULL,NULL)!=0 ){
            ret = AVERROR(ENOMEM);
            return ret;
        }
		DEBUG("add_device VideoDevice");
        time_base = (AVRational) { 10000000/fps, 10000000 };
        st->avg_frame_rate = av_inv_q(time_base);
        st->r_frame_rate = av_inv_q(time_base);
        par->codec_type = AVMEDIA_TYPE_VIDEO;
        par->width      = width;
        par->height     = height;
        par->format = android_pixfmt(fmt);
        par->codec_tag = android_pixfmt_tag(fmt);
        par->codec_id = AV_CODEC_ID_RAWVIDEO;
        par->bits_per_coded_sample = android_pixfmt_prebits(fmt);
    } else{
		DEBUG("add_device AudioDevice");
        par->codec_type  = AVMEDIA_TYPE_AUDIO;
        par->format      = waveform_format(ctx->sample_size);
        par->codec_id    = waveform_codec_id(par->format);
        par->sample_rate = ctx->sample_rate;
        par->channels    = ctx->channels;
    }

    avpriv_set_pts_info(st, 64, 1, 10000000);

    return 0;
}

static int android_read_header(AVFormatContext *avctx)
{
    int ret = AVERROR(EIO);
    int android_pix_fmt,android_sample_fmt;
    struct android_camera_ctx * ctx;
    ctx = avctx->priv_data;

	debug = ctx->debug;
    if (!ctx->list_devices && !parse_device_name(avctx)) {
        av_log(avctx, AV_LOG_ERROR, "Malformed android_camera input string.\n");
        return ret;
    }
    ctx->video_codec_id = avctx->video_codec_id ? avctx->video_codec_id
                                                : AV_CODEC_ID_RAWVIDEO;
    /*
     *  如果有点格式但是不等于AV_CODEC_ID_RAWVIDEO返回错误
     */
    if (ctx->pixel_format != AV_PIX_FMT_NONE) {
        if (ctx->video_codec_id != AV_CODEC_ID_RAWVIDEO) {
            av_log(avctx, AV_LOG_ERROR, "Pixel format may only be set when "
                    "video codec is not set or set to rawvideo\n");
            ret = AVERROR(EINVAL);
            return ret;
        }
    }
    if (ctx->framerate) {
        ret = av_parse_video_rate(&ctx->requested_framerate, ctx->framerate);
        if (ret < 0) {
            av_log(avctx, AV_LOG_ERROR, "Could not parse framerate '%s'.\n", ctx->framerate);
            return ret;
        }
    }
    /*
     * 枚举设备
     */
    if (ctx->list_devices) {
        av_log(avctx, AV_LOG_INFO, "Android camera devices (some may be both video and audio devices)\n");
        android_cycle_devices(avctx, VideoDevice);
        av_log(avctx, AV_LOG_INFO, "Android audio devices\n");
        android_cycle_devices(avctx, AudioDevice);
        ret = AVERROR_EXIT;
        return ret;
    }
    /*
     * 枚举设备参数
     */
    if (ctx->list_options) {
        if (ctx->device_name[VideoDevice])
        if ((ret = android_list_device_options(avctx, VideoDevice))) {
            return ret;
        }
        if (ctx->device_name[AudioDevice]) {
            if (android_list_device_options(avctx, AudioDevice)) {
                /* show audio options from combined video+audio sources as fallback */
                if ((ret = android_list_device_options(avctx, AudioDevice))) {
                    return ret;
                }
            }
        }
        ret = AVERROR_EXIT;
        return ret;
    }
    /*
     * 打开android摄像头设备和录音设备
     */
    int iDevice = parse_device_id(ctx->device_name[VideoDevice]);
    if(iDevice>=0) {
        android_setDemuxerCallback(android_grab_buffer);
		
		DEBUG("android_read_header android_openDemuxer:oes=%d,dev=%d,w=%d,h=%d,pixfmt=%d,fps=%d,ch=%d,bits=%d,rate=%d",
               ctx->oes_texture, iDevice, ctx->requested_width,
               ctx->requested_height,
               ctx->pixel_format, av_q2d(ctx->requested_framerate),
               ctx->channels, 16, ctx->sample_rate);
			   
        android_pix_fmt = android_pixfmt2av(ctx->pixel_format);
        android_sample_fmt = 16;
        
        int result = android_openDemuxer(ctx->oes_texture, iDevice, ctx->requested_width,
                                         ctx->requested_height,
                                         android_pix_fmt, av_q2d(ctx->requested_framerate),
                                         ctx->channels, android_sample_fmt, ctx->sample_rate);
        if(result){
            av_log(avctx, AV_LOG_ERROR, "android_openDemuxer return %d\n",result);
            return ret;
        }
        if( ctx->oes_texture >= -1 ) {
            ret = add_device(avctx, VideoDevice);
            if (ret < 0) {
                av_log(avctx, AV_LOG_ERROR, "add_device VideoDevice return %d\n", ret);
                return ret;
            }
        }
        if( ctx->channels > 0 ) {
            ret = add_device(avctx, AudioDevice);
            if (ret < 0) {
                av_log(avctx, AV_LOG_ERROR, "add_device AudioDevice return %d\n", ret);
                return ret;
            }
        }
        if(pthread_mutex_init(&ctx->mutex,NULL)){
            av_log(avctx, AV_LOG_ERROR, "pthread_mutex_init non-zero");
            return ret;
        }
        if(pthread_cond_init(&ctx->cond,NULL)){
            av_log(avctx, AV_LOG_ERROR, "pthread_cond_init non-zero");
            return ret;
        }
        /*
         * 一次只能打开一个俘获android设备
         */
        _avctx = avctx;
        ctx->bufsize = 0;
        ret = 0;
    }else{
        av_log(avctx, AV_LOG_ERROR, "android_read_header videoDevice = %s\n",ctx->device_name[VideoDevice]);
    }
	DEBUG("android_read_header return %d",ret);
    return ret;
}

static int android_read_packet(AVFormatContext *s, AVPacket *pkt)
{
    struct android_camera_ctx *ctx = s->priv_data;
    AVPacketList *pktl = NULL;

    /*
     * 俘获线程和读取线程，需要同步处理
     */
	DEBUG("android_read_packet :");
    while (!ctx->eof && !pktl) {
        //WaitForSingleObject(ctx->mutex, INFINITE);
        DEBUG("	android_read_packet lock");
        pthread_mutex_lock(&ctx->mutex);
        DEBUG("	android_read_packet lock in");
        pktl = ctx->pktl;
        if (pktl) {
            *pkt = pktl->pkt;
            ctx->pktl = ctx->pktl->next;
            av_free(pktl);
            ctx->bufsize -= pkt->size;
        }
        //ResetEvent(ctx->event[1]);
        //ReleaseMutex(ctx->mutex);

        if (!pktl) {
            if (_avctx==NULL || android_isClosed()) {
                ctx->eof = 1;
                DEBUG("	android_read_packet eof = 1");
            } else if (s->flags & AVFMT_FLAG_NONBLOCK) {
                DEBUG("	android_read_packet unlock error 0");
                pthread_mutex_unlock(&ctx->mutex);
                DEBUG("	android_read_packet unlock error 1");
                return AVERROR(EAGAIN);
            } else {
                //WaitForMultipleObjects(2, ctx->event, 0, INFINITE);
                DEBUG("	android_read_packet wait 0");
                pthread_cond_wait(&ctx->cond,&ctx->mutex);
                DEBUG("	android_read_packet wait 1");
            }
        }
        DEBUG("	android_read_packet unlock 0");
        pthread_mutex_unlock(&ctx->mutex);
        DEBUG("	android_read_packet unlock 1");
    }

    DEBUG("android_read_packet return ctx->eof = %d",ctx->eof);
    return ctx->eof ? AVERROR(EIO) : pkt->size;
}

static int android_read_close(AVFormatContext *s)
{
    struct android_camera_ctx *ctx = s->priv_data;
    AVPacketList *pktl;

	DEBUG("android_read_close");
    /*
     * 关闭android俘获设备，即使没有打开也可以调用
     */
    android_closeDemuxer();

    /*
     * 只有在成功打开才进行释放
     */
    if(_avctx) {
        _avctx = NULL;
        pthread_mutex_destroy(&ctx->mutex);
        pthread_cond_destroy(&ctx->cond);
    }

    if (ctx->device_name[0])
        av_freep(&ctx->device_name[0]);
    if (ctx->device_name[1])
        av_freep(&ctx->device_name[1]);

    /*
     * 释放缓冲区
     */
    pktl = ctx->pktl;
    while (pktl) {
        AVPacketList *next = pktl->next;
        av_packet_unref(&pktl->pkt);
        av_free(pktl);
        pktl = next;
    }
	DEBUG("android_read_close return");
    return 0;
}

#define OFFSET(x) offsetof(struct android_camera_ctx, x)
#define DEC AV_OPT_FLAG_DECODING_PARAM
static const AVOption options[] = {
        { "video_size", "set video size given a string such as 640x480.", OFFSET(requested_width), AV_OPT_TYPE_IMAGE_SIZE, {.str = NULL}, 0, 0, DEC },
        { "pixel_format", "set video pixel format", OFFSET(pixel_format), AV_OPT_TYPE_PIXEL_FMT, {.i64 = AV_PIX_FMT_NONE}, -1, INT_MAX, DEC },
        { "framerate", "set video frame rate", OFFSET(framerate), AV_OPT_TYPE_STRING, {.str = NULL}, 0, 0, DEC },
        { "oes_texture", "set android preview oes texture", OFFSET(oes_texture), AV_OPT_TYPE_INT, {.i64 = 0}, 0, INT_MAX, DEC },
		{ "debug", "ouput debug info", OFFSET(debug), AV_OPT_TYPE_INT, {.i64 = 0}, 0, INT_MAX, DEC },
        { "sample_format", "set audio sample width 8 or 16,32", OFFSET(sample_format), AV_OPT_TYPE_INT, {.i64 = 0}, 0, INT_MAX, DEC },
        { "sample_rate", "set audio sample rate", OFFSET(sample_rate), AV_OPT_TYPE_INT, {.i64 = 0}, 0, INT_MAX, DEC },
        { "sample_size", "set audio sample size", OFFSET(sample_size), AV_OPT_TYPE_INT, {.i64 = 0}, 0, 16, DEC },
        { "channels", "set number of audio channels, such as 1 or 2", OFFSET(channels), AV_OPT_TYPE_INT, {.i64 = 0}, 0, INT_MAX, DEC },
        { "list_devices", "list available devices",                      OFFSET(list_devices), AV_OPT_TYPE_BOOL, {.i64=0}, 0, 1, DEC },
        { "list_options", "list available options for specified device", OFFSET(list_options), AV_OPT_TYPE_BOOL, {.i64=0}, 0, 1, DEC },
        { NULL },
};

static const AVClass android_class = {
        .class_name = "android indev",
        .item_name  = av_default_item_name,
        .option     = options,
        .version    = LIBAVUTIL_VERSION_INT,
        .category   = AV_CLASS_CATEGORY_DEVICE_VIDEO_INPUT,
};

AVInputFormat ff_android_demuxer = {
        .name           = "android_camera",
        .long_name      = NULL_IF_CONFIG_SMALL("Android camera"),
        .priv_data_size = sizeof(struct android_camera_ctx),
        .read_header    = android_read_header,
        .read_packet    = android_read_packet,
        .read_close     = android_read_close,
        .flags          = AVFMT_NOFILE,
        .priv_class     = &android_class,
};
#endif