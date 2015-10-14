#ifndef __FFENC__H__
#define __FFENC__H__

#include "ffraw.h"
#include "ffcommon.h"

int read_media_file(const char *filename, const char *outfile);
int read_trancode(const char *filename, const char *outfile);

#define STREAM_DURATION   10.0
#define STREAM_FRAME_RATE 25 /* 25 images/s */
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */
#define SCALE_FLAGS SWS_BICUBIC
#define ALIGN32(x) FFALIGN(x,32)
#define ALIGN16(x) FFALIGN(x,16)

#define ERROR_BUFFER_SIZE 1024

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
	int _isflush;
	int _buffer_size; //原生数据缓冲区尺寸在kb
	int _nb_raws; //原生数据帧个数
	AVRaw * _video_head;
	AVRaw * _video_tail;
	AVRaw * _audio_head;
	AVRaw * _audio_tail;

	std::thread * _encode_thread;
	int _stop_thread;
	mutex_t *_mutex;
	condition_t *_cond;
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

/*
 * 返回音频每个帧的采样数量,和通道数
 */
int ffGetAudioSamples(AVEncodeContext *pec);

int ffGetAudioChannels(AVEncodeContext *pec);

/*
 * 表示所有数据都已经发送完毕
 */
void ffFlush(AVEncodeContext *pec);

/**
 * 关闭编码上下文
 */
void ffCloseEncodeContext( AVEncodeContext *pec);

/**
 * 加入音频帧或者视频帧
 */
int ffAddFrame(AVEncodeContext *pec,AVRaw *praw);

/*
 * 取缓冲大小,单位kb
 */
int ffGetBufferSizeKB(AVEncodeContext *pec);

/*
 * 取缓冲区大小
 */
int ffGetBufferSize(AVEncodeContext *pec);

#endif