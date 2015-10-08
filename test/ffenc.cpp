#include "ffenc.h"

/**
* 设置日志输出函数
*/
static tLogFunc _gLogFunc = NULL;
void ffSetLogHandler(tLogFunc logfunc)
{
	_gLogFunc = logfunc;
}

/**
* 日志输出
*/
void ffLog(const char * fmt,...)
{
	char buf[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, 1024 - 3, fmt, args);
	va_end(args);
	if(_gLogFunc)
		_gLogFunc(buf);
}

/*
 * 向AVFormatContext加入新的流
 */
static int add_stream(AVEncodeContext *pec, AVCodecID codec_id,
	int w,int h,int stream_frame_rate,int stream_bit_rate)
{
	AVCodec *codec;
	AVStream *st;
	AVCodecContext *c;
	int i;

	codec = avcodec_find_encoder(codec_id);
	if (!codec)
	{
		ffLog("Could not find encoder '%s'\n", avcodec_get_name(codec_id));
		return -1;
	}

	st = avformat_new_stream(pec->_ctx, codec);
	if (!st)
	{
		ffLog("Could not allocate stream\n");
		return -1;
	}
	st->id = pec->_ctx->nb_streams - 1;
	c = st->codec;

	switch (codec->type)
	{
	case AVMEDIA_TYPE_AUDIO:
		pec->_audio_st = st;

		c->sample_fmt = codec->sample_fmts ?
			codec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
		c->bit_rate = stream_bit_rate;
		c->sample_rate = stream_frame_rate;
		if (codec->supported_samplerates) {
			c->sample_rate = codec->supported_samplerates[0];
			for (i = 0; codec->supported_samplerates[i]; i++) {
				if (codec->supported_samplerates[i] == stream_frame_rate)
					c->sample_rate = stream_frame_rate;
			}
		}
		c->channels = av_get_channel_layout_nb_channels(c->channel_layout);
		c->channel_layout = AV_CH_LAYOUT_STEREO;
		if (codec->channel_layouts) {
			c->channel_layout = codec->channel_layouts[0];
			for (i = 0; codec->channel_layouts[i]; i++) {
				if (codec->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
					c->channel_layout = AV_CH_LAYOUT_STEREO;
			}
		}
		c->channels = av_get_channel_layout_nb_channels(c->channel_layout);
		st->time_base.den = 1;
		st->time_base.num = c->sample_rate;
		break;
	case AVMEDIA_TYPE_VIDEO:
		pec->_video_st = st;

		c->codec_id = codec_id;
		c->bit_rate = stream_bit_rate;
		/* 分辨率必须是2的倍数，这里需要作检查 */
		c->width = w;
		c->height = h;
		st->time_base.den = 1;
		st->time_base.num = stream_frame_rate;
		c->time_base = st->time_base;

		c->gop_size = 12; /* emit one intra frame every twelve frames at most */
		c->pix_fmt = STREAM_PIX_FMT;
		if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
			/* just for testing, we also add B frames */
			c->max_b_frames = 2;
		}
		if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
			/* Needed to avoid using macroblocks in which some coeffs overflow.
			* This does not happen with normal video, it just happens here as
			* the motion of the chroma plane does not match the luma plane. */
			c->mb_decision = 2;
		}
		break;
	default:
		ffLog("Unknow stream type\n");
		return -1;
	}

	/* Some formats want stream headers to be separate. */
	if (pec->_ctx->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	return 0;
}

/*
 * 关闭流
 */
static void close_stream(AVStream *st)
{
	avcodec_close(st->codec);
	/*
		av_frame_free(&ost->frame);
		av_frame_free(&ost->tmp_frame);
		sws_freeContext(ost->sws_ctx);
		swr_free(&ost->swr_ctx);
	*/
}

static AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
{
	AVFrame *picture;
	int ret;

	picture = av_frame_alloc();
	if (!picture)
		return NULL;

	picture->format = pix_fmt;
	picture->width = width;
	picture->height = height;

	/* allocate the buffers for the frame data */
	ret = av_frame_get_buffer(picture, 32);
	if (ret < 0) {
		ffLog("Could not allocate frame data.\n");
		return NULL;
	}

	return picture;
}

/*
 * 打开视频编码器
 */
static int open_video(AVEncodeContext *pec, AVCodecID video_codec_id,AVDictionary *opt_arg)
{
	int ret;
	AVCodecContext *c = pec->_video_st->codec;
	AVDictionary *opt = NULL;
	AVCodec *codec;

	codec = avcodec_find_encoder(video_codec_id);
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
		char errmsg[1024];
		av_strerror(ret, errmsg, 1024);
		ffLog("Could not open video codec: %s\n",errmsg);
		return -1;
	}

	/* allocate and init a re-usable frame */
	pec->_video_frame = alloc_picture(c->pix_fmt, c->width, c->height);
	if (!pec->_video_frame) {
		ffLog("Could not allocate video frame\n");
		return -1;
	}

	/* If the output format is not YUV420P, then a temporary YUV420P
	* picture is needed too. It is then converted to the required
	* output format. */
	pec->_video_tmp_frame = NULL;
	if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
		pec->_video_tmp_frame = alloc_picture(AV_PIX_FMT_YUV420P, c->width, c->height);
		if (!pec->_video_tmp_frame) {
			ffLog("Could not allocate temporary picture\n");
			return -1;
		}
	}
	return 0;
}

/*
 * 分配音频帧
 */
static AVFrame *alloc_audio_frame(enum AVSampleFormat sample_fmt,
	uint64_t channel_layout,
	int sample_rate, int nb_samples)
{
	AVFrame *frame = av_frame_alloc();
	int ret;

	if (!frame) {
		ffLog("Error allocating an audio frame\n");
		return NULL;
	}

	frame->format = sample_fmt;
	frame->channel_layout = channel_layout;
	frame->sample_rate = sample_rate;
	frame->nb_samples = nb_samples;

	if (nb_samples) {
		ret = av_frame_get_buffer(frame, 0);
		if (ret < 0) {
			ffLog("Error allocating an audio buffer\n");
			return NULL;
		}
	}

	return frame;
}

/*
 * 打开音频编码器
 */
static int open_audio(AVEncodeContext *pec, AVCodecID audio_codec_id, AVDictionary *opt_arg)
{
	AVCodecContext *c;
	int nb_samples;
	int ret;
	AVDictionary *opt = NULL;
	AVCodec * codec;

	c = pec->_audio_st->codec;

	codec = avcodec_find_encoder(audio_codec_id);
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
		char errmsg[1024];
		av_strerror(ret, errmsg, 1024);
		ffLog("Could not open audio codec: %s\n", errmsg);
		return -1;
	}

	/* init signal generator */
	pec->_audio_t = 0;
	pec->_audio_tincr = 2 * M_PI * 110.0 / c->sample_rate;
	/* increment frequency by 110 Hz per second */
	pec->_audio_tincr2 = 2 * M_PI * 110.0 / c->sample_rate / c->sample_rate;

	if (c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
		nb_samples = 10000;
	else
		nb_samples = c->frame_size;

	pec->_audio_frame = alloc_audio_frame(c->sample_fmt, c->channel_layout,
		c->sample_rate, nb_samples);
	pec->_audio_tmp_frame = alloc_audio_frame(AV_SAMPLE_FMT_S16, c->channel_layout,
		c->sample_rate, nb_samples);

	/* create resampler context */
	pec->_audio_swr_ctx = swr_alloc();
	if (!pec->_audio_swr_ctx) {
		ffLog("Could not allocate resampler context\n");
		return -1;
	}

	/* set options */
	av_opt_set_int(pec->_audio_swr_ctx, "in_channel_count", c->channels, 0);
	av_opt_set_int(pec->_audio_swr_ctx, "in_sample_rate", c->sample_rate, 0);
	av_opt_set_sample_fmt(pec->_audio_swr_ctx, "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);
	av_opt_set_int(pec->_audio_swr_ctx, "out_channel_count", c->channels, 0);
	av_opt_set_int(pec->_audio_swr_ctx, "out_sample_rate", c->sample_rate, 0);
	av_opt_set_sample_fmt(pec->_audio_swr_ctx, "out_sample_fmt", c->sample_fmt, 0);

	/* initialize the resampling context */
	if ((ret = swr_init(pec->_audio_swr_ctx)) < 0) {
		ffLog("Failed to initialize the resampling context\n");
		return -1;
	}
	return 0;
}

static AVRaw * ffPopFrame(AVEncodeContext * pec)
{
	std::unique_lock<std::mutex> lock(*pec->_mutex);
	AVRaw * praw = NULL;
	/*
	 * 如果队列空就等待，当处于等待状态时_mutex被临时解锁
	 */
	if (!pec->_head)
	{
		pec->_cond->wait(lock);
	}
	if (pec->_head)
	{
		praw = pec->_head;
		pec->_head = praw->next;
		if (praw == pec->_tail)
		{
			pec->_tail = pec->_head;
		}
	}
	return praw;
}

int encode_thread_proc(AVEncodeContext * pec)
{
	while (!pec->_stop_thread)
	{
		AVRaw * praw = ffPopFrame(pec);
		/*
		 * 压缩原生数据并写入到文件中
		 */
		ffFreeRaw(praw);
	}
	return 0;
}

/**
 * 创建编码上下文
 */
AVEncodeContext* ffCreateEncodeContext(const char* filename, 
	int w, int h, int frameRate, int videoBitRate,AVCodecID video_codec_id,
	int sampleRate, int audioBitRate, AVCodecID audio_codec_id, AVDictionary * opt_arg)
{
	AVEncodeContext * pec;
	AVFormatContext *ofmt_ctx;
	AVOutputFormat *ofmt;
	int ret;

	pec = (AVEncodeContext *)malloc(sizeof(AVEncodeContext));
	if (!pec)
	{
		ffLog("ffCreateEncodeContext malloc return nullptr\n");
		return pec;
	}
	memset(pec, 0, sizeof(AVEncodeContext));
	pec->_width = w;
	pec->_height = h;
	pec->_fileName = strdup(filename);
	/*
	 * 创建输出上下文
	 */
	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, filename);
	if (!ofmt_ctx){
		ffLog("avformat_alloc_output_context2 return failed.");
		ffCloseEncodeContext(pec);
		return NULL;
	}
	pec->_ctx = ofmt_ctx;
	/*
	 * 加入视频流和音频流
	 */
	if (AV_CODEC_ID_NONE != video_codec_id)
	{
		if (add_stream(pec, video_codec_id, w, h, frameRate, videoBitRate) < 0)
		{
			ffLog("Add video stream failed\n");
			ffCloseEncodeContext(pec);
			return NULL;
		}
		pec->has_video = 1;
	}
	if (AV_CODEC_ID_NONE != audio_codec_id)
	{
		if (add_stream(pec, audio_codec_id, 0, 0, sampleRate, audioBitRate) < 0)
		{
			ffLog("Add audio stream failed\n");
			ffCloseEncodeContext(pec);
			return NULL;
		}
		pec->has_audio = 1;
	}
	/*
	 * 打开视频编码器和音频编码器
	 */
	if (pec->has_video)
	{
		if (open_video(pec, video_codec_id, opt_arg) < 0)
		{
			ffCloseEncodeContext(pec);
			return NULL;
		}
		pec->encode_video = 1;
	}
	if (pec->has_audio)
	{
		if (open_audio(pec, audio_codec_id, opt_arg) < 0)
		{
			ffCloseEncodeContext(pec);
			return NULL;
		}
		pec->encode_audio = 1;
	}
	/*
	 * 如果有必要打开一个文件输出流
	 */
	ofmt = ofmt_ctx->oformat;
	if (!(ofmt->flags & AVFMT_NOFILE)) {
		ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
		if (ret < 0) {
			ffLog("Could not open output file '%s'\n", filename);
			ffCloseEncodeContext(pec);
			return NULL;
		}
	}
	/*
	 * 写入媒体头文件
	 */
	ret = avformat_write_header(ofmt_ctx, NULL);
	if (ret < 0) {
		ffLog("Error occurred when opening output file \n");
		ffCloseEncodeContext(pec);
		return NULL;
	}
	av_dump_format(ofmt_ctx, 0, filename, 1);

	/*
	 * 启动压缩压缩线程
	 * FIXME!
	 *	  应该尝试分配多个线程并行压缩以榨取多处理器的全部性能。
	 */
	pec->_mutex = new std::mutex();
	pec->_cond = new std::condition_variable();
	pec->_encode_thread = new std::thread(encode_thread_proc,pec);
	return pec;
}

/**
* 关闭编码上下文
*/
void ffCloseEncodeContext(AVEncodeContext *pec)
{
	if (pec)
	{
		/*
		 * 停止压缩线程,压缩在没有数据可处理并且_encode_thread=1是退出
		 */
		if (pec->_encode_thread)
		{
			pec->_stop_thread = 1;
			pec->_encode_thread->join();
		}
		if (pec->_ctx)
		{
			/* Write the trailer, if any. The trailer must be written before you
			* close the CodecContexts open when you wrote the header; otherwise
			* av_write_trailer() may try to use memory that was freed on
			* av_codec_close(). 
			*/
			av_write_trailer(pec->_ctx);
			/*
			 * 如果有必要关闭文件流
			 */
			if (pec->_ctx->oformat && !(pec->_ctx->oformat->flags & AVFMT_NOFILE))
			{
				avio_closep(&pec->_ctx->pb);
			}
			avformat_free_context(pec->_ctx);
		}
		/*
		* 关闭编码器
		*/
		if (pec->_video_st)
			close_stream(pec->_video_st);
		if (pec->_audio_st)
			close_stream(pec->_audio_st);
		if (pec->_video_frame)
			av_frame_free(&pec->_video_frame);
		if (pec->_video_tmp_frame)
			av_frame_free(&pec->_video_tmp_frame);
		if (pec->_audio_frame)
			av_frame_free(&pec->_audio_frame);
		if (pec->_audio_tmp_frame)
			av_frame_free(&pec->_audio_tmp_frame);
		if (pec->_audio_swr_ctx)
			swr_free(&pec->_audio_swr_ctx);
		if (pec->_audio_sws_ctx)
			swr_free(&pec->_audio_sws_ctx);

		free((void*)pec->_fileName);
		free(pec);
	}
}

AVRaw * ffMakeYUV420PRaw(uint8_t * pdata[NUM_DATA_POINTERS], int linesize[NUM_DATA_POINTERS],int w, int h)
{
	AVRaw *praw;

	praw = (AVRaw *)malloc(sizeof(AVRaw));
	if (!praw)
	{
		ffLog("ffMakeYUV420PRaw malloc return 0\n");
		return NULL;
	}
	memset(praw, 0, sizeof(AVRaw));
	praw->type = RAW_IMAGE;
	praw->format = AV_PIX_FMT_YUV420P;
	praw->width = w;
	praw->height = h;
	for (int i = 0; i < NUM_DATA_POINTERS; i++)
	{
		praw->data[i] = pdata[i];
		praw->linesize[i] = linesize[i];
	}
	return praw;
}

AVRaw * ffMakeAudioS16Raw(uint8_t * pdata, int chanles, int samples)
{
	AVRaw *praw;

	praw = (AVRaw *)malloc(sizeof(AVRaw));
	if (!praw)
	{
		ffLog("ffMakeAudioS16Raw malloc return 0\n");
		return NULL;
	}
	memset(praw, 0, sizeof(AVRaw));
	praw->type = RAW_AUDIO;
	praw->format = AV_SAMPLE_FMT_S16;
	praw->channels = chanles;
	praw->samples = samples;
	praw->data[0] = pdata;
	return praw;
}

void ffFreeRaw(AVRaw * praw)
{
	if (praw)
	{
		/*
		 * 这里假设data里存储的指针是通过malloc分配的整块内存，并且praw->data[0]是头。
		 */
		if (praw->data[0])
			free(praw->data[0]);
		free(praw);
	}
}

int ffGetBufferSizeKB(AVEncodeContext *pec)
{
	return pec->_buffer_size;
}

int ffGetBufferSize(AVEncodeContext *pec)
{
	return pec->_nb_raws;
}

static int getAVRawSizeKB(AVRaw *praw)
{
	if (praw->type == RAW_IMAGE)
	{
		if (praw->format == AV_PIX_FMT_YUV420P)
		{
			/*
			 * 图像如果是YUV420P,每个点占用12bpp
			 */
			return (int)(praw->width*praw->height * 12.0 / (8.0 * 1024));
		}
		else if (praw->format == AV_PIX_FMT_RGB24)
		{
			return (int)(praw->width*praw->height * 24.0 / (8.0 * 1024));
		}
	}
	else
	{
		if (praw->format == AV_SAMPLE_FMT_S16)
		{
			return (int)(praw->channels*praw->samples / 1024.0);
		}
	}
	/*
	 * 未知的格式，不加入到统计中
	 */
	return 0;
}

void ffAddFrame(AVEncodeContext *pec, AVRaw *praw)
{
	pec->_mutex->lock();
	if (!pec->_head)
	{
		pec->_head = praw;
		pec->_tail = praw;
		pec->_nb_raws=1;
		pec->_buffer_size = getAVRawSizeKB(praw);
	}
	else
	{
		pec->_tail->next = praw;
		pec->_tail = praw;
		pec->_nb_raws++;
		pec->_buffer_size += getAVRawSizeKB(praw);
	}
	pec->_cond->notify_one();
	pec->_mutex->unlock();
}