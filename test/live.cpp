#include "ffcommon.h"
#include "live.h"
#include "ffenc.h"
#include "ffdec.h"
#include "cocos2d.h"

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
	
	static const enum AVPixelFormat libx264_fmts[] = {
		AV_PIX_FMT_YUV420P,
		AV_PIX_FMT_YUVJ420P,
		AV_PIX_FMT_YUV422P,
		AV_PIX_FMT_YUVJ422P,
		AV_PIX_FMT_YUV444P,
		AV_PIX_FMT_YUVJ444P,
		AV_PIX_FMT_NV12,
		AV_PIX_FMT_NV16,
		AV_PIX_FMT_NV21
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

			if (!praw){
				av_log(NULL, AV_LOG_ERROR, "liveLoop break : praw = NULL\n");
				break;
			}

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
						av_log(NULL, AV_LOG_ERROR, "liveLoop break : liveLoop video frame synchronize error, nsyn > MAX_NSYN , nsyn = %d\n", nsyn);
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
					if (ret = ffAddFrame(pec, praw) < 0){
						av_log(NULL, AV_LOG_ERROR, "liveLoop break : ret < 0 , ret = %d\n",ret);
						break;
					}
#ifdef _DEBUG
		//					av_log(NULL, AV_LOG_INFO, "[V] ncsyn:%d timestrap:%I64d time: %.4fs\n",
		//						ncsyn, praw->pts, (double)(ctimer - stimer) / (double)AV_TIME_BASE);
#endif
				}
				else if (ncsyn > MAX_NSYN){
					av_log(NULL, AV_LOG_ERROR, "liveLoop break : video frame make up error, nsyn > MAX_NSYN , ncsyn = %d\n", ncsyn);
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
					av_log(NULL, AV_LOG_ERROR, "liveLoop break : video frame synchronize error, nsyn > MAX_NSYN , nsyn = %d nsynacc = %d\n", nsyn, nsynacc);
					if (cb && pls){
						pls->state = LIVE_ERROR;
						cb(pls);
					}
					break;
				}
				/*
				 * ��Ȼʱ����
				 */
				double dsyn = (double)abs(ctimer - stimer - at)/(double)AV_TIME_BASE;
				if (dsyn > MAX_ASYN){
					av_log(NULL, AV_LOG_ERROR, "liveLoop break : audio frame synchronize error, dsyn > MAX_ASYN , nsyn = %.4f\n", dsyn);
					if (cb && pls){
						pls->state = LIVE_ERROR;
						cb(pls);
					}
					break;
				}
#ifdef _DEBUG
				av_log(NULL, AV_LOG_INFO, "[A] nsyn:%d dsyn:%.4fs acc=%d timestrap:%I64d time: %.4fs bs:%.2fmb\n",
					nsyn, dsyn, nsynacc,praw->pts, (double)(ctimer - stimer) / (double)AV_TIME_BASE, (double)ffGetBufferSizeKB(pec) / 1024.0);
#endif
				cocos2d::CCLog("[A] nsyn:%d dsyn:%.4fs acc=%d timestrap:%I64d time: %.4fs bs:%.2fmb\n",
					nsyn, dsyn, nsynacc, praw->pts, (double)(ctimer - stimer) / (double)AV_TIME_BASE, (double)ffGetBufferSizeKB(pec) / 1024.0);

				nsynacc = 0;
			}
			/*
			 * �ص�
			 */
			if (nframe % 2 == 0){
				if (cb && pls){
					pls->nframes = nframe;
					pls->ntimes = ctimer - stimer;
					if (cb(pls)){
						av_log(NULL, AV_LOG_ERROR, "liveLoop break : nframe % 2 == 0 ,nframe = %d\n",nframe);
						break;
					}
				}
			}
			release_raw(praw);
		}
		release_raw(praw);
	}
	static liveState state;
	static void to_cclog(const char *format, va_list arg)
	{
		char szLine[1024 * 8];
		vsprintf(szLine, format, arg);
		cocos2d::CCLog(szLine, "");
	}
	static void log_callback(void * acl, int level, const char *format, va_list arg)
	{
		if (level == AV_LOG_ERROR || level == AV_LOG_FATAL){
			if (state.nerror > MAX_ERRORMSG_COUNT)
				state.nerror = 0;
			snprintf(state.errorMsg[state.nerror++], MAX_ERRORMSG_LENGTH, format, arg);
		}
		av_log_default_callback(acl, level, format, arg);
		to_cclog(format,arg);
	}

	void liveOnRtmp(
		const char * rtmp_publisher,
		const char * camera_name, int w, int h, int fps, const char * pix_fmt_name, int vbitRate,
		const char * phone_name, int rate, const char * sample_fmt_name, int abitRate,
		int ow, int oh, int ofps,
		liveCB cb)
	{
		AVDecodeCtx * pdc = NULL;
		AVEncodeContext * pec = NULL;
		AVDictionary *opt = NULL;
		AVCodecID vid, aid;
		AVPixelFormat pixFmt = pix_fmt_name ? av_get_pix_fmt(pix_fmt_name) : AV_PIX_FMT_NONE;
		AVSampleFormat sampleFmt = sample_fmt_name ? av_get_sample_fmt(sample_fmt_name) : AV_SAMPLE_FMT_NONE;
		
		const char *outFmt;

		cocos2d::CCLog("liveOnRtmp rtmp_publisher:%s\ncamera_name = %s,w=%d h=%d fps=%d,pix_fmt_name=%s,vbitRate=%d,\n\
						phone_name = %s , rate=%d sample_fmt_name=%s abitRate=%d\n\
						ow = %d,oh = %d,ofps = %d",
						rtmp_publisher ? rtmp_publisher:"", camera_name ? camera_name : "", 
						w, h, fps, pix_fmt_name ? pix_fmt_name : "", vbitRate,
						phone_name ? phone_name:"", rate, sample_fmt_name ? sample_fmt_name : "", 
						abitRate,ow,oh,ofps);

		memset(&state, 0, sizeof(state));

		if (cb){
			state.state = LIVE_BEGIN;
			cb(&state);
		}

		ffInit();
		
		av_log_set_callback(log_callback);

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
			 * 直播,FIXME:忽略直播的独立帧率，目前和输入帧率相同
			 */
			vid = camera_name ? AV_CODEC_ID_H264 : AV_CODEC_ID_NONE;
			aid = phone_name ? AV_CODEC_ID_AAC : AV_CODEC_ID_NONE;
			if ((rtmp_publisher[0] == 'R' || rtmp_publisher[0] == 'r') &&
				(rtmp_publisher[1] == 'T' || rtmp_publisher[1] == 't') &&
				(rtmp_publisher[2] == 'M' || rtmp_publisher[2] == 'm') &&
				(rtmp_publisher[3] == 'P' || rtmp_publisher[3] == 'p')){
				outFmt = "flv";
			}
			else{
				outFmt = "mp4";
			}
			pec = ffCreateEncodeContext(rtmp_publisher, outFmt, ow, oh, AVRational{ fps, 1 }, vbitRate, vid,
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

			state.state = LIVE_FRAME;
			liveLoop(pdc,pec,cb,&state);
			break;
		}

		cocos2d::CCLog("=================liveOnRtmp be close==================");
		//马上把俘获设备关闭
		if (pdc){
			ffCloseDecodeContext(pdc);
		}
		//将为发送的数据发送出去然后关闭
		if (pec){
			ffFlush(pec);
			ffCloseEncodeContext(pec);
		}
		//通知回调，直播结束
		if (cb){
			if (state.nerror){
				state.state = LIVE_ERROR;
				cb(&state);
			}
			state.state = LIVE_END;
			cb(&state);
		}
		cocos2d::CCLog("=================liveOnRtmp closed==================");
		av_log_set_callback(av_log_default_callback);
		//cocos2d::CCLog("=================liveOnRtmp free opt==================");
		//av_dict_free(&opt);
		//cocos2d::CCLog("=================liveOnRtmp end==================");
	}
}