#include "ffdec.h"

#define CAP_DEVICE_NAME "dshow"

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
		av_log(NULL,AV_LOG_FATAL,"Could not find encoder '%s'\n", avcodec_get_name(audio_codec_id));
		return -1;
	}

	/* open it */
	av_dict_copy(&opt, opt_arg, 0);
	ret = avcodec_open2(c, codec, &opt);
	av_dict_free(&opt);
	if (ret < 0) {
		char errmsg[ERROR_BUFFER_SIZE];
		av_strerror(ret, errmsg, ERROR_BUFFER_SIZE);
		av_log(NULL,AV_LOG_FATAL,"Could not open audio codec: %s\n", errmsg);
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
		av_log(NULL,AV_LOG_FATAL,"Could not allocate resampler context.\n");
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
		av_log(NULL,AV_LOG_FATAL,"Failed to initialize the resampling context\n");
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
		av_log(NULL,AV_LOG_FATAL,"Could not find encoder '%s'\n", avcodec_get_name(video_codec_id));
		return -1;
	}

	av_dict_copy(&opt, opt_arg, 0);

	/* open the codec */
	ret = avcodec_open2(c, codec, &opt);
	av_dict_free(&opt);
	if (ret < 0) {
		char errmsg[ERROR_BUFFER_SIZE];
		av_strerror(ret, errmsg, ERROR_BUFFER_SIZE);
		av_log(NULL,AV_LOG_FATAL,"Could not open video codec: %s\n", errmsg);
		return -1;
	}

	pec->_vctx.st = pec->_video_st;
	/* allocate and init a re-usable frame */
	pec->_vctx.frame = alloc_picture(c->pix_fmt, c->width, c->height);
	if (!pec->_vctx.frame) {
		av_log(NULL,AV_LOG_FATAL,"Could not allocate video frame\n");
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
		memset(pdc, 0, sizeof(AVDecodeCtx));
		pdc->_fileName = strdup(filename);
		pdc->_ctx = avformat_alloc_context();
		if (!pdc->_ctx)
		{
			av_log(NULL,AV_LOG_FATAL,"ffCreateDecodeContext : could not allocate context.\n");
			break;
		}
		//filename = "video=.." ,open dshow device
		if (filename && strstr(filename, "video=") == filename){
			file_iformat = av_find_input_format("dshow");
			if (!file_iformat){
				av_log(NULL, AV_LOG_FATAL, "Unknown input format: 'dshow'\n");
				break;
			}
		}
		av_dict_copy(&opt, opt_arg, 0);
		ret = avformat_open_input(&pdc->_ctx, filename, file_iformat, &opt);
		av_dict_free(&opt);
		opt = NULL;
		if (ret < 0)
		{
			char errmsg[ERROR_BUFFER_SIZE];
			av_strerror(ret, errmsg, ERROR_BUFFER_SIZE);
			av_log(NULL,AV_LOG_FATAL,"ffCreateDecodeContext %s.\n",errmsg);
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
			av_log(NULL,AV_LOG_FATAL,"ffCreateDecodeContext %s.\n", errmsg);
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
AVRational ffGetFrameRate(AVDecodeCtx *pdc)
{
	if (pdc && pdc->_video_st&&pdc->_video_st->codec)
	{
		return pdc->_video_st->r_frame_rate;
	}
	AVRational ret;
	ret.den = 1;
	ret.num = 1;
	return ret;
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
			av_log(NULL,AV_LOG_FATAL,"ffReadFrame av_read_frame : %s.\n", errmsg);
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
				av_free_packet(&pkt);
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
				av_free_packet(&pkt);
				return praw;
			}
		}

		av_free_packet(&pkt);
	}
	return NULL;
}

int ffCapDevicesList(AVDevice *pdevices, int nmax)
{
	int i,count, ret;
	AVInputFormat *file_iformat = NULL;
	AVDictionary *opt = NULL;
	const char * filename = "dummy";
	AVFormatContext * ctx = NULL;
	char buf[256];
	AVDeviceType type = AV_DEVICE_NONE;
	while (true)
	{
		/*
		 * 初始结构
		 */
		memset(pdevices, 0, nmax*sizeof(AVDevice));

		ctx = avformat_alloc_context();
		if (!ctx)
		{
			av_log(NULL, AV_LOG_FATAL, "ffCreateDecodeContext : could not allocate context.\n");
			break;
		}
		file_iformat = av_find_input_format(CAP_DEVICE_NAME);
		if (!file_iformat){
			av_log(NULL, AV_LOG_FATAL, "Unknown input format: 'dshow'\n");
			break;
		}
		
		/*
		 * 使用日志回调函数来取得俘获设备的信息
		 */
		av_dict_set(&opt, "list_devices", "true", 0);

		i = 0;
		static auto cb = [&](void * acl, int level, const char *format, va_list arg)->void
		{
			if (level == AV_LOG_INFO && i < nmax ){
				if (strstr(format, "DirectShow video devices") == format){
					type = AV_DEVICE_VIDEO;
				}
				else if (strstr(format, "DirectShow audio devices") == format){
					type = AV_DEVICE_AUDIO;
				}
				else if (strstr(format, " \"%s\"\n") == format){
					strcpy(pdevices[i].name, va_arg(arg, char*));
					pdevices[i].type = type;
				}
				else if (strstr(format, "    Alternative name \"%s\"\n") == format){
					strcpy(pdevices[i].alternative_name, va_arg(arg, char*));
					i++;
				}
				else if (strstr(format, "  min s=") == format){
					//min %ldx%ld,%g, max %ldx%ld,fps %g
					int index = pdevices[i].capability_count;
					if (index < MAX_CAPABILITY_COUNT && index >= 0){
						pdevices[i].capability[index].video.min_w = va_arg(arg, int);
						pdevices[i].capability[index].video.min_h = va_arg(arg, int);
						pdevices[i].capability[index].video.min_fps = va_arg(arg, double);
						pdevices[i].capability[index].video.max_w = va_arg(arg, int);
						pdevices[i].capability[index].video.max_h = va_arg(arg, int);
						pdevices[i].capability[index].video.max_fps = va_arg(arg, double);
						pdevices[i].capability_count++;
					}
				}
				else if (strstr(format, "  min ch=") == format){
					//"  min ch=%lu bits=%lu rate=%6lu max ch=%lu bits=%lu rate=%6lu\n"
					int index = pdevices[i].capability_count;
					if (index < MAX_CAPABILITY_COUNT && index >= 0){
						pdevices[i].capability[index].audio.min_ch = va_arg(arg, int);
						pdevices[i].capability[index].audio.min_bit = va_arg(arg, int);
						pdevices[i].capability[index].audio.min_rate = va_arg(arg, int);
						pdevices[i].capability[index].audio.max_ch = va_arg(arg, int);
						pdevices[i].capability[index].audio.max_bit = va_arg(arg, int);
						pdevices[i].capability[index].audio.max_rate = va_arg(arg, int);
						pdevices[i].capability_count++;
					}
				}
				else if (strstr(format, "  pixel_format=") == format){
					int index = pdevices[i].capability_count;
					if (index < MAX_CAPABILITY_COUNT && index >= 0){
						char * pf = va_arg(arg, char*);
						if (strlen(pf) < MAX_FORMAT_LENGTH)
							strcpy(pdevices[i].capability[index].video.pix_format, pf);
						else
							strcpy(pdevices[i].capability[index].video.pix_format, "overflow");
					}
				}
				else if (strstr(format, "  vcodec=") == format){
					//%s
					int index = pdevices[i].capability_count;
					if (index < MAX_CAPABILITY_COUNT && index >= 0){
						char * pf = va_arg(arg, char*);
						if (strlen(pf) < MAX_FORMAT_LENGTH)
							strcpy(pdevices[i].capability[index].video.codec_name, pf);
						else
							strcpy(pdevices[i].capability[index].video.codec_name, "overflow");
					}
				}
				else if (strstr(format, "  unknown compression type") == format){
					//%X
					//printf("vcodec=%X\n", va_arg(arg, int));
				}
				else if (strstr(format, "DirectShow %s device options (from %s devices)\n") == format){
					//printf("DirectShow %s device options (from %s devices)\n", arg);
				}
				else if (strstr(format, " Pin \"%s\" (alternative pin name \"%s\")\n") == format){
					//printf(" Pin \"%s\" (alternative pin name \"%s\")\n", arg);
				}
			}
		};
		av_log_set_callback(
			[](void * acl, int level, const char *format, va_list arg)->void
			{
				cb(acl,level,format,arg);
			}
		);

		ret = avformat_open_input(&ctx, filename, file_iformat, &opt);

		av_dict_set(&opt, "list_devices", "false", 0);
		av_dict_set(&opt, "list_options", "true", 0);
		count = i;
		for (i = 0; i < count; i++){
			char filename[512];
			if (pdevices[i].type==AV_DEVICE_VIDEO)
				snprintf(filename, 512, "video=%s", pdevices[i].alternative_name);
			else if (pdevices[i].type == AV_DEVICE_AUDIO)
				snprintf(filename, 512, "audio=%s", pdevices[i].alternative_name);
			else{
				printf("Unknow device type\n");
				continue;
			}

			ret = avformat_open_input(&ctx, filename, file_iformat, &opt);
		}

		av_log_set_callback(av_log_default_callback);

		av_dict_free(&opt);
		avformat_close_input(&ctx);

		return count;
	}

	if (ctx)
		avformat_close_input(&ctx);
	return -1;
}

AVDecodeCtx *ffCreateCapDeviceDecodeContext(
	const char *video_device, int w, int h, int fps,
	const char *audio_device, int chancel, int bit, int rate, AVDictionary * opt)
{
	char buf[32];
	char filename[2*MAX_DEVICE_NAME_LENGTH];

	if (video_device){
		
		strcpy(filename, "video=");
		strcat(filename, video_device);

		if (audio_device){
			strcat(filename, ":audio=");
			strcat(filename, audio_device);
		}
	}
	else if(audio_device){
		strcat(filename, "audio=");
		strcat(filename, audio_device);
	}
	else return NULL;

	if (video_device){
		snprintf(buf, 32, "%dx%d", w, h);
		av_dict_set(&opt, "video_size", buf, 0);
		snprintf(buf, 32, "%d", fps);
		av_dict_set(&opt, "framerate", buf, 0);
	//俘获缓冲区大小
	//	snprintf(buf, 32, "%d", 8*1024*1024);
	//	av_dict_set(&opt, "rtbufsize", buf, 0);
	}
	if (audio_device){
		snprintf(buf, 32, "%d", chancel);
		av_dict_set(&opt, "channels",buf, 0);
		snprintf(buf, 32, "%d", bit);
		av_dict_set(&opt, "sample_size", buf, 0);
		snprintf(buf, 32, "%d", rate);
		av_dict_set(&opt, "sample_rate", buf, 0);
	}

	return ffCreateDecodeContext(filename,opt);
}