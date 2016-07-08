#ifndef _FFRAW_H_
#define _FFRAW_H_

#include <inttypes.h>
#include "ffcommon.h"

namespace ff
{
#define NUM_DATA_POINTERS 8
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
		int ref;
		int size;
		int seek_sample;
		int recount;
		int64_t pts;
		AVRational time_base;
		AVRawType type;
		AVRaw *next;
	};

	/*
	* ����ͼ�����Ƶ����
	*/
	AVRaw *make_image_raw(int format, int w, int h);
	AVRaw *make_audio_raw(int format, int channel, int samples);

	/*
	* raw���ݵ��ͷŻ���ʹ�����û���
	* ���ü���<=0��ִ���������ͷŲ���,make������raw�������ü���=0
	*/
	int retain_raw(AVRaw * praw);
	int release_raw(AVRaw * praw);

	void list_push_raw(AVRaw ** head, AVRaw ** tail, AVRaw *praw);
	AVRaw * list_pop_raw(AVRaw ** head, AVRaw **tail);
}
#endif