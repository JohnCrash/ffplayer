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
	int _buffer_size; //ԭ�����ݻ������ߴ���kb
	int _nb_raws; //ԭ������֡����
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
 * ������־�������
 */
void ffSetLogHandler( tLogFunc logfunc );

/**
 * ��־���
 */
void ffLog(const char * fmt, ...);

/*
 * ��ʼ��ff��,ע���豸����ʼ���硣
 */
void ffInit();

/**
 * ��������������
 * ����������ļ�������Ƶ�ĳߴ�(w,h)
 * video_codec_id Ҫʹ�õ���Ƶ����id
 * frameRate ��Ƶ֡��
 * videoBitRate ��Ƶ������
 * audio_codec_id Ҫʹ�õ���Ƶ����id
 * sampleRate ��Ƶ������
 * audioBitRate ��Ƶ������
 *
 */
AVEncodeContext* ffCreateEncodeContext( 
	const char* filename,
	int w, int h, int frameRate, int videoBitRate, AVCodecID video_codec_id,
	int sampleRate, int audioBitRate, AVCodecID audio_codec_id,AVDictionary * opt_arg);

/*
 * ������Ƶÿ��֡�Ĳ�������,��ͨ����
 */
int ffGetAudioSamples(AVEncodeContext *pec);

int ffGetAudioChannels(AVEncodeContext *pec);

/*
 * ��ʾ�������ݶ��Ѿ��������
 */
void ffFlush(AVEncodeContext *pec);

/**
 * �رձ���������
 */
void ffCloseEncodeContext( AVEncodeContext *pec);

/**
 * ������Ƶ֡������Ƶ֡
 */
int ffAddFrame(AVEncodeContext *pec,AVRaw *praw);

/*
 * ȡ�����С,��λkb
 */
int ffGetBufferSizeKB(AVEncodeContext *pec);

/*
 * ȡ��������С
 */
int ffGetBufferSize(AVEncodeContext *pec);

#endif