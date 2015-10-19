// test.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "ffenc.h"
#include "cap.h"
#include "transcode.h"

namespace ff
{
	int CCLog( char const * fmt,...)
	{
		return 1;
	}
}
static AVRaw * make_video_frame(AVEncodeContext* pec,int linex,int liney,int r,int g,int b)
{
	AVRaw * praw = make_image_raw(AV_PIX_FMT_RGB24, pec->_width, pec->_height);
	/*
		AVRaw * praw = make_image_raw(AV_PIX_FMT_YUV420P, pec->_width, pec->_height);
		memset(praw->data[0], 0, size);
	*/

	memset(praw->data[0],0,praw->size);
	if (praw)
	{
		uint8_t * p = praw->data[0] + praw->linesize[0]*liney;
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
		printf("%.0f%\n", (double)i*100.0 /(double)t);
}

int _tmain(int argc, _TCHAR* argv[])
{
	AVDictionary * opt = NULL;
	int i, j;
	int w, h;
	w = 1024;
	h = 480;

	ffInit();

	/*
	 * 测试转码
	 */
	int ret = ffTranscode("g:\\1.mpg", "g:\\transcode_test_1.mp4", 
		AV_CODEC_ID_MPEG4, 1, 1,1000*1000,
		AV_CODEC_ID_AAC, 24 * 1000, progress);
	if (ret<0)
	{
		printf("ffTranscode error: %s \n",ffLastError());
	}
	/*
	av_dict_set(&opt, "strict", "-2",0); //aac 编码器是实验性质的需要strict -2参数
	av_dict_set(&opt, "threads", "4", 0); //可以启用多线程压缩

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
		int fps = 24;
		int64_t t,dt,dts;
		int count = 0;
		cap->setProperty(cv::CV_CAP_PROP_FRAME_WIDTH,w);
		cap->setProperty(cv::CV_CAP_PROP_FRAME_HEIGHT, h);
		cap->setProperty(cv::CV_CAP_PROP_FPS, fps);

		w = cap->getProperty(cv::CV_CAP_PROP_FRAME_WIDTH);
		h = cap->getProperty(cv::CV_CAP_PROP_FRAME_HEIGHT);
		fps = cap->getProperty(cv::CV_CAP_PROP_FPS);
		AVEncodeContext* pec = ffCreateEncodeContext("g:\\cap_test.mp4",
			w, h, fps, 400000, AV_CODEC_ID_MPEG4,
			SAMPLE_RATE, 64000, AV_CODEC_ID_NONE, opt);

		if (pec)
		{
			t = GetTickCount64();
			
			double dps = 1000.0 / fps;
			for (int i = 0; i < 10 * fps;  i++)
			{
				DWORD st1, st2, st3;
				DWORD st0 = GetTickCount();
				if (cap->grabFrame())
				{
					st1 = GetTickCount(); 
					AVRaw * praw = cap->retrieveFrame(0);
					st2 = GetTickCount();
					if (praw)
						ffAddFrame(pec, praw);
					st3 = GetTickCount();
				}
				printf("grab = %d retrieve = %d,ffAddFrame = %d\n",st1-st0,st2-st1,st3-st2);
				count = 0;
				dts = (i+1)*dps;
				do
				{
					dt = GetTickCount64() - t;
					Sleep(1);
					count++;
				} while (dt < dts);
				printf("%d dt=%d s %I64d\n", count,dt/1000,(dt-dts));
			}
		}
		delete cap;
		ffFlush(pec);
		ffCloseEncodeContext(pec);
	}
	else
	{
		printf("could not fund camera.\n");
	}
	*/



	//音频压缩测试
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
	//视频编码测试
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

	//测试从一个文件读取，然后压缩进入另一个文件
	//read_trancode("g:\\2.mp4", "g:\\test_out.mp4");
	return 0;
}

