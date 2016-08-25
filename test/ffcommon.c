#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavfilter/avfilter.h"
#include "libswresample/swresample.h"
#include "libavutil/channel_layout.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/fifo.h"
#include "libavutil/internal.h"
#include "libavutil/dict.h"
#include "libavutil/pixdesc.h"

#define ERROR_BUFFER_SIZE 1024

struct SwrContext * av_swr_alloc(int in_ch,int in_rate,enum AVSampleFormat in_fmt,
                          int out_ch,int out_rate,enum AVSampleFormat out_fmt)
{
    int ret;

    struct SwrContext * swr = swr_alloc();
    if (!swr) {
        av_log(NULL, AV_LOG_FATAL, "Could not allocate resampler context.\n");
        return NULL;
    }

    /* set options */
    av_opt_set_int(swr, "in_channel_count", in_ch, 0);
    av_opt_set_int(swr, "in_sample_rate", in_rate, 0);
    av_opt_set_sample_fmt(swr, "in_sample_fmt", in_fmt, 0);
    av_opt_set_int(swr, "out_channel_count", out_ch, 0);
    av_opt_set_int(swr, "out_sample_rate", out_rate, 0);
    av_opt_set_sample_fmt(swr, "out_sample_fmt", out_fmt, 0);

    /* initialize the resampling context */
    if ((ret = swr_init(swr)) < 0) {
        av_log(NULL, AV_LOG_FATAL, "Failed to initialize the resampling context\n");
        return NULL;
    }
    return swr;
}

struct SwsContext * av_sws_alloc(int in_w,int in_h,enum AVPixelFormat in_fmt,
                          int out_w,int out_h,enum AVPixelFormat out_fmt)
{
    return sws_getContext(in_w,in_h,in_fmt,out_w,out_h,out_fmt,SWS_BICUBIC, NULL, NULL, NULL);
}

int avcodec_decode_init(AVCodecContext *c,enum AVCodecID codec_id, AVDictionary *opt_arg)
{
    AVCodec * codec;
    AVDictionary *opt = NULL;
    int ret;

    codec = avcodec_find_decoder(codec_id);
    if (!codec)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not find decoder '%s'\n", avcodec_get_name(codec_id));
        return -1;
    }
    /* open it */
    av_dict_copy(&opt, opt_arg, 0);
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
        char errmsg[ERROR_BUFFER_SIZE];
        av_strerror(ret, errmsg, ERROR_BUFFER_SIZE);
        av_log(NULL, AV_LOG_FATAL, "Could not open codec: %s\n", errmsg);
        return -2;
    }
    return 0;
}

int avcodec_encode_init(AVCodecContext *c,enum AVCodecID codec_id, AVDictionary *opt_arg)
{
    AVCodec * codec;
    AVDictionary *opt = NULL;
    int ret;

    codec = avcodec_find_encoder(codec_id);
    if (!codec)
    {
        av_log(NULL, AV_LOG_FATAL, "Could not find encoder '%s'\n", avcodec_get_name(codec_id));
        return -1;
    }

    /* open it */
    av_dict_copy(&opt, opt_arg, 0);
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
        char errmsg[ERROR_BUFFER_SIZE];
        av_strerror(ret, errmsg, ERROR_BUFFER_SIZE);
        av_log(NULL, AV_LOG_FATAL, "Could not open codec: %s\n", errmsg);
        return -2;
    }
    return 0;
}

static int _ff_init = 0;

void av_ff_init()
{
    if( _ff_init )return;

    avdevice_register_all();
    avfilter_register_all();
    av_register_all();
    avformat_network_init();

#ifdef __ANDROID__
    {
        extern AVInputFormat ff_android_demuxer;
        av_register_input_format(&ff_android_demuxer);
    }
#endif
    _ff_init = 1;
}