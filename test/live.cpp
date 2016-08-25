#include "ffcommon.h"
#include "live.h"
#include "ffenc.h"
#include "ffdec.h"

namespace ff
{
#define AUDIO_CHANNEL 1
#define AUDIO_CHANNELBIT 16
#define MAX_NSYN 120
#define MAX_ASYN 5
	static const char * preset[] = {
		"placebo",
		"veryslow",
		"slower",
		"slow",
		"medium",
		"fast",
		"faster",
		"veryfast",
		"superfast",
		"ultrafast", //9
	};
	static void liveLoop(AVDecodeCtx * pdc, AVEncodeContext * pec, liveCB cb, liveState* pls)
	{
		int ret, nsyn,ncsyn,nsynacc;
		AVRational audio_time_base, video_time_base;
		AVRaw * praw = NULL;
		int64_t ctimer; //��Ȼʱ��
		int64_t nsamples = 0; //��Ƶ��������
		int64_t nframe = 0; //��Ƶ֡����
		int64_t begin_pts = 0; //��һ֡���豸pts
		int64_t nsyndt = 0;
		int64_t nsynbt = 0;
		int64_t stimer = av_gettime_relative(); //��Ȼʱ����ʼ��
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

			if (!praw)break;

			retain_raw(praw);
			if (!pdc->has_audio && begin_pts == 0){
				begin_pts = praw->pts;
				stimer = av_gettime_relative();
			}

			if (praw->type == RAW_IMAGE && pdc->has_video){
				if (!pdc->has_audio){
					//���û����Ƶ�豸����Ƶ֡����Ȼʱ�䱣��ͬ��
					int64_t bf = av_rescale_q(ctimer - stimer, AVRational{1,AV_TIME_BASE},video_time_base);
					ncsyn = bf - nframe;
					if (abs(ncsyn) > MAX_NSYN){
						av_log(NULL, AV_LOG_ERROR, "video frame synchronize error, nsyn > MAX_NSYN , nsyn = %d\n", nsyn);
						break;
					}
				}
				else if ( nsyndt>0 && nsynbt!=0 ){
					/*
					 * ƽ������,��ʼʱ��nsynbt,��nsyndtʱ���ﲹ��nsyn֡����
					 */
					int nsynp = (int)(nsyn*(ctimer - nsynbt) / nsyndt);
					
					ncsyn = nsynp - nsynacc;
					nsynacc += ncsyn;
				}
				else
					ncsyn = 0;

				if (ncsyn >= 0 && ncsyn < MAX_NSYN ){
				//	av_log(NULL, AV_LOG_INFO, "ncsyn = %d\n",ncsyn );
					praw->recount += ncsyn;
					nframe += praw->recount;
					if (ret = ffAddFrame(pec, praw) < 0)
						break;
#ifdef _DEBUG
		//					av_log(NULL, AV_LOG_INFO, "[V] ncsyn:%d timestrap:%I64d time: %.4fs\n",
		//						ncsyn, praw->pts, (double)(ctimer - stimer) / (double)AV_TIME_BASE);
#endif
				}
				else if (ncsyn > MAX_NSYN){
					av_log(NULL, AV_LOG_ERROR, "video frame make up error, nsyn > MAX_NSYN , ncsyn = %d\n", ncsyn);
					break;
				}
				else{
					//���������֡
		//			av_log(NULL, AV_LOG_INFO, "discard video frame\n");
				}
#ifdef _DEBUG
		//		av_log(NULL, AV_LOG_INFO, "[V] ncsyn:%d timestrap:%I64d time: %.4fs\n",
		//			ncsyn, praw->pts, (double)(ctimer - stimer) / (double)AV_TIME_BASE);
#endif
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
				 * ������Ƶ֡����
				 */
				int64_t at = av_rescale_q(nsamples, audio_time_base, AVRational{ 1, AV_TIME_BASE });
				int64_t vt = av_rescale_q(nframe, video_time_base, AVRational{ 1, AV_TIME_BASE });
				nsyndt = av_rescale_q(praw->samples, audio_time_base, AVRational{ 1, AV_TIME_BASE });
				nsyn = (int)av_rescale_q((at - vt), AVRational{ 1, AV_TIME_BASE }, video_time_base);
				nsynbt = ctimer;
				nsyn += nsynacc;
				if (abs(nsyn) > MAX_NSYN || abs(nsynacc) > MAX_NSYN ){
					av_log(NULL, AV_LOG_ERROR, "video frame synchronize error, nsyn > MAX_NSYN , nsyn = %d nsynacc = %d\n", nsyn, nsynacc);
					break;
				}
				/*
				 * ��Ȼʱ����
				 */
				double dsyn = (double)abs(ctimer - stimer - at)/(double)AV_TIME_BASE;
				if (dsyn > MAX_ASYN){
					av_log(NULL, AV_LOG_ERROR, "audio frame synchronize error, dsyn > MAX_ASYN , nsyn = %.4f\n", dsyn);
					break;
				}
#ifdef _DEBUG
				av_log(NULL, AV_LOG_INFO, "[A] nsyn:%d dsyn:%.4fs acc=%d timestrap:%I64d time: %.4fs bs:%.2fmb\n",
					nsyn, dsyn, nsynacc,praw->pts, (double)(ctimer - stimer) / (double)AV_TIME_BASE, (double)ffGetBufferSizeKB(pec) / 1024.0);
				//av_log(NULL, AV_LOG_INFO, "nsyn:%d , dsyn:%.4fs , buffer size:%.4fmb\n", 
				//	nsyn, dsyn, (double)ffGetBufferSizeKB(pec)/1024.0);
#endif
				nsynacc = 0;
			}
			/*
			 * �ص�
			 */
			if (nframe % 8 == 0){
				if (cb && pls){
					pls->nframes = nframe;
					pls->ntimes = ctimer - stimer;
					if (cb(pls))break;
				}
			}
			release_raw(praw);
		}
		release_raw(praw);
	}

	void liveOnRtmp(
		const char * rtmp_publisher,
		const char * camera_name, int w, int h, int fps, const char * pix_fmt_name, int vbitRate,
		const char * phone_name, int rate, const char * sample_fmt_name, int abitRate,
		liveCB cb)
	{
		AVDecodeCtx * pdc = NULL;
		AVEncodeContext * pec = NULL;
		AVDictionary *opt = NULL;
		AVRational afps = AVRational{ fps, 1 };
		AVCodecID vid, aid;
		AVPixelFormat pixFmt = av_get_pix_fmt(pix_fmt_name);
		AVSampleFormat sampleFmt = av_get_sample_fmt(sample_fmt_name);
		liveState state;

		memset(&state, 0, sizeof(state));
		static auto log_cb = [&](void * acl, int level, const char *format, va_list arg)->void
		{
			if (level == AV_LOG_ERROR || level == AV_LOG_FATAL){
				if (state.nerror<4)
					snprintf(state.errorMsg[state.nerror++], 256, format, arg);
			}
			av_log_default_callback(acl,level,format,arg);
		};

		if (cb){
			state.state = LIVE_BEGIN;
			cb(&state);
		}

		av_log_set_callback(
			[](void * acl, int level, const char *format, va_list arg)->void
				{
					log_cb(acl, level, format, arg);
				}
			);

		av_dict_set(&opt, "strict", "-2", 0);
		av_dict_set(&opt, "threads", "4", 0);

		av_dict_set(&opt, "crf", "22", 0);

		av_dict_set(&opt, "preset", preset[9], 0);

		//av_dict_set(&opt, "tune", "zerolatency", 0);

		//av_dict_set(&opt, "opencl", "true", 0);

		while (1){
			/*
			 * ��ʼ��������
			 */
			pdc = ffCreateCapDeviceDecodeContext(camera_name, w, h, fps, pixFmt, phone_name, AUDIO_CHANNEL, AUDIO_CHANNELBIT, rate, opt);
			if (!pdc || !pdc->_video_st || !pdc->_video_st->codec){
				if (cb){
					state.state = LIVE_ERROR;
					cb(&state);
				}
				break;
			}
			AVCodecContext *ctx = pdc->_video_st->codec;
			/*
			 * ���ȳ�ʼ��������
			 */
			vid = camera_name ? AV_CODEC_ID_H264 : AV_CODEC_ID_NONE;
			aid = phone_name ? AV_CODEC_ID_AAC : AV_CODEC_ID_NONE;
			pec = ffCreateEncodeContext(rtmp_publisher, "flv", w, h, afps, vbitRate, vid,
										ctx->width, ctx->height, ctx->pix_fmt,
										rate, abitRate, aid,
										AUDIO_CHANNEL, rate, sampleFmt,
										opt);
			if (!pec){
				if (cb){
					state.state = LIVE_ERROR;
					cb(&state);
				}
				break;
			}

			liveLoop(pdc,pec,cb,&state);
			break;
		}

		if (pec){
			ffFlush(pec);
			ffCloseEncodeContext(pec);
		}
		if (pdc){
			ffCloseDecodeContext(pdc);
		}

		if (cb){
			if (state.nerror){
				state.state = LIVE_ERROR;
				cb(&state);
			}
			state.state = LIVE_END;
			cb(&state);
		}
		av_log_set_callback(av_log_default_callback);
		av_dict_free(&opt);
	}
}