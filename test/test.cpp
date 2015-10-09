// test.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "ffenc.h"

static AVRaw * make_video_frame(AVEncodeContext* pec)
{
	AVRaw * praw = (AVRaw*)malloc(sizeof(AVRaw));
	int h = pec->_height;
	praw->type = RAW_IMAGE;
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
	return praw;
}

static AVRaw * make_audio_frame(AVEncodeContext* pec)
{
	AVRaw * praw = (AVRaw*)malloc(sizeof(AVRaw));
	int h = pec->_height;
	praw->type = RAW_AUDIO;
	praw->format = AV_SAMPLE_FMT_S16;
	praw->channels = pec->_actx.tmp_frame->channels;
	praw->samples = pec->_actx.tmp_frame->nb_samples;
	int size = praw->samples*praw->channels * 2;
	praw->data[0] = (uint8_t *)malloc(size);
	memset(praw->data[0], 0, size);
	return praw;
}

int _tmain(int argc, _TCHAR* argv[])
{
	AVDictionary * opt = NULL;
	int i, j;
	//read_media_file("g:\\1.m3u8","g:\\test_out.mp4");
	ffInit();

	av_dict_set(&opt, "strict", "-2",0); //aac 编码器是实验性质的需要strict -2参数
	AVEncodeContext* pec = ffCreateEncodeContext("g:\\test.mp4",
		320, 200, 25, 400000, AV_CODEC_ID_MPEG4,
		44100, 64000, AV_CODEC_ID_AAC, opt);

	if (pec)
	{
		/*
		 * 尝试向视频中加入帧
		 */
		for (i = 0; i < 60; i++)
		{
			for (j = 0; j < 12; j++)
				ffAddFrame(pec, make_video_frame(pec));
			ffAddFrame(pec, make_audio_frame(pec));
			for (j = 0; j < 13; j++)
				ffAddFrame(pec, make_video_frame(pec));
		}
		ffCloseEncodeContext(pec);
	}
	return 0;
}

