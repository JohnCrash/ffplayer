#ifndef __FFDEPENDS_H__
#define __FFDEPENDS_H__
#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include "SDLImp.h"

#if __cplusplus
extern "C" {
#endif
#include "ffconfig.h"
#include "libavutil/avstring.h"
#include "libavutil/colorspace.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/dict.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/avassert.h"
#include "libavutil/time.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavutil/display.h"
#include "libavutil/eval.h"
#include "libavutil/dict.h"
#include "libavcodec/avfft.h"
#include "libswresample/swresample.h"

#if CONFIG_VDA
#  include "libavcodec/vda.h"
#endif
    
#if CONFIG_VIDEOTOOLBOX
#  include "libavcodec/videotoolbox.h"
#endif
    
#if CONFIG_AVFILTER
# include "libavcodec/avcodec.h"
# include "libavfilter/avfilter.h"
# include "libavfilter/buffersink.h"
# include "libavfilter/buffersrc.h"
#endif

#if __cplusplus
}
#endif

#ifdef WIN32
#define snprintf _snprintf
#endif

#include <assert.h>

namespace ff{
#define MAX_QUEUE_SIZE (15 * 1024 * 1024)
#define MIN_FRAMES 50

	/* Minimum SDL audio buffer size, in samples. */
#define AUDIO_MIN_BUFFER_SIZE 512
	/* Calculate actual buffer size keeping in mind not cause too frequent audio callbacks */
#define AUDIO_MAX_CALLBACKS_PER_SEC 30

	/* no AV sync correction is done if below the minimum AV sync threshold */
#define AV_SYNC_THRESHOLD_MIN 0.04
	/* AV sync correction is done if above the maximum AV sync threshold */
#define AV_SYNC_THRESHOLD_MAX 0.1
	/* If a frame duration is longer than this, it will not be duplicated to compensate AV sync */
#define AV_SYNC_FRAMEDUP_THRESHOLD 0.1
	/* no AV correction is done if too big error */
#define AV_NOSYNC_THRESHOLD 10.0

	/* maximum audio speed change to get correct sync */
#define SAMPLE_CORRECTION_PERCENT_MAX 10

	/* external clock speed adjustment constants for realtime sources based on buffer fullness */
#define EXTERNAL_CLOCK_SPEED_MIN  0.900
#define EXTERNAL_CLOCK_SPEED_MAX  1.010
#define EXTERNAL_CLOCK_SPEED_STEP 0.001

	/* we use about AUDIO_DIFF_AVG_NB A-V differences to make the average */
#define AUDIO_DIFF_AVG_NB   20

	/* polls for possible required screen refresh at least this often, should be less than 1/fps */
#define REFRESH_RATE 0.01

	/* NOTE: the size must be big enough to compensate the hardware audio buffersize size */
	/* TODO: We assume that a decoded and resampled frame fits into this buffer */
#define SAMPLE_ARRAY_SIZE (8 * 65536)

#define CURSOR_HIDE_DELAY 1000000

	static int64_t sws_flags = SWS_BICUBIC;

	struct MyAVPacketList {
		AVPacket pkt;
		struct MyAVPacketList *next;
		int serial;
	};

	struct PacketQueue {
		MyAVPacketList *first_pkt, *last_pkt;
		int nb_packets;
		int size;
		int abort_request;
		int serial;
		int eof;
		mutex_t *mutex;
		cond_t *cond;
	};

#define VIDEO_PICTURE_QUEUE_SIZE 6
#define SUBPICTURE_QUEUE_SIZE 6
#define SAMPLE_QUEUE_SIZE 9
#define FRAME_QUEUE_SIZE FFMAX(SAMPLE_QUEUE_SIZE, FFMAX(VIDEO_PICTURE_QUEUE_SIZE, SUBPICTURE_QUEUE_SIZE))

	struct AudioParams {
		int freq;
		int channels;
		int64_t channel_layout;
		enum AVSampleFormat fmt;
		int frame_size;
		int bytes_per_sec;
	};

	typedef struct Clock {
		double pts;           /* clock base */
		double pts_drift;     /* clock base minus time at which we updated the clock */
		double last_updated;
		double speed;
		int serial;           /* clock is based on a packet with this serial */
		int paused;
		int *queue_serial;    /* pointer to the current packet queue serial, used for obsolete clock detection */
	} Clock;

	/* Common struct for handling all types of decoded data and allocated render buffers. */
	struct Frame {
		AVFrame *frame;
		AVSubtitle sub;
		int serial;
		double pts;           /* presentation timestamp for the frame */
		double duration;      /* estimated duration of the frame */
		int64_t pos;          /* byte position of the frame in the input file */
		int allocated;
		int reallocate;
		int width;
		int height;
		AVRational sar;
	};

	struct FrameQueue {
		Frame queue[FRAME_QUEUE_SIZE];
		int rindex;
		int windex;
		int size;
		int max_size;
		int keep_last;
		int rindex_shown;
		int eof;
		mutex_t *mutex;
		cond_t *cond;
		PacketQueue *pktq;
	};

	enum {
		AV_SYNC_AUDIO_MASTER, /* default choice */
		AV_SYNC_VIDEO_MASTER,
		AV_SYNC_EXTERNAL_CLOCK, /* synchronize to an external clock */
	};

	struct Decoder {
		AVPacket pkt;
		AVPacket pkt_temp;
		PacketQueue *queue;
		AVCodecContext *avctx;
		int pkt_serial;
		int finished;
		int packet_pending;
		cond_t *empty_queue_cond;
		int64_t start_pts;
		AVRational start_pts_tb;
		int64_t next_pts;
		AVRational next_pts_tb;
		thread_t *decoder_tid;
	};

	enum ShowMode {
		SHOW_MODE_NONE = -1, SHOW_MODE_VIDEO = 0, SHOW_MODE_WAVES, SHOW_MODE_RDFT, SHOW_MODE_NB
	};

    enum HWAccelID {
        HWACCEL_NONE = 0,
        HWACCEL_AUTO,
        HWACCEL_VDPAU,
        HWACCEL_DXVA2,
        HWACCEL_VDA,
        HWACCEL_VIDEOTOOLBOX,
        HWACCEL_QSV,
    };
    
    typedef struct HWAccel {
        const char *name;
        int(*init)(AVCodecContext *s);
        enum HWAccelID id;
        enum AVPixelFormat pix_fmt;
    } HWAccel;
    
    typedef struct yuv420p{
        int w,h;
        uint8_t * data[3];
        int linesize[3];
    } yuv420p;
    
	struct VideoState {
		thread_t *read_tid;
		AVInputFormat *iformat;
		int abort_request;
		int force_refresh;
		int paused;
		int last_paused;
		int queue_attachments_req;
		int seek_req;
		int seek_flags;
		int64_t seek_pos;
		int64_t seek_rel;
		int read_pause_return;
		AVFormatContext *ic;
		int realtime;

		Clock audclk;
		Clock vidclk;
		Clock extclk;

		FrameQueue pictq;
		FrameQueue subpq;
		FrameQueue sampq;

		Decoder auddec;
		Decoder viddec;
		Decoder subdec;

		int audio_stream;

		int av_sync_type;

		double audio_clock;
		int audio_clock_serial;
		double audio_diff_cum; /* used for AV difference average computation */
		double audio_diff_avg_coef;
		double audio_diff_threshold;
		int audio_diff_avg_count;
		AVStream *audio_st;
		PacketQueue audioq;
		int audio_hw_buf_size;
		uint8_t silence_buf[AUDIO_MIN_BUFFER_SIZE];
		uint8_t *audio_buf;
		uint8_t *audio_buf1;
		unsigned int audio_buf_size; /* in bytes */
		unsigned int audio_buf1_size;
		int audio_buf_index; /* in bytes */
		int audio_write_buf_size;
		struct AudioParams audio_src;
#if CONFIG_AVFILTER
		struct AudioParams audio_filter_src;
#endif
		struct AudioParams audio_tgt;
		struct SwrContext *swr_ctx;
		int frame_drops_early;
		int frame_drops_late;

		int16_t sample_array[SAMPLE_ARRAY_SIZE];
		int sample_array_index;
		int last_i_start;
		RDFTContext *rdft;
		int rdft_bits;
		FFTSample *rdft_data;
		int xpos;
		double last_vis_time;

		int subtitle_stream;
		AVStream *subtitle_st;
		PacketQueue subtitleq;

		double frame_timer;
		double frame_last_returned_time;
		double frame_last_filter_delay;
		int video_stream;
		AVStream *video_st;
		PacketQueue videoq;
		double max_frame_duration;      // maximum duration of a frame - above this, we consider the jump a timestamp discontinuity
#if !CONFIG_AVFILTER
		struct SwsContext *img_convert_ctx;
#endif
		Rect last_display_rect;
		int eof;

		char filename[1024];
		int width, height, xleft, ytop;
		int step;

#if CONFIG_AVFILTER
		int vfilter_idx;
		AVFilterContext *in_video_filter;   // the first filter in the video chain
		AVFilterContext *out_video_filter;  // the last filter in the video chain
		AVFilterContext *in_audio_filter;   // the first filter in the audio chain
		AVFilterContext *out_audio_filter;  // the last filter in the audio chain
		AVFilterGraph *agraph;              // audio filter graph
#endif

		int last_video_stream, last_audio_stream, last_subtitle_stream;

		cond_t *continue_read_thread;

		ShowMode show_mode;
		/*
		一个RGB缓冲区
		*/
	//	Surface *pscreen;
	//	Surface *pscreen2;
        yuv420p pyuv420p;
        AVFrame * _currentFrame;
        int     isNewFrame;
        int     isVideoOpen;
        
		double current;
		int nMIN_FRAMES;
		const char *errmsg;
		int errcode;
        
#if CONFIG_VIDEOTOOLBOX
        /* hwaccel options */
        enum HWAccelID hwaccel_id;
        char  *hwaccel_device;
        
        /* hwaccel context */
        enum HWAccelID active_hwaccel_id;
        void  *hwaccel_ctx;
        void(*hwaccel_uninit)(AVCodecContext *s);
        int(*hwaccel_get_buffer)(AVCodecContext *s, AVFrame *frame, int flags);
        int(*hwaccel_retrieve_data)(AVCodecContext *s, AVFrame *frame);
        enum AVPixelFormat hwaccel_pix_fmt;
        enum AVPixelFormat hwaccel_retrieved_pix_fmt;
#endif
	};

	/*
	外部函数
	*/
	void initFF(); //初始化ffmpeg，仅需要调用一次
	/*
	打开一个视频文件或者网络文件,成功返回一个视频播放上行文
	*/
	VideoState *stream_open(const char *filename, AVInputFormat *iformat);
	void stream_close(VideoState *is); //停止播放线程，释放所有内存
	void stream_toggle_pause(VideoState *is); //转换播放和暂停
	void toggle_pause(VideoState *is); //同上
	int is_stream_pause(VideoState *is); //判断视频是否被暂停了

	void video_refresh(VideoState *is, double *remaining_time);
    
    uint8_t* yuv420pToRgb(yuv420p * pyuv420p);
    void freeRgb(void *pdata);

	/*
	跳到指定位置播放
	*/
	void seek_chapter(VideoState *is, int incr);
	void stream_seek(VideoState *is, int64_t pos, int64_t rel, int seek_by_bytes);
	void step_to_next_frame(VideoState *is);
	int64_t frame_queue_last_pos(FrameQueue *f);
	int frame_queue_nb_remaining(FrameQueue *f);

	void stream_cycle_channel(VideoState *is, int codec_type);
	void toggle_audio_display(VideoState *is);

	double get_master_clock(VideoState *is);
	/*
	内部函数
	*/
	void refresh_loop_wait_event(VideoState *is, Event *event);
	void do_exit(VideoState *is);
    
    void My_log(void* p,int inval,const char* fmt,...);
}

#endif