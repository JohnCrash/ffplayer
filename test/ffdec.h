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
 * 创建一个解码器上下文，对视频文件进行解码操作
 */
AVDecodeContex *ffCreateDecodeContext(
		const char * filename,AVDictionary *opt_arg
	);

void ffCloseDecodeContext(AVDecodeContex *pdc);

/*
 * 取得视频的帧率，宽度，高度
 */
int ffGetFrameRate(AVDecodeContex *pdc);
int ffGetFrameWidth(AVDecodeContex *pdc);
int ffGetFrameHeight(AVDecodeContex *pdc);

/*
 * 从视频文件中取得一帧，可以是图像帧，也可以是一个音频包
 */
AVRaw * ffReadFrame(AVDecodeContex *pdc);

#endif