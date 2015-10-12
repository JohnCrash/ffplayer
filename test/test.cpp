// test.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "ffenc.h"

static AVRaw * make_video_frame(AVEncodeContext* pec,int linex,int liney,int r,int g,int b)
{
	AVRaw * praw = (AVRaw*)malloc(sizeof(AVRaw));
	int h = pec->_height;
	praw->type = RAW_IMAGE;
	/*
	praw->format = AV_PIX_FMT_YUV420P;
	praw->width = pec->_width;
	praw->height = pec->_height;
	praw->next = 0;
	for (int i = 0; i < 3;i++ )
		praw->linesize[i] = pec->_vctx.frame->linesize[i];
	int size = praw->linesize[0] * h + praw->linesize[1] * h / 2 + praw->linesize[2] * h / 2;
	praw->data[0] = (uint8_t *)malloc(size);
	praw->data[1] = praw->data[0] + praw->linesize[0] * h;
	praw->data[2] = praw->data[0] + praw->linesize[0] * h + praw->linesize[1] * h / 2;

	memset(praw->data[0], 0, size);
	*/
	praw->format = AV_PIX_FMT_RGB24;
	praw->width = pec->_width;
	praw->height = pec->_height;
	praw->next = 0;
	praw->linesize[0] = ALIGN32(3*praw->width);
	int size = praw->linesize[0] * h;
	praw->data[0] = (uint8_t *)malloc(size);
	memset(praw->data[0], 0, size);
	uint8_t * p;
	p = praw->data[0] + praw->linesize[0] * liney;
	for (int i = 0; i < praw->width; i++)
	{
		*p++ = (uint8_t)r;
		*p++ = (uint8_t)g;
		*p++ = (uint8_t)b;
	}
	for (int i = 0; i < praw->height; i++)
	{
		p = praw->data[0] + praw->linesize[0] *i + 3*linex;
		p[0] = (uint8_t)b;
		p[1] = (uint8_t)g;
		p[2] = (uint8_t)r;
	}
	return praw;
}

static AVRaw * make_audio_frame(AVEncodeContext* pec)
{
	AVRaw * praw = (AVRaw*)malloc(sizeof(AVRaw));
	int h = pec->_height;
	praw->type = RAW_AUDIO;
	praw->format = AV_SAMPLE_FMT_S16;
	praw->channels = pec->_actx.frame->channels;
	praw->samples = pec->_actx.frame->nb_samples;
	int size = praw->samples*praw->channels * 2;
	praw->data[0] = (uint8_t *)malloc(size);
	memset(praw->data[0], 0, size);
	return praw;
}

int _tmain(int argc, _TCHAR* argv[])
{
	AVDictionary * opt = NULL;
	int i, j;
	int w, h;
	w = 1024;
	h = 480;

	ffInit();

	av_dict_set(&opt, "strict", "-2",0); //aac 编码器是实验性质的需要strict -2参数
	av_dict_set(&opt, "threads", "4", 0); //可以启用多线程压缩

	//音频压缩测试
	AVEncodeContext* pec = ffCreateEncodeContext("g:\\test.mp3",
		w, h, 25, 400000, AV_CODEC_ID_NONE,
		44100, 64000, AV_CODEC_ID_MP3, opt);

	if (pec)
	{
		for (i = 0; i < 60; i++)
		{

		}
		ffCloseEncodeContext(pec);
	}
	/*
	//视频编码测试
	AVEncodeContext* pec = ffCreateEncodeContext("g:\\test.mp4",
		w, h, 25, 400000, AV_CODEC_ID_MPEG4,
		44100, 64000, AV_CODEC_ID_NONE, opt);

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
	//		ffAddFrame(pec, make_audio_frame(pec));
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
		}

		ffCloseEncodeContext(pec);
	}

	read_media_file("g:\\test.mp4", "g:\\test_out.mp4");
	*/
	return 0;
}

