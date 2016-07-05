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

#define MAX_DEVICE_NAME_LENGTH 256
#define MAX_FORMAT_LENGTH 32
#define MAX_CAPABILITY_COUNT 128

enum AVDeviceType{
	AV_DEVICE_NONE = 0,
	AV_DEVICE_VIDEO = 1,
	AV_DEVICE_AUDIO = 2,
};

union AVDeviceCap{
	struct {
		int min_w, min_h;
		int max_w, max_h;
		double min_fps, max_fps;
		char pix_format[MAX_FORMAT_LENGTH];
		char codec_name[MAX_FORMAT_LENGTH];
	} video;
	struct {
		int min_ch, min_bit, min_rate;
		int max_ch, max_bit, max_rate;
		char sample_format[MAX_FORMAT_LENGTH];
		char codec_name[MAX_FORMAT_LENGTH];
	} audio;
};

struct AVDevice{
	char name[MAX_DEVICE_NAME_LENGTH];
	char alternative_name[MAX_DEVICE_NAME_LENGTH];
	AVDeviceType type;
	AVDeviceCap capability[MAX_CAPABILITY_COUNT];
	int capability_count;
};
/**
 * �г������豸
 * �ɹ������豸������ʧ�ܷ���-1
 */
int ffCapDevicesList(AVDevice *pdevices,int nmax);

/**
 * ����һ���豸��������ͨ���豸����
 */
AVDecodeCtx *ffCreateCapDeviceDecodeContext(
	const char *video_device,
	const char *audio_device);

/*
 * ȡ����Ƶ��֡�ʣ���ȣ��߶�
 */
AVRational ffGetFrameRate(AVDecodeCtx *pdc);
int ffGetFrameWidth(AVDecodeCtx *pdc);
int ffGetFrameHeight(AVDecodeCtx *pdc);

/*
 * ����Ƶ�ļ���ȡ��һ֡��������ͼ��֡��Ҳ������һ����Ƶ��
 */
AVRaw * ffReadFrame(AVDecodeCtx *pdc);

#endif