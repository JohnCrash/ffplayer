#ifndef _FFRAW_H_
#define _FFRAW_H_
#include <inttypes.h>

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
	AVRawType type;
	AVRaw *next;
};

/*
* 分配图像和音频数据
*/
AVRaw *make_image_raw(int format, int w, int h);
AVRaw *make_audio_raw(int format, int channel, int samples);

/*
* raw数据的释放机制使用引用机制
* 引用计数<=0将执行真正的释放操作,make出来的raw数据引用计数=0
*/
int retain_raw(AVRaw * praw);
int release_raw(AVRaw * praw);

#endif