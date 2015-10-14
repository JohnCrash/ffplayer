#ifndef __FFDEC__H__
#define __FFDEC__H__

#include "ffraw.h"
#include "ffcommon.h"

struct AVDecodeContex
{
	const char * _fileName;
	int _width;
	int _height;
	AVFormatContext * _ctx;
	AVStream *_video_st;
	AVStream * _audio_st;

	AVCtx _vctx;
	AVCtx _actx;

	int has_audio, has_video, encode_audio, encode_video, isopen;
};

/*
 * ����һ�������������ģ�����Ƶ�ļ����н������
 */
AVDecodeContex *ffCreateDecodeContext(
		const char * filename,AVDictionary *opt_arg
	);

void ffCloseDecodeContext(AVDecodeContex *pdc);

/*
 * ȡ����Ƶ��֡�ʣ���ȣ��߶�
 */
int ffGetFrameRate(AVDecodeContex *pdc);
int ffGetFrameWidth(AVDecodeContex *pdc);
int ffGetFrameHeight(AVDecodeContex *pdc);

/*
 * ����Ƶ�ļ���ȡ��һ֡��������ͼ��֡��Ҳ������һ����Ƶ��
 */
AVRaw * ffReadFrame(AVDecodeContex *pdc);

#endif