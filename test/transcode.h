#ifndef _TRANSCODE_H_
#define _TRANSCODE_H_

#include "ffcommon.h"

namespace ff
{
#define AV_CODEC_COPY ((AVCodecID)-1)
#define TRANSCODE_DEFAULT (-1)

	/*
	 * 返回转码进度，total表示总共的帧数,i真正进行的帧数
	 */
	typedef void(*transproc_t)(int64_t total, int64_t i);

	/*
	* 转码操作
	* input 输入文件
	* output 输出文件
	* video_id 输出文件视频编码,AV_CODEC_COPY保持和输入相同,AV_CODEC_ID_NONE 关闭视频编码
	* w,h 输出文件和原来视频的比例，1保持不变,0.5 减半
	* bitRate 视频流的比特率 400*1024 =400kb,-1取默认值
	* audio_id 音频编码，AV_CODEC_COPY保持和输入相同,AV_CODEC_ID_NONE 关闭音频编码
	* audioBitRate 音频编码的码率,-1取默认值
	* 成功返回0,失败返回-1
	*/
	int ffTranscode(const char *input, const char *output, const char *fmt,
		AVCodecID video_id, float w, float h, int bitRate,
		AVCodecID audio_id, int audioBitRate,
		transproc_t progress);
}
#endif