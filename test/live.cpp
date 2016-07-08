#include "ffcommon.h"
#include "live.h"
#include "ffenc.h"
#include "ffdec.h"

namespace ff
{
#define MAX_NSYN 30
#define MAX_ASYN 1

	static void liveLoop(AVDecodeCtx * pdc,AVEncodeContext * pec,liveCB cb)
	{
		int ret, nsyn = 0;
		AVRational audio_time_base, video_time_base;
		AVRaw * praw = NULL;
		int64_t ctimer; //自然时间
		int64_t nsamples = 0; //音频采样数量
		int64_t nframe = 0; //视频帧数量
		int64_t begin_pts = 0; //第一帧的设备pts
		int64_t stimer = av_gettime_relative(); //自然时间起始点

		if (pdc->has_audio)
			audio_time_base = pdc->_audio_st->codec->time_base;
		if (pdc->has_video)
			video_time_base = pdc->_video_st->codec->time_base;
		while (1){
			ctimer = av_gettime_relative();
			praw = ffReadFrame(pdc);
			
			if (!praw)break;

			if (begin_pts == 0){
				begin_pts = praw->pts;
				stimer = av_gettime_relative();
			}

			if (praw->type == RAW_IMAGE && pdc->has_video){
				if (!pdc->has_audio){
					//如果没有音频设备，视频帧和自然时间保持同步
					int64_t bf = av_rescale_q(ctimer - stimer, AVRational{1,AV_TIME_BASE},video_time_base);
					nsyn = bf - nframe;
					if (abs(nsyn) > MAX_NSYN){
						av_log(NULL, AV_LOG_ERROR, "video frame synchronize error, nsyn > MAX_NSYN , nsyn = %d\n", nsyn);
						break;
					}
				}
				if (nsyn >= 0){
					praw->recount += nsyn;
					nsyn = 0;
					nframe += praw->recount;
					if (ret = ffAddFrame(pec, praw) < 0)
						break;
				}
				else if (nsyn < 0)
				{
					nsyn -= praw->recount;
				}
			}
			else if (praw->type == RAW_AUDIO && pdc->has_audio){
				nsamples += praw->samples;
				if( ret=ffAddFrame(pec, praw) < 0 )
					break;
				/*
				 * 视频帧补偿
				 */
				int64_t at = av_rescale_q(nsamples, audio_time_base, AVRational{ 1, AV_TIME_BASE });
				int64_t vt = av_rescale_q(nframe, video_time_base, AVRational{ 1, AV_TIME_BASE });
				nsyn = (int)av_rescale_q((at - vt), AVRational{ 1, AV_TIME_BASE }, video_time_base);
				nsyn--;
				if (abs(nsyn) > MAX_NSYN){
					av_log(NULL, AV_LOG_ERROR, "video frame synchronize error, nsyn > MAX_NSYN , nsyn = %d\n", nsyn);
					break;
				}
				/*
				 * 自然时间检查
				 */
				double dsyn = (double)abs(ctimer - stimer - at)/(double)AV_TIME_BASE;
				if (dsyn > MAX_ASYN){
					av_log(NULL, AV_LOG_ERROR, "audio frame synchronize error, dsyn > MAX_ASYN , nsyn = %.4f\n", dsyn);
					break;
				}
			}

			release_raw(praw);
		}
		release_raw(praw);
	}

	void liveOnRtmp(
		const char * rtmp_publisher,
		const char * camera_name, int w, int h, int fps,int vbitRate,
		const char * phone_name, int ch, int bit, int rate,int abitRate,
		liveCB cb)
	{
		AVDecodeCtx * pdc = NULL;
		AVEncodeContext * pec = NULL;
		AVDictionary *opt = NULL;
		AVRational afps = AVRational{ fps, 1 };
		AVCodecID vid, aid;
		av_dict_set(&opt, "strict", "-2", 0);
		av_dict_set(&opt, "threads", "4", 0);

		while (1){
			/*
			 * 首先初始化编码器
			 */
			vid = camera_name ? AV_CODEC_ID_H264 : AV_CODEC_ID_NONE;
			aid = phone_name ? AV_CODEC_ID_AAC : AV_CODEC_ID_NONE;
			pec = ffCreateEncodeContext(rtmp_publisher, "flv", w, h, afps, vbitRate, vid,
				rate, abitRate, aid, opt);
			if (!pec)break;
			/*
			 * 初始化解码器
			 */
			pdc = ffCreateCapDeviceDecodeContext(camera_name, w, h, fps, phone_name, ch, bit, rate, opt);
			if (!pdc)break;

			liveLoop(pdc,pec,cb);
			break;
		}

		if (pec){
			ffFlush(pec);
			ffCloseEncodeContext(pec);
		}
		if (pdc){
			ffCloseDecodeContext(pdc);
		}

		av_dict_free(&opt);
	}
}