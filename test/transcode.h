#ifndef _TRANSCODE_H_
#define _TRANSCODE_H_

#include "ffcommon.h"

namespace ff
{
#define AV_CODEC_COPY ((AVCodecID)-1)
#define TRANSCODE_DEFAULT (-1)

	/*
	 * ����ת����ȣ�total��ʾ�ܹ���֡��,i�������е�֡��
	 */
	typedef void(*transproc_t)(int64_t total, int64_t i);

	/*
	* ת�����
	* input �����ļ�
	* output ����ļ�
	* video_id ����ļ���Ƶ����,AV_CODEC_COPY���ֺ�������ͬ,AV_CODEC_ID_NONE �ر���Ƶ����
	* w,h ����ļ���ԭ����Ƶ�ı�����1���ֲ���,0.5 ����
	* bitRate ��Ƶ���ı����� 400*1024 =400kb,-1ȡĬ��ֵ
	* audio_id ��Ƶ���룬AV_CODEC_COPY���ֺ�������ͬ,AV_CODEC_ID_NONE �ر���Ƶ����
	* audioBitRate ��Ƶ���������,-1ȡĬ��ֵ
	* �ɹ�����0,ʧ�ܷ���-1
	*/
	int ffTranscode(const char *input, const char *output, const char *fmt,
		AVCodecID video_id, float w, float h, int bitRate,
		AVCodecID audio_id, int audioBitRate,
		transproc_t progress);
}
#endif