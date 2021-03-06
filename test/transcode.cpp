#include "transcode.h"
#include "ffenc.h"
#include "ffdec.h"

namespace ff
{
	/*
	 * FIXBUG:ffmpeg库的对齐函数不完全精确
	 * 如 ： FFALIGN(360,32) = 364
	 * 这里写一个简单的32位对齐函数
	 */
	int ffAlign32(int x)
	{
		int m = x % 4;
		return m == 0 ? x : x + 4 - m;
	}

	/*
	 * 将praw重新分块到列表中
	 */
	static void rebuffer_sample(AVRaw *praw, AVRaw **head, AVRaw **tail, AVSampleFormat fmt, int channel, int samples)
	{
		AVRaw * pnew;
		int ds, ss;

		if (praw)
		{
			/* 如果表还是空的就插入一个新的 */
			if (!(*tail))
			{
				pnew = make_audio_raw(fmt, channel, samples);
				if (pnew)
					list_push_raw(head, tail, pnew);
				else
					return;
			}

			/* 如果最后一个是满的，就需要在分配一个 */
			if ((*tail)->samples == (*tail)->seek_sample)
			{
				pnew = make_audio_raw(fmt, channel, samples);
				if (pnew)
					list_push_raw(head, tail, pnew);
				else
					return;
			}
			else
			{
				pnew = *tail;
			}

			int offset = 0;
			while (offset < praw->samples)
			{
				/* ds目的空间，ss剩余未填充数据 */
				ds = pnew->samples - pnew->seek_sample;
				ss = praw->samples - offset;
				if (ds >= ss)
				{
					/* 空间足够容纳praw */
					av_samples_copy(pnew->data, praw->data, pnew->seek_sample, offset, ss, channel, fmt);
					pnew->seek_sample += ss;
					return;
				}
				else
				{
					av_samples_copy(pnew->data, praw->data, pnew->seek_sample, offset, ds, channel, fmt);
					offset += ds;
					pnew->seek_sample += ds;
					if (pnew->seek_sample == pnew->samples)
					{ /* 如果还有多余的数据 */
						pnew = make_audio_raw(fmt, channel, samples);
						if (pnew)
							list_push_raw(head, tail, pnew);
						else
							return;
					}
				}
			}
		}
	}

	/*
	 * 如果头是完整的raw就返回该raw并从列表中弹出。如果不完整就不操作列表返回NULL
	 */
	static AVRaw * rebuffer_pop_raw(AVRaw **head, AVRaw **tail)
	{
		if (*head && (*head)->seek_sample == (*head)->samples)
			return list_pop_raw(head, tail);
		return NULL;
	}

	/*
	 * 该算法基于不改变帧率的转码，仅仅改变视频的大小，格式，和码率
	 * 如果帧率改变就需要删除帧或者增加帧，对于音频也需要相应的转换。
	 * 时间戳要重新计算，这样算法的复杂度将增加很多。
	 */
	int ffTranscode(const char *input, const char *output, const char *fmt,
		AVCodecID video_id, float width, float height, int bitRate,
		AVCodecID audio_id, int audioBitRate, transproc_t progress)
	{
		AVDecodeCtx * pdc;
		AVEncodeContext * pec;
		AVDictionary *opt = NULL;
		int sampleRate = 44100;
		int ret = 0;
		int w, h;
		int64_t total, i;

		av_dict_set(&opt, "strict", "-2", 0);    //aac 编码器是实验性质的需要strict -2参数
		av_dict_set(&opt, "threads", "4", 0); //可以启用多线程压缩

		pdc = ffCreateDecodeContext(input, opt);
		if (pdc)
		{
			if (!pdc->has_audio)
			{
				audio_id = AV_CODEC_ID_NONE;
			}
			else
			{
				video_id = video_id == AV_CODEC_COPY ? pdc->_video_st->codec->codec_id : video_id;
				sampleRate = pdc->_audio_st->codec->sample_rate;
			}
			if (!pdc->has_video)
			{
				video_id = AV_CODEC_ID_NONE;
			}
			else
			{
				audio_id = audio_id == AV_CODEC_COPY ? pdc->_audio_st->codec->codec_id : audio_id;
			}
			w = (int)(width*ffGetFrameWidth(pdc));
			h = (int)(height*ffGetFrameHeight(pdc));
			w = ffAlign32(w);
			h = ffAlign32(h);
			bitRate = bitRate <= 0 ? 8 * 1024 * 1024 : bitRate;
			audioBitRate = audioBitRate <= 0 ? 64 * 1024 : audioBitRate;

			if (pdc->has_video)
			{
				total = pdc->_video_st->nb_frames;
				if (total <= 0 && pdc->_video_st->time_base.den > 0 && pdc->_video_st->duration != AV_NOPTS_VALUE)
				{
					double sl = (double)(pdc->_video_st->duration*pdc->_video_st->time_base.num) / (double)pdc->_video_st->time_base.den;
					total = (int64_t)(pdc->_video_st->r_frame_rate.num*sl / (double)pdc->_video_st->r_frame_rate.den);
				}
			}
			else if (pdc->has_audio)
			{
				total = pdc->_audio_st->nb_frames;
			}
			else
				total = 0;
			i = 1;

			pec = ffCreateEncodeContext(output, fmt, w, h, ffGetFrameRate(pdc), bitRate, video_id,
				w, h, AV_PIX_FMT_YUV420P,
				sampleRate, audioBitRate, audio_id, 
				2, 44100,AV_SAMPLE_FMT_S16, 
				opt);
			if (pec)
			{
				/*
				 * 如果解码器的音频采样数，和编码器的音频的采样数不同就需要进行重新进行分包
				 * 将解码的音频数据进行缓冲，当达到编码器的采样数就写入。
				 */
				AVSampleFormat enc_fmt;
				int enc_channel;
				int enc_samples;
				AVRaw * praw, *head, *tail, *praw2;

				tail = head = NULL;
				if (pec->has_audio&&pdc->has_audio)
				{
					enc_samples = pec->_actx.frame->nb_samples;
					enc_fmt = (AVSampleFormat)pec->_actx.frame->format;
					enc_channel = pec->_actx.frame->channels;
				}
				else
				{
					enc_fmt = AV_SAMPLE_FMT_NONE;
					enc_channel = 0;
					enc_samples = 0;
				}
				do
				{
					praw = ffReadFrame(pdc);
					praw2 = praw;
					bool need_resamples = false;

					if (praw)
						need_resamples = (enc_samples&&praw->type == RAW_AUDIO&&praw->samples != enc_samples);

					while (praw2)
					{
						if (need_resamples)
						{
							if (praw == praw2)
							{ /* 第一次进来来，将数据重新组织 */
								rebuffer_sample(praw2, &head, &tail, enc_fmt, enc_channel, enc_samples);
								/* 需要释放 */
								release_raw(praw2);
							}
							praw2 = rebuffer_pop_raw(&head, &tail);
							if (!praw2)
								break;
						}
						/*
						 * 如果压缩队列太多就等待
						 */
						while (ffGetBufferSizeKB(pec) > TRANSCODE_MAXBUFFER_SIZE)
						{
							/*
							 * 压缩线程停止工作了或者编码器在等待喂数据就不再等待
							 */
							if (ffIsWaitingOrStop(pec))
								break;
							std::this_thread::sleep_for(std::chrono::milliseconds(10));
						}
						ret = ffAddFrame(pec, praw2);
						if (ret < 0)
							break;
						/*
						 * 进度回调,有视频优先视频
						 */
						if (progress)
						{
							if (pdc->has_video)
							{
								if (praw->type == RAW_IMAGE)
									progress(total, i++);
							}
							else if (pdc->has_audio)
							{
								if (praw->type == RAW_AUDIO)
									progress(total, i++);
							}
						}
						/* 如果不需要对声音进行重新采样 */
						if (!need_resamples)
							break;
					}
				} while (praw);

				ffFlush(pec);
				ffCloseEncodeContext(pec);
			}
			ffCloseDecodeContext(pdc);
			return ret;
		}

		av_dict_free(&opt);
		return -1;
	}
}