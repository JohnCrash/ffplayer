#ifndef __FFENC__H__
#define __FFENC__H__

#define inline __inline
#include "config.h"
#define HAVE_STRUCT_POLLFD 1
#define snprintf _snprintf
extern "C"
{
	#include "libavformat/avformat.h"
	#include "libavdevice/avdevice.h"
	#include "libswresample/swresample.h"
	#include "libavutil/opt.h"
	#include "libavutil/channel_layout.h"
	#include "libavutil/parseutils.h"
	#include "libavutil/samplefmt.h"
	#include "libavutil/fifo.h"
	#include "libavutil/internal.h"
	#include "libavutil/intreadwrite.h"
	#include "libavutil/dict.h"
	#include "libavutil/mathematics.h"
	#include "libavutil/pixdesc.h"
	#include "libavutil/avstring.h"
	#include "libavutil/libm.h"
	#include "libavutil/imgutils.h"
	#include "libavutil/timestamp.h"
	#include "libavutil/bprint.h"
	#include "libavutil/time.h"
	#include "libavutil/threadmessage.h"
	#include "libavcodec/mathops.h"
	#include "libavformat/os_support.h"

	# include "libavfilter/avfilter.h"
	# include "libavfilter/buffersrc.h"
	# include "libavfilter/buffersink.h"

	#include "libavutil/timestamp.h"
}
int read_media_file(const char *filename, const char *outfile);

#define STREAM_DURATION   10.0
#define STREAM_FRAME_RATE 25 /* 25 images/s */
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */

struct AVEncodeContext
{
	const char *_fileName;
	int _width;
	int _height;
	AVFormatContext *_ctx;
	AVStream *_video_st;
	AVStream * _audio_st;
};

typedef void (*tLogFunc)(char *s);
/**
 * 设置日志输出函数
 */
void ffSetLogHanlder( tLogFunc logfunc );

/**
 * 日志输出
 */
void ffLog(const char * fmt, ...);

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
	int sampleRate, int audioBitRate, AVCodecID audio_codec_id);

/**
 * 关闭编码上下文
 */
void ffCloseEncodeContext( AVEncodeContext *pec);

/**
 * 写入视频帧
 */

/**
 * 加入音频帧
 */

#endif