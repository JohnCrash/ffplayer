#include "ff.h"
#include "ffdepends.h"
#include "cocos2d.h"

namespace ff
{
/*
    static double cc_clock()
    {
        double clock;
#if CC_TARGET_PLATFORM == CC_PLATFORM_WIN32
        clock = (double)GetTickCount();
#else
        timeval tv;
        gettimeofday(&tv,NULL);
        clock = (double)tv.tv_sec*1000.0 + (double)(tv.tv_usec)/1000.0;
#endif
        return clock;
    }
*/
    VideoPixelFormat FFVideo::getPixelFormat()
    {
        return VIDEO_PIX_YUV420P;
    }

	static bool isInitFF = false;

	FFVideo::FFVideo() :_ctx(nullptr)
	{
		if (!isInitFF){
			initFF();
			isInitFF = true;
		}
	}

	FFVideo::~FFVideo()
	{
		close();
	}

	bool FFVideo::open(const char *url)
	{
		_first = true;
		close();
        _ctx = stream_open(url, NULL);
		return _ctx != nullptr;
	}

	void FFVideo::seek(double t)
	{
		VideoState* _vs = (VideoState*)_ctx;
		if (_vs)
		{
			if (t > length())
				t = length();
			if (t < 0)
				t = 0;
			double pos = cur_clock();
			int64_t ts = t * AV_TIME_BASE;
			int64_t ref = (int64_t)((t - pos) *AV_TIME_BASE);
			stream_seek(_vs, ts, ref, 0);
		}
	}

	bool FFVideo::isEnd() const
	{
		if (isOpen())
		{
			VideoState* is = (VideoState*)_ctx;
			if (is)
			{
				if ((!is->audio_st || (is->auddec.finished == is->audioq.serial && frame_queue_nb_remaining(&is->sampq) == 0)) &&
					(!is->video_st || (is->viddec.finished == is->videoq.serial && frame_queue_nb_remaining(&is->pictq) == 0)))
					return true;
			}
		}
		return false;
	}

	void FFVideo::set_preload_nb(int n)
	{
		VideoState* is = (VideoState*)_ctx;
		if (is)
		{
			is->nMIN_FRAMES = n;
		}
	}

	int FFVideo::preload_packet_nb() const
	{
		VideoState* is = (VideoState*)_ctx;
		if (is)
		{
		//	return FFMAX(is->videoq.nb_packets,is->audioq.nb_packets);
			if (is->video_st)
				return is->videoq.nb_packets;
			else if (is->audio_st)
				return is->audioq.nb_packets;
		}
		return -1;
	}

	double FFVideo::cur_clock() const
	{
		VideoState* _vs = (VideoState*)_ctx;
		if (_vs)
		{
			double pos;
			pos = get_master_clock(_vs);
			if (isnan(pos))
				pos = (double)_vs->seek_pos / AV_TIME_BASE;
			return pos;
		}
		return -1;
	}

	double FFVideo::cur() const
	{
		VideoState* _vs = (VideoState*)_ctx;
		if (_vs)
		{
			if (_vs->current > 0)
				return _vs->current;
			else
				return cur_clock();
		}
		return -1;
	}

	double FFVideo::length() const
	{
		VideoState* _vs = (VideoState*)_ctx;
		if (_vs && _vs->ic)
		{
			return (double)_vs->ic->duration / 1000000LL;
		}
		return 0;
	}

	bool FFVideo::hasVideo() const
	{
		VideoState* _vs = (VideoState*)_ctx;
		if (_vs)
		{
			return _vs->video_st ? true : false;
		}
		return false;
	}

	bool FFVideo::hasAudio() const
	{
		VideoState* _vs = (VideoState*)_ctx;
		if (_vs)
		{
			return _vs->audio_st ? true : false;
		}
		return false;
	}

	bool FFVideo::isPause() const
	{
		return isOpen() && is_stream_pause((VideoState*)_ctx);
	}

	bool FFVideo::isSeeking() const
	{
		if (!isOpen()) return false;

		VideoState* _vs = (VideoState*)_ctx;
		if (_vs == NULL) return false;
		return _vs->seek_req || _vs->step;
	}

	bool FFVideo::isPlaying() const
	{
		if (!hasVideo() || !isOpen()) return false;
		return !isPause();
	}

	bool FFVideo::isOpen() const
	{
		VideoState* _vs = (VideoState*)_ctx;
		return  (_vs && (_vs->audio_st || _vs->video_st));
	}

	int FFVideo::codec_width() const
	{
		if (!isOpen())return -1;
		VideoState* _vs = (VideoState*)_ctx;
		if (!_vs->video_st)return -1;
		if (!_vs->video_st->codec)return -1;
		return _vs->video_st->codec->width;
	}

	int FFVideo::codec_height() const
	{
		if (!isOpen())return -1;
		VideoState* _vs = (VideoState*)_ctx;
		if (!_vs->video_st)return -1;
		if (!_vs->video_st->codec)return -1;
		return _vs->video_st->codec->height;
	}

	int FFVideo::width() const
	{
		if (!isOpen())return -1;
		VideoState* _vs = (VideoState*)_ctx;
		return _vs->width;
	}

	int FFVideo::height() const
	{
		if (!isOpen())return -1;
		VideoState* _vs = (VideoState*)_ctx;
		return _vs->height;
	}

//    static double t0 = 0;
	void *FFVideo::refresh()
	{
//        if(t0==0)
//            t0=cc_clock();
//        CCLOG("refresh == %f ",cc_clock()-t0);
//        t0 = cc_clock();
        
		VideoState* _vs = (VideoState*)_ctx;
		if (_vs)
		{
			double r = 1.0 / 30.0;
			if (!is_stream_pause((VideoState*)_ctx))
			{
				video_refresh(_vs, &r);
			}
			if (_vs->pyuv420p.w > 0 && _vs->pyuv420p.h > 0)
			{
				if (_first)
				{
					pause();
					_first = false;
				}
				return &_vs->pyuv420p;
			}
		}
		return nullptr;
	}

    void *FFVideo::allocRgbBufferFormYuv420p(void *pyuv)
    {
		return yuv420pToRgb((yuv420p *)pyuv);
    }
    
    void FFVideo::freeRgbBuffer(void * prgb)
    {
        freeRgb(prgb);
    }

	void FFVideo::pause()
	{
		if (isOpen() && !is_stream_pause((VideoState*)_ctx) && !isSeeking())
		{
			toggle_pause((VideoState*)_ctx);
		}
	}

	void FFVideo::play()
	{
		if (isOpen() && is_stream_pause((VideoState*)_ctx) && !isSeeking())
		{
			toggle_pause((VideoState*)_ctx);
		}
	}

	void FFVideo::close()
	{
		if (_ctx)
		{
			stream_close((VideoState*)_ctx);
			_ctx = nullptr;
		}
	}

	bool FFVideo::isError() const
	{
		VideoState* _vs = (VideoState*)_ctx;
		if (_vs)
		{
			return _vs->errcode != 0;
		}
		return false;
	}

	const char * FFVideo::errorMsg() const
	{
		VideoState* _vs = (VideoState*)_ctx;
		if (_vs)
		{
			return _vs->errmsg;
		}
		return nullptr;
	}

	/*
	 * º∆À„ ”∆µªÚ“Ù∆µ¡˜µƒ∆Ωæ˘∞¸¬ £®√ø√Î∂‡…Ÿ∏ˆ∞¸£�?
	 */
	static double calc_avg_pocket_rate(AVStream *st)
	{
		if (st){
			if (st->avg_frame_rate.den>0 && st->avg_frame_rate.num>0) 
				return (double)st->avg_frame_rate.num / (double)st->avg_frame_rate.den;//»Áπ˚“—æ≠”–∆Ωæ˘∞¸¬ £¨÷±Ω”∑µªÿ

			if (st->time_base.den > 0){
				double tt = st->duration* (double)st->time_base.num / (double)st->time_base.den; //¡˜◊‹µƒ ±º�?
				if ( tt > 0 )
					return (double)st->nb_frames / tt;
			}
		}
		return -1;
	}

	static double calc_stream_preload_time(PacketQueue *pq, AVStream *st)
	{
		if (pq && pq->last_pkt && pq->first_pkt && st && st->time_base.den > 0){
			if (pq->last_pkt->pkt.buf != NULL){
				return (double)(pq->last_pkt->pkt.pts - pq->first_pkt->pkt.pts) *(double)st->time_base.num / (double)st->time_base.den;
			}
			else{
				/* 
				 * »Áπ˚ ”∆µ“—æ≠Ω· ¯◊Ó∫Û“ª∏ˆ∞¸µƒ ˝æ›≤ª∞¸¿®pts£¨PacketQueue «“ª∏ˆµ•œÚ∂”¡–Œ“≥¢ ‘¥”Õ∑≤øø™ º—∞’“◊Ó∫Û“ª∏ˆ’˝»∑µƒ∞�?
				 */
				MyAVPacketList *last = NULL;
				for (MyAVPacketList *it = pq->first_pkt; it != NULL; it = it->next){
					if (it->pkt.buf != NULL)
						last = it;
					else
						break;
				}
				if (last){
					return (double)(last->pkt.pts - pq->first_pkt->pkt.pts) *(double)st->time_base.num / (double)st->time_base.den;
				}
			}
		}
		return 0;
	}

	double FFVideo::preload_time()
	{
		VideoState* is = (VideoState*)_ctx;
		if (is)
		{
			//return FFMAX(calc_stream_preload_time(&is->videoq,is->video_st),calc_stream_preload_time(&is->audioq,is->audio_st));
			if (is->video_st)
				return calc_stream_preload_time(&is->videoq, is->video_st);
			else if (is->audio_st)
				return calc_stream_preload_time(&is->audioq, is->audio_st);
		}
		return -1;
	}

	bool FFVideo::set_preload_time(double t)
	{
		VideoState* is = (VideoState*)_ctx;
		if (is && t>=0 )
		{
			if (!is->video_st)return false;
			double apr = calc_avg_pocket_rate(is->video_st);
			if (apr > 0){
				set_preload_nb((int)(apr*t));
				return true;
			}
			apr = calc_avg_pocket_rate(is->audio_st);
			if (apr > 0){
				set_preload_nb((int)(apr*t));
				return true;
			}
			//»Áπ˚≤ªƒ‹’˝»∑º∆À„∆Ωæ˘∞¸¬ æÕª÷∏¥Œ™ƒ¨»œ…Ë÷√
			set_preload_nb(150);
		}
		return false;
	}
}