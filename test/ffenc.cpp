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
			printf("no codec could be found with id %d \n",cd->codec_id);
			return;
		}
		printf("\t\t name=%s type=%d\n",codec->name,codec->type);
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

int read_media_file(const char *filename,const char *outfile)
{
	AVFormatContext *ic, *ofmt_ctx;
	AVInputFormat *file_iformat = NULL;
	AVDictionary * format_opts, ** opts = NULL;
	AVPacket pkt1,* pkt = &pkt1;
	int err,i,ret;
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
		AVStream * st = ic->streams[pkt->stream_index];
		printf("[%d] size=%d dts=%llu pts=%llu flags=%X dur=%d time=%.2fs\n",
			pkt->stream_index,pkt->size,pkt->dts,pkt->pts,pkt->flags,pkt->duration,
			((double)pkt->pts*st->time_base.num)/(double)st->time_base.den);

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
	printf("total size : %d bytes\n", total_size);
	avformat_close_input(&ic);

	av_write_trailer(ofmt_ctx);
	/* close output */
	if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
		avio_closep(&ofmt_ctx->pb);

	avformat_free_context(ofmt_ctx);
	return 0;
}

/**
* 设置日志输出函数
*/
static tLogFunc _gLogFunc = NULL;
void ffSetLogHanlder(tLogFunc logfunc)
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

/**
 * 创建编码上下文
 */
AVEncodeContext* ffCreateEncodeContext(const char* filename, 
	int w, int h, int frameRate, int videoBitRate,AVCodecID video_codec_id,
	int sampleRate,int audioBitRate, AVCodecID audio_codec_id)
{
	AVEncodeContext * pec;
	AVFormatContext *ofmt_ctx;
	AVOutputFormat *ofmt;
	int ret,err,i;

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
	if (add_stream(pec, video_codec_id, w, h, frameRate, videoBitRate) < 0)
	{
		ffLog("Add video stream failed\n");
		ffCloseEncodeContext(pec);
		return NULL;
	}
	if (add_stream(pec, audio_codec_id, 0, 0, sampleRate, audioBitRate) < 0)
	{
		ffLog("Add audio stream failed\n");
		ffCloseEncodeContext(pec);
		return NULL;
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

	return pec;
}

/**
* 关闭编码上下文
*/
void ffCloseEncodeContext(AVEncodeContext *pec)
{
	if (pec)
	{
		if (pec->_ctx)
		{
			/* Write the trailer, if any. The trailer must be written before you
			* close the CodecContexts open when you wrote the header; otherwise
			* av_write_trailer() may try to use memory that was freed on
			* av_codec_close(). 
			*/
			av_write_trailer(pec->_ctx);

			/*
			 * 关闭编码器
			 */
			if (pec->_video_st)
				close_stream(pec->_video_st);
			if (pec->_audio_st)
				close_stream(pec->_audio_st);

			/*
			 * 如果有必要关闭文件流
			 */
			if (pec->_ctx->oformat && !(pec->_ctx->oformat->flags & AVFMT_NOFILE))
			{
				avio_closep(&pec->_ctx->pb);
			}
			avformat_free_context(pec->_ctx);
		}
		free((void*)pec->_fileName);
	}
	free(pec);
}