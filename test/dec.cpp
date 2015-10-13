#include "ffenc.h"

AVDictionary *codec_opts = NULL;

void av_log_error(int err)
{
	char errmsg[1024];
	av_strerror(err, errmsg, 1024);
	printf("avformat_open_input : %s\n", errmsg);
}

void av_log_streams(AVFormatContext *ic)
{
	printf("=============================\n");
	for (int i = 0; i < ic->nb_streams; i++){
		AVStream *st = ic->streams[i];
		AVCodecContext *cd = st->codec;
		printf("[%d] id=%d codec name=%s codec_type=%d codec_id=%d,codec_tag=%d avg_frame_rate=[%d/%d] start=%llu duration=%d\n",
			st->index, st->id, st->codec->codec_name, st->codec->codec_type,
			st->codec->codec_id, st->codec->codec_tag,
			st->avg_frame_rate.den, st->avg_frame_rate.num, //avg_frame_rate
			st->start_time, st->duration);

		printf("\t\t time_base=[%d/%d] \n",
			st->time_base.den, st->time_base.num);

		AVCodec *codec = avcodec_find_decoder(cd->codec_id);
		if (!codec){
			printf("no codec could be found with id %d \n", cd->codec_id);
			return;
		}
		printf("\t\t name=%s type=%d\n", codec->name, codec->type);
	}
}

void do_exit(int c){
}

//FIXME imporment it
int check_stream_specifier(AVFormatContext *s, AVStream *st, const char *spec)
{
	int ret = avformat_match_stream_specifier(s, st, spec);
	if (ret < 0)
		av_log(s, AV_LOG_ERROR, "Invalid stream specifier: %s.\n", spec);
	return ret;
}

AVDictionary *filter_codec_opts(AVDictionary *opts, enum AVCodecID codec_id,
	AVFormatContext *s, AVStream *st, AVCodec *codec)
{
	AVDictionary    *ret = NULL;
	AVDictionaryEntry *t = NULL;
	int            flags = s->oformat ? AV_OPT_FLAG_ENCODING_PARAM
		: AV_OPT_FLAG_DECODING_PARAM;
	char          prefix = 0;
	const AVClass    *cc = avcodec_get_class();

	if (!codec)
		codec = s->oformat ? avcodec_find_encoder(codec_id)
		: avcodec_find_decoder(codec_id);

	switch (st->codec->codec_type) {
	case AVMEDIA_TYPE_VIDEO:
		prefix = 'v';
		flags |= AV_OPT_FLAG_VIDEO_PARAM;
		break;
	case AVMEDIA_TYPE_AUDIO:
		prefix = 'a';
		flags |= AV_OPT_FLAG_AUDIO_PARAM;
		break;
	case AVMEDIA_TYPE_SUBTITLE:
		prefix = 's';
		flags |= AV_OPT_FLAG_SUBTITLE_PARAM;
		break;
	}

	while (t = av_dict_get(opts, "", t, AV_DICT_IGNORE_SUFFIX)) {
		char *p = strchr(t->key, ':');

		/* check stream specification in opt name */
		if (p)
			switch (check_stream_specifier(s, st, p + 1)) {
			case  1: *p = 0; break;
			case  0:         continue;
			default:         do_exit(NULL);
		}

		if (av_opt_find(&cc, t->key, NULL, flags, AV_OPT_SEARCH_FAKE_OBJ) ||
			!codec ||
			(codec->priv_class &&
			av_opt_find(&codec->priv_class, t->key, NULL, flags,
			AV_OPT_SEARCH_FAKE_OBJ)))
			av_dict_set(&ret, t->key, t->value, 0);
		else if (t->key[0] == prefix &&
			av_opt_find(&cc, t->key + 1, NULL, flags,
			AV_OPT_SEARCH_FAKE_OBJ))
			av_dict_set(&ret, t->key + 1, t->value, 0);

		if (p)
			*p = ':';
	}
	return ret;

}

//FIXME imporment it
AVDictionary **setup_find_stream_info_opts(AVFormatContext *s,
	AVDictionary *codec_opts)
{
	int i;
	AVDictionary **opts;

	if (!s->nb_streams)
		return NULL;
	opts = (AVDictionary **)av_mallocz_array(s->nb_streams, sizeof(*opts));
	if (!opts) {
		av_log(NULL, AV_LOG_ERROR,
			"Could not alloc memory for stream options.\n");
		return NULL;
	}
	for (i = 0; i < s->nb_streams; i++)
		opts[i] = filter_codec_opts(codec_opts, s->streams[i]->codec->codec_id,
		s, s->streams[i], NULL);
	return opts;
}

int read_media_file(const char *filename, const char *outfile)
{
	AVFormatContext *ic, *ofmt_ctx;
	AVInputFormat *file_iformat = NULL;
	AVDictionary * format_opts, ** opts = NULL;
	AVPacket pkt1, *pkt = &pkt1;
	int err, i, ret;
	int orig_nb_streams;
	AVOutputFormat *ofmt = NULL;

#if CONFIG_AVDEVICE
	avdevice_register_all();
#endif
#if CONFIG_AVFILTER
	avfilter_register_all();
#endif
	av_register_all();
	avformat_network_init();

	ic = avformat_alloc_context();
	if (!ic){
		printf("avformat_alloc_context : could not allocate context.\n");
		return -1;
	}
	format_opts = NULL;
	err = avformat_open_input(&ic, filename, file_iformat, &format_opts);
	if (err < 0){
		av_log_error(err);
		return err;
	}

	av_format_inject_global_side_data(ic);

	//opts = setup_find_stream_info_opts(ic, codec_opts);
	orig_nb_streams = ic->nb_streams;
	err = avformat_find_stream_info(ic, opts);
	/*
	for (i = 0; i < orig_nb_streams; i++)
	av_dict_free(&opts[i]);
	av_freep(&opts);
	*/
	if (err < 0){
		av_log_error(err);
		return err;
	}

	av_log_streams(ic);


	/*
	* 输出到新的文件中，为输出流产生信息
	*/
	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, outfile);
	if (!ofmt_ctx){
		printf("avformat_alloc_output_context2 return failed.");
		return 0;
	}
	for (i = 0; i < ic->nb_streams; i++) {
		AVStream *in_stream = ic->streams[i];
		AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
		if (!out_stream) {
			printf("Failed allocating output stream\n");
			ret = AVERROR_UNKNOWN;
			return 0;
		}

		ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
		if (ret < 0) {
			printf("Failed to copy context from input to output stream codec context\n");
			return 0;
		}
		out_stream->codec->codec_tag = 0;
		if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
			out_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}
	/*
	* 打开一输出文件流
	*/
	ofmt = ofmt_ctx->oformat;
	if (!(ofmt->flags & AVFMT_NOFILE)) {
		ret = avio_open(&ofmt_ctx->pb, outfile, AVIO_FLAG_WRITE);
		if (ret < 0) {
			printf("Could not open output file '%s'\n", outfile);
			return 0;
		}
	}
	ret = avformat_write_header(ofmt_ctx, NULL);
	if (ret < 0) {
		printf("Error occurred when opening output file\n");
		return 0;
	}
	av_dump_format(ofmt_ctx, 0, outfile, 1);
	int total_size = 0;
	int total_frame = 0;
	for (;;){
		ret = av_read_frame(ic, pkt);

		if (ret < 0){
			av_log_error(ret);
			if (ret == AVERROR_EOF || avio_feof(ic->pb))
				break;
			if (ic->pb&&ic->pb->error)
				break;
		}
		total_size += pkt->size;
		total_frame++;
		AVStream * st = ic->streams[pkt->stream_index];
		printf("[%d] size=%d dts=%llu pts=%llu flags=%X dur=%d time=%.2fs\n",
			pkt->stream_index, pkt->size, pkt->dts, pkt->pts, pkt->flags, pkt->duration,
			((double)pkt->pts*st->time_base.num) / (double)st->time_base.den);

		AVStream *in_stream, *out_stream;
		in_stream = ic->streams[pkt->stream_index];
		out_stream = ic->streams[pkt->stream_index];
		/*
		重新设置dts和pts?
		*/
		pkt->pts = av_rescale_q_rnd(pkt->pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt->dts = av_rescale_q_rnd(pkt->dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt->duration = av_rescale_q(pkt->duration, in_stream->time_base, out_stream->time_base);
		pkt->pos = -1;

		ret = av_interleaved_write_frame(ofmt_ctx, pkt);
		if (ret < 0) {
			printf("Error muxing packet\n");
			break;
		}
		av_free_packet(pkt);
	}
	printf("total size : %d bytes\ntotal frame : %d\n", total_size,total_frame);
	avformat_close_input(&ic);

	av_write_trailer(ofmt_ctx);
	/* close output */
	if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
		avio_closep(&ofmt_ctx->pb);

	avformat_free_context(ofmt_ctx);
	return 0;
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

int read_trancode(const char *filename, const char *outfile)
{
	AVFormatContext *ic;
	AVInputFormat *file_iformat = NULL;
	AVDictionary * format_opts, ** opts = NULL;
	AVPacket pkt1, *pkt = &pkt1;
	int err, i, ret;
	int orig_nb_streams;
	AVOutputFormat *ofmt = NULL;

#if CONFIG_AVDEVICE
	avdevice_register_all();
#endif
#if CONFIG_AVFILTER
	avfilter_register_all();
#endif
	av_register_all();
	avformat_network_init();

	ic = avformat_alloc_context();
	if (!ic){
		printf("avformat_alloc_context : could not allocate context.\n");
		return -1;
	}
	format_opts = NULL;
	err = avformat_open_input(&ic, filename, file_iformat, &format_opts);
	if (err < 0){
		av_log_error(err);
		return err;
	}

	av_format_inject_global_side_data(ic);

	//opts = setup_find_stream_info_opts(ic, codec_opts);
	orig_nb_streams = ic->nb_streams;
	err = avformat_find_stream_info(ic, opts);
	/*
	for (i = 0; i < orig_nb_streams; i++)
	av_dict_free(&opts[i]);
	av_freep(&opts);
	*/
	if (err < 0){
		av_log_error(err);
		return err;
	}
	int w, h, sample_rate;
	AVFrame * frame;
	AVFrame * aframe;

	frame = 0;
	aframe = 0;
	w = h = sample_rate = 0;

	for (i = 0; i < ic->nb_streams; i++) {
		AVStream *in_stream = ic->streams[i];
		AVCodecContext *ctx = in_stream[0].codec;
		AVCodec *codec;
		AVDictionary *opts = NULL;
		if (ctx->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			int nb_samples;
			if (ctx->codec && ctx->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
				nb_samples = 10000;
			else
				nb_samples = ctx->frame_size;

			sample_rate = ctx->sample_rate;
			aframe = alloc_audio_frame(ctx->sample_fmt, ctx->channel_layout,
				ctx->sample_rate, nb_samples);
			codec = avcodec_find_decoder(ctx->codec_id);

			int ret = avcodec_open2(ctx, codec, &opts);
			if (ret < 0)
			{
				return -1;
			}
		}
		else if (ctx->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			w = ctx->width;
			h = ctx->height;
			frame = alloc_picture(ctx->pix_fmt, w, h);
			codec = avcodec_find_decoder(ctx->codec_id);

			int ret = avcodec_open2(ctx, codec, &opts);
			if (ret < 0)
			{
				return -1;
			}
		}
	}

	//sample_rate = 0;

	AVDictionary * opt = NULL;
	av_dict_set(&opt, "strict", "-2", 0); //aac 编码器是实验性质的需要strict -2参数
	av_dict_set(&opt, "threads", "4", 0); //可以启用多线程压缩
	AVEncodeContext* pec = ffCreateEncodeContext("g:\\test.mp4",
		w, h, 18, 400000, w == 0 ? AV_CODEC_ID_NONE : AV_CODEC_ID_MPEG4,
		sample_rate, 64000, sample_rate==0? AV_CODEC_ID_NONE: AV_CODEC_ID_AAC, opt);

	av_log_streams(ic);

	int total_size = 0;
	int total_frame = 0;
	int channel = ffGetAudioChannels(pec);
	int nb_samples = ffGetAudioSamples(pec);
	int got_frame;
	if (pec){
		for (;;){
			ret = av_read_frame(ic, pkt);

			if (ret < 0){
				av_log_error(ret);
				if (ret == AVERROR_EOF || avio_feof(ic->pb))
					break;
				if (ic->pb&&ic->pb->error)
					break;
			}
			total_size += pkt->size;
			total_frame++;
			
			AVStream * st = ic->streams[pkt->stream_index];
			printf("[%d] size=%d dts=%llu pts=%llu flags=%X dur=%d time=%.2fs\n",
				pkt->stream_index, pkt->size, pkt->dts, pkt->pts, pkt->flags, pkt->duration,
				((double)pkt->pts*st->time_base.num) / (double)st->time_base.den);

			AVStream *in_stream, *out_stream;
			in_stream = ic->streams[pkt->stream_index];
			AVCodecContext *ctx = in_stream->codec;
			if (ctx->codec_type == AVMEDIA_TYPE_AUDIO)
			{
				ret = avcodec_decode_audio4(ctx, aframe, &got_frame, pkt);
				if (got_frame)
				{
					AVRaw  *praw = make_audio_raw((AVSampleFormat)aframe->format,channel , nb_samples);
					if (praw)
					{
						av_samples_copy(praw->data, aframe->data, 0, 0, nb_samples, channel, (AVSampleFormat)aframe->format);
						ffAddFrame(pec, praw);
					}
				}
			}
			else if (ctx->codec_type == AVMEDIA_TYPE_VIDEO)
			{
				ret = avcodec_decode_video2(ctx,frame,&got_frame,pkt);
				if (got_frame){
					AVRaw  *praw = make_image_raw((AVPixelFormat)frame->format, w, h);
					if (praw)
					{
						av_image_copy(praw->data, praw->linesize, (const uint8_t **)frame->data, frame->linesize, (AVPixelFormat)frame->format, w, h);
						ffAddFrame(pec, praw);
					}
				}
			}
			/*
			重新设置dts和pts?
			*/
			/*
			pkt->pts = av_rescale_q_rnd(pkt->pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
			pkt->dts = av_rescale_q_rnd(pkt->dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
			pkt->duration = av_rescale_q(pkt->duration, in_stream->time_base, out_stream->time_base);
			pkt->pos = -1;
			*/
			av_free_packet(pkt);
		}
		ffFlush(pec);
		ffCloseEncodeContext(pec);
	}

	printf("total size : %d bytes\ntotal frame : %d\n", total_size, total_frame);
	avformat_close_input(&ic);

	return 0;
}