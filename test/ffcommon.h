#ifndef _FF_COMMON_H_
#define _FF_COMMON_H_

#include <mutex>
#include <condition_variable>
#include <thread>

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

#include "libswscale/swscale.h"

# include "libavfilter/avfilter.h"
# include "libavfilter/buffersrc.h"
# include "libavfilter/buffersink.h"

#include "libavutil/timestamp.h"
}

struct AVCtx
{
	AVStream * st;
	AVFrame * frame;

	int64_t next_pts;
	int samples_count;

	SwrContext *swr_ctx;
	SwsContext *sws_ctx;
};

typedef std::mutex mutex_t;
typedef std::condition_variable condition_t;
typedef std::unique_lock<std::mutex> mutex_lock_t;

#endif