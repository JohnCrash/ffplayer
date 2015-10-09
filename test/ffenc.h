#ifndef __FFENC__H__
#define __FFENC__H__

#include <mutex>
#include <condition_variable>
#include <thread>

#define inline __inline
#include "config.h"
#define HAVE_STRUCT_POLLFD 1
#define snprintf _snprintf
extern "C"
{
	#include "libavformat/avformat.h"
	#include "libavdevice/avdevice.h"
	#include "libswresample/swresample.h"
	#include "libavutil/opt.h"
	#include "libavutil/channel_layout.h"
	#include "libavutil/parseutils.h"
	#include "libavutil/samplefmt.h"
	#include "libavutil/fifo.h"
	#include "libavutil/internal.h"
	#include "libavutil/intreadwrite.h"
	#include "libavutil/dict.h"
	#include "libavutil/mathematics.h"
	#include "libavutil/pixdesc.h"
	#include "libavutil/avstring.h"
	#include "libavutil/libm.h"
	#include "libavutil/imgutils.h"
	#include "libavutil/timestamp.h"
	#include "libavutil/bprint.h"
	#include "libavutil/time.h"
	#include "libavutil/threadmessage.h"
	#include "libavcodec/mathops.h"
	#include "libavformat/os_support.h"

	# include "libavfilter/avfilter.h"
	# include "libavfilter/buffersrc.h"
	# include "libavfilter/buffersink.h"

	#include "libavutil/timestamp.h"
}

int read_media_file(const char *filename, const char *outfile);

#define STREAM_DURATION   10.0
#define STREAM_FRAME_RATE 25 /* 25 images/s */
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */

#define NUM_DATA_POINTERS 3
enum AVRawType
{
	RAW_IMAGE,
	RAW_AUDIO,
};
struct AVRaw
{
	uint8_t *data[NUM_DATA_POINTERS];
	int linesize[NUM_DATA_POINTERS];
	int width;
	int height;
	int channels;
	int samples;
	int format;
	AVRawType type;
	AVRaw *next;
};

struct AVCtx
{
	AVFrame * frame;
	AVFrame * tmp_frame;

	int64_t next_pts;
	int samples_count;

	SwrContext *swr_ctx;
};

struct AVEncodeContext
{
	const char *_fileName;
	int _width;
	int _height;
	AVFormatContext *_ctx;
	AVStream *_video_st;
	AVStream * _audio_st;

	AVCtx _vctx;
	AVCtx _actx;

	int has_audio, has_video, encode_audio, encode_video,isopen;
	int _buffer_size; //原生数据缓冲区尺寸在kb
	int _nb_raws; //原生数据帧个数
	AVRaw * _head;
	AVRaw * _tail;
	std::thread * _encode_thread;
	int _stop_thread;
	std::mutex *_mutex;
	std::condition_variable *_cond;
};

typedef void (*tLogFunc)(char *s);
/**
 * 设置日志输出函数
 */
void ffSetLogHandler( tLogFunc logfunc );

/**
 * 日志输出
 */
void ffLog(const char * fmt, ...);

/*
 * 初始化ff库,注册设备，初始网络。
 */
void ffInit();

/**
 * 创建编码上下文
 * 给出编码的文件名和视频的尺寸(w,h)
 * video_codec_id 要使用的视频编码id
 * frameRate 视频帧率
 * videoBitRate 视频比特率
 * audio_codec_id 要使用的音频编码id
 * sampleRate 音频采样率
 * audioBitRate 音频比特率
 *
 */
AVEncodeContext* ffCreateEncodeContext( 
	const char* filename,
	int w, int h, int frameRate, int videoBitRate, AVCodecID video_codec_id,
	int sampleRate, int audioBitRate, AVCodecID audio_codec_id,AVDictionary * opt_arg);

/**
 * 关闭编码上下文
 */
void ffCloseEncodeContext( AVEncodeContext *pec);

/**
 * 加入音频帧或者视频帧
 */
int ffAddFrame(AVEncodeContext *pec,AVRaw *praw);

/*
 * 创建视频原始数据帧格式为YUV420P
 * pdata必须是一个以pdata[0]起始的通过malloc分配的整块空间。
 */
AVRaw * ffMakeYUV420PRaw(
	uint8_t * pdata[NUM_DATA_POINTERS],
	int linesize[NUM_DATA_POINTERS], 
	int w, int h);

/*
 * 创建音频原始数据帧格式为S16
 */
AVRaw * ffMakeAudioS16Raw(uint8_t * pdata,int chanles,int samples);

/*
 * 释放原生数据
 */
void ffFreeRaw(AVRaw * praw);

/*
 * 取缓冲大小,单位kb
 */
int ffGetBufferSizeKB(AVEncodeContext *pec);

/*
 * 取缓冲区大小
 */
int ffGetBufferSize(AVEncodeContext *pec);

#endif