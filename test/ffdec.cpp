#include "ffdec.h"

/*
* 打开音频编码器
*/
static int open_audio(AVDecodeCtx *pec, AVCodecID audio_codec_id, AVDictionary *opt_arg)
{
	AVCodecContext *c;
	int nb_samples;
	int ret;
	AVDictionary *opt = NULL;
	AVCodec * codec;

	c = pec->_audio_st->codec;

	codec = avcodec_find_decoder(audio_codec_id);
	if (!codec)
	{
		ffLog("Could not find encoder '%s'\n", avcodec_get_name(audio_codec_id));
		return -1;
	}

	/* open it */
	av_dict_copy(&opt, opt_arg, 0);
	ret = avcodec_open2(c, codec, &opt);
	av_dict_free(&opt);
	if (ret < 0) {
		char errmsg[ERROR_BUFFER_SIZE];
		av_strerror(ret, errmsg, ERROR_BUFFER_SIZE);
		ffLog("Could not open audio codec: %s\n", errmsg);
		return -1;
	}

	if (c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
		nb_samples = 10000;
	else
		nb_samples = c->frame_size;

	pec->_actx.st = pec->_audio_st;

	pec->_actx.frame = alloc_audio_frame(c->sample_fmt, c->channel_layout,
		c->sample_rate, nb_samples);

	if (!pec->_actx.frame)
		return -1;

	/* create resampler context */
	pec->_actx.swr_ctx = swr_alloc();
	if (!pec->_actx.swr_ctx) {
		ffLog("Could not allocate resampler context.\n");
		return -1;
	}

	/* set options */
	av_opt_set_int(pec->_actx.swr_ctx, "in_channel_count", c->channels, 0);
	av_opt_set_int(pec->_actx.swr_ctx, "in_sample_rate", c->sample_rate, 0);
	av_opt_set_sample_fmt(pec->_actx.swr_ctx, "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);
	av_opt_set_int(pec->_actx.swr_ctx, "out_channel_count", c->channels, 0);
	av_opt_set_int(pec->_actx.swr_ctx, "out_sample_rate", c->sample_rate, 0);
	av_opt_set_sample_fmt(pec->_actx.swr_ctx, "out_sample_fmt", c->sample_fmt, 0);

	/* initialize the resampling context */
	if ((ret = swr_init(pec->_actx.swr_ctx)) < 0) {
		ffLog("Failed to initialize the resampling context\n");
		return -1;
	}
	return 0;
}

/*
* 打开视频编码器
*/
static int open_video(AVDecodeCtx *pec, AVCodecID video_codec_id, AVDictionary *opt_arg)
{
	int ret;
	AVCodecContext *c = pec->_video_st->codec;
	AVDictionary *opt = NULL;
	AVCodec *codec;

	codec = avcodec_find_decoder(video_codec_id);
	if (!codec)
	{
		ffLog("Could not find encoder '%s'\n", avcodec_get_name(video_codec_id));
		return -1;
	}

	av_dict_copy(&opt, opt_arg, 0);

	/* open the codec */
	ret = avcodec_open2(c, codec, &opt);
	av_dict_free(&opt);
	if (ret < 0) {
		char errmsg[ERROR_BUFFER_SIZE];
		av_strerror(ret, errmsg, ERROR_BUFFER_SIZE);
		ffLog("Could not open video codec: %s\n", errmsg);
		return -1;
	}

	pec->_vctx.st = pec->_video_st;
	/* allocate and init a re-usable frame */
	pec->_vctx.frame = alloc_picture(c->pix_fmt, c->width, c->height);
	if (!pec->_vctx.frame) {
		ffLog("Could not allocate video frame\n");
		return -1;
	}

	return 0;
}

/*
* 创建一个解码器上下文，对视频文件进行解码操作
*/
AVDecodeCtx *ffCreateDecodeContext(
	const char * filename, AVDictionary *opt_arg
	)
{
	int i,ret;
	AVInputFormat *file_iformat = NULL;
	AVDecodeCtx * pdc;
	AVDictionary * opt = NULL;

	pdc = (AVDecodeCtx *)malloc(sizeof(AVDecodeCtx));
	while (pdc)
	{
		memset(pdc, 0, sizeof(pdc));
		pdc->_fileName = strdup(filename);
		pdc->_ctx = avformat_alloc_context();
		if (!pdc->_ctx)
		{
			ffLog("ffCreateDecodeContext : could not allocate context.\n");
			break;
		}
		
		av_dict_copy(&opt, opt_arg, 0);
		ret = avformat_open_input(&pdc->_ctx, filename, file_iformat, &opt);
		av_dict_free(&opt);
		opt = NULL;
		if (ret < 0)
		{
			char errmsg[ERROR_BUFFER_SIZE];
			av_strerror(ret, errmsg, ERROR_BUFFER_SIZE);
			ffLog("ffCreateDecodeContext %s.\n",errmsg);
			break;
		}
		av_format_inject_global_side_data(pdc->_ctx);
		
		av_dict_copy(&opt, opt_arg, 0);
		ret = avformat_find_stream_info(pdc->_ctx, NULL);
		av_dict_free(&opt);
		opt = NULL;
		if (ret < 0)
		{
			char errmsg[ERROR_BUFFER_SIZE];
			av_strerror(ret, errmsg, ERROR_BUFFER_SIZE);
			ffLog("ffCreateDecodeContext %s.\n", errmsg);
			break;
		}
		/*
		 * 查找视频流和音频流
		 */
		ret = av_find_best_stream(pdc->_ctx, AVMEDIA_TYPE_VIDEO, -1,-1, NULL, 0);
		if (ret >= 0)
		{
			pdc->has_video = 1;
			pdc->_video_st = pdc->_ctx->streams[ret];
			pdc->_video_st_index = ret;
		}
		ret = av_find_best_stream(pdc->_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
		if (ret >= 0)
		{
			pdc->has_audio = 1;
			pdc->_audio_st = pdc->_ctx->streams[ret];
			pdc->_audio_st_index = ret;
		}
		if (pdc->has_video)
		{
			if (open_video(pdc, pdc->_video_st->codec->codec_id, opt_arg) < 0)
			{
				ffCloseDecodeContext(pdc);
				return NULL;
			}
			pdc->encode_video = 1;
		}
		if (pdc->has_audio)
		{
			if (open_audio(pdc, pdc->_audio_st->codec->codec_id, opt_arg) < 0)
			{
				ffCloseDecodeContext(pdc);
				return NULL;
			}
			pdc->encode_audio = 1;
		}

		return pdc;
	}

	/*
	 * 失败清理
	 */
	ffCloseDecodeContext(pdc);
	return NULL;
}

static void ffFreeAVCtx(AVCtx *ctx)
{
	if (ctx->frame)
		av_frame_free(&ctx->frame);
	if (ctx->swr_ctx)
		swr_free(&ctx->swr_ctx);
	if (ctx->sws_ctx)
		sws_freeContext(ctx->sws_ctx);
}

void ffCloseDecodeContext(AVDecodeCtx *pdc)
{
	if (pdc)
	{
		if (pdc->_fileName)
			free((void*)pdc->_fileName);
		if (pdc->_ctx)
		{
			avformat_close_input(&pdc->_ctx);
		}
		
		ffFreeAVCtx(&pdc->_vctx);
		ffFreeAVCtx(&pdc->_actx);

		free(pdc);
	}
}

/*
* 取得视频的帧率，宽度，高度
*/
int ffGetFrameRate(AVDecodeCtx *pdc)
{
	if (pdc && pdc->_video_st&&pdc->_video_st->codec)
	{
		return (int)(pdc->_video_st->r_frame_rate.num / pdc->_video_st->r_frame_rate.den);
	}
	return -1;
}

int ffGetFrameWidth(AVDecodeCtx *pdc)
{
	if (pdc && pdc->_video_st &&pdc->_video_st->codec)
	{
		return pdc->_video_st->codec->width;
	}
	return -1;
}

int ffGetFrameHeight(AVDecodeCtx *pdc)
{
	if (pdc && pdc->_video_st &&pdc->_video_st->codec)
	{
		return pdc->_video_st->codec->height;
	}
	return -1;
}

/*
* 从视频文件中取得一帧，可以是图像帧，也可以是一个音频包
*/
AVRaw * ffReadFrame(AVDecodeCtx *pdc)
{
	int ret, got_frame;
	AVPacket pkt;
	AVCodecContext *ctx;
	AVFrame * frame;
	while (true)
	{
		ret = av_read_frame(pdc->_ctx, &pkt);

		if (ret < 0)
		{
			char errmsg[ERROR_BUFFER_SIZE];
			av_strerror(ret, errmsg, ERROR_BUFFER_SIZE);
			ffLog("ffReadFrame av_read_frame : %s.\n", errmsg);
			return NULL;
		}
		if (pkt.stream_index == pdc->_video_st_index)
		{
			ctx = pdc->_video_st->codec;
			frame = pdc->_vctx.frame;
			ret = avcodec_decode_video2(ctx,frame , &got_frame, &pkt);
			if (got_frame)
			{
				AVRaw * praw = make_image_raw(ctx->pix_fmt,ctx->width,ctx->height);
				av_image_copy(praw->data, praw->linesize, (const uint8_t **)frame->data, frame->linesize, ctx->pix_fmt, ctx->width, ctx->height);
				return praw;
			}
		}
		else if (pkt.stream_index == pdc->_audio_st_index)
		{
			ctx = pdc->_audio_st->codec;
			frame = pdc->_vctx.frame;
			ret = avcodec_decode_audio4(ctx, frame, &got_frame, &pkt);
			if (got_frame)
			{
				AVRaw * praw = make_audio_raw(ctx->sample_fmt, frame->channels, frame->nb_samples);
				av_samples_copy(praw->data, frame->data, 0, 0, frame->nb_samples, frame->channels, ctx->sample_fmt);
				return praw;
			}
		}

		av_free_packet(&pkt);
	}
	return NULL;
}