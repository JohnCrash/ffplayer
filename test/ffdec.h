#ifndef __FFDEC__H__
#define __FFDEC__H__

#include "ffraw.h"
#include "ffcommon.h"

#define TRANSCODE_MAXBUFFER_SIZE 64*1024

struct AVDecodeCtx
{
	const char * _fileName;
	int _width;
	int _height;
	AVFormatContext * _ctx;
	AVStream *_video_st;
	AVStream * _audio_st;
	
	int _video_st_index;
	int _audio_st_index;

	AVCtx _vctx;
	AVCtx _actx;

	int has_audio, has_video, encode_video,encode_audio,isopen;
};

/*
 * ����һ�������������ģ�����Ƶ�ļ����н������
 */
AVDecodeCtx *ffCreateDecodeContext(
		const char * filename,AVDictionary *opt_arg
	);

void ffCloseDecodeContext(AVDecodeCtx *pdc);

/*
 * ȡ����Ƶ��֡�ʣ���ȣ��߶�
 */
int ffGetFrameRate(AVDecodeCtx *pdc);
int ffGetFrameWidth(AVDecodeCtx *pdc);
int ffGetFrameHeight(AVDecodeCtx *pdc);

/*
 * ����Ƶ�ļ���ȡ��һ֡��������ͼ��֡��Ҳ������һ����Ƶ��
 */
AVRaw * ffReadFrame(AVDecodeCtx *pdc);

#endif