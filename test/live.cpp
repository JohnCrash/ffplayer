#include "ffcommon.h"
#include "live.h"
#include "ffenc.h"
#include "ffdec.h"

namespace ff
{
#define MAX_NSYN 30
#define MAX_ASYN 2

	static void liveLoop(AVDecodeCtx * pdc,AVEncodeContext * pec,liveCB cb)
	{
		int ret, nsyn,ncsyn,nsynacc;
		AVRational audio_time_base, video_time_base;
		AVRaw * praw = NULL;
		int64_t ctimer; //自然时间
		int64_t nsamples = 0; //音频采样数量
		int64_t nframe = 0; //视频帧数量
		int64_t begin_pts = 0; //第一帧的设备pts
		int64_t nsyndt = 0;
		int64_t nsynbt = 0;
		int64_t stimer = av_gettime_relative(); //自然时间起始点
		nsyn = 0;
		ncsyn = 0;
		nsynacc = 0;
		if (pdc->has_audio)
			audio_time_base = pdc->_audio_st->codec->time_base;
		if (pdc->has_video)
			video_time_base = pdc->_video_st->codec->time_base;
		while (1){
			ctimer = av_gettime_relative();
			praw = ffReadFrame(pdc);
			retain_raw(praw);

			if (!praw)break;

			if (!pdc->has_audio && begin_pts == 0){
				begin_pts = praw->pts;
				stimer = av_gettime_relative();
			}

			if (praw->type == RAW_IMAGE && pdc->has_video){
				if (!pdc->has_audio){
					//如果没有音频设备，视频帧和自然时间保持同步
					int64_t bf = av_rescale_q(ctimer - stimer, AVRational{1,AV_TIME_BASE},video_time_base);
					ncsyn = bf - nframe;
					if (abs(ncsyn) > MAX_NSYN){
						av_log(NULL, AV_LOG_ERROR, "video frame synchronize error, nsyn > MAX_NSYN , nsyn = %d\n", nsyn);
						break;
					}
				}
				else if ( nsyndt>0 && nsyn > 0 && nsynbt!=0 ){
					/*
					 * 平滑补偿,起始时间nsynbt,在nsyndt时间里补偿nsyn帧数据
					 */
					int nsynp = (int)(nsyn*(ctimer - nsynbt) / nsyndt);
					
					ncsyn = nsynp - nsynacc;
					nsynacc += ncsyn;
				}
				else
					ncsyn = 0;

				if (ncsyn >= 0 && ncsyn < MAX_NSYN ){
					av_log(NULL, AV_LOG_INFO, "ncsyn = %d\n",ncsyn );
					praw->recount += ncsyn;
					nframe += praw->recount;
					if (ret = ffAddFrame(pec, praw) < 0)
						break;
				}
				else if (ncsyn > MAX_NSYN){
					av_log(NULL, AV_LOG_ERROR, "video frame make up error, nsyn > MAX_NSYN , ncsyn = %d\n", ncsyn);
					break;
				}
				else{
					//丢弃多余的帧
					av_log(NULL, AV_LOG_INFO, "discard video frame\n");
				}
			}
			else if (praw->type == RAW_AUDIO && pdc->has_audio){
				if (begin_pts == 0){
					begin_pts = praw->pts;
					stimer = av_gettime_relative();
				}
				nsamples += praw->samples;
				if( ret=ffAddFrame(pec, praw) < 0 )
					break;
				/*
				 * 计算视频帧补偿
				 */
				int64_t at = av_rescale_q(nsamples, audio_time_base, AVRational{ 1, AV_TIME_BASE });
				int64_t vt = av_rescale_q(nframe, video_time_base, AVRational{ 1, AV_TIME_BASE });
				nsyndt = av_rescale_q(praw->samples, audio_time_base, AVRational{ 1, AV_TIME_BASE });
				nsyn = (int)av_rescale_q((at - vt), AVRational{ 1, AV_TIME_BASE }, video_time_base);
				nsynbt = ctimer;
				nsynacc = 0;
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
#ifdef _DEBUG
				av_log(NULL, AV_LOG_INFO, "nsyn = %d , dsyn = %.4fs , buffer size = %.4fmb\n", 
					nsyn, dsyn, (double)ffGetBufferSizeKB(pec)/1024.0);
#endif
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