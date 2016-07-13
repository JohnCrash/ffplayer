// test.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include "ffdec.h"
#include "ffenc.h"
#include "cap.h"
#include "transcode.h"
#include "live.h"



namespace ff
{
	int CCLog(char const * fmt, ...)
	{
		return 1;
	}

	static AVRaw * make_video_frame(AVEncodeContext* pec, int linex, int liney, int r, int g, int b)
	{
		AVRaw * praw = make_image_raw(AV_PIX_FMT_RGB24, pec->_width, pec->_height);
		/*
			AVRaw * praw = make_image_raw(AV_PIX_FMT_YUV420P, pec->_width, pec->_height);
			memset(praw->data[0], 0, size);
			*/

		memset(praw->data[0], 0, praw->size);
		if (praw)
		{
			uint8_t * p = praw->data[0] + praw->linesize[0] * liney;
			for (int i = 0; i < praw->width; i++)
			{
				*p++ = (uint8_t)r;
				*p++ = (uint8_t)g;
				*p++ = (uint8_t)b;
			}
			for (int i = 0; i < praw->height; i++)
			{
				p = praw->data[0] + praw->linesize[0] * i + 3 * linex;
				p[0] = (uint8_t)b;
				p[1] = (uint8_t)g;
				p[2] = (uint8_t)r;
			}
		}
		return praw;
	}

	//#define SAMPLE_RATE 44100
#define SAMPLE_RATE 22050
	double t = 0;
	double tincr = 2 * M_PI * 110.0 / SAMPLE_RATE;
	double tincr2 = 2 * M_PI * 110.0 / SAMPLE_RATE / SAMPLE_RATE;
	double tincr3 = tincr2 / 10000;

	static AVRaw * make_audio_frame(AVEncodeContext* pec)
	{
		AVRaw * praw = make_audio_raw(AV_SAMPLE_FMT_S16, ffGetAudioChannels(pec), ffGetAudioSamples(pec));

		if (praw)
		{
			memset(praw->data[0], 0, praw->size);
			int16_t * q = (int16_t *)praw->data[0];
			for (int i = 0; i < praw->samples; i++)
			{
				int v = sin(t) * 10000;

				q[2 * i] = v;
				q[2 * i + 1] = v;
				t += tincr;
				tincr += tincr2;
				//	tincr2 -= tincr3;
			}
		}
		return praw;
	}

#include "SDLImp.h"

	static void record_callback(void *pd, uint8_t *stream, int len)
	{

	}

	static void progress(int64_t t, int64_t i)
	{
		if (t)
			printf("%.0f%\n", (double)i*100.0 / (double)t);
	}

	/*
	 * ��cv:CvCapture�в�����ƵȻ���͵�rtmp�����ļ�
	 */
	static void record_test()
	{
		AVDictionary * opt = NULL;
		av_dict_set(&opt, "strict", "-2", 0); //aac ��������ʵ�����ʵ���Ҫstrict -2����
		av_dict_set(&opt, "threads", "4", 0); //�������ö��߳�ѹ��

		ff::AudioSpec audio_cap;
		audio_cap.format = AUDIO_S16SYS;
		audio_cap.silence = 0;
		audio_cap.samples = 1024;
		audio_cap.channels = 2;
		audio_cap.freq = SAMPLE_RATE;
		audio_cap.callback = record_callback;
		audio_cap.userdata = 0;

		//	int ret = ff::OpenAudioDevice(0, 1, &audio_cap, &audio_cap, 1);
		//	if (ret > 0)
		//		ff::PauseAudio(0);

		cv::CvCapture * cap = cv::cvCreateCameraCapture_VFW(0);

		if (cap)
		{
			int w = 720;
			int h = 540;
			int fps = 30;
			int64_t t, dt, dts;
			int count = 0;
			cap->setProperty(cv::CV_CAP_PROP_FRAME_WIDTH, w);
			cap->setProperty(cv::CV_CAP_PROP_FRAME_HEIGHT, h);
			cap->setProperty(cv::CV_CAP_PROP_FPS, fps);

			w = cap->getProperty(cv::CV_CAP_PROP_FRAME_WIDTH);
			h = cap->getProperty(cv::CV_CAP_PROP_FRAME_HEIGHT);
			fps = cap->getProperty(cv::CV_CAP_PROP_FPS);
			AVEncodeContext* pec = ffCreateEncodeContext("rtmp://localhost/myapp/mystream", "flv",
				w, h, { fps, 1 }, 400000, AV_CODEC_ID_H264,
				w,h,AV_PIX_FMT_YUV420P,
				SAMPLE_RATE, 64000, AV_CODEC_ID_NONE,
				2,44100,AV_SAMPLE_FMT_S16,
				opt);
			/*
			AVEncodeContext* pec = ffCreateEncodeContext("g:\\test_video\\vfwcap.mp4", NULL,
			w, h, { fps,1 }, 400000, AV_CODEC_ID_H264,
			SAMPLE_RATE, 64000, AV_CODEC_ID_NONE, opt);
			*/
			//	AVEncodeContext* pec = ffCreateEncodeContext("g:\\test_video\\vfwcap.mp4", NULL,
			//		w, h, { fps, 1 }, 400000, AV_CODEC_ID_H264,
			//		SAMPLE_RATE, 64000, AV_CODEC_ID_NONE, opt);

			if (pec)
			{
				t = GetTickCount64();
				int64_t nframe = 0;
				AVRaw * praw = NULL;
				int nFrameCount = 600 * fps;
				while (nframe < nFrameCount)
				{
					if (cap->grabFrame())
					{
						praw = cap->retrieveFrame(0);
						if (praw){
							dt = GetTickCount64() - t;
							int64_t pn = (int64_t)((dt * fps) / 1000);
							if (pn - nframe > 0){
								praw->recount += (pn - nframe - 1);
							}
							ffAddFrame(pec, praw);
							nframe += praw->recount;
						}
					}

					do
					{
						dt = GetTickCount64() - t;
						Sleep(1);
					} while (dt * fps <= nframe * 1000ll);

					printf("%.4f ,recount = %d\n", (double)(GetTickCount64() - t) / 1000.0, praw->recount);
				}
			}
			delete cap;
			if (pec){
				ffFlush(pec);
				ffCloseEncodeContext(pec);
			}
		}
		else
		{
			printf("could not fund camera.\n");
		}
	}

	/*
	* ���㷨���ڲ��ı�֡�ʵ�ת�룬�����ı���Ƶ�Ĵ�С����ʽ��������
	* ���֡�ʸı����Ҫɾ��֡��������֡��������ƵҲ��Ҫ��Ӧ��ת����
	* ʱ���Ҫ���¼��㣬�����㷨�ĸ��ӶȽ����Ӻܶࡣ
	*/
	int ffTranscode2(const char *input, const char *output, const char *fmt,
		AVCodecID video_id, float width, float height, int bitRate,
		AVCodecID audio_id, int audioBitRate, transproc_t progress)
	{
		AVDecodeCtx * pdc;
		AVEncodeContext * pec;
		AVDictionary *opt = NULL;
		int sampleRate = 44100;
		int ret = 0;
		int w, h;
		int64_t total, i;
		AVDevice caps[8];

		av_dict_set(&opt, "strict", "-2", 0); //aac ��������ʵ�����ʵ���Ҫstrict -2����
		av_dict_set(&opt, "threads", "4", 0); //�������ö��߳�ѹ��

		//pdc = ffCreateDecodeContext(input, opt);
		int count = ffCapDevicesList(caps, 8);
		char * video_name = NULL;
		char * audio_name = NULL;
		for (int m = 0; m < count; m++){
			if (!video_name && caps[m].type == AV_DEVICE_VIDEO){
				video_name = caps[m].alternative_name;
			}
			else if (!audio_name && caps[m].type == AV_DEVICE_AUDIO){
				audio_name = caps[m].alternative_name;
			}
		}
		pdc = ffCreateCapDeviceDecodeContext(video_name, 640, 480, 30,AV_PIX_FMT_YUV420P, audio_name, 2, 16, 44100, opt);
		if (pdc)
		{
			if (!pdc->has_audio)
			{
				audio_id = AV_CODEC_ID_NONE;
			}
			else
			{
				video_id = video_id == AV_CODEC_COPY ? pdc->_video_st->codec->codec_id : video_id;
				sampleRate = pdc->_audio_st->codec->sample_rate;
			}
			if (!pdc->has_video)
			{
				video_id = AV_CODEC_ID_NONE;
			}
			else
			{
				audio_id = audio_id == AV_CODEC_COPY ? pdc->_audio_st->codec->codec_id : audio_id;
			}
			w = (int)(width*ffGetFrameWidth(pdc));
			h = (int)(height*ffGetFrameHeight(pdc));
			w = ALIGN32(w);
			h = ALIGN32(h);
			bitRate = bitRate <= 0 ? 8 * 1024 * 1024 : bitRate;
			audioBitRate = audioBitRate <= 0 ? 64 * 1024 : audioBitRate;

			if (pdc->has_video)
			{
				total = pdc->_video_st->nb_frames;
				if (total <= 0 && pdc->_video_st->time_base.den > 0 && pdc->_video_st->duration != AV_NOPTS_VALUE)
				{
					double sl = (double)(pdc->_video_st->duration*pdc->_video_st->time_base.num) / (double)pdc->_video_st->time_base.den;
					total = (int64_t)(pdc->_video_st->r_frame_rate.num*sl / (double)pdc->_video_st->r_frame_rate.den);
				}
			}
			else if (pdc->has_audio)
			{
				total = pdc->_audio_st->nb_frames;
			}
			else
				total = 0;
			i = 1;
			AVRational fps = ffGetFrameRate(pdc);

			pec = ffCreateEncodeContext(output, fmt, w, h, fps, bitRate, video_id, 
				w,h,AV_PIX_FMT_YUV420P,
				sampleRate, audioBitRate, audio_id,
				2, 44100, AV_SAMPLE_FMT_S16,
				opt);
			if (pec)
			{
				/*
				* �������������Ƶ���������ͱ���������Ƶ�Ĳ�������ͬ����Ҫ�������½��зְ�
				* ���������Ƶ���ݽ��л��壬���ﵽ�������Ĳ�������д�롣
				*/
				AVSampleFormat enc_fmt;
				int enc_channel;
				int enc_samples;
				AVRaw * praw;

				if (pec->has_audio&&pdc->has_audio)
				{
					enc_samples = pec->_actx.frame->nb_samples;
					enc_fmt = (AVSampleFormat)pec->_actx.frame->format;
					enc_channel = pec->_actx.frame->channels;
				}
				else
				{
					enc_fmt = AV_SAMPLE_FMT_NONE;
					enc_channel = 0;
					enc_samples = 0;
				}
				int64_t timer_start = av_gettime_relative();
				int64_t cur_time, dt, nframe, bframe;
				int64_t nsamples = 0;
				int64_t first_video_frame_pts = 0;
				nframe = 0;
				do
				{
					praw = ffReadFrame(pdc);
					cur_time = av_gettime_relative();
					dt = cur_time - timer_start;
					if (praw->type == RAW_IMAGE){
						if (first_video_frame_pts == 0)
							first_video_frame_pts = praw->pts;
						bframe = dt * fps.num / fps.den / 1000000ll;
						printf("*video pts = %I64d , nframe = %I64d , time = %.4fs\n", praw->pts, nframe, (double)nframe * (double)fps.den / (double)fps.num);
						if (bframe - nframe >= 1){
							praw->recount += (bframe - nframe - 1);
							nframe += (bframe - nframe - 1);
						}
						else if (bframe - nframe < 0){
							unsigned int us;
							us = (nframe - bframe) * 1000000ll * fps.den / fps.num;
							av_usleep(us);
						}
						nframe++;
					}
					else if (praw->type == RAW_AUDIO){
						if (first_video_frame_pts == 0){
							continue;
						}
						if (praw->pts < first_video_frame_pts)
							continue;
						printf("*audio pts = %I64d , nsamples = %I64d , time = %.4fs\n", praw->pts, nsamples, (double)nsamples / 44100.0);
						nsamples += praw->samples;
					}

					/*
					* ���ѹ������̫��͵ȴ�
					*/
					while (ffGetBufferSizeKB(pec) > TRANSCODE_MAXBUFFER_SIZE)
					{
						/*
						* ѹ���߳�ֹͣ�����˻��߱������ڵȴ�ι���ݾͲ��ٵȴ�
						*/
						if (pec->_stop_thread || pec->_encode_waiting)
						{
							break;
						}
						std::this_thread::sleep_for(std::chrono::milliseconds(10));
					}
					ret = ffAddFrame(pec, praw);
					if (ret < 0)
						break;
					/*
					* ���Ȼص�,����Ƶ������Ƶ
					*/
					if (progress)
					{
						if (pdc->has_video)
						{
							if (praw->type == RAW_IMAGE)
								progress(total, i++);
						}
						else if (pdc->has_audio)
						{
							if (praw->type == RAW_AUDIO)
								progress(total, i++);
						}
					}
				} while (praw);

				ffFlush(pec);
				ffCloseEncodeContext(pec);
			}
			ffCloseDecodeContext(pdc);
			av_dict_free(&opt);
			return ret;
		}

		av_dict_free(&opt);
		return -1;
	}

	/*
	 * ������Ƶ�ƻ�
	 */
	int ffTranscode3(const char *input, const char *output, const char *fmt,
		AVCodecID video_id, float width, float height, int bitRate,
		AVCodecID audio_id, int audioBitRate, transproc_t progress)
	{
		AVDecodeCtx * pdc;
		AVEncodeContext * pec;
		AVDictionary *opt = NULL;
		int sampleRate = 44100;
		int ret = 0;
		int w, h;
		int64_t total, i;
		AVDevice caps[8];

		av_dict_set(&opt, "strict", "-2", 0);    //aac ��������ʵ�����ʵ���Ҫstrict -2����
		av_dict_set(&opt, "threads", "4", 0); //�������ö��߳�ѹ��

		//pdc = ffCreateDecodeContext(input, opt);
		int count = ffCapDevicesList(caps, 8);
		char * video_name = NULL;
		char * audio_name = NULL;
		for (int m = 0; m < count; m++){
			if (!video_name && caps[m].type == AV_DEVICE_VIDEO){
				video_name = caps[m].alternative_name;
			}
			else if (!audio_name && caps[m].type == AV_DEVICE_AUDIO){
				audio_name = caps[m].alternative_name;
			}
		}
		pdc = ffCreateCapDeviceDecodeContext(video_name, 640, 480, 30, AV_PIX_FMT_YUV420P, audio_name, 2, 16, 44100, opt);
		if (pdc)
		{
			if (!pdc->has_audio)
			{
				audio_id = AV_CODEC_ID_NONE;
			}
			else
			{
				video_id = video_id == AV_CODEC_COPY ? pdc->_video_st->codec->codec_id : video_id;
				sampleRate = pdc->_audio_st->codec->sample_rate;
			}
			if (!pdc->has_video)
			{
				video_id = AV_CODEC_ID_NONE;
			}
			else
			{
				audio_id = audio_id == AV_CODEC_COPY ? pdc->_audio_st->codec->codec_id : audio_id;
			}
			w = (int)(width*ffGetFrameWidth(pdc));
			h = (int)(height*ffGetFrameHeight(pdc));
			w = ALIGN32(w);
			h = ALIGN32(h);
			bitRate = bitRate <= 0 ? 8 * 1024 * 1024 : bitRate;
			audioBitRate = audioBitRate <= 0 ? 64 * 1024 : audioBitRate;

			if (pdc->has_video)
			{
				total = pdc->_video_st->nb_frames;
				if (total <= 0 && pdc->_video_st->time_base.den > 0 && pdc->_video_st->duration != AV_NOPTS_VALUE)
				{
					double sl = (double)(pdc->_video_st->duration*pdc->_video_st->time_base.num) / (double)pdc->_video_st->time_base.den;
					total = (int64_t)(pdc->_video_st->r_frame_rate.num*sl / (double)pdc->_video_st->r_frame_rate.den);
				}
			}
			else if (pdc->has_audio)
			{
				total = pdc->_audio_st->nb_frames;
			}
			else
				total = 0;
			i = 1;
			AVRational fps = ffGetFrameRate(pdc);
			{
				AVRaw * praw;
				int64_t timer_start = av_gettime_relative();
				int64_t cur_time, dt, nframe, bframe;
				int64_t nsamples = 0;
				int64_t first_video_frame_pts = 0;
				nframe = 0;
				do
				{
					praw = ffReadFrame(pdc);
					cur_time = av_gettime_relative();
					dt = cur_time - timer_start;
					if (praw->type == RAW_IMAGE){
						if (first_video_frame_pts == 0)
							first_video_frame_pts = praw->pts;
						bframe = dt * fps.num / fps.den / 1000000ll;
						printf("*video pts = %I64d , nframe = %I64d , time = %.4fs\n", praw->pts, nframe, (double)nframe * (double)fps.den / (double)fps.num);
						if (bframe - nframe >= 1){
							praw->recount += (bframe - nframe - 1);
							nframe += (bframe - nframe - 1);
						}
						else if (bframe - nframe < 0){
							unsigned int us;
							us = (nframe - bframe) * 1000000ll * fps.den / fps.num;
							av_usleep(us);
						}
						nframe++;
					}
					else if (praw->type == RAW_AUDIO){
						if (first_video_frame_pts == 0){
							continue;
						}
						if (praw->pts < first_video_frame_pts)
							continue;
						printf("*audio pts = %I64d , nsamples = %I64d , time = %.4fs\n", praw->pts, nsamples, (double)nsamples / 44100.0);
						nsamples += praw->samples;
					}
				} while (praw);
			}
			ffCloseDecodeContext(pdc);
			av_dict_free(&opt);
			return ret;
		}

		av_dict_free(&opt);
		return -1;
	}

	static void test_cap_from_device()
	{
		int ret = ffTranscode2(
			"video=@device_pnp_\\\\?\\usb#vid_046d&pid_0825&mi_00#7&3458783e&0&0000#{65e8773d-8f56-11d0-a3b9-00a0c9223196}\\global",
			"rtmp://localhost/myapp/mystream", "flv",
			AV_CODEC_ID_H264, 1, 1, 640 * 1024,
			AV_CODEC_ID_AAC, 64 * 1000, progress);
		if (ret < 0)
		{
			av_log(NULL, AV_LOG_FATAL, "ffTranscode error");
		}
	}

}

using namespace ::ff;

static int liveCallback(liveState *pls)
{
//	printf("live state : %d\n", pls->state);
	if (pls->state == LIVE_ERROR){
		for (int i = 0; i < pls->nerror; i++){
			printf(pls->errorMsg[i]);
		}
	}
	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
	AVDevice caps[8];
	ffInit();

	int count = ffCapDevicesList(caps, 8);
	char * video_name = NULL;
	char * audio_name = NULL;
	int vindex = 0;
	if (argc>1 && argv[1]){
		vindex = _wtoi(argv[1]);
	}
	int vi = 0;
	for (int m = 0; m < count; m++){
		if (!video_name && caps[m].type == AV_DEVICE_VIDEO){
			if (vi == vindex){
				video_name = caps[m].alternative_name;
				printf("select : %s\n", caps[m].name);
			}
			vi++;
		}
		else if (!audio_name && caps[m].type == AV_DEVICE_AUDIO){
			audio_name = caps[m].alternative_name;
		}
		if (argc == 1){
			printf("device name : %s \n %s\n", caps[m].name,caps[m].alternative_name);
			
			for (int i = 0; i < caps[m].capability_count; i++){
				if (caps[m].type == AV_DEVICE_VIDEO){
					int min_w = caps[m].capability[i].video.min_w;
					int min_h = caps[m].capability[i].video.min_h;
					int min_fps = caps[m].capability[i].video.min_fps;
					int max_w = caps[m].capability[i].video.max_w;
					int max_h = caps[m].capability[i].video.max_h;
					int max_fps = caps[m].capability[i].video.max_fps;
					char * pix = caps[m].capability[i].video.pix_format;
					char * name = caps[m].capability[i].video.codec_name;
					printf("min w = %d min h = %d min fps = %d "
						"max w = %d max h = %d max fps = %d "
						"fmt = %s\n",
						min_w, min_h, min_fps,
						max_w, max_h, max_fps,
						pix);
				}
				else{
					int min_ch = caps[m].capability[i].audio.min_ch;
					int min_bit = caps[m].capability[i].audio.min_bit;
					int min_rate = caps[m].capability[i].audio.min_rate;
					int max_ch = caps[m].capability[i].audio.max_ch;
					int max_bit = caps[m].capability[i].audio.max_bit;
					int max_rate = caps[m].capability[i].audio.max_rate;
					char * fmt = caps[m].capability[i].audio.sample_format;
					char * name = caps[m].capability[i].audio.codec_name;
					printf("min ch = %d min bit = %d min samples = %d "
						"max ch = %d max bit = %d max samples = %d "
						"fmt = %s\n",
						min_ch, min_bit, min_rate,
						max_ch, max_bit, max_rate,
						fmt);
				}
			}
		}
	}
	char fmt_name[256];
	int w, h, fps;
	w = 640;
	h = 480;
	fps = 30;
	if(argc>2 && argv[2]){
		memset(fmt_name, 0, 255);
		TCHAR * p = argv[2];
		for (int i = 0; i < 254; i++){
			if (p[i] != 0)
				fmt_name[i] = (char)p[i];
		}
	}else{
		strcpy(fmt_name, "yuv420p");
	}
	if(argc>5 && argv[3] && argv[4] && argv[5]){
		w = _wtoi(argv[3]);
		h = _wtoi(argv[4]);
		fps = _wtoi(argv[5]);
	}
	liveOnRtmp("rtmp://192.168.7.157/myapp/mystream",
		video_name,w,h,fps,fmt_name,1024*1024,
		audio_name,22050,"s16",32*1024,
		liveCallback);
#if 0
	AVDictionary * opt = NULL;
	int i, j;
	int w, h;
	w = 1024;
	h = 480;
	AVDevice avds[8];
	ffInit();

	//test_cap_from_device();
	//record_test();
	/*
	 * ����ת��
	 */
	test_cap_from_device();
	//int count = ffCapDevicesList(avds, 8);
	//av_log(NULL, AV_LOG_INFO, "devices %d\n",count);
	//��Ƶѹ������
	/*
	AVEncodeContext* pec = ffCreateEncodeContext("g:\\test.m4a",
		w, h, 25, 400000, AV_CODEC_ID_NONE,
		44100, 64000, AV_CODEC_ID_AAC, opt);

	if (pec)
	{
		for (i = 0; i < 60*43; i++)
		{
			ffAddFrame(pec,make_audio_frame(pec));
		}
		ffCloseEncodeContext(pec);
	}
	*/
//	read_media_file("g:\\test.m4a", "g:\\test_out.m4a");

	/*
	//��Ƶ�������
	AVEncodeContext* pec = ffCreateEncodeContext("g:\\test.mp4",
		w, h, 25, 400000, AV_CODEC_ID_MPEG4,
		SAMPLE_RATE, 64000, AV_CODEC_ID_AAC, opt);

	if (pec)
	{
		int x = 0;
		int y = 0;

		for (i = 0; i < 60; i++)
		{
			for (j = 0; j < 12; j++)
			{
				ffAddFrame(pec, make_video_frame(pec, x,y, 255, 0, 0));
				x++;
				y++;
				if (y>h-1)
					y = 0;
				if (x>w-1)
					x = 0;
			}

			for (j = 0; j < 44;j++)
				ffAddFrame(pec, make_audio_frame(pec));

			for (j = 0; j < 13; j++)
			{
				ffAddFrame(pec, make_video_frame(pec, x,y, 255, 0, 0));
				x++;
				y++;
				if (y>h-1)
					y = 0;
				if (x>w-1)
					x = 0;
			}
			Sleep(100);
		}

		ffFlush(pec);
		ffCloseEncodeContext(pec);
	}
	
	read_media_file("g:\\test.mp4", "g:\\test_out.mp4");
	*/

	//���Դ�һ���ļ���ȡ��Ȼ��ѹ��������һ���ļ�
	//read_trancode("g:\\2.mp4", "g:\\test_out.mp4");
#endif
	return 0;
}

