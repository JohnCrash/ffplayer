#include "ffenc.h"

namespace ff
{
	/*
	 * 向AVFormatContext加入新的流
	 */
	static int add_stream(AVEncodeContext *pec, AVCodecID codec_id,
		int w, int h, AVRational stream_frame_rate, int stream_bit_rate)
	{
		AVCodec *codec;
		AVStream *st;
		AVCodecContext *c;
		int i;

		codec = avcodec_find_encoder(codec_id);
		if (!codec)
		{
			av_log(NULL, AV_LOG_FATAL, "Could not find encoder '%s'\n", avcodec_get_name(codec_id));
			return -1;
		}

		st = avformat_new_stream(pec->_ctx, codec);
		if (!st)
		{
			av_log(NULL, AV_LOG_FATAL, "Could not allocate stream\n");
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
			c->sample_rate = stream_frame_rate.num;
			if (codec->supported_samplerates) {
				c->sample_rate = codec->supported_samplerates[0];
				for (i = 0; codec->supported_samplerates[i]; i++) {
					if (codec->supported_samplerates[i] == stream_frame_rate.num)
						c->sample_rate = stream_frame_rate.num;
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
			//st->time_base.den = stream_frame_rate.num;
			//st->time_base.num = stream_frame_rate.den;

			st->avg_frame_rate = stream_frame_rate;

			c->time_base = av_inv_q(stream_frame_rate);
			c->sample_aspect_ratio = { 0, 1 };
			//c->ticks_per_frame = 2;

			//c->gop_size = 12; /* emit one intra frame every twelve frames at most */
			//c->pix_fmt = STREAM_PIX_FMT;
			c->pix_fmt = AV_PIX_FMT_YUV420P;
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
			av_log(NULL, AV_LOG_FATAL, "Unknow stream type\n");
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
			av_log(NULL, AV_LOG_FATAL, "Could not allocate frame data.\n");
			return NULL;
		}

		return picture;
	}

	/*
	 * 打开视频编码器
	 */
	static int open_video(AVEncodeContext *pec, AVCodecID video_codec_id,
		int in_w,int in_h,AVPixelFormat in_fmt, //FIXME: 添加不同尺寸和格式进行转换的代码
		AVDictionary *opt_arg)
	{
		int ret;
		AVCodecContext *c = pec->_video_st->codec;
		AVDictionary *opt = NULL;
		AVCodec *codec;
		int w, h, fmt;

		codec = avcodec_find_encoder(video_codec_id);
		if (!codec)
		{
			av_log(NULL, AV_LOG_FATAL, "Could not find encoder '%s'\n", avcodec_get_name(video_codec_id));
			return -1;
		}

		av_dict_copy(&opt, opt_arg, 0);

		/* open the codec */
		ret = avcodec_open2(c, codec, &opt);
		av_dict_free(&opt);
		if (ret < 0) {
			char errmsg[ERROR_BUFFER_SIZE];
			av_strerror(ret, errmsg, ERROR_BUFFER_SIZE);
			av_log(NULL, AV_LOG_FATAL, "Could not open video codec: %s\n", errmsg);
			return -1;
		}

		pec->_vctx.st = pec->_video_st;
		/* allocate and init a re-usable frame */
		pec->_vctx.frame = alloc_picture(c->pix_fmt, c->width, c->height);
		if (!pec->_vctx.frame) {
			av_log(NULL, AV_LOG_FATAL, "Could not allocate video frame\n");
			return -1;
		}

		if (c->width != in_w || c->height != in_h || in_fmt != c->pix_fmt){
			pec->_vctx.sws_ctx = sws_getContext(in_w, in_h, in_fmt, 
				c->width, c->height, c->pix_fmt, 
				SCALE_FLAGS, NULL, NULL, NULL);
			if (!pec->_vctx.sws_ctx){
				av_log(NULL, AV_LOG_FATAL, "Could not initialize the conversion context,in_w=%d,in_h=%d,in_fmt=%d\n", in_w, in_h, in_fmt);
				return -1;
			}
		}
		return 0;
	}

	/*
	 * 分配音频帧
	 */
	AVFrame *alloc_audio_frame(enum AVSampleFormat sample_fmt,
		uint64_t channel_layout,
		int sample_rate, int nb_samples)
	{
		AVFrame *frame = av_frame_alloc();
		int ret;

		if (!frame) {
			av_log(NULL, AV_LOG_FATAL, "Error allocating an audio frame\n");
			return NULL;
		}

		frame->format = sample_fmt;
		frame->channel_layout = channel_layout;
		frame->sample_rate = sample_rate;
		frame->nb_samples = nb_samples;

		if (nb_samples) {
			ret = av_frame_get_buffer(frame, 0);
			if (ret < 0) {
				av_log(NULL, AV_LOG_FATAL, "Error allocating an audio buffer\n");
				return NULL;
			}
		}

		return frame;
	}

	/*
	 * 打开音频编码器
	 */
	static int open_audio(AVEncodeContext *pec, AVCodecID audio_codec_id, 
		int in_ch,int in_rate,AVSampleFormat in_fmt,
		AVDictionary *opt_arg)
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
			av_log(NULL, AV_LOG_FATAL, "Could not find encoder '%s'\n", avcodec_get_name(audio_codec_id));
			return -1;
		}

		/* open it */
		av_dict_copy(&opt, opt_arg, 0);
		ret = avcodec_open2(c, codec, &opt);
		av_dict_free(&opt);
		if (ret < 0) {
			char errmsg[ERROR_BUFFER_SIZE];
			av_strerror(ret, errmsg, ERROR_BUFFER_SIZE);
			av_log(NULL, AV_LOG_FATAL, "Could not open audio codec: %s\n", errmsg);
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
			av_log(NULL, AV_LOG_FATAL, "Could not allocate resampler context.\n");
			return -1;
		}

		/* set options */
		av_opt_set_int(pec->_actx.swr_ctx, "in_channel_count", in_ch, 0);
		av_opt_set_int(pec->_actx.swr_ctx, "in_sample_rate", in_rate, 0);
		av_opt_set_sample_fmt(pec->_actx.swr_ctx, "in_sample_fmt", in_fmt, 0);
		av_opt_set_int(pec->_actx.swr_ctx, "out_channel_count", c->channels, 0);
		av_opt_set_int(pec->_actx.swr_ctx, "out_sample_rate", c->sample_rate, 0);
		av_opt_set_sample_fmt(pec->_actx.swr_ctx, "out_sample_fmt", c->sample_fmt, 0);

		/* initialize the resampling context */
		if ((ret = swr_init(pec->_actx.swr_ctx)) < 0) {
			av_log(NULL, AV_LOG_FATAL, "Failed to initialize the resampling context\n");
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
			return praw->size / 1024;
		}
		/*
		* 未知的格式，不加入到统计中
		*/
		return 0;
	}

	void list_push_raw(AVRaw ** head, AVRaw ** tail, AVRaw *praw)
	{
		retain_raw(praw);
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
		AVCtx * pctx;
		if(pec->has_audio){
			pctx = &pec->_actx;
			mutex_lock_t lock(*pctx->mutex);
			pctx->isflush = 1;
			pctx->cond->notify_one();
		}
		if (pec->has_video){
			pctx = &pec->_vctx;
			mutex_lock_t lock(*pctx->mutex);
			pctx->isflush = 1;
			pctx->cond->notify_one();
		}
	}

#if 0
	static AVRaw * ffPopFrame(AVEncodeContext * pec)
	{
		mutex_lock_t lock(*pec->_mutex);
		AVRaw * praw = NULL;

		if (pec->encode_audio && pec->encode_video)
		{
#if 0
			/*
			 * 音频和视频的数据要交替写入到流文件
			 */
			if (av_compare_ts(pec->_actx.next_pts, pec->_audio_st->codec->time_base,
				pec->_vctx.next_pts, pec->_video_st->codec->time_base) >= 0)
			{
				/*
				 * 这里取一个视频帧，如果没有视频帧就等在这里。直到有一个视频帧到来
				 * 或者通过isflush获知已经没有跟多数据了。如果在while循环结束还是没有数据
				 * list_pop_raw将返回一个NULL指针，进而是压缩线程终止。
				 */
				pec->_encode_waiting = 1;
				while (!pec->_video_head && !pec->_isflush)
					pec->_cond->wait(lock);
				pec->_encode_waiting = 0;
				praw = list_pop_raw(&pec->_video_head, &pec->_video_tail);
			}
			else
			{
				pec->_encode_waiting = 1;
				while (!pec->_audio_head && !pec->_isflush)
					pec->_cond->wait(lock);
				pec->_encode_waiting = 0;
				praw = list_pop_raw(&pec->_audio_head, &pec->_audio_tail);
			}
#endif
			pec->_encode_waiting = 1;
			//如果pec->_video_head == NULL && pec->_video_head == NULL
			while (!(pec->_video_head || pec->_audio_head) && !pec->_isflush)
				pec->_cond->wait(lock);
			pec->_encode_waiting = 0;

			if (pec->_video_head &&
				av_compare_ts(pec->_actx.next_pts, pec->_audio_st->codec->time_base,
				pec->_vctx.next_pts, pec->_video_st->codec->time_base) >= 0)
			{
				praw = list_pop_raw(&pec->_video_head, &pec->_video_tail);
			}
			else if (pec->_audio_head){
				praw = list_pop_raw(&pec->_audio_head, &pec->_audio_tail);
			}
			else if (pec->_video_head){
				praw = list_pop_raw(&pec->_video_head, &pec->_video_tail);
			}
		}
		else if (pec->encode_video)
		{
			pec->_encode_waiting = 1;
			if (!pec->_video_head&& !pec->_isflush)
				pec->_cond->wait(lock);
			pec->_encode_waiting = 0;
			praw = list_pop_raw(&pec->_video_head, &pec->_video_tail);
		}
		else if (pec->encode_audio)
		{
			pec->_encode_waiting = 1;
			if (!pec->_audio_head&& !pec->_isflush)
				pec->_cond->wait(lock);
			pec->_encode_waiting = 0;
			praw = list_pop_raw(&pec->_audio_head, &pec->_audio_tail);
		}

		if (praw)
		{
			pec->_nb_raws--;
			pec->_buffer_size -= getAVRawSizeKB(praw);
		}
		return praw;
	}
#endif

	static AVFrame * make_video_frame(AVCtx * ctx, AVRaw * praw)
	{
		AVCodecContext *c = ctx->st->codec;
		AVFrame * frame = ctx->frame;

		/*
		 * 编码器要求的格式和输入格式相同，这里不需要进行复杂转换。
		 */
		if (!ctx->sws_ctx)
		{
			/*
			 * 如果格式相同可以进去简单的拷贝
			 * FIXME: 如果简单的使用praw中的指针传递给frame，压缩完成在恢复可以节省copy操作
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
				av_log(NULL, AV_LOG_FATAL, "make_video_frame av_frame_make_writable : %s\n", errmsg);
				return NULL;
			}
			/*
			 * 这里做数据复制
			 */
			av_image_copy(frame->data, frame->linesize, (const uint8_t **)praw->data, praw->linesize, c->pix_fmt, praw->width, praw->height);

			frame->pts = ctx->next_pts++;
			return frame;
		}
		else
		{
			sws_scale(ctx->sws_ctx,
				(const uint8_t * const *)praw->data, praw->linesize,
				0, praw->height, frame->data, frame->linesize);

			frame->pts = ctx->next_pts++;
			return frame;
		}
	}

	int ffIsWaitingOrStop(AVEncodeContext *pec)
	{
		if (pec->has_audio){
			if (pec->_actx.encode_waiting || pec->_actx.stop_thread)
				return 1;
		}
		if (pec->has_video){
			if (pec->_vctx.encode_waiting || pec->_vctx.stop_thread)
				return 1;
		}
		return 0;
	}

	static int write_frame(AVEncodeContext *pec,AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
	{
		mutex_lock_t lock(*pec->write_mutex);
		/* rescale output packet timestamp values from codec to stream timebase */
		av_packet_rescale_ts(pkt, *time_base, st->time_base);
		pkt->stream_index = st->index;

		/* Write the compressed frame to the media file. */
		return av_interleaved_write_frame(fmt_ctx, pkt);
	}

	/*
	 * encode one video frame and send it to the muxer
	 * return 1 when encoding is finished, 0 otherwise
	 * 出错返回-1
	 */
	static int write_video_frame(AVEncodeContext * pec, AVRaw *praw)
	{
		int ret;
		AVCodecContext *c;
		AVStream * st;
		AVFrame *frame = NULL;
		int got_packet = 0;

		c = pec->_video_st->codec;
		st = pec->_video_st;
		for (int i = 0; i < praw->recount; i++){

			if (!frame)
				frame = make_video_frame(&pec->_vctx, praw);
			else
				frame->pts = pec->_vctx.next_pts++;

			if (!frame)
				return -1;

			//		printf("video pts = %I64d, %I64d\n", praw->pts, frame->pts);

			if (pec->_ctx->oformat->flags & AVFMT_RAWPICTURE) {
				/* a hack to avoid data copy with some raw video muxers */
				AVPacket pkt;
				av_init_packet(&pkt);

				if (!frame)
					return -1;

				pkt.flags |= AV_PKT_FLAG_KEY;
				pkt.stream_index = st->index;
				pkt.data = (uint8_t *)frame;
				pkt.size = sizeof(AVPicture);

				pkt.pts = pkt.dts = frame->pts;

				ret = write_frame(pec, pec->_ctx, &c->time_base, st, &pkt);
			}
			else {
				AVPacket pkt = { 0 };
				av_init_packet(&pkt);

				/* encode the image */
				ret = avcodec_encode_video2(c, &pkt, frame, &got_packet);
				if (ret < 0) {
					char errmsg[ERROR_BUFFER_SIZE];
					av_strerror(ret, errmsg, ERROR_BUFFER_SIZE);
					av_log(NULL, AV_LOG_FATAL, "Error encoding video frame: %s\n", errmsg);
					return -1;
				}
				
				if (got_packet) {
					ret = write_frame(pec,pec->_ctx, &c->time_base, st, &pkt);
				}
				else {
					ret = 0;
				}
			}

			if (ret < 0) {
				char errmsg[ERROR_BUFFER_SIZE];
				av_strerror(ret, errmsg, ERROR_BUFFER_SIZE);
				av_log(NULL, AV_LOG_FATAL, "Error while writing video frame: %s\n", errmsg);
				return -1;
			}
		}
		return (frame || got_packet) ? 0 : 1;
	}

	static int resample_audio_frame(AVCtx * ctx, AVRaw *praw, AVFrame ** pframe)
	{
		AVFrame *frame;
		AVCodecContext * c;
		int ret;

		c = ctx->st->codec;
		frame = ctx->frame;

		*pframe = NULL;

		/* when we pass a frame to the encoder, it may keep a reference to it
		* internally;
		* make sure we do not overwrite it here
		*/
		ret = av_frame_make_writable(ctx->frame);
		if (ret < 0)
		{
			av_log(NULL, AV_LOG_FATAL, "get_audio_frame av_frame_make_writable return <0\n");
			return -1;
		}
		/* convert to destination format */
		ret = swr_convert(ctx->swr_ctx,
			frame->data, frame->nb_samples,
			(const uint8_t**)praw->data, praw->samples);
		if (ret < 0) {
			av_log(NULL, AV_LOG_FATAL, "get_audio_frame error while converting\n");
			return -1;
		}

		frame->pts = ctx->next_pts;
		ctx->next_pts += frame->nb_samples;
		*pframe = frame;
		return 0;
	}

	static int flush_audio_frame(AVCtx * ctx, AVFrame **pframe)
	{
		AVFrame *frame;
		AVCodecContext * c;
		int ret;

		c = ctx->st->codec;
		frame = ctx->frame;

		*pframe = NULL;
		/*
		 * 采样器中没有足够的数据填满一帧
		 */
		int nd = swr_get_delay(ctx->swr_ctx, c->sample_rate);
		if (nd < frame->nb_samples)
			return 1;

		/* when we pass a frame to the encoder, it may keep a reference to it
		* internally;
		* make sure we do not overwrite it here
		*/
		ret = av_frame_make_writable(ctx->frame);
		if (ret < 0)
		{
			av_log(NULL, AV_LOG_FATAL, "get_audio_frame av_frame_make_writable return <0\n");
			return -1;
		}
		/* convert to destination format */
		ret = swr_convert(ctx->swr_ctx,
			frame->data, frame->nb_samples,
			NULL, 0);
		if (ret < 0) {
			av_log(NULL, AV_LOG_FATAL, "get_audio_frame error while converting\n");
			return -1;
		}

		frame->pts = ctx->next_pts;
		ctx->next_pts += frame->nb_samples;
		*pframe = frame;
		return 0;
	}
	/*
	* encode one audio frame and send it to the muxer
	* return 1 when encoding is finished, 0 otherwise
	* 出错返回-1
	*/
	static int write_audio_frame(AVEncodeContext * pec, AVRaw *praw)
	{
		AVCodecContext *c;
		AVPacket pkt = { 0 }; // data and size must be 0;
		AVStream * st;
		AVFrame *frame;
		AVCtx * ctx;
		AVRational avrat;
		int ret, result;
		int got_packet;

		ctx = &pec->_actx;
		st = pec->_audio_st;
		av_init_packet(&pkt);
		c = st->codec;


		frame = NULL;
		/*
		 * 将音频数据放入采样器，如果有成功取得输出帧，就继续写入
		 */
		result = resample_audio_frame(&pec->_actx, praw, &frame);

		while (result >= 0 && frame){
			avrat.den = c->sample_rate;
			avrat.num = 1;
			frame->pts = av_rescale_q(ctx->samples_count, avrat, c->time_base);
			//		printf("audio pts = %I64d , %I64d\n", praw->pts,frame->pts);

			ctx->samples_count += frame->nb_samples;

			ret = avcodec_encode_audio2(c, &pkt, frame, &got_packet);
			if (ret < 0) {
				char errmsg[ERROR_BUFFER_SIZE];
				av_strerror(ret, errmsg, ERROR_BUFFER_SIZE);
				av_log(NULL, AV_LOG_FATAL, "Error encoding audio frame: %s\n", errmsg);
				return -1;
			}

			if (got_packet) {
				ret = write_frame(pec,pec->_ctx, &c->time_base, st, &pkt);
				if (ret < 0) {
					char errmsg[ERROR_BUFFER_SIZE];
					av_strerror(ret, errmsg, ERROR_BUFFER_SIZE);
					av_log(NULL, AV_LOG_FATAL, "Error while writing audio frame: %s\n",
						errmsg);
					return -1;
				}
			}

			/*
			 * 继续如果采样器中还有足够的数据，就继续读取下一个帧
			 */
			result = flush_audio_frame(&pec->_actx, &frame);
		}

		return (frame || got_packet) ? 0 : 1;
	}

	static void write_delay_audio_frame(AVEncodeContext *pec)
	{
		int ret;
		AVCodecContext *c;
		AVStream * st;
		int got_packet = 1;

		if (pec->has_audio && pec->_audio_st){
			c = pec->_audio_st->codec;
			st = pec->_audio_st;
			while (got_packet){
				AVPacket pkt = { 0 };
				av_init_packet(&pkt);

				ret = avcodec_encode_audio2(c, &pkt, NULL, &got_packet);
				if (ret < 0){
					break;
				}
				if (got_packet){
					ret = write_frame(pec,pec->_ctx, &c->time_base, st, &pkt);
				}
			}
		}
	}

	static void write_delay_video_frame(AVEncodeContext *pec)
	{
		int ret;
		AVCodecContext *c;
		AVStream * st;
		int got_packet = 1;

		if (pec->has_video && pec->_video_st){
			c = pec->_video_st->codec;
			st = pec->_video_st;
			while (got_packet){
				AVPacket pkt = { 0 };
				av_init_packet(&pkt);

				ret = avcodec_encode_video2(c, &pkt, NULL, &got_packet);
				if (ret < 0){
					break;
				}
				if (got_packet){
					ret = write_frame(pec,pec->_ctx, &c->time_base, st, &pkt);
				}
			}
		}
	}

	static AVRaw * popFromList(AVCtx * pctx)
	{
		AVRaw * praw = NULL;
		mutex_lock_t lock(*pctx->mutex);
		pctx->encode_waiting = 1;

		while ( !pctx->head && !pctx->isflush ){
			pctx->cond->wait(lock);
		}
		pctx->encode_waiting = 0;
		
		if (pctx->isflush)return NULL;

		praw = list_pop_raw(&pctx->head, &pctx->tail);
		
		return praw;
	}

	static int video_encode_thread_proc(AVEncodeContext * pec)
	{
		int ret;
		AVRaw * praw;
		AVCtx * pctx = &pec->_vctx;

		while (!pctx->stop_thread)
		{
			praw = popFromList(pctx);

			if (praw){
				pec->_nb_raws--;
				pec->_buffer_size -= getAVRawSizeKB(praw);
				ret = write_video_frame(pec, praw);
				if (ret < 0){
					release_raw(praw);
					break;
				}
				release_raw(praw);
			}
			else {
				/*
				* 如果返回NULL表示已经没有数据了。
				* 写入延时帧
				*/
				write_delay_video_frame(pec);
				break;
			}
		}
		pctx->stop_thread = 1;
		return 0;
	}

	static int audio_encode_thread_proc(AVEncodeContext * pec)
	{
		int ret;
		AVRaw * praw;
		AVCtx * pctx = &pec->_actx;

		while (!pctx->stop_thread)
		{
			praw = popFromList( pctx );

			if (praw){
				pec->_nb_raws--;
				pec->_buffer_size -= getAVRawSizeKB(praw);
				ret = write_audio_frame(pec, praw);
				if (ret < 0){
					release_raw(praw);
					break;
				}
				release_raw(praw);
			}
			else {
				/*
				* 如果返回NULL表示已经没有数据了。
				* 写入延时帧
				*/
				write_delay_audio_frame(pec);
				break;
			}
		}
		pctx->stop_thread = 1;
		return 0;
	}
	/*
	 * 编码写入线程
	 */
#if 0
	int encode_thread_proc(AVEncodeContext * pec)
	{
		int ret;
		AVRaw * praw;

		while (!pec->_stop_thread)
		{
			praw = ffPopFrame(pec);
			/*
			 * 压缩原生数据并写入到文件中
			 */
			if (praw)
			{
				if (praw->type == RAW_IMAGE)
				{
					if (pec->encode_video)
					{
						ret = write_video_frame(pec, praw);
						if (ret < 0){
							release_raw(praw);
							break;
						}
					}
				}
				else if (praw->type == RAW_AUDIO)
				{
					if (pec->encode_audio)
					{
						ret = write_audio_frame(pec, praw);
						if (ret < 0){
							release_raw(praw);
							break;
						}
					}
				}
				else
				{
					av_log(NULL, AV_LOG_FATAL, "Unknow raw type.\n");
					break;
				}
				release_raw(praw);
			}
			else
			{
				/*
				 * 如果返回NULL表示已经没有数据了。
				 * 写入延时帧
				 */
				write_delay_frame(pec);
				break;
			}
		}
		pec->_stop_thread = 1;
		return 0;
	}
#endif
	static void initAVCtx(AVEncodeContext *pec,AVCtx * pctx, int(*encode_thread_proc)(AVEncodeContext *))
	{
		pctx->mutex = new mutex_t();
		pctx->cond = new condition_t();
		pctx->encode_thread = new std::thread(encode_thread_proc, pec);
	}

	/**
	 * 创建编码上下文
	 */
	AVEncodeContext* ffCreateEncodeContext(const char* filename, const char *fmt,
		int w, int h, AVRational frameRate, int videoBitRate, AVCodecID video_codec_id,
		int in_w, int in_h, AVPixelFormat in_fmt,
		int sampleRate, int audioBitRate, AVCodecID audio_codec_id,
		int in_ch, int in_sampleRate, AVSampleFormat in_sampleFmt,
		AVDictionary * opt_arg)
	{
		AVEncodeContext * pec;
		AVFormatContext *ofmt_ctx;
		AVOutputFormat *ofmt;
		int ret;

		pec = (AVEncodeContext *)malloc(sizeof(AVEncodeContext));
		if (!pec)
		{
			av_log(NULL, AV_LOG_ERROR, "ffCreateEncodeContext malloc return nullptr\n");
			return pec;
		}
		memset(pec, 0, sizeof(AVEncodeContext));
		pec->_width = w;
		pec->_height = h;
		pec->_fileName = strdup(filename);
		/*
		 * 创建输出上下文
		 */
		avformat_alloc_output_context2(&ofmt_ctx, NULL, fmt, filename);
		if (!ofmt_ctx){
			av_log(NULL, AV_LOG_ERROR, "avformat_alloc_output_context2 return failed.");
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
				av_log(NULL, AV_LOG_ERROR, "Add video stream failed\n");
				ffCloseEncodeContext(pec);
				return NULL;
			}
			pec->has_video = 1;
		}
		if (AV_CODEC_ID_NONE != audio_codec_id)
		{
			AVRational rat;
			rat.num = sampleRate;
			rat.den = 1;
			if (add_stream(pec, audio_codec_id, 0, 0, rat, audioBitRate) < 0)
			{
				av_log(NULL, AV_LOG_ERROR, "Add audio stream failed\n");
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
			if (open_video(pec, video_codec_id, 
				in_w,in_h,in_fmt,
				opt_arg) < 0)
			{
				ffCloseEncodeContext(pec);
				return NULL;
			}
			pec->encode_video = 1;
		}
		if (pec->has_audio)
		{
			if (open_audio(pec, audio_codec_id, 
				in_ch,in_sampleRate,in_sampleFmt,
				opt_arg) < 0)
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
				av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'\n", filename);
				ffCloseEncodeContext(pec);
				return NULL;
			}
		}
		/*
		 * 写入媒体头文件
		 */
		ret = avformat_write_header(ofmt_ctx, NULL);
		if (ret < 0) {
			av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file \n");
			ffCloseEncodeContext(pec);
			return NULL;
		}
		pec->isopen = 1;

		av_dump_format(ofmt_ctx, 0, filename, 1);

		/*
		 * 启动压缩压缩线程
		 */
		pec->write_mutex = new mutex_t();

		if (pec->has_audio)
			initAVCtx(pec, &pec->_actx, audio_encode_thread_proc);

		if (pec->has_video)
			initAVCtx(pec, &pec->_vctx, video_encode_thread_proc);

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

	static void ffStopThreadAVCtx(AVCtx *ctx)
	{
		if (ctx->encode_thread){
			ctx->cond->notify_one();
			ctx->encode_thread->join();
			delete ctx->mutex;
			delete ctx->cond;
			delete ctx->encode_thread;
		}
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
			ffStopThreadAVCtx(&pec->_actx);
			ffStopThreadAVCtx(&pec->_vctx);

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
					 * 如果有必要关闭文件流
					 */
					if (pec->_ctx->oformat && !(pec->_ctx->oformat->flags & AVFMT_NOFILE))
					{
						avio_closep(&pec->_ctx->pb);
					}
				}
				/*
				* 关闭编码器
				*/
				if (pec->_video_st)
					close_stream(pec->_video_st);
				if (pec->_audio_st)
					close_stream(pec->_audio_st);

				avformat_free_context(pec->_ctx);
			}

			ffFreeAVCtx(&pec->_vctx);
			ffFreeAVCtx(&pec->_actx);
			
			delete pec->write_mutex;

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

	int ffAddFrame(AVEncodeContext *pec, AVRaw *praw)
	{
		AVCtx * pctx;

		if (praw->type == RAW_IMAGE){
			pctx = &pec->_vctx;
			if (!pec->has_video)
				return 0; //igrone
		}
		else if (praw->type == RAW_AUDIO){
			pctx = &pec->_actx;
			if (!pec->has_audio)
				return 0; //igrone
		}
		else
		{
			av_log(NULL, AV_LOG_FATAL, "ffAddFrame unknow type of AVRaw\n");
			return -1;
		}
		if (pctx->stop_thread){
			av_log(NULL, AV_LOG_FATAL, "ffAddFrame %s encode thread already stoped.\n", praw->type == RAW_IMAGE?"video":"audio");
			return -1;
		}

		mutex_lock_t lk(*pctx->mutex);
		list_push_raw(&pctx->head, &pctx->tail, praw);
		pec->_nb_raws++;
		pec->_buffer_size += getAVRawSizeKB(praw);
		pctx->cond->notify_one();
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
}