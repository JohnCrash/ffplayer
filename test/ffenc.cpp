#include "ffenc.h"

static char _lastError[ERROR_BUFFER_SIZE];
/**
* ������־�������
*/
static tLogFunc _gLogFunc = NULL;
void ffSetLogHandler(tLogFunc logfunc)
{
	_gLogFunc = logfunc;
}

const char * ffLastError()
{
	return _lastError;
}

/**
* ��־���
*/
void ffLog(const char * fmt,...)
{
	char buf[ERROR_BUFFER_SIZE];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, ERROR_BUFFER_SIZE - 3, fmt, args);
	va_end(args);
	if (_gLogFunc)
		_gLogFunc(buf);
	else
		printf("%s", buf);
}

/*
 * ��AVFormatContext�����µ���
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
		/* �ֱ��ʱ�����2�ı�����������Ҫ����� */
		c->width = w;
		c->height = h;
		st->time_base.den = stream_frame_rate;
		st->time_base.num = 1;
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
 * �ر���
 */
static void close_stream(AVStream *st)
{
	avcodec_close(st->codec);
	/*
		av_frame_free(&ost->frame);
		sws_freeContext(ost->sws_ctx);
		swr_free(&ost->swr_ctx);
	*/
}

AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
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
 * ����Ƶ������
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
		char errmsg[ERROR_BUFFER_SIZE];
		av_strerror(ret, errmsg, ERROR_BUFFER_SIZE);
		ffLog("Could not open video codec: %s\n",errmsg);
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
 * ������Ƶ֡
 */
AVFrame *alloc_audio_frame(enum AVSampleFormat sample_fmt,
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
 * ����Ƶ������
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

int ffGetAudioSamples(AVEncodeContext *pec)
{
	if (pec)
	{
		if (pec->_audio_st)
		{
			AVCodecContext *c = pec->_audio_st->codec;
			if (c)
			{
				int nb_samples;
				if (c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
					nb_samples = 10000;
				else
					nb_samples = c->frame_size;

				return nb_samples;
			}
		}
	}
	return -1;
}

int ffGetAudioChannels(AVEncodeContext *pec)
{
	if (pec)
	{
		if (pec->_audio_st)
		{
			AVCodecContext *c = pec->_audio_st->codec;
			if (c)
			{
				return c->channels;
			}
		}
	}
	return -1;
}

static int getAVRawSizeKB(AVRaw *praw)
{
	if (praw)
	{
		return praw->size/1024;
	}
	/*
	* δ֪�ĸ�ʽ�������뵽ͳ����
	*/
	return 0;
}

AVRaw * list_pop_raw(AVRaw ** head, AVRaw **tail)
{
	AVRaw * praw = NULL;
	if (*head)
	{
		praw = *head;
		*head = praw->next;
		if (praw == *tail)
		{
			*tail = *head;
		}
	}
	return praw;
}

void ffFlush(AVEncodeContext *pec)
{
	mutex_lock_t lock(*pec->_mutex);
	pec->_isflush = 1;
	pec->_cond->notify_one();
}

static AVRaw * ffPopFrame(AVEncodeContext * pec)
{
	mutex_lock_t lock(*pec->_mutex);
	AVRaw * praw = NULL;

	if (pec->encode_audio && pec->encode_video)
	{
		/*
		 * ��Ƶ����Ƶ������Ҫ����д�뵽���ļ�
		 */
		if (av_compare_ts(pec->_actx.next_pts, pec->_audio_st->codec->time_base,
			pec->_vctx.next_pts, pec->_video_st->codec->time_base) >= 0)
		{
			/*
			 * ����ȡһ����Ƶ֡�����û����Ƶ֡�͵������ֱ����һ����Ƶ֡����
			 * ����ͨ��isflush��֪�Ѿ�û�и��������ˡ������whileѭ����������û������
			 * list_pop_raw������һ��NULLָ�룬������ѹ���߳���ֹ��
			 */
			while (!pec->_video_head && !pec->_isflush)
				pec->_cond->wait(lock);
			praw = list_pop_raw(&pec->_video_head, &pec->_video_tail);
		}
		else
		{
			while (!pec->_audio_head && !pec->_isflush)
				pec->_cond->wait(lock);
			praw = list_pop_raw(&pec->_audio_head, &pec->_audio_tail);
		}
	}
	else if (pec->encode_video)
	{
		if (!pec->_video_head&& !pec->_isflush)
			pec->_cond->wait(lock);
		praw = list_pop_raw(&pec->_video_head, &pec->_video_tail);
	}
	else if (pec->encode_audio)
	{
		if (!pec->_audio_head&& !pec->_isflush)
			pec->_cond->wait(lock);
		praw = list_pop_raw(&pec->_audio_head, &pec->_audio_tail);
	}

	if (praw)
	{
		pec->_nb_raws--;
		pec->_buffer_size -= getAVRawSizeKB(praw);
	}
	return praw;
}

AVFrame * make_video_frame(AVCtx * ctx,AVRaw * praw)
{
	AVCodecContext *c = ctx->st->codec;
	AVFrame * frame = ctx->frame;

	/*
	 * ������Ҫ��ĸ�ʽ�������ʽ��ͬ�����ﲻ��Ҫ���и���ת����
	 */
	if (praw->format==c->pix_fmt && praw->width==c->width&&praw->height==c->height)
	{
		/*
		 * �����ʽ��ͬ���Խ�ȥ�򵥵Ŀ���
		 * FIXME: ����򵥵�ʹ��praw�е�ָ�봫�ݸ�frame��ѹ������ڻָ����Խ�ʡcopy����
		 */
		/* when we pass a frame to the encoder, it may keep a reference to it
		* internally;
		* make sure we do not overwrite it here
		*/
		int ret = av_frame_make_writable(frame);
		if (ret < 0)
		{
			char errmsg[ERROR_BUFFER_SIZE];
			av_strerror(ret, errmsg, ERROR_BUFFER_SIZE);
			ffLog("make_video_frame av_frame_make_writable : %s\n", errmsg);
			return NULL;
		}
		/*
		 * ���������ݸ���
		 */
		av_image_copy(frame->data, frame->linesize, (const uint8_t **)praw->data, praw->linesize, c->pix_fmt,praw->width,praw->height);

		frame->pts = ctx->next_pts++;
		return frame;
	}
	else
	{
		/*
		 * ��ʼ����ʽת��������
		 */
		if (!ctx->sws_ctx)
		{
			ctx->sws_ctx = sws_getContext(praw->width, praw->height,
				(AVPixelFormat)praw->format,
				c->width, c->height,
				c->pix_fmt,
				SCALE_FLAGS, NULL, NULL, NULL);
			if (!ctx->sws_ctx) {
				ffLog("Could not initialize the conversion context\n");
				return NULL;
			}
		}

		sws_scale(ctx->sws_ctx,
			(const uint8_t * const *)praw->data, praw->linesize,
			0, praw->height,frame->data, frame->linesize);

		frame->pts = ctx->next_pts++;
		return frame;
	}
}

static int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
{
	/* rescale output packet timestamp values from codec to stream timebase */
	av_packet_rescale_ts(pkt, *time_base, st->time_base);
	pkt->stream_index = st->index;

	/* Write the compressed frame to the media file. */
	return av_interleaved_write_frame(fmt_ctx, pkt);
}

/*
 * encode one video frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 * ������-1
 */
static int write_video_frame(AVEncodeContext * pec, AVRaw *praw)
{
	int ret;
	AVCodecContext *c;
	AVStream * st;
	AVFrame *frame;
	int got_packet = 0;

	c = pec->_video_st->codec;
	st = pec->_video_st;

	frame = make_video_frame(&pec->_vctx,praw);
	
	if (!frame)
	{
		return -1;
	}
	if (pec->_ctx->oformat->flags & AVFMT_RAWPICTURE) {
		/* a hack to avoid data copy with some raw video muxers */
		AVPacket pkt;
		av_init_packet(&pkt);

		if (!frame)
			return 1;

		pkt.flags |= AV_PKT_FLAG_KEY;
		pkt.stream_index = st->index;
		pkt.data = (uint8_t *)frame;
		pkt.size = sizeof(AVPicture);

		pkt.pts = pkt.dts = frame->pts;
		av_packet_rescale_ts(&pkt, c->time_base, st->time_base);

		ret = av_interleaved_write_frame(pec->_ctx, &pkt);
	}
	else {
		AVPacket pkt = { 0 };
		av_init_packet(&pkt);

		/* encode the image */
		ret = avcodec_encode_video2(c, &pkt, frame, &got_packet);
		if (ret < 0) {
			char errmsg[ERROR_BUFFER_SIZE];
			av_strerror(ret, errmsg, ERROR_BUFFER_SIZE);
			ffLog("Error encoding video frame: %s\n", errmsg);
			return -1;
		}

		if (got_packet) {
			ret = write_frame(pec->_ctx, &c->time_base, st, &pkt);
		}
		else {
			ret = 0;
		}
	}

	if (ret < 0) {
		char errmsg[ERROR_BUFFER_SIZE];
		av_strerror(ret, errmsg, ERROR_BUFFER_SIZE);
		ffLog("Error while writing video frame: %s\n", errmsg);
		return -1;
	}

	return (frame || got_packet) ? 0 : 1;
}

AVFrame * get_audio_frame(AVCtx * ctx,AVRaw *praw)
{
	AVFrame *frame;
	AVCodecContext * c;
	int ret, dst_nb_samples;

	c = ctx->st->codec;
	frame = ctx->frame;

	if (c->sample_fmt == praw->format && praw->samples==frame->nb_samples && praw->channels==frame->channels)
	{
		/*
		 * ������ʽ��ֱͬ�ӽ��и��ƾͿ�����
		 */
		av_samples_copy(frame->data, praw->data, 0, 0, praw->samples, praw->channels, c->sample_fmt);
	}
	else
	{
		/* convert samples from native format to destination codec format, using the resampler */
		/* compute destination number of samples */
		dst_nb_samples = av_rescale_rnd(swr_get_delay(ctx->swr_ctx, c->sample_rate) + frame->nb_samples,
			c->sample_rate, c->sample_rate, AV_ROUND_UP);
		if (dst_nb_samples != frame->nb_samples)
		{
			ffLog("get_audio_frame assert dst_nb_samples == frame->nb_samples\n");
			return NULL;
		}
		
		/* when we pass a frame to the encoder, it may keep a reference to it
		* internally;
		* make sure we do not overwrite it here
		*/
		ret = av_frame_make_writable(ctx->frame);
		if (ret < 0)
		{
			ffLog("get_audio_frame av_frame_make_writable return <0\n");
			return NULL;
		}
		/* convert to destination format */
		ret = swr_convert(ctx->swr_ctx,
			frame->data, dst_nb_samples,
			(const uint8_t**)praw->data, praw->samples);
		if (ret < 0) {
			ffLog("get_audio_frame error while converting\n");
			return NULL;
		}
	}
	
	frame->pts = ctx->next_pts;
	ctx->next_pts += frame->nb_samples;

	return frame;
}

/*
* encode one audio frame and send it to the muxer
* return 1 when encoding is finished, 0 otherwise
* ������-1
*/
static int write_audio_frame(AVEncodeContext * pec, AVRaw *praw)
{
	AVCodecContext *c;
	AVPacket pkt = { 0 }; // data and size must be 0;
	AVStream * st;
	AVFrame *frame;
	AVCtx * ctx;
	AVRational avrat;
	int ret;
	int got_packet;

	ctx = &pec->_actx;
	st = pec->_audio_st;
	av_init_packet(&pkt);
	c = st->codec;

	frame = get_audio_frame(&pec->_actx,praw);

	if (!frame)
		return -1;

	if (frame) {
		avrat.den = c->sample_rate;
		avrat.num = 1;
		frame->pts = av_rescale_q(ctx->samples_count, avrat, c->time_base);
		ctx->samples_count += frame->nb_samples;
	}

	ret = avcodec_encode_audio2(c, &pkt, frame, &got_packet);
	if (ret < 0) {
		char errmsg[ERROR_BUFFER_SIZE];
		av_strerror(ret, errmsg, ERROR_BUFFER_SIZE);
		ffLog("Error encoding audio frame: %s\n", errmsg);
		return -1;
	}

	if (got_packet) {
		ret = write_frame(pec->_ctx, &c->time_base, st, &pkt);
		if (ret < 0) {
			char errmsg[ERROR_BUFFER_SIZE];
			av_strerror(ret, errmsg, ERROR_BUFFER_SIZE);
			ffLog("Error while writing audio frame: %s\n",
				errmsg);
			return -1;
		}
	}

	return (frame || got_packet) ? 0 : 1;
}

/*
 * ����д���߳�
 */
int encode_thread_proc(AVEncodeContext * pec)
{
	int ret;
	int total_frame = 0;

	while (!pec->_stop_thread)
	{
		AVRaw * praw = ffPopFrame(pec);
		/*
		 * ѹ��ԭ�����ݲ�д�뵽�ļ���
		 */
		total_frame++;
		if (praw)
		{
			if (praw->type == RAW_IMAGE)
			{
				if (pec->encode_video)
				{
					ret = write_video_frame(pec, praw);
					if (ret < 0)
						break;
				}
			}
			else if (praw->type == RAW_AUDIO)
			{
				if (pec->encode_audio)
				{
					ret = write_audio_frame(pec, praw);
					if (ret < 0)
						break;
				}
			}
			else
			{
				ffLog("Unknow raw type.\n");
				break;
			}
			release_raw(praw);
		}
		else
		{
			/*
			 * �������NULL��ʾ�Ѿ�û�������ˡ�
			 */
			break;
		}
		printf("buffer : %d, %.2f mb\n",pec->_nb_raws,pec->_buffer_size/1024.0);
	}
	pec->_stop_thread = 1;
	printf("write total frame : %d \n",total_frame);
	return 0;
}

/**
 * ��������������
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
	 * �������������
	 */
	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, filename);
	if (!ofmt_ctx){
		ffLog("avformat_alloc_output_context2 return failed.");
		ffCloseEncodeContext(pec);
		return NULL;
	}
	pec->_ctx = ofmt_ctx;
	/*
	 * ������Ƶ������Ƶ��
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
	 * ����Ƶ����������Ƶ������
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
	 * ����б�Ҫ��һ���ļ������
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
	 * д��ý��ͷ�ļ�
	 */
	ret = avformat_write_header(ofmt_ctx, NULL);
	if (ret < 0) {
		ffLog("Error occurred when opening output file \n");
		ffCloseEncodeContext(pec);
		return NULL;
	}
	pec->isopen = 1;

	av_dump_format(ofmt_ctx, 0, filename, 1);

	/*
	 * ����ѹ��ѹ���߳�
	 */
	pec->_mutex = new mutex_t();
	pec->_cond = new condition_t();
	pec->_encode_thread = new std::thread(encode_thread_proc,pec);
	return pec;
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
/**
* �رձ���������
*/
void ffCloseEncodeContext(AVEncodeContext *pec)
{
	if (pec)
	{
		/*
		 * ֹͣѹ���߳�,ѹ����û�����ݿɴ�����_encode_thread=1���˳�
		 */
		if (pec->_encode_thread)
		{
			pec->_cond->notify_one();
			pec->_encode_thread->join();
			delete pec->_mutex;
			delete pec->_cond;
			delete pec->_encode_thread;
		}
		if (pec->_ctx)
		{
			if (pec->isopen)
			{
				/* Write the trailer, if any. The trailer must be written before you
				* close the CodecContexts open when you wrote the header; otherwise
				* av_write_trailer() may try to use memory that was freed on
				* av_codec_close().
				*/
				av_write_trailer(pec->_ctx);
				/*
				 * ����б�Ҫ�ر��ļ���
				 */
				if (pec->_ctx->oformat && !(pec->_ctx->oformat->flags & AVFMT_NOFILE))
				{
					avio_closep(&pec->_ctx->pb);
				}
			}
			/*
			* �رձ�����
			*/
			if (pec->_video_st)
				close_stream(pec->_video_st);
			if (pec->_audio_st)
				close_stream(pec->_audio_st);

			avformat_free_context(pec->_ctx);
		}

		ffFreeAVCtx(&pec->_vctx);
		ffFreeAVCtx(&pec->_actx);

		free((void*)pec->_fileName);
		free(pec);
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

void list_push_raw(AVRaw ** head, AVRaw ** tail,AVRaw *praw)
{
	if (!*head)
	{
		*head = praw;
		*tail = praw;
	}
	else
	{
		(*tail)->next = praw;
		*tail = praw;
	}
}

int ffAddFrame(AVEncodeContext *pec, AVRaw *praw)
{
	if (pec->_stop_thread)
	{
		ffLog("ffAddFrame encode thread already stoped.\n");
		return -1;
	}

	mutex_lock_t lk(*pec->_mutex);

	if(praw->type==RAW_IMAGE)
	{
		list_push_raw(&pec->_video_head, &pec->_video_tail, praw);
		pec->_nb_raws++;
		pec->_buffer_size += getAVRawSizeKB(praw);
		pec->_cond->notify_one();
	}
	else if (praw->type == RAW_AUDIO)
	{
		list_push_raw(&pec->_audio_head, &pec->_audio_tail, praw);
		pec->_nb_raws++;
		pec->_buffer_size += getAVRawSizeKB(praw);
		pec->_cond->notify_one();
	}
	else
		ffLog("ffAddFrame unknow type of AVRaw\n"); 

	return 0;
}

void ffInit()
{
#if CONFIG_AVDEVICE
	avdevice_register_all();
#endif
#if CONFIG_AVFILTER
	avfilter_register_all();
#endif
	av_register_all();
	avformat_network_init();
}
