#include "transcode.h"
#include "ffenc.h"
#include "ffdec.h"

namespace ff
{
	/*
	 * FIXBUG:ffmpeg��Ķ��뺯������ȫ��ȷ
	 * �� �� FFALIGN(360,32) = 364
	 * ����дһ���򵥵�32λ���뺯��
	 */
	int ffAlign32(int x)
	{
		int m = x % 4;
		return m == 0 ? x : x + 4 - m;
	}

	/*
	 * ��praw���·ֿ鵽�б���
	 */
	static void rebuffer_sample(AVRaw *praw, AVRaw **head, AVRaw **tail, AVSampleFormat fmt, int channel, int samples)
	{
		AVRaw * pnew;
		int ds, ss;

		if (praw)
		{
			/* ������ǿյľͲ���һ���µ� */
			if (!(*tail))
			{
				pnew = make_audio_raw(fmt, channel, samples);
				if (pnew)
					list_push_raw(head, tail, pnew);
				else
					return;
			}

			/* ������һ�������ģ�����Ҫ�ڷ���һ�� */
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
				/* dsĿ�Ŀռ䣬ssʣ��δ������� */
				ds = pnew->samples - pnew->seek_sample;
				ss = praw->samples - offset;
				if (ds >= ss)
				{
					/* �ռ��㹻����praw */
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
					{ /* ������ж�������� */
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
	 * ���ͷ��������raw�ͷ��ظ�raw�����б��е���������������Ͳ������б���NULL
	 */
	static AVRaw * rebuffer_pop_raw(AVRaw **head, AVRaw **tail)
	{
		if (*head && (*head)->seek_sample == (*head)->samples)
			return list_pop_raw(head, tail);
		return NULL;
	}

	/*
	 * ���㷨���ڲ��ı�֡�ʵ�ת�룬�����ı���Ƶ�Ĵ�С����ʽ��������
	 * ���֡�ʸı����Ҫɾ��֡��������֡��������ƵҲ��Ҫ��Ӧ��ת����
	 * ʱ���Ҫ���¼��㣬�����㷨�ĸ��ӶȽ����Ӻܶࡣ
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

		av_dict_set(&opt, "strict", "-2", 0);    //aac ��������ʵ�����ʵ���Ҫstrict -2����
		av_dict_set(&opt, "threads", "4", 0); //�������ö��߳�ѹ��

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

			pec = ffCreateEncodeContext(output, fmt, w, h, ffGetFrameRate(pdc), bitRate, video_id, sampleRate, audioBitRate, audio_id, opt);
			if (pec)
			{
				/*
				 * �������������Ƶ���������ͱ���������Ƶ�Ĳ�������ͬ����Ҫ�������½��зְ�
				 * ���������Ƶ���ݽ��л��壬���ﵽ�������Ĳ�������д�롣
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
							{ /* ��һ�ν�������������������֯ */
								rebuffer_sample(praw2, &head, &tail, enc_fmt, enc_channel, enc_samples);
								/* ��Ҫ�ͷ� */
								release_raw(praw2);
							}
							praw2 = rebuffer_pop_raw(&head, &tail);
							if (!praw2)
								break;
						}
						/*
						 * ���ѹ������̫��͵ȴ�
						 */
						while (ffGetBufferSizeKB(pec) > TRANSCODE_MAXBUFFER_SIZE)
						{
							/*
							 * ѹ���߳�ֹͣ�����˻��߱������ڵȴ�ι���ݾͲ��ٵȴ�
							 */
							if (pec->_stop_thread || pec->_encode_waiting)
							{
								break;
							}
							std::this_thread::sleep_for(std::chrono::milliseconds(10));
						}
						ret = ffAddFrame(pec, praw2);
						if (ret < 0)
							break;
						/*
						 * ���Ȼص�,����Ƶ������Ƶ
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
						/* �������Ҫ�������������²��� */
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