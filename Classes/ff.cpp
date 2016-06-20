/*
	ª˘”⁄ffmpeg¿˝◊”≤•∑≈∆˜ffplay
*/
#include "ffdepends.h"
#include "cmdutils_cxx.h"
#include "ff.h"

#include "cocos2d.h"

#if CONFIG_VIDEOTOOLBOX
extern "C" int videotoolbox_init(AVCodecContext *s);
#endif

namespace ff{
/* options specified by the user */
//AVInputFormat *file_iformat;
//const char *input_filename;
const char *window_title;
int fs_screen_width;
int fs_screen_height;
int default_width = 640;
int default_height = 480;
int screen_width = 0;
int screen_height = 0;
int audio_disable;
int video_disable;
int subtitle_disable;
const char* wanted_stream_spec[AVMEDIA_TYPE_NB] = { 0 };
int seek_by_bytes = -1;
int display_disable;
int show_status = 0;
int av_sync_type = AV_SYNC_AUDIO_MASTER;
int64_t start_time = AV_NOPTS_VALUE;
int64_t duration = AV_NOPTS_VALUE;
int fast = 0;
int genpts = 0;
int lowres = 0;
int decoder_reorder_pts = -1;
int autoexit;
//int exit_on_keydown;
//int exit_on_mousedown;
int loop = 1;
int framedrop = -1;
int infinite_buffer = -1;
enum ShowMode show_mode = SHOW_MODE_NONE;
const char *audio_codec_name;
const char *subtitle_codec_name;
const char *video_codec_name;
double rdftspeed = 0.02;
int64_t cursor_last_shown;
//int cursor_hidden = 0;
#if CONFIG_AVFILTER
const char **vfilters_list = NULL;
int nb_vfilters = 0;
char *afilters = NULL;
#endif
int autorotate = 1;

/* current context */
int64_t audio_callback_time;

AVPacket flush_pkt;

//SwsContext *sws_opts;
//AVDictionary *swr_opts, *sws_dict;
//AVDictionary *format_opts, *codec_opts, *resample_opts;

int CCLog(const char *pszFmt, ...)
{
	char szLine[1024 * 10];
	va_list	va;
	va_start(va, pszFmt);
	vsprintf(szLine, pszFmt, va);
	va_end(va);

	cocos2d::CCLog(szLine,"");
	return 0;
}

void My_log(void* p,int inval,const char* fmt,...)
{
	char buf[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, 1024 - 3, fmt, args);
	va_end(args);
	CCLog(buf);
}
    
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

static inline
int cmp_audio_fmts(enum AVSampleFormat fmt1, int64_t channel_count1,
enum AVSampleFormat fmt2, int64_t channel_count2)
{
	/* If channel count == 1, planar and non-planar formats are the same */
	if (channel_count1 == 1 && channel_count2 == 1)
		return av_get_packed_sample_fmt(fmt1) != av_get_packed_sample_fmt(fmt2);
	else
		return channel_count1 != channel_count2 || fmt1 != fmt2;
}

static inline
int64_t get_valid_channel_layout(int64_t channel_layout, int channels)
{
	if (channel_layout && av_get_channel_layout_nb_channels(channel_layout) == channels)
		return channel_layout;
	else
		return 0;
}

#if 0
void print_error(const char *filename, int err)
{
	char errbuf[128];
	const char *errbuf_ptr = errbuf;

	if (av_strerror(err, errbuf, sizeof(errbuf)) < 0)
		errbuf_ptr = strerror(AVUNERROR(err));
	My_log(NULL, AV_LOG_ERROR, "%s: %s\n", filename, errbuf_ptr);
}
#endif

void do_exit(VideoState *is)
{
	if (is) {
		stream_close(is);
	}
	av_lockmgr_register(NULL);
	//uninit_opts();
#if CONFIG_AVFILTER
	av_freep(&vfilters_list);
#endif
	avformat_network_deinit();
	Quit();
	My_log(NULL, AV_LOG_QUIET, "FFMPEG do_exit!");
	//exit(0);
}

static void calculate_display_rect(Rect *rect,
	int scr_xleft, int scr_ytop, int scr_width, int scr_height,
	int pic_width, int pic_height, AVRational pic_sar)
{
	float aspect_ratio;
	int width, height, x, y;

	if (pic_sar.num == 0)
		aspect_ratio = 0;
	else
		aspect_ratio = av_q2d(pic_sar);

	if (aspect_ratio <= 0.0)
		aspect_ratio = 1.0;
	aspect_ratio *= (float)pic_width / (float)pic_height;

	/* XXX: we suppose the screen has a 1.0 pixel ratio */
	height = scr_height;
	width = ((int)rint(height * aspect_ratio)) & ~1;
	if (width > scr_width) {
		width = scr_width;
		height = ((int)rint(width / aspect_ratio)) & ~1;
	}
	x = (scr_width - width) / 2;
	y = (scr_height - height) / 2;
	rect->x = scr_xleft + x;
	rect->y = scr_ytop + y;
	rect->w = FFMAX(width, 1);
	rect->h = FFMAX(height, 1);
}

static void set_default_window_size(int width, int height, AVRational sar)
{
	Rect rect;
	calculate_display_rect(&rect, 0, 0, INT_MAX, height, width, height, sar);
	default_width = rect.w;
	default_height = rect.h;
}

uint8_t* yuv420pToRgb(yuv420p * pyuv420p)
{
    /*
    int w,h;
    Rect rect;
    
    w = pyuv420p->w;
    h = pyuv420p->h;
    Surface * psurface = CreateRGBSurface(SWSURFACE, w, h, 24, 0x000000ff,0x0000ff00,0x00ff0000,0xff000000);
    Overlay * pbmp = CreateYUVOverlay(w,h,
    		YV12_OVERLAY,
    		psurface);
    DisplayYUVOverlay(pbmp, &rect);
     */
    uint8_t Y,U,V,R,G,B;
    uint8_t *py, *pu, *pv;
    uint8_t *prgbline;
    int x2,linesize;
    uint8_t *pdata;
    pdata = (uint8_t *)malloc(pyuv420p->w*pyuv420p->h*3);
    linesize = pyuv420p->w*3;
    for(int y=0;y<pyuv420p->h;y++){
        py = pyuv420p->data[0]+y*pyuv420p->linesize[0];
        x2 = y>>1;
        pu = pyuv420p->data[1]+x2*pyuv420p->linesize[1];
        pv = pyuv420p->data[2]+x2*pyuv420p->linesize[2];
        prgbline = pdata+y*linesize;
        for(int x=0;x<pyuv420p->w;x++){
            Y = *(py+x);
            x2 = x>>1;
            U = *(pu+x2);
            V = *(pv+x2);
            
            
          // r = y + 1.13983f*(v-0.5f);
          // g = y - 0.39465f*(u-0.5f) - 0.58060f*(v-0.5f);
          // b = y + 2.03211f*(u-0.5f);
            
            R = Y + 1.4075f*(V-128);
            G = Y - 0.3455f*(U-128) - 0.7169f*(V-128);
            B = Y + 1.779f*(U-128);
            
            *prgbline++ = R;
            *prgbline++ = G;
            *prgbline++ = B;
        }
    }
    return pdata;
}

void freeRgb(void *pdata)
{
    free(pdata);
}
    
static int video_open(VideoState *is, int force_set_video_mode, Frame *vp)
{
	int w, h;

	/*
	FIXBUG : ∫Û√Êµƒ◊™ªª≤Ÿ◊˜ «1:1µƒ◊™ªª(DisplayYUVOverlay)£¨’‚¿Ô∏ˆ±«Èøˆª·µ˜’˚øÌ∏ﬂ±»(vp->sar)
	–ﬁ∏ƒŒ™ºÚµ•µƒ…Ë÷√øÌ∏ﬂŒ™÷°µƒøÌ∏ﬂ°£»Áπ˚–Ë“™ π”√œ‘ æøÌ∏ﬂ£¨ƒ«√¥◊Ó∫√Ã·π©“ª∏ˆ∫Ø ˝”√¿¥∑µªÿœ‘ æøÌ∏ﬂ£¨
	‘⁄≤ƒ÷ µƒ≤„√Ê…œΩ¯––Àı∑≈–ß¬ ª·±»Ωœ∏ﬂ°£
	if (vp && vp->width)
	set_default_window_size(vp->width, vp->height, vp->sar);
	*/

	default_width = vp->width;
	default_height = vp->height;

	w = default_width;
	h = default_height;

	w = FFMIN(16383, w);

	//SDL_SetVideoMode(w, h, 0, HWSURFACE | ASYNCBLIT | HWACCEL);
//	if (!is->pscreen2)
//		is->pscreen2 = (Surface*)CreateRGBSurface(SWSURFACE,w, h,24,0x000000ff,0x0000ff00,0x00ff0000,0xff000000);
//	if (!is->pscreen2) {
//		My_log(NULL, AV_LOG_FATAL, "SDL: could not set video mode - exiting\n");
//		do_exit(is);
//	}
	if (!window_title)
		window_title = is->filename;
	
	WM_SetCaption(window_title, window_title);

	is->width = w;
	is->height = h;

	return 0;
}


static inline int compute_mod(int a, int b)
{
	return a < 0 ? a%b + b : a%b;
}

#define ALPHA_BLEND(a, oldp, newp, s)\
	((((oldp << s) * (255 - (a))) + (newp * (a))) / (255 << s))

#define RGBA_IN(r, g, b, a, s)\
{\
	unsigned int v = ((const uint32_t *)(s))[0]; \
	a = (v >> 24) & 0xff; \
	r = (v >> 16) & 0xff; \
	g = (v >> 8) & 0xff; \
	b = v & 0xff; \
}

#define YUVA_IN(y, u, v, a, s, pal)\
{\
	unsigned int val = ((const uint32_t *)(pal))[*(const uint8_t*)(s)]; \
	a = (val >> 24) & 0xff; \
	y = (val >> 16) & 0xff; \
	u = (val >> 8) & 0xff; \
	v = val & 0xff; \
}

#define YUVA_OUT(d, y, u, v, a)\
{\
	((uint32_t *)(d))[0] = (a << 24) | (y << 16) | (u << 8) | v; \
}


#define BPP 1

static void blend_subrect(AVPicture *dst, const AVSubtitleRect *rect, int imgw, int imgh)
{
	int wrap, wrap3, width2, skip2;
	int y, u, v, a, u1, v1, a1, w, h;
	uint8_t *lum, *cb, *cr;
	const uint8_t *p;
	const uint32_t *pal;
	int dstx, dsty, dstw, dsth;

	dstw = av_clip(rect->w, 0, imgw);
	dsth = av_clip(rect->h, 0, imgh);
	dstx = av_clip(rect->x, 0, imgw - dstw);
	dsty = av_clip(rect->y, 0, imgh - dsth);
	lum = dst->data[0] + dsty * dst->linesize[0];
	cb = dst->data[1] + (dsty >> 1) * dst->linesize[1];
	cr = dst->data[2] + (dsty >> 1) * dst->linesize[2];

	width2 = ((dstw + 1) >> 1) + (dstx & ~dstw & 1);
	skip2 = dstx >> 1;
	wrap = dst->linesize[0];
	wrap3 = rect->pict.linesize[0];
	p = rect->pict.data[0];
	pal = (const uint32_t *)rect->pict.data[1];  /* Now in YCrCb! */

	if (dsty & 1) {
		lum += dstx;
		cb += skip2;
		cr += skip2;

		if (dstx & 1) {
			YUVA_IN(y, u, v, a, p, pal);
			lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
			cb[0] = ALPHA_BLEND(a >> 2, cb[0], u, 0);
			cr[0] = ALPHA_BLEND(a >> 2, cr[0], v, 0);
			cb++;
			cr++;
			lum++;
			p += BPP;
		}
		for (w = dstw - (dstx & 1); w >= 2; w -= 2) {
			YUVA_IN(y, u, v, a, p, pal);
			u1 = u;
			v1 = v;
			a1 = a;
			lum[0] = ALPHA_BLEND(a, lum[0], y, 0);

			YUVA_IN(y, u, v, a, p + BPP, pal);
			u1 += u;
			v1 += v;
			a1 += a;
			lum[1] = ALPHA_BLEND(a, lum[1], y, 0);
			cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u1, 1);
			cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v1, 1);
			cb++;
			cr++;
			p += 2 * BPP;
			lum += 2;
		}
		if (w) {
			YUVA_IN(y, u, v, a, p, pal);
			lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
			cb[0] = ALPHA_BLEND(a >> 2, cb[0], u, 0);
			cr[0] = ALPHA_BLEND(a >> 2, cr[0], v, 0);
			p++;
			lum++;
		}
		p += wrap3 - dstw * BPP;
		lum += wrap - dstw - dstx;
		cb += dst->linesize[1] - width2 - skip2;
		cr += dst->linesize[2] - width2 - skip2;
	}
	for (h = dsth - (dsty & 1); h >= 2; h -= 2) {
		lum += dstx;
		cb += skip2;
		cr += skip2;

		if (dstx & 1) {
			YUVA_IN(y, u, v, a, p, pal);
			u1 = u;
			v1 = v;
			a1 = a;
			lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
			p += wrap3;
			lum += wrap;
			YUVA_IN(y, u, v, a, p, pal);
			u1 += u;
			v1 += v;
			a1 += a;
			lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
			cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u1, 1);
			cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v1, 1);
			cb++;
			cr++;
			p += -wrap3 + BPP;
			lum += -wrap + 1;
		}
		for (w = dstw - (dstx & 1); w >= 2; w -= 2) {
			YUVA_IN(y, u, v, a, p, pal);
			u1 = u;
			v1 = v;
			a1 = a;
			lum[0] = ALPHA_BLEND(a, lum[0], y, 0);

			YUVA_IN(y, u, v, a, p + BPP, pal);
			u1 += u;
			v1 += v;
			a1 += a;
			lum[1] = ALPHA_BLEND(a, lum[1], y, 0);
			p += wrap3;
			lum += wrap;

			YUVA_IN(y, u, v, a, p, pal);
			u1 += u;
			v1 += v;
			a1 += a;
			lum[0] = ALPHA_BLEND(a, lum[0], y, 0);

			YUVA_IN(y, u, v, a, p + BPP, pal);
			u1 += u;
			v1 += v;
			a1 += a;
			lum[1] = ALPHA_BLEND(a, lum[1], y, 0);

			cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u1, 2);
			cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v1, 2);

			cb++;
			cr++;
			p += -wrap3 + 2 * BPP;
			lum += -wrap + 2;
		}
		if (w) {
			YUVA_IN(y, u, v, a, p, pal);
			u1 = u;
			v1 = v;
			a1 = a;
			lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
			p += wrap3;
			lum += wrap;
			YUVA_IN(y, u, v, a, p, pal);
			u1 += u;
			v1 += v;
			a1 += a;
			lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
			cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u1, 1);
			cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v1, 1);
			cb++;
			cr++;
			p += -wrap3 + BPP;
			lum += -wrap + 1;
		}
		p += wrap3 + (wrap3 - dstw * BPP);
		lum += wrap + (wrap - dstw - dstx);
		cb += dst->linesize[1] - width2 - skip2;
		cr += dst->linesize[2] - width2 - skip2;
	}
	/* handle odd height */
	if (h) {
		lum += dstx;
		cb += skip2;
		cr += skip2;

		if (dstx & 1) {
			YUVA_IN(y, u, v, a, p, pal);
			lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
			cb[0] = ALPHA_BLEND(a >> 2, cb[0], u, 0);
			cr[0] = ALPHA_BLEND(a >> 2, cr[0], v, 0);
			cb++;
			cr++;
			lum++;
			p += BPP;
		}
		for (w = dstw - (dstx & 1); w >= 2; w -= 2) {
			YUVA_IN(y, u, v, a, p, pal);
			u1 = u;
			v1 = v;
			a1 = a;
			lum[0] = ALPHA_BLEND(a, lum[0], y, 0);

			YUVA_IN(y, u, v, a, p + BPP, pal);
			u1 += u;
			v1 += v;
			a1 += a;
			lum[1] = ALPHA_BLEND(a, lum[1], y, 0);
			cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u, 1);
			cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v, 1);
			cb++;
			cr++;
			p += 2 * BPP;
			lum += 2;
		}
		if (w) {
			YUVA_IN(y, u, v, a, p, pal);
			lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
			cb[0] = ALPHA_BLEND(a >> 2, cb[0], u, 0);
			cr[0] = ALPHA_BLEND(a >> 2, cr[0], v, 0);
		}
	}
}

static Frame *frame_queue_peek(FrameQueue *f)
{
	return &f->queue[(f->rindex + f->rindex_shown) % f->max_size];
}

static Frame *frame_queue_peek_next(FrameQueue *f)
{
	return &f->queue[(f->rindex + f->rindex_shown + 1) % f->max_size];
}

static Frame *frame_queue_peek_last(FrameQueue *f)
{
	return &f->queue[f->rindex];
}

static Frame *frame_queue_peek_writable(FrameQueue *f,VideoState *is)
{
	/* wait until we have space to put a new frame */
	//lockMutex(f->mutex);
 //   double t0 = cc_clock();
	std::unique_lock<mutex_t> lk(*f->mutex);
//    if(&is->pictq==f){
//        My_log(0,0, "size %d , max = %d", f->size,f->max_size);
//    }
	while (f->size >= f->max_size &&
		!f->pktq->abort_request) {
		//waitCond(f->cond, f->mutex);
		f->cond->wait(lk);
	}
 //  if(&is->pictq ==f){
 //       My_log(0,0, "peek %f", cc_clock()-t0);
 //   }
	//unlockMutex(f->mutex);

	if (f->pktq->abort_request)
		return NULL;

	return &f->queue[f->windex];
}

static Frame *frame_queue_peek_readable(FrameQueue *f)
{
	/* wait until we have a readable a new frame */
	//lockMutex(f->mutex);
	std::unique_lock<mutex_t> lk(*f->mutex);
	while (f->size - f->rindex_shown <= 0 &&
		!f->pktq->abort_request) {
		//waitCond(f->cond, f->mutex);
		if (f->eof)
			return NULL;
		f->cond->wait(lk);
	}
	//unlockMutex(f->mutex);

	if (f->pktq->abort_request)
		return NULL;

	return &f->queue[(f->rindex + f->rindex_shown) % f->max_size];
}

static void frame_queue_push(FrameQueue *f)
{
	if (++f->windex == f->max_size)
		f->windex = 0;
	lockMutex(f->mutex);
	f->size++;
	signalCond(f->cond);
	unlockMutex(f->mutex);
}

static void frame_queue_unref_item(Frame *vp)
{
	av_frame_unref(vp->frame);
	avsubtitle_free(&vp->sub);
}

static void frame_queue_next(FrameQueue *f)
{
	if (f->keep_last && !f->rindex_shown) {
		f->rindex_shown = 1;
		return;
	}
	frame_queue_unref_item(&f->queue[f->rindex]);
	if (++f->rindex == f->max_size)
		f->rindex = 0;
	lockMutex(f->mutex);
	f->size--;
	signalCond(f->cond);
	unlockMutex(f->mutex);
}
/* jump back to the previous frame if available by resetting rindex_shown */
static int frame_queue_prev(FrameQueue *f)
{
	int ret = f->rindex_shown;
	f->rindex_shown = 0;
	return ret;
}

/* return the number of undisplayed frames in the queue */
int frame_queue_nb_remaining(FrameQueue *f)
{
	return f->size - f->rindex_shown;
}

/* return last shown position */
int64_t frame_queue_last_pos(FrameQueue *f)
{
	Frame *fp = &f->queue[f->rindex];
	if (f->rindex_shown && fp->serial == f->pktq->serial)
		return fp->pos;
	else
		return -1;
}
    
//static double _videoT = 0;

static void video_image_display(VideoState *is)
{
	Frame *vp;
//	Frame *sp;
//	AVPicture pict;
	Rect rect;
//	int i;

//    double t0 = cc_clock();
//    double t1,t2;
//    if(_videoT==0)
//        _videoT = t0;
	vp = frame_queue_peek(&is->pictq);
	if (vp->frame) {
        /*
		if (is->subtitle_st) {
			if (frame_queue_nb_remaining(&is->subpq) > 0) {
				sp = frame_queue_peek(&is->subpq);

				if (vp->pts >= sp->pts + ((float)sp->sub.start_display_time / 1000)) {
					LockYUVOverlay(vp->bmp);

					pict.data[0] = vp->bmp->pixels[0];
					pict.data[1] = vp->bmp->pixels[2];
					pict.data[2] = vp->bmp->pixels[1];

					pict.linesize[0] = vp->bmp->pitches[0];
					pict.linesize[1] = vp->bmp->pitches[2];
					pict.linesize[2] = vp->bmp->pitches[1];

					for (i = 0; i < (int)sp->sub.num_rects; i++)
						blend_subrect(&pict, sp->sub.rects[i],
						vp->bmp->w, vp->bmp->h);

					UnlockYUVOverlay(vp->bmp);
				}
			}
		}
         */
//        t1 = cc_clock();
		calculate_display_rect(&rect, is->xleft, is->ytop, is->width, is->height, vp->width, vp->height, vp->sar);

//        t2 = cc_clock();
        av_frame_unref((is->_currentFrame));
        av_frame_ref(is->_currentFrame,vp->frame);
        
        if(is->pyuv420p.w==-10) //first frame
            is->step = 0;

        is->pyuv420p.w = vp->frame->width;
        is->pyuv420p.h = vp->frame->height;
        is->pyuv420p.linesize[0] = vp->frame->linesize[0];
        is->pyuv420p.data[0] = vp->frame->data[0];
        is->pyuv420p.linesize[1] = vp->frame->linesize[1];
        is->pyuv420p.data[1] = vp->frame->data[1];
        is->pyuv420p.linesize[2] = vp->frame->linesize[2];
        is->pyuv420p.data[2] = vp->frame->data[2];
   //     if(is->toRGB){
   //         DisplayYUVOverlay(vp->bmp, &rect);
   //     }
		double pos = get_master_clock(is);
		if (!isnan(pos))
		{
			//≤ø∑÷mp3ª·”–“ª∏ˆÕºœÒ∑‚√Ê,’‚¿Ô»∑±£’‚÷÷«Èøˆœ¬current<0
			if (is->current == -1)
				is->current = -0.001;
			else
				is->current = pos;
		}
	//	if (!is->pscreen){
	//		is->pscreen = is->pscreen2;
	//		is->step = 0;
	//	}
	}
//    double t3 = cc_clock();
//    My_log(0,0, "video %f , loop %f , lock %f , calc %f ,disp %f",t3-t0,t3-_videoT,t1-t0,t2-t1,t3-t2);
//    _videoT = t3;
}

/* display the current picture, if any */
static void video_display(VideoState *is)
{
   // if (!is->pscreen2)
   //     video_open(is, 0, NULL);

	video_image_display(is);
}

static double get_clock(Clock *c)
{
	if (*c->queue_serial != c->serial)
		return NAN;
	if (c->paused) {
//        My_log(0, 0, "c->pts = %f \n",c->pts);
		return c->pts;
	}
	else {
		double time = av_gettime_relative() / 1000000.0;
		double clock_value = c->pts_drift + time - (time - c->last_updated) * (1.0 - c->speed);
//        My_log(0,0,"pts_drift = %f  ,  last_updated = %f ,  speed = %f  ,  time = %f ,%f\n",
//               c->pts_drift,c->last_updated,c->speed,time,clock_value);
		return clock_value;
	}
}

static void set_clock_at(Clock *c, double pts, int serial, double time)
{
	c->pts = pts;
	c->last_updated = time;
	c->pts_drift = c->pts - time;
	c->serial = serial;
//	My_log(0, 0, "set_clock_at c->pts_drift = %f c->last_updated = %f c->pts = %f", c->pts_drift, c->last_updated, c->pts);
}

static void set_clock(Clock *c, double pts, int serial)
{
	double time = av_gettime_relative() / 1000000.0;
//	My_log(0, 0, "set_clock pts = %f ",pts);
	set_clock_at(c, pts, serial, time);
}

static void set_clock_speed(Clock *c, double speed)
{
	set_clock(c, get_clock(c), c->serial);
	c->speed = speed;
}

static void init_clock(Clock *c, int *queue_serial)
{
	c->speed = 1.0;
	c->paused = 0;
	c->queue_serial = queue_serial;
	set_clock(c, NAN, -1);
}

static void sync_clock_to_slave(Clock *c, Clock *slave)
{
	double clock = get_clock(c);
	double slave_clock = get_clock(slave);
	if (!isnan(slave_clock) && (isnan(clock) || fabs(clock - slave_clock) > AV_NOSYNC_THRESHOLD)){
//		My_log(0, 0, "sync_clock_to_slave");
		set_clock(c, slave_clock, slave->serial);
	}
}

static int is_realtime(AVFormatContext *s)
{
	if (!strcmp(s->iformat->name, "rtp")
		|| !strcmp(s->iformat->name, "rtsp")
		|| !strcmp(s->iformat->name, "sdp")
		)
		return 1;

	if (s->pb && (!strncmp(s->filename, "rtp:", 4)
		|| !strncmp(s->filename, "udp:", 4)
		)
		)
		return 1;
	return 0;
}

static int frame_queue_init(FrameQueue *f, PacketQueue *pktq, int max_size, int keep_last)
{
	int i;
	memset(f, 0, sizeof(FrameQueue));
	if (!(f->mutex = createMutex()))
		return AVERROR(ENOMEM);
	if (!(f->cond = createCond()))
		return AVERROR(ENOMEM);
	f->eof = 0;
	f->pktq = pktq;
	f->max_size = FFMIN(max_size, FRAME_QUEUE_SIZE);
	f->keep_last = !!keep_last;
	for (i = 0; i < f->max_size; i++)
	if (!(f->queue[i].frame = av_frame_alloc()))
		return AVERROR(ENOMEM);
	return 0;
}


static int packet_queue_put_private(PacketQueue *q, AVPacket *pkt)
{
	MyAVPacketList *pkt1;

	if (q->abort_request)
		return -1;

	pkt1 = (MyAVPacketList *)av_malloc(sizeof(MyAVPacketList));
	if (!pkt1)
		return -1;
	pkt1->pkt = *pkt;
	pkt1->next = NULL;
	if (pkt == &flush_pkt)
		q->serial++;
	pkt1->serial = q->serial;

	if (!q->last_pkt)
		q->first_pkt = pkt1;
	else
		q->last_pkt->next = pkt1;
	q->last_pkt = pkt1;
	q->nb_packets++;
	q->size += pkt1->pkt.size + sizeof(*pkt1);
	/* XXX: should duplicate packet data in DV case */
	signalCond(q->cond);
	return 0;
}

static int packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
	int ret;

	/* duplicate the packet */
	if (pkt != &flush_pkt && av_dup_packet(pkt) < 0)
		return -1;

	lockMutex(q->mutex);
	ret = packet_queue_put_private(q, pkt);
	unlockMutex(q->mutex);

	if (pkt != &flush_pkt && ret < 0)
		av_free_packet(pkt);

	return ret;
}

static int packet_queue_put_nullpacket(PacketQueue *q, int stream_index)
{
	AVPacket pkt1, *pkt = &pkt1;
	av_init_packet(pkt);
	pkt->data = NULL;
	pkt->size = 0;
	pkt->stream_index = stream_index;
	return packet_queue_put(q, pkt);
}

/* packet queue handling */
static void packet_queue_init(PacketQueue *q)
{
	memset(q, 0, sizeof(PacketQueue));
	q->mutex = createMutex();
	q->cond = createCond();
	q->abort_request = 1;
	q->eof = 0;
}

static void packet_queue_flush(PacketQueue *q)
{
	MyAVPacketList *pkt, *pkt1;

	lockMutex(q->mutex);
	for (pkt = q->first_pkt; pkt; pkt = pkt1) {
		pkt1 = pkt->next;
		av_free_packet(&pkt->pkt);
		av_freep(&pkt);
	}
	q->last_pkt = NULL;
	q->first_pkt = NULL;
	q->nb_packets = 0;
	q->size = 0;
	unlockMutex(q->mutex);
}


static void free_picture(Frame *vp)
{
//	if (vp->bmp) {
//		FreeYUVOverlay(vp->bmp);
//		vp->bmp = NULL;
//	}
}

static void packet_queue_destroy(PacketQueue *q)
{
	packet_queue_flush(q);
	destroyMutex(q->mutex);
	destroyCond(q->cond);
}

static void decoder_destroy(Decoder *d) {
	av_free_packet(&d->pkt);
}

static void frame_queue_destory(FrameQueue *f)
{
	int i;
	for (i = 0; i < f->max_size; i++) {
		Frame *vp = &f->queue[i];
		frame_queue_unref_item(vp);
		av_frame_free(&vp->frame);
		free_picture(vp);
	}
	destroyMutex(f->mutex);
	destroyCond(f->cond);
}

void stream_close(VideoState *is)
{
	/* XXX: use a special url_shutdown call to abort parse cleanly */
    if(is->_currentFrame){
        av_frame_free(&is->_currentFrame);
    }
	is->abort_request = 1;
	waitThread(is->read_tid, NULL);
	packet_queue_destroy(&is->videoq);
	packet_queue_destroy(&is->audioq);
	packet_queue_destroy(&is->subtitleq);

	/* free all pictures */
	frame_queue_destory(&is->pictq);
	frame_queue_destory(&is->sampq);
	frame_queue_destory(&is->subpq);
	destroyCond(is->continue_read_thread);

#if !CONFIG_AVFILTER
	sws_freeContext(is->img_convert_ctx);
#endif
    
//	if (is->pscreen2){
//		FreeSurface(is->pscreen2);
//		is->pscreen2 = NULL;
//		is->pscreen = NULL;
//	}
	av_free(is);
}

static int decode_interrupt_cb(void *ctx)
{
	VideoState *is = (VideoState *)ctx;
	return is->abort_request;
}

static void frame_queue_signal(FrameQueue *f)
{
	lockMutex(f->mutex);
	signalCond(f->cond);
	unlockMutex(f->mutex);
}

static void packet_queue_abort(PacketQueue *q)
{
	lockMutex(q->mutex);

	q->abort_request = 1;

	signalCond(q->cond);

	unlockMutex(q->mutex);
}

static void packet_queue_start(PacketQueue *q)
{
	lockMutex(q->mutex);
	q->abort_request = 0;
	packet_queue_put_private(q, &flush_pkt);
	unlockMutex(q->mutex);
}

/* return < 0 if aborted, 0 if no packet and > 0 if packet.  */
static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block, int *serial)
{
	MyAVPacketList *pkt1;
	int ret;

	//lockMutex(q->mutex);
	std::unique_lock<mutex_t> lk(*q->mutex);

	for (;;) {
		if (q->abort_request) {
			ret = -1;
			break;
		}

		pkt1 = q->first_pkt;
		if (pkt1) {
			q->first_pkt = pkt1->next;
			if (!q->first_pkt)
				q->last_pkt = NULL;
			q->nb_packets--;
			q->size -= pkt1->pkt.size + sizeof(*pkt1);
			*pkt = pkt1->pkt;
			if (serial)
				*serial = pkt1->serial;
			av_free(pkt1);
			ret = 1;
			break;
		}
		else if (!block) {
			ret = 0;
			break;
		}
		else {
			//waitCond(q->cond, q->mutex);
			if (!q->eof)
				q->cond->wait(lk);
			else
			{
				ret = -1;
				break;
			}
		}
	}
	//unlockMutex(q->mutex);
	return ret;
}

static void decoder_abort(Decoder *d, FrameQueue *fq)
{
	packet_queue_abort(d->queue);
	frame_queue_signal(fq);
	waitThread(d->decoder_tid, NULL);
	d->decoder_tid = NULL;
	packet_queue_flush(d->queue);
}

static int get_master_sync_type(VideoState *is) {
	if (is->av_sync_type == AV_SYNC_VIDEO_MASTER) {
		if (is->video_st)
			return AV_SYNC_VIDEO_MASTER;
		else
			return AV_SYNC_AUDIO_MASTER;
	}
	else if (is->av_sync_type == AV_SYNC_AUDIO_MASTER) {
		if (is->audio_st)
			return AV_SYNC_AUDIO_MASTER;
		else
			return AV_SYNC_EXTERNAL_CLOCK;
	}
	else {
		return AV_SYNC_EXTERNAL_CLOCK;
	}
}

/* get the current master clock value */
double get_master_clock(VideoState *is)
{
	double val;

	switch (get_master_sync_type(is)) {
	case AV_SYNC_VIDEO_MASTER:
	//	My_log(NULL, 0, "video_master");
		val = get_clock(&is->vidclk);
		break;
	case AV_SYNC_AUDIO_MASTER:
	//	My_log(NULL, 0, "audio_master");
		val = get_clock(&is->audclk);
		break;
	default:
	//	My_log(NULL, 0, "extclk_master");
		val = get_clock(&is->extclk);
		break;
	}
	return val;
}

static void check_external_clock_speed(VideoState *is) {
	if (is->video_stream >= 0 && is->videoq.nb_packets <= is->nMIN_FRAMES / 2 ||
		is->audio_stream >= 0 && is->audioq.nb_packets <= is->nMIN_FRAMES / 2) {
		set_clock_speed(&is->extclk, FFMAX(EXTERNAL_CLOCK_SPEED_MIN, is->extclk.speed - EXTERNAL_CLOCK_SPEED_STEP));
	}
	else if ((is->video_stream < 0 || is->videoq.nb_packets > is->nMIN_FRAMES * 2) &&
		(is->audio_stream < 0 || is->audioq.nb_packets > is->nMIN_FRAMES * 2)) {
		set_clock_speed(&is->extclk, FFMIN(EXTERNAL_CLOCK_SPEED_MAX, is->extclk.speed + EXTERNAL_CLOCK_SPEED_STEP));
	}
	else {
		double speed = is->extclk.speed;
		if (speed != 1.0)
			set_clock_speed(&is->extclk, speed + EXTERNAL_CLOCK_SPEED_STEP * (1.0 - speed) / fabs(1.0 - speed));
	}
}

/* return the wanted number of samples to get better sync if sync_type is video
* or external master clock */
static int synchronize_audio(VideoState *is, int nb_samples)
{
	int wanted_nb_samples = nb_samples;

	/* if not master, then we try to remove or add samples to correct the clock */
	if (get_master_sync_type(is) != AV_SYNC_AUDIO_MASTER) {
		double diff, avg_diff;
		int min_nb_samples, max_nb_samples;

		diff = get_clock(&is->audclk) - get_master_clock(is);

		if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD) {
			is->audio_diff_cum = diff + is->audio_diff_avg_coef * is->audio_diff_cum;
			if (is->audio_diff_avg_count < AUDIO_DIFF_AVG_NB) {
				/* not enough measures to have a correct estimate */
				is->audio_diff_avg_count++;
			}
			else {
				/* estimate the A-V difference */
				avg_diff = is->audio_diff_cum * (1.0 - is->audio_diff_avg_coef);

				if (fabs(avg_diff) >= is->audio_diff_threshold) {
					wanted_nb_samples = nb_samples + (int)(diff * is->audio_src.freq);
					min_nb_samples = ((nb_samples * (100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100));
					max_nb_samples = ((nb_samples * (100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100));
					wanted_nb_samples = FFMIN(FFMAX(wanted_nb_samples, min_nb_samples), max_nb_samples);
				}
				My_log(NULL, AV_LOG_TRACE, "diff=%f adiff=%f sample_diff=%d apts=%0.3f %f\n",
					diff, avg_diff, wanted_nb_samples - nb_samples,
					is->audio_clock, is->audio_diff_threshold);
			}
		}
		else {
			/* too big difference : may be initial PTS errors, so
			reset A-V filter */
			is->audio_diff_avg_count = 0;
			is->audio_diff_cum = 0;
		}
	}

	return wanted_nb_samples;
}

/**
* Decode one audio frame and return its uncompressed size.
*
* The processed audio frame is decoded, converted if required, and
* stored in is->audio_buf, with size in bytes given by the return
* value.
*/
//static double lastread = 0;
static int audio_decode_frame(VideoState *is)
{
	int data_size, resampled_data_size;
	int64_t dec_channel_layout;
	av_unused double audio_clock0;
	int wanted_nb_samples;
	Frame *af;

	if (is->paused)
		return -1;

	do {
#if defined(_WIN32)
		while (frame_queue_nb_remaining(&is->sampq) == 0) {
			if ((av_gettime_relative() - audio_callback_time) > 1000000LL * is->audio_hw_buf_size / is->audio_tgt.bytes_per_sec / 2)
				return -1;
			av_usleep(1000);
		}
#endif
		if (!(af = frame_queue_peek_readable(&is->sampq)))
			return -1;
	//	if (lastread == 0)
	//		lastread = af->pts;
	//	My_log(0,0,"frame_queue_peek_readable af->pts = %f,readable_diff = %f",af->pts,(double)(af->pts-lastread));
	//	lastread = af->pts;
		frame_queue_next(&is->sampq);
	} while (af->serial != is->audioq.serial);

	data_size = av_samples_get_buffer_size(NULL, av_frame_get_channels(af->frame),
		af->frame->nb_samples,
		(AVSampleFormat)af->frame->format, 1);

	dec_channel_layout =
		(af->frame->channel_layout && av_frame_get_channels(af->frame) == av_get_channel_layout_nb_channels(af->frame->channel_layout)) ?
		af->frame->channel_layout : av_get_default_channel_layout(av_frame_get_channels(af->frame));
	wanted_nb_samples = synchronize_audio(is, af->frame->nb_samples);

	if (af->frame->format != is->audio_src.fmt ||
		dec_channel_layout != is->audio_src.channel_layout ||
		af->frame->sample_rate != is->audio_src.freq ||
		(wanted_nb_samples != af->frame->nb_samples && !is->swr_ctx)) {
		swr_free(&is->swr_ctx);
		is->swr_ctx = swr_alloc_set_opts(NULL,
			is->audio_tgt.channel_layout, is->audio_tgt.fmt, is->audio_tgt.freq,
			dec_channel_layout, (AVSampleFormat)af->frame->format, af->frame->sample_rate,
			0, NULL);
		if (!is->swr_ctx || swr_init(is->swr_ctx) < 0) {
			av_log(NULL, AV_LOG_ERROR,
				"Cannot create sample rate converter for conversion of %d Hz %s %d channels to %d Hz %s %d channels!\n",
				af->frame->sample_rate, av_get_sample_fmt_name((AVSampleFormat)af->frame->format), av_frame_get_channels(af->frame),
				is->audio_tgt.freq, av_get_sample_fmt_name(is->audio_tgt.fmt), is->audio_tgt.channels);
			swr_free(&is->swr_ctx);
			return -1;
		}
		is->audio_src.channel_layout = dec_channel_layout;
		is->audio_src.channels = av_frame_get_channels(af->frame);
		is->audio_src.freq = af->frame->sample_rate;
		is->audio_src.fmt = (AVSampleFormat)af->frame->format;
	}

	if (is->swr_ctx) {
		const uint8_t **in = (const uint8_t **)af->frame->extended_data;
		uint8_t **out = &is->audio_buf1;
		int out_count = (int64_t)wanted_nb_samples * is->audio_tgt.freq / af->frame->sample_rate + 256;
		int out_size = av_samples_get_buffer_size(NULL, is->audio_tgt.channels, out_count, is->audio_tgt.fmt, 0);
		int len2;
		if (out_size < 0) {
			av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size() failed\n");
			return -1;
		}
		if (wanted_nb_samples != af->frame->nb_samples) {
			if (swr_set_compensation(is->swr_ctx, (wanted_nb_samples - af->frame->nb_samples) * is->audio_tgt.freq / af->frame->sample_rate,
				wanted_nb_samples * is->audio_tgt.freq / af->frame->sample_rate) < 0) {
				av_log(NULL, AV_LOG_ERROR, "swr_set_compensation() failed\n");
				return -1;
			}
		}
		av_fast_malloc(&is->audio_buf1, &is->audio_buf1_size, out_size);
		if (!is->audio_buf1)
			return AVERROR(ENOMEM);
		len2 = swr_convert(is->swr_ctx, out, out_count, in, af->frame->nb_samples);
		if (len2 < 0) {
			av_log(NULL, AV_LOG_ERROR, "swr_convert() failed\n");
			return -1;
		}
		if (len2 == out_count) {
			av_log(NULL, AV_LOG_WARNING, "audio buffer is probably too small\n");
			if (swr_init(is->swr_ctx) < 0)
				swr_free(&is->swr_ctx);
		}
		is->audio_buf = is->audio_buf1;
		resampled_data_size = len2 * is->audio_tgt.channels * av_get_bytes_per_sample(is->audio_tgt.fmt);
	}
	else {
		is->audio_buf = af->frame->data[0];
		resampled_data_size = data_size;
	}

	audio_clock0 = is->audio_clock;
	/* update the audio clock with the pts */
	if (!isnan(af->pts)){
		is->audio_clock = af->pts + (double)af->frame->nb_samples / af->frame->sample_rate;
	//	My_log(0, 0, "audio_clock = %f,af->pts = %f , af->frame->nb_samples = %d , af->frame->sample_rate = %d ,", is->audio_clock,af->pts, af->frame->nb_samples, af->frame->sample_rate);
	}
	else
		is->audio_clock = NAN;
	is->audio_clock_serial = af->serial;
#ifdef DEBUG
	/*
	{
		static double last_clock;
		printf("audio: delay=%0.3f clock=%0.3f clock0=%0.3f\n",
			is->audio_clock - last_clock,
			is->audio_clock, audio_clock0);
		last_clock = is->audio_clock;
	}
	*/
#endif
	return resampled_data_size;
}


/* copy samples for viewing in editor window */
static void update_sample_display(VideoState *is, short *samples, int samples_size)
{
	int size, len;

	size = samples_size / sizeof(short);
	while (size > 0) {
		len = SAMPLE_ARRAY_SIZE - is->sample_array_index;
		if (len > size)
			len = size;
		memcpy(is->sample_array + is->sample_array_index, samples, len * sizeof(short));
		samples += len;
		is->sample_array_index += len;
		if (is->sample_array_index >= SAMPLE_ARRAY_SIZE)
			is->sample_array_index = 0;
		size -= len;
	}
}

//double mct = 0;
/* prepare a new audio buffer */
static void sdl_audio_callback(void *opaque, Uint8 *stream, int len)
{
	VideoState *is = (VideoState *)opaque;
	int audio_size, len1;

	audio_callback_time = av_gettime_relative();
//	if (mct == 0)mct = cc_clock();
//	double t = cc_clock();
//	My_log(0, 0, "sdl_audio_callback audio_callback_time = %f,loop = %f", audio_callback_time,t-mct);
//	mct = t;
	while (len > 0) {
		if (is->audio_buf_index >= (int)is->audio_buf_size) {
			audio_size = audio_decode_frame(is);
//			My_log(0, 0, "audio_size = %d,len = %d", audio_size,len);
			if (audio_size < 0) {
				/* if error, just output silence */
				is->audio_buf = is->silence_buf;
				if( is->audio_tgt.frame_size != 0 )
					is->audio_buf_size = sizeof(is->silence_buf) / is->audio_tgt.frame_size * is->audio_tgt.frame_size;
				else
					//FIXME: is->audio_tgt.frame_size = 0?
					is->audio_buf_size = 0;
			}
			else {
				if (is->show_mode != SHOW_MODE_VIDEO)
					update_sample_display(is, (int16_t *)is->audio_buf, audio_size);
				is->audio_buf_size = audio_size;
			}
			is->audio_buf_index = 0;
		}
		len1 = is->audio_buf_size - is->audio_buf_index;
		if (len1 > len)
			len1 = len;
		memcpy(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, len1);
		len -= len1;
		stream += len1;
		is->audio_buf_index += len1;
	}
	is->audio_write_buf_size = is->audio_buf_size - is->audio_buf_index;
	/* Let's assume the audio driver that is used by SDL has two periods. */
	if (!isnan(is->audio_clock)) {
//		My_log(0, 0, "callback audio_clock=%f,audio_hw_buf_size=%d,audio_write_buf_size=%d,bytes_per_sec=%d , audio_clock_serial=%d,audio_callback_time = %f",
//			is->audio_clock, is->audio_hw_buf_size, is->audio_write_buf_size, is->audio_tgt.bytes_per_sec, is->audio_clock_serial, (double)(audio_callback_time / 1000000.0));
		set_clock_at(&is->audclk, is->audio_clock - (double)(2 * is->audio_hw_buf_size + is->audio_write_buf_size) / is->audio_tgt.bytes_per_sec, is->audio_clock_serial, audio_callback_time / 1000000.0);
		sync_clock_to_slave(&is->extclk, &is->audclk);
	}
}

typedef void(*AudioCallBack)(void *opaque, Uint8 *stream, int len);
struct AudioChanel
{
	VideoState * _is;
	AudioCallBack _callback;
	AudioChanel(VideoState * is, AudioCallBack cb) :_is(is), _callback(cb)
	{}
};
#define MAXCHANEL 16
static AudioChanel* mxAudioChanel[MAXCHANEL];
static AudioChanel* OpenAudioChanel(VideoState *is, AudioCallBack pcallback)
{
	for (int i = 0; i < MAXCHANEL; i++)
	{
		if (!mxAudioChanel[i])
		{
			AudioChanel * pac = new AudioChanel(is, pcallback);
			mxAudioChanel[i] = pac;
			return pac;
		}
	}
	return nullptr;
}

static void CloseAudioChanel(AudioChanel *pac)
{
	for (int i = 0; i < MAXCHANEL; i++)
	{
		if (mxAudioChanel[i] == pac)
		{
			mxAudioChanel[i] = nullptr;
			delete pac;
			return;
		}
	}
}

static int getAudioChanelCount()
{
	int count = 0;
	for (int i = 0; i < MAXCHANEL; i++)
	{
		if (mxAudioChanel[i])
			count++;
	}
	return count;
}
const int max_audioval = ((1 << (16 - 1)) - 1);
const int min_audioval = -(1 << (16 - 1));
int clamp(int d,int mi,int ma)
{
	if (d>ma)
		return ma;
	if (d < mi)
		return mi;
	return d;
}

static void MixAudioFormat(Sint16 *des, Sint16 *src, AudioFormat format,int len,float volume)
{
	Sint16 A, B;
	while (len--){
		A = *(src++);
		B = *des;
		*des = (Sint16)clamp((int)A + (int)B, min_audioval, max_audioval);
		des++;
	}
}

static void sdl_mx_audio_callback(void *pd, Uint8 *stream, int len)
{
	AudioFormat format = AUDIO_S16SYS;
	memset(stream, 0, len);
	Uint8 * mixData = new Uint8[len];
	for (int i = 0; i < MAXCHANEL; i++)
	{
		AudioChanel * pac = mxAudioChanel[i];
		if (pac)
		{
			pac->_callback(pac->_is, mixData, len);
			//SDL_MixAudioFormat(stream, mixData, format,len, 128);
			if (!(pac->_is->step || pac->_is->seek_req))
				MixAudioFormat((Sint16*)stream, (Sint16*)mixData, format, len / 2, 128);
		}
	}
	delete [] mixData;
}

static bool gInitAudio = false;
static AudioSpec gSpec;
static int initAudio(AudioSpec *desired, AudioSpec *obtained)
{
	int ret = 0;
	if (!OpenAudioChanel((VideoState *)desired->userdata, desired->callback))
		return -1;
	if (!gInitAudio)
	{
		desired->callback = sdl_mx_audio_callback;
		ret = OpenAudio(desired, &gSpec);
		if (gSpec.format != AUDIO_S16SYS)
		{
			My_log(NULL, AV_LOG_ERROR, "Close audio device!\n");
			CloseAudio();
			return -1;
		}
		if ( ret >= 0 )
			PauseAudio(0);
		gInitAudio = true;
	}
	*obtained = gSpec;
	return ret;
}

static void CloseAudioChanelByVideoState(VideoState *pvs)
{
	CloseAudio();

	for (int i = 0; i < MAXCHANEL; i++)
	{
		AudioChanel * pac = mxAudioChanel[i];
		if (pac && pac->_is == pvs)
		{
			mxAudioChanel[i] = nullptr;
			delete pac;
			break;
		}
	}
	/*
		‘⁄»´≤øµƒ…˘“ÙªÏ∫œÕ®µ¿πÿ±’∫Û£¨πÿ±’…˘“Ù…Ë±∏
		’‚“≤–Ìµº÷¬∆µ∑±µƒ¥Úø™πÿ±’…˘“Ù…Ë±∏µº÷¬µÁ‘Î“Ù
	*/
	for (int i = 0; i < MAXCHANEL; i++)
	{
		if (mxAudioChanel[i])
			return;
	}
	//all audio chanel is close.
	
	gInitAudio = false;
}

static int audio_open(void *opaque, int64_t wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate, struct AudioParams *audio_hw_params)
{
	AudioSpec wanted_spec, spec;
	const char *env;
	static const int next_nb_channels[] = { 0, 0, 1, 6, 2, 6, 4, 6 };
	static const int next_sample_rates[] = { 0, 44100, 48000, 96000, 192000 };
	int next_sample_rate_idx = FF_ARRAY_ELEMS(next_sample_rates) - 1;

	env = my_getenv("SDL_AUDIO_CHANNELS");
	if (env) {
		wanted_nb_channels = atoi(env);
		wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
	}
	if (!wanted_channel_layout || wanted_nb_channels != av_get_channel_layout_nb_channels(wanted_channel_layout)) {
		wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
		wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
	}
	wanted_nb_channels = av_get_channel_layout_nb_channels(wanted_channel_layout);
	wanted_spec.channels = wanted_nb_channels;
	wanted_spec.freq = wanted_sample_rate;
	if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0) {
		My_log(NULL, AV_LOG_ERROR, "Invalid sample rate or channel count!\n");
		return -1;
	}
	while (next_sample_rate_idx && next_sample_rates[next_sample_rate_idx] >= wanted_spec.freq)
		next_sample_rate_idx--;
	wanted_spec.format = AUDIO_S16SYS;
	wanted_spec.silence = 0;
	wanted_spec.samples = FFMAX(AUDIO_MIN_BUFFER_SIZE, 2 << av_log2(wanted_spec.freq / AUDIO_MAX_CALLBACKS_PER_SEC));
	wanted_spec.callback = sdl_audio_callback;
	wanted_spec.userdata = opaque;
	while (initAudio(&wanted_spec, &spec) < 0) {
		My_log(NULL, AV_LOG_WARNING, "OpenAudio (%d channels, %d Hz): %s\n",
			wanted_spec.channels, wanted_spec.freq, GetError());
		wanted_spec.channels = next_nb_channels[FFMIN(7, wanted_spec.channels)];
		if (!wanted_spec.channels) {
			wanted_spec.freq = next_sample_rates[next_sample_rate_idx--];
			wanted_spec.channels = wanted_nb_channels;
			if (!wanted_spec.freq) {
				My_log(NULL, AV_LOG_ERROR,
					"No more combinations to try, audio open failed\n");
				CloseAudio();
				return -1;
			}
		}
		wanted_channel_layout = av_get_default_channel_layout(wanted_spec.channels);
	}
	if (spec.format != AUDIO_S16SYS) {
		My_log(NULL, AV_LOG_ERROR,
			"SDL advised audio format %d is not supported!\n", spec.format);
		CloseAudio();
		return -1;
	}
	if (spec.channels != wanted_spec.channels) {
		wanted_channel_layout = av_get_default_channel_layout(spec.channels);
		if (!wanted_channel_layout) {
			My_log(NULL, AV_LOG_ERROR,
				"SDL advised channel count %d is not supported!\n", spec.channels);
			CloseAudio();
			return -1;
		}
	}

	audio_hw_params->fmt = AV_SAMPLE_FMT_S16;
	audio_hw_params->freq = spec.freq;
	audio_hw_params->channel_layout = wanted_channel_layout;
	audio_hw_params->channels = spec.channels;
	audio_hw_params->frame_size = av_samples_get_buffer_size(NULL, audio_hw_params->channels, 1, audio_hw_params->fmt, 1);
	audio_hw_params->bytes_per_sec = av_samples_get_buffer_size(NULL, audio_hw_params->channels, audio_hw_params->freq, audio_hw_params->fmt, 1);
	if (audio_hw_params->bytes_per_sec <= 0 || audio_hw_params->frame_size <= 0) {
		My_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size failed\n");
		CloseAudio();
		return -1;
	}
	return spec.size;
}

static void stream_component_close(VideoState *is, int stream_index)
{
	AVFormatContext *ic = is->ic;
	AVCodecContext *avctx;

	if (stream_index < 0 || stream_index >= (int)ic->nb_streams)
		return;
	avctx = ic->streams[stream_index]->codec;

	switch (avctx->codec_type) {
	case AVMEDIA_TYPE_AUDIO:
		decoder_abort(&is->auddec, &is->sampq);
		CloseAudioChanelByVideoState(is);
		//CloseAudio();
		decoder_destroy(&is->auddec);
		swr_free(&is->swr_ctx);
		av_freep(&is->audio_buf1);
		is->audio_buf1_size = 0;
		is->audio_buf = NULL;

		if (is->rdft) {
			av_rdft_end(is->rdft);
			av_freep(&is->rdft_data);
			is->rdft = NULL;
			is->rdft_bits = 0;
		}
		break;
	case AVMEDIA_TYPE_VIDEO:
		decoder_abort(&is->viddec, &is->pictq);
		decoder_destroy(&is->viddec);
		break;
	case AVMEDIA_TYPE_SUBTITLE:
		decoder_abort(&is->subdec, &is->subpq);
		decoder_destroy(&is->subdec);
		break;
	default:
		break;
	}

	ic->streams[stream_index]->discard = AVDISCARD_ALL;
	avcodec_close(avctx);
	switch (avctx->codec_type) {
	case AVMEDIA_TYPE_AUDIO:
		is->audio_st = NULL;
		is->audio_stream = -1;
		break;
	case AVMEDIA_TYPE_VIDEO:
		is->video_st = NULL;
		is->video_stream = -1;
		break;
	case AVMEDIA_TYPE_SUBTITLE:
		is->subtitle_st = NULL;
		is->subtitle_stream = -1;
		break;
	default:
		break;
	}
}

static double compute_target_delay(double delay, VideoState *is)
{
	double sync_threshold, diff;

	/* update delay to follow master synchronisation source */
	if (get_master_sync_type(is) != AV_SYNC_VIDEO_MASTER) {
		/* if video is slave, we try to correct big delays by
		duplicating or deleting a frame */
		diff = get_clock(&is->vidclk) - get_master_clock(is);

		/* skip or repeat frame. We take into account the
		delay to compute the threshold. I still don't know
		if it is the best guess */
		sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
		if (!isnan(diff) && fabs(diff) < is->max_frame_duration) {
			if (diff <= -sync_threshold)
				delay = FFMAX(0, delay + diff);
			else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD)
				delay = delay + diff;
			else if (diff >= sync_threshold)
				delay = 2 * delay;
		}
	}
	if( show_status )
		My_log(NULL, AV_LOG_TRACE, "video: delay=%0.3f A-V=%f\n",
			delay, -diff);

	return delay;
}

static double vp_duration(VideoState *is, Frame *vp, Frame *nextvp) {
	if (vp->serial == nextvp->serial) {
		double duration = nextvp->pts - vp->pts;
		if (isnan(duration) || duration <= 0 || duration > is->max_frame_duration)
			return vp->duration;
		else
			return duration;
	}
	else {
		return 0.0;
	}
}

static void update_video_pts(VideoState *is, double pts, int64_t pos, int serial) {
	/* update current video pts */
//	My_log(0, 0, "update_video_pts");
	set_clock(&is->vidclk, pts, serial);
	sync_clock_to_slave(&is->extclk, &is->vidclk);
}

int is_stream_pause(VideoState *is)
{
	return is->paused;
}

/* pause or resume the video */
void stream_toggle_pause(VideoState *is)
{
	if (is->paused) {
		is->frame_timer += av_gettime_relative() / 1000000.0 - is->vidclk.last_updated;
		if (is->read_pause_return != AVERROR(ENOSYS)) {
		is->vidclk.paused = 0;
		}
	//	My_log(0, 0, "stream_toggle_pause");
		set_clock(&is->vidclk, get_clock(&is->vidclk), is->vidclk.serial);
	}
//	My_log(0, 0, "stream_toggle_pause 2");
	set_clock(&is->extclk, get_clock(&is->extclk), is->extclk.serial);
	is->paused = is->audclk.paused = is->vidclk.paused = is->extclk.paused = !is->paused;
}

void toggle_pause(VideoState *is)
{
	stream_toggle_pause(is);
	is->step = 0;
}

/* called to display each frame */
void video_refresh(VideoState *is, double *remaining_time)
{
	double time;

	Frame *sp, *sp2;

	if (!is->paused && get_master_sync_type(is) == AV_SYNC_EXTERNAL_CLOCK && is->realtime)
		check_external_clock_speed(is);

	if (!display_disable && is->show_mode != SHOW_MODE_VIDEO && is->audio_st) {
		time = av_gettime_relative() / 1000000.0;
		if (is->force_refresh || is->last_vis_time + rdftspeed < time) {
			video_display(is);
			is->last_vis_time = time;
		}
		*remaining_time = FFMIN(*remaining_time, is->last_vis_time + rdftspeed - time);
	}

	if (is->video_st) {
		int redisplay = 0;
		if (is->force_refresh)
			redisplay = frame_queue_prev(&is->pictq);
	retry:
		if (frame_queue_nb_remaining(&is->pictq) == 0) {
			// nothing to do, no picture to display in the queue
            //My_log(0,0,"nothing to do");
		}
		else {
			double last_duration, duration, delay;
			Frame *vp, *lastvp;

			/* dequeue the picture */
			lastvp = frame_queue_peek_last(&is->pictq);
			vp = frame_queue_peek(&is->pictq);

			if (vp->serial != is->videoq.serial) {
				frame_queue_next(&is->pictq);
				redisplay = 0;
				goto retry;
			}

			if (lastvp->serial != vp->serial && !redisplay)
				is->frame_timer = av_gettime_relative() / 1000000.0;

			if (is->paused)
				goto display;

			/* compute nominal last_duration */
			last_duration = vp_duration(is, lastvp, vp);
			if (redisplay)
				delay = 0.0;
			else
				delay = compute_target_delay(last_duration, is);

			time = av_gettime_relative() / 1000000.0;
			if (time < is->frame_timer + delay && !redisplay) {
				*remaining_time = FFMIN(is->frame_timer + delay - time, *remaining_time);
               // My_log(0,0,"video_refresh return time = %f , frame_timer = %f, delay = %f,redisplay = %f",time,is->frame_timer,delay,redisplay);
				return;
			}

			is->frame_timer += delay;
			if (delay > 0 && time - is->frame_timer > AV_SYNC_THRESHOLD_MAX)
				is->frame_timer = time;

			lockMutex(is->pictq.mutex);
			if (!redisplay && !isnan(vp->pts))
				update_video_pts(is, vp->pts, vp->pos, vp->serial);
			unlockMutex(is->pictq.mutex);

			if (frame_queue_nb_remaining(&is->pictq) > 1) {
				Frame *nextvp = frame_queue_peek_next(&is->pictq);
				duration = vp_duration(is, vp, nextvp);
				if (!is->step && (redisplay || framedrop>0 || (framedrop && get_master_sync_type(is) != AV_SYNC_VIDEO_MASTER)) && time > is->frame_timer + duration){
					if (!redisplay)
						is->frame_drops_late++;
					frame_queue_next(&is->pictq);
					redisplay = 0;
					goto retry;
				}
			}

			if (is->subtitle_st) {
				while (frame_queue_nb_remaining(&is->subpq) > 0) {
					sp = frame_queue_peek(&is->subpq);

					if (frame_queue_nb_remaining(&is->subpq) > 1)
						sp2 = frame_queue_peek_next(&is->subpq);
					else
						sp2 = NULL;

					if (sp->serial != is->subtitleq.serial
						|| (is->vidclk.pts > (sp->pts + ((float)sp->sub.end_display_time / 1000)))
						|| (sp2 && is->vidclk.pts > (sp2->pts + ((float)sp2->sub.start_display_time / 1000))))
					{
						frame_queue_next(&is->subpq);
					}
					else {
						break;
					}
				}
			}

		display:
			/* display picture */
			if (!display_disable && is->show_mode == SHOW_MODE_VIDEO)
				video_display(is);
            
			frame_queue_next(&is->pictq);

			if (is->step && !is->paused){
				stream_toggle_pause(is);
				is->step = 0;
			}
		}
	}
	is->force_refresh = 0;
	/*
	if (show_status) {
		static int64_t last_time;
		int64_t cur_time;
		int aqsize, vqsize, sqsize;
		double av_diff;

		cur_time = av_gettime_relative();
		if (!last_time || (cur_time - last_time) >= 30000) {
			aqsize = 0;
			vqsize = 0;
			sqsize = 0;
			if (is->audio_st)
				aqsize = is->audioq.size;
			if (is->video_st)
				vqsize = is->videoq.size;
			if (is->subtitle_st)
				sqsize = is->subtitleq.size;
			av_diff = 0;
			if (is->audio_st && is->video_st)
				av_diff = get_clock(&is->audclk) - get_clock(&is->vidclk);
			else if (is->video_st)
				av_diff = get_master_clock(is) - get_clock(&is->vidclk);
			else if (is->audio_st)
				av_diff = get_master_clock(is) - get_clock(&is->audclk);
			My_log(NULL, AV_LOG_INFO,
				"%7.2f %s:%7.3f fd=%4d aq=%5dKB vq=%5dKB sq=%5dB f=%"PRId64"/%"PRId64"   \r",
				get_master_clock(is),
				(is->audio_st && is->video_st) ? "A-V" : (is->video_st ? "M-V" : (is->audio_st ? "M-A" : "   ")),
				av_diff,
				is->frame_drops_early + is->frame_drops_late,
				aqsize / 1024,
				vqsize / 1024,
				sqsize,
				is->video_st ? is->video_st->codec->pts_correction_num_faulty_dts : 0,
				is->video_st ? is->video_st->codec->pts_correction_num_faulty_pts : 0);
			fflush(stdout);
			last_time = cur_time;
		}
	}*/
}

/* allocate a picture (needs to do that in main thread to avoid
potential locking problems */
//ΩˆΩˆƒ⁄¥Êƒ£ƒ‚≤ª–Ë“™‘⁄÷˜œﬂ≥Ã÷–Ω¯––∑÷≈‰£¨
static void alloc_picture(VideoState *is)
{
	Frame *vp;
//	int64_t bufferdiff;

	vp = &is->pictq.queue[is->pictq.windex];
	free_picture(vp);

	video_open(is, 0, vp);
	
//	vp->bmp = CreateYUVOverlay(vp->width, vp->height,
//		YV12_OVERLAY,
//		is->pscreen2);
//	bufferdiff = vp->bmp ? FFMAX(vp->bmp->pixels[0], vp->bmp->pixels[1]) - FFMIN(vp->bmp->pixels[0], vp->bmp->pixels[1]) : 0;
//	if (!vp->bmp || vp->bmp->pitches[0] < vp->width || bufferdiff < (int64_t)vp->height * vp->bmp->pitches[0]) {
		/* SDL allocates a buffer smaller than requested if the video
		* overlay hardware is unable to support the requested size. */
//		My_log(NULL, AV_LOG_FATAL,
//			"Error: the video system does not support an image\n"
//			"size of %dx%d pixels. Try using -lowres or -vf \"scale=w:h\"\n"
//			"to reduce the image size.\n", vp->width, vp->height);
//		do_exit(is);
//	}
	vp->allocated = 1;
}

static void duplicate_right_border_pixels(Overlay *bmp) {
	int i, width, height;
	Uint8 *p, *maxp;
	for (i = 0; i < 3; i++) {
		width = bmp->w;
		height = bmp->h;
		if (i > 0) {
			width >>= 1;
			height >>= 1;
		}
		if (bmp->pitches[i] > width) {
			maxp = bmp->pixels[i] + bmp->pitches[i] * height - 1;
			for (p = bmp->pixels[i] + width - 1; p < maxp; p += bmp->pitches[i])
				*(p + 1) = *p;
		}
	}
}

static int queue_picture(VideoState *is, AVFrame *src_frame, double pts, double duration, int64_t pos, int serial)
{
	Frame *vp;

#if defined(DEBUG_SYNC) && 0
	printf("frame_type=%c pts=%0.3f\n",
		av_get_picture_type_char(src_frame->pict_type), pts);
#endif

	if (!(vp = frame_queue_peek_writable(&is->pictq,is)))
		return -1;

	vp->sar = src_frame->sample_aspect_ratio;

	/* alloc or resize hardware picture buffer */
	if (
		vp->width != src_frame->width ||
		vp->height != src_frame->height) {
		vp->allocated = 0;
		vp->reallocate = 0;
		vp->width = src_frame->width;
		vp->height = src_frame->height;

		alloc_picture(is);

		if (is->videoq.abort_request)
			return -1;
	}
	/* if the frame is not skipped, then display it */
	if (vp->frame) {
	//	AVPicture pict = { { 0 } };
		/* get a pointer on the bitmap */
	//	LockYUVOverlay(vp->bmp);
    
	//	pict.data[0] = vp->bmp->pixels[0];
	//	pict.data[1] = vp->bmp->pixels[2];
	//	pict.data[2] = vp->bmp->pixels[1];

	//	pict.linesize[0] = vp->bmp->pitches[0];
	//	pict.linesize[1] = vp->bmp->pitches[2];
	//	pict.linesize[2] = vp->bmp->pitches[1];
        
#if CONFIG_AVFILTER
		// FIXME use direct rendering
        av_frame_ref(vp->frame,src_frame);
		//av_picture_copy(&pict, (AVPicture *)src_frame,
		//	(AVPixelFormat)src_frame->format, vp->width, vp->height);
#else
		av_opt_get_int(sws_opts, "sws_flags", 0, &sws_flags);
		is->img_convert_ctx = sws_getCachedContext(is->img_convert_ctx,
			vp->width, vp->height, src_frame->format, vp->width, vp->height,
			AV_PIX_FMT_YUV420P, sws_flags, NULL, NULL, NULL);
		if (!is->img_convert_ctx) {
			My_log(NULL, AV_LOG_FATAL, "Cannot initialize the conversion context\n");
			//exit(1);
			do_exit(is);
		}
		sws_scale(is->img_convert_ctx, src_frame->data, src_frame->linesize,
			0, vp->height, pict.data, pict.linesize);
#endif
		/* workaround SDL PITCH_WORKAROUND */
	//	duplicate_right_border_pixels(vp->bmp);
       // double t3 = cc_clock();
        
		/* update the bitmap content */
	//	UnlockYUVOverlay(vp->bmp);

		vp->pts = pts;
		vp->duration = duration;
		vp->pos = pos;
		vp->serial = serial;
		/* now we can update the picture count */
       // My_log(0,0,"queue_picture done!");
		frame_queue_push(&is->pictq);
    }
	return 0;
}

    /*
    void LogBin2(int size, unsigned char * buf)
    {
        int i = 0;
        while (size >= 16){
            My_log(0, 0, "%X %X %X %X %X %X %X %X - %X %X %X %X %X %X %X %X\n",
                   buf[i], buf[i+1], buf[i+2], buf[i+3], buf[i+4], buf[i+5], buf[i+6], buf[i+7],
                   buf[i+8], buf[i + 9], buf[i + 10], buf[i + 11], buf[i + 12], buf[i + 13], buf[i + 14], buf[i + 15]);
            i += 16;
            size -= 16;
        }
    }
     */
 //   static double _lastT0 = 0;
 //   static double _decodeT = 0;
//static int64_t lastT = 0;
//static int64_t lastPts = 0;
static int decoder_decode_frame(Decoder *d, AVFrame *frame, AVSubtitle *sub) {
	int got_frame = 0;
    VideoState *is;
    double t0;
	do {
		int ret = -1;

		if (d->queue->abort_request)
			return -1;

		if (!d->packet_pending || d->queue->serial != d->pkt_serial) {
			AVPacket pkt;
			do {
				if (d->queue->nb_packets == 0)
					signalCond(d->empty_queue_cond);
				if (packet_queue_get(d->queue, &pkt, 1, &d->pkt_serial) < 0)
					return -1;
				if (pkt.data == flush_pkt.data) {
					avcodec_flush_buffers(d->avctx);
					d->finished = 0;
					d->next_pts = d->start_pts;
					d->next_pts_tb = d->start_pts_tb;
				}
			} while (pkt.data == flush_pkt.data || d->queue->serial != d->pkt_serial);
			av_free_packet(&d->pkt);
			d->pkt_temp = d->pkt = pkt;
			d->packet_pending = 1;
		}

		switch (d->avctx->codec_type) {
		case AVMEDIA_TYPE_VIDEO:
   //             if(_lastT0==0)
   //                 _lastT0 = cc_clock();
   //             t0 = cc_clock();
			ret = avcodec_decode_video2(d->avctx, frame, &got_frame, &d->pkt_temp);
   //             _decodeT += (cc_clock()-t0);
			if (got_frame) {
#if CONFIG_VIDEOTOOLBOX
      //          double t = cc_clock();
                is = (VideoState *)d->avctx->opaque;
                if(is && is->hwaccel_retrieve_data){
                    is->hwaccel_retrieve_data(d->avctx,frame);
                }
       //         double t1 = cc_clock();
       //         My_log(0,0,"decodet= %f , dt=%f ,retrieve_data = %f ",_decodeT,t1-_lastT0,t1-t);
       //        _decodeT = 0;
       //         _lastT0 = t1;
#endif
				if (decoder_reorder_pts == -1) {
					frame->pts = av_frame_get_best_effort_timestamp(frame);
				}
				else if (decoder_reorder_pts) {
					frame->pts = frame->pkt_pts;
				}
				else {
					frame->pts = frame->pkt_dts;
				}
			}
			break;
		case AVMEDIA_TYPE_AUDIO:
			ret = avcodec_decode_audio4(d->avctx, frame, &got_frame, &d->pkt_temp);
			if (got_frame) {
				AVRational tb;// = (AVRational){ 1, frame->sample_rate };
				tb.num = 1;
				tb.den = frame->sample_rate;
//				if (lastT == 0)
//					lastT = frame->pkt_pts;
//				if (frame->pkt_pts - lastT > 2095)
//					My_log(0, 0, "???????????????????????????????????");
//				My_log(0, 0, "frame->pkt_pts = %f ,tb.den = %d,diff = %f,frame->nb_samples=%d", (double)frame->pkt_pts, tb.den, (double)(frame->pkt_pts - lastT), frame->nb_samples);
//				lastT = frame->pkt_pts;

				if (frame->pts != AV_NOPTS_VALUE){
					frame->pts = av_rescale_q(frame->pts, d->avctx->time_base, tb);
//					My_log(0, 0, "1 frame->pts = %f", (double)frame->pts);
				}
				else if (frame->pkt_pts != AV_NOPTS_VALUE){
					frame->pts = av_rescale_q(frame->pkt_pts, av_codec_get_pkt_timebase(d->avctx), tb);
//					if (lastPts == 0)
//						lastPts = frame->pts;
//					My_log(0, 0, "2 frame->pts = %f,frame_diff = %f", (double)frame->pts,(double)(frame->pts-lastPts));
//					lastPts = frame->pts;
				}
				else if (d->next_pts != AV_NOPTS_VALUE){
					frame->pts = av_rescale_q(d->next_pts, d->next_pts_tb, tb);
//					My_log(0, 0, "3 frame->pts = %f", (double)frame->pts);
				}
				if (frame->pts != AV_NOPTS_VALUE) {
					d->next_pts = frame->pts + frame->nb_samples;
					d->next_pts_tb = tb;
				}
			}
			break;
		case AVMEDIA_TYPE_SUBTITLE:
			ret = avcodec_decode_subtitle2(d->avctx, sub, &got_frame, &d->pkt_temp);
			break;
		}

		if (ret < 0) {
			d->packet_pending = 0;
		}
		else {
			d->pkt_temp.dts =
				d->pkt_temp.pts = AV_NOPTS_VALUE;
			if (d->pkt_temp.data) {
				if (d->avctx->codec_type != AVMEDIA_TYPE_AUDIO)
					ret = d->pkt_temp.size;
				d->pkt_temp.data += ret;
				d->pkt_temp.size -= ret;
				if (d->pkt_temp.size <= 0)
					d->packet_pending = 0;
			}
			else {
				if (!got_frame) {
					d->packet_pending = 0;
					d->finished = d->pkt_serial;
				}
			}
		}
	} while (!got_frame && !d->finished);

	return got_frame;
}

static int get_video_frame(VideoState *is, AVFrame *frame)
{
	int got_picture;

	if ((got_picture = decoder_decode_frame(&is->viddec, frame, NULL)) < 0)
		return -1;

	if (got_picture) {
		double dpts = NAN;

        if (frame->pts != AV_NOPTS_VALUE){
			dpts = av_q2d(is->video_st->time_base) * frame->pts;
        }

		frame->sample_aspect_ratio = av_guess_sample_aspect_ratio(is->ic, is->video_st, frame);

		if (framedrop>0 || (framedrop && get_master_sync_type(is) != AV_SYNC_VIDEO_MASTER)) {
			if (frame->pts != AV_NOPTS_VALUE) {
                double mc = get_master_clock(is);
				double diff = dpts - mc;
//				My_log(NULL, 0, "dpts = %f , mc = %f , frame->pts ="PRIx64, dpts, mc, frame->pts);
//				if (fabs(diff) < AV_NOSYNC_THRESHOLD){
//					My_log(NULL, 0, "diff = %f,AV_NOSYNC_THRESHOLD = %f,is->frame_last_filter_delay = %f,is->viddec.pkt_serial=%d,is->vidclk.serial=%d",
//						diff,
//						AV_NOSYNC_THRESHOLD,
//						is->frame_last_filter_delay,
//						is->viddec.pkt_serial,
//						is->vidclk.serial);
//				}
				/*
                My_log(NULL,0, "diff = %f,AV_NOSYNC_THRESHOLD = %f,is->frame_last_filter_delay = %f,is->viddec.pkt_serial=%d,is->vidclk.serial=%d",
                       diff,
                       AV_NOSYNC_THRESHOLD,
                       is->frame_last_filter_delay,
                       is->viddec.pkt_serial,
                       is->vidclk.serial);
                My_log(NULL,0,"%s , %s , %s , %s",
                       fabs(diff) < AV_NOSYNC_THRESHOLD ? "true":"false",
                       diff - is->frame_last_filter_delay < 0 ? "true":"false",
                       is->viddec.pkt_serial == is->vidclk.serial ? "true":"false",
                       is->videoq.nb_packets ? "true":"false");
                 */
				if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD &&
					diff - is->frame_last_filter_delay < 0 &&
					is->viddec.pkt_serial == is->vidclk.serial &&
					is->videoq.nb_packets) {
//					My_log(NULL, 0, "======lost======");
					is->frame_drops_early++;
					av_frame_unref(frame);
					got_picture = 0;
  				}
			}
		}
	}

	return got_picture;
}

#if CONFIG_AVFILTER
static int configure_filtergraph(AVFilterGraph *graph, const char *filtergraph,
	AVFilterContext *source_ctx, AVFilterContext *sink_ctx)
{
	int ret, i;
	int nb_filters = graph->nb_filters;
	AVFilterInOut *outputs = NULL, *inputs = NULL;

	if (filtergraph) {
		outputs = avfilter_inout_alloc();
		inputs = avfilter_inout_alloc();
		if (!outputs || !inputs) {
			ret = AVERROR(ENOMEM);
			goto fail;
		}

		outputs->name = av_strdup("in");
		outputs->filter_ctx = source_ctx;
		outputs->pad_idx = 0;
		outputs->next = NULL;

		inputs->name = av_strdup("out");
		inputs->filter_ctx = sink_ctx;
		inputs->pad_idx = 0;
		inputs->next = NULL;

		if ((ret = avfilter_graph_parse_ptr(graph, filtergraph, &inputs, &outputs, NULL)) < 0)
			goto fail;
	}
	else {
		if ((ret = avfilter_link(source_ctx, 0, sink_ctx, 0)) < 0)
			goto fail;
	}

	/* Reorder the filters to ensure that inputs of the custom filters are merged first */
	for (i = 0; i < (int)graph->nb_filters - nb_filters; i++)
		FFSWAP(AVFilterContext*, graph->filters[i], graph->filters[i + nb_filters]);

	ret = avfilter_graph_config(graph, NULL);
fail:
	avfilter_inout_free(&outputs);
	avfilter_inout_free(&inputs);
	return ret;
}

static int configure_video_filters(AVFilterGraph *graph, VideoState *is, const char *vfilters, AVFrame *frame)
{
	static const enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
	char sws_flags_str[512] = "";
	char buffersrc_args[256];
	int ret;
	AVFilterContext *filt_src = NULL, *filt_out = NULL, *last_filter = NULL;
	AVCodecContext *codec = is->video_st->codec;
	AVRational fr = av_guess_frame_rate(is->ic, is->video_st, NULL);
	AVDictionaryEntry *e = NULL;

	while ((e = av_dict_get(sws_dict, "", e, AV_DICT_IGNORE_SUFFIX))) {
		if (!strcmp(e->key, "sws_flags")) {
			av_strlcatf(sws_flags_str, sizeof(sws_flags_str), "%s=%s:", "flags", e->value);
		}
		else
			av_strlcatf(sws_flags_str, sizeof(sws_flags_str), "%s=%s:", e->key, e->value);
	}
	if (strlen(sws_flags_str))
		sws_flags_str[strlen(sws_flags_str) - 1] = '\0';

	graph->scale_sws_opts = av_strdup(sws_flags_str);

	snprintf(buffersrc_args, sizeof(buffersrc_args),
		"video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
		frame->width, frame->height, frame->format,
		is->video_st->time_base.num, is->video_st->time_base.den,
		codec->sample_aspect_ratio.num, FFMAX(codec->sample_aspect_ratio.den, 1));
	if (fr.num && fr.den)
		av_strlcatf(buffersrc_args, sizeof(buffersrc_args), ":frame_rate=%d/%d", fr.num, fr.den);

	if ((ret = avfilter_graph_create_filter(&filt_src,
		avfilter_get_by_name("buffer"),
		"ffplay_buffer", buffersrc_args, NULL,
		graph)) < 0)
		goto fail;

	ret = avfilter_graph_create_filter(&filt_out,
		avfilter_get_by_name("buffersink"),
		"ffplay_buffersink", NULL, NULL, graph);
	if (ret < 0)
		goto fail;

	if ((ret = av_opt_set_int_list(filt_out, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN)) < 0)
		goto fail;

	last_filter = filt_out;

	/* Note: this macro adds a filter before the lastly added filter, so the
	* processing order of the filters is in reverse */
#define INSERT_FILT(name, arg) do {                                          \
	AVFilterContext *filt_ctx;                                               \
	\
	ret = avfilter_graph_create_filter(&filt_ctx, \
	avfilter_get_by_name(name), \
	"ffplay_" name, arg, NULL, graph);    \
	if (ret < 0)                                                             \
	goto fail;                                                           \
	\
	ret = avfilter_link(filt_ctx, 0, last_filter, 0);                        \
	if (ret < 0)                                                             \
	goto fail;                                                           \
	\
	last_filter = filt_ctx;                                                  \
	} while (0)

	/* SDL YUV code is not handling odd width/height for some driver
	* combinations, therefore we crop the picture to an even width/height. */
	INSERT_FILT("crop", "floor(in_w/2)*2:floor(in_h/2)*2");

	if (autorotate) {
		double theta = get_rotation(is->video_st);

		if (fabs(theta - 90) < 1.0) {
			INSERT_FILT("transpose", "clock");
		}
		else if (fabs(theta - 180) < 1.0) {
			INSERT_FILT("hflip", NULL);
			INSERT_FILT("vflip", NULL);
		}
		else if (fabs(theta - 270) < 1.0) {
			INSERT_FILT("transpose", "cclock");
		}
		else if (fabs(theta) > 1.0) {
			char rotate_buf[64];
			snprintf(rotate_buf, sizeof(rotate_buf), "%f*PI/180", theta);
			INSERT_FILT("rotate", rotate_buf);
		}
	}

	if ((ret = configure_filtergraph(graph, vfilters, filt_src, last_filter)) < 0)
		goto fail;

	is->in_video_filter = filt_src;
	is->out_video_filter = filt_out;

fail:
	return ret;
}

static int configure_audio_filters(VideoState *is, const char *afilters, int force_output_format)
{
	static const enum AVSampleFormat sample_fmts[] = { AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_NONE };
	int sample_rates[2] = { 0, -1 };
	int64_t channel_layouts[2] = { 0, -1 };
	int channels[2] = { 0, -1 };
	AVFilterContext *filt_asrc = NULL, *filt_asink = NULL;
	char aresample_swr_opts[512] = "";
	AVDictionaryEntry *e = NULL;
	char asrc_args[256];
	int ret;

	avfilter_graph_free(&is->agraph);
	if (!(is->agraph = avfilter_graph_alloc()))
		return AVERROR(ENOMEM);

	while ((e = av_dict_get(swr_opts, "", e, AV_DICT_IGNORE_SUFFIX)))
		av_strlcatf(aresample_swr_opts, sizeof(aresample_swr_opts), "%s=%s:", e->key, e->value);
	if (strlen(aresample_swr_opts))
		aresample_swr_opts[strlen(aresample_swr_opts) - 1] = '\0';
	av_opt_set(is->agraph, "aresample_swr_opts", aresample_swr_opts, 0);

	ret = snprintf(asrc_args, sizeof(asrc_args),
		"sample_rate=%d:sample_fmt=%s:channels=%d:time_base=%d/%d",
		is->audio_filter_src.freq, av_get_sample_fmt_name(is->audio_filter_src.fmt),
		is->audio_filter_src.channels,
		1, is->audio_filter_src.freq);
	if (is->audio_filter_src.channel_layout)
		snprintf(asrc_args + ret, sizeof(asrc_args)-ret,
		":channel_layout=0x%" PRIx64, is->audio_filter_src.channel_layout);

	ret = avfilter_graph_create_filter(&filt_asrc,
		avfilter_get_by_name("abuffer"), "ffplay_abuffer",
		asrc_args, NULL, is->agraph);
	if (ret < 0)
		goto end;


	ret = avfilter_graph_create_filter(&filt_asink,
		avfilter_get_by_name("abuffersink"), "ffplay_abuffersink",
		NULL, NULL, is->agraph);
	if (ret < 0)
		goto end;

	if ((ret = av_opt_set_int_list(filt_asink, "sample_fmts", sample_fmts, AV_SAMPLE_FMT_NONE, AV_OPT_SEARCH_CHILDREN)) < 0)
		goto end;
	if ((ret = av_opt_set_int(filt_asink, "all_channel_counts", 1, AV_OPT_SEARCH_CHILDREN)) < 0)
		goto end;

	if (force_output_format) {
		channel_layouts[0] = is->audio_tgt.channel_layout;
		channels[0] = is->audio_tgt.channels;
		sample_rates[0] = is->audio_tgt.freq;
		if ((ret = av_opt_set_int(filt_asink, "all_channel_counts", 0, AV_OPT_SEARCH_CHILDREN)) < 0)
			goto end;
		if ((ret = av_opt_set_int_list(filt_asink, "channel_layouts", channel_layouts, -1, AV_OPT_SEARCH_CHILDREN)) < 0)
			goto end;
		if ((ret = av_opt_set_int_list(filt_asink, "channel_counts", channels, -1, AV_OPT_SEARCH_CHILDREN)) < 0)
			goto end;
		if ((ret = av_opt_set_int_list(filt_asink, "sample_rates", sample_rates, -1, AV_OPT_SEARCH_CHILDREN)) < 0)
			goto end;
	}


	if ((ret = configure_filtergraph(is->agraph, afilters, filt_asrc, filt_asink)) < 0)
		goto end;

	is->in_audio_filter = filt_asrc;
	is->out_audio_filter = filt_asink;

end:
	if (ret < 0)
		avfilter_graph_free(&is->agraph);
	return ret;
}
#endif  /* CONFIG_AVFILTER */

static void decoder_start(Decoder *d, int(*fn)(void *), void *arg)
{
	packet_queue_start(d->queue);
	d->decoder_tid = createThread(fn, arg);
}

static void decoder_init(Decoder *d, AVCodecContext *avctx, PacketQueue *queue, cond_t *empty_queue_cond) {
	memset(d, 0, sizeof(Decoder));
	d->avctx = avctx;
	d->queue = queue;
	d->empty_queue_cond = empty_queue_cond;
	d->start_pts = AV_NOPTS_VALUE;
}

//static double prevTT = 0;
static int audio_thread(void *arg){
	VideoState *is = (VideoState *)arg;
	AVFrame *frame = av_frame_alloc();
	Frame *af;
#if CONFIG_AVFILTER
	int last_serial = -1;
	int64_t dec_channel_layout;
	int reconfigure;
#endif
	int got_frame = 0;
	AVRational tb;
	int ret = 0;

	if (!frame)
		return AVERROR(ENOMEM);

	do {
		if ((got_frame = decoder_decode_frame(&is->auddec, frame, NULL)) < 0)
		{
			if (is->audioq.eof)
			{
				if (is->auddec.queue->abort_request)
				{
					goto the_end;
				}
				is->sampq.eof = 1;
				Delay(10);
				continue;
			}
			goto the_end;
		}
		is->sampq.eof = 0;
		if (got_frame) {
			//tb = (AVRational){ 1, frame->sample_rate };
			tb.num = 1;
			tb.den = frame->sample_rate;
//			My_log(0, 0, "frame->pts = %f", (double)frame->pts);
#if CONFIG_AVFILTER
			dec_channel_layout = get_valid_channel_layout(frame->channel_layout, av_frame_get_channels(frame));

			reconfigure =
				cmp_audio_fmts(is->audio_filter_src.fmt, is->audio_filter_src.channels,
				(AVSampleFormat)frame->format, av_frame_get_channels(frame)) ||
				is->audio_filter_src.channel_layout != dec_channel_layout ||
				is->audio_filter_src.freq != frame->sample_rate ||
				is->auddec.pkt_serial != last_serial;

			if (reconfigure) {
				char buf1[1024], buf2[1024];
				av_get_channel_layout_string(buf1, sizeof(buf1), -1, is->audio_filter_src.channel_layout);
				av_get_channel_layout_string(buf2, sizeof(buf2), -1, dec_channel_layout);
				My_log(NULL, AV_LOG_DEBUG,
					"Audio frame changed from rate:%d ch:%d fmt:%s layout:%s serial:%d to rate:%d ch:%d fmt:%s layout:%s serial:%d\n",
					is->audio_filter_src.freq, is->audio_filter_src.channels, av_get_sample_fmt_name(is->audio_filter_src.fmt), buf1, last_serial,
					frame->sample_rate, av_frame_get_channels(frame), av_get_sample_fmt_name((AVSampleFormat)frame->format), buf2, is->auddec.pkt_serial);

				is->audio_filter_src.fmt = (AVSampleFormat)frame->format;
				is->audio_filter_src.channels = av_frame_get_channels(frame);
				is->audio_filter_src.channel_layout = dec_channel_layout;
				is->audio_filter_src.freq = frame->sample_rate;
				last_serial = is->auddec.pkt_serial;

				if ((ret = configure_audio_filters(is, afilters, 1)) < 0)
					goto the_end;
			}

			if ((ret = av_buffersrc_add_frame(is->in_audio_filter, frame)) < 0)
				goto the_end;

			while ((ret = av_buffersink_get_frame_flags(is->out_audio_filter, frame, 0)) >= 0) {
				tb = is->out_audio_filter->inputs[0]->time_base;
#endif
				if (!(af = frame_queue_peek_writable(&is->sampq,is)))
					goto the_end;

				af->pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
//				if (prevTT == 0)
//					prevTT = af->pts;
//				My_log(0, 0, "frame_queue_push af->pts = %f,af_diff = %f", af->pts,af->pts-prevTT);
//				prevTT = af->pts;
				af->pos = av_frame_get_pkt_pos(frame);
				af->serial = is->auddec.pkt_serial;
				//af->duration = av_q2d((AVRational){ frame->nb_samples, frame->sample_rate });
				AVRational tmp;
				tmp.num = frame->nb_samples;
				tmp.den = frame->sample_rate;
				af->duration = av_q2d(tmp);

				av_frame_move_ref(af->frame, frame);
				frame_queue_push(&is->sampq);

#if CONFIG_AVFILTER
				if (is->audioq.serial != is->auddec.pkt_serial)
					break;
			}
			if (ret == AVERROR_EOF)
				is->auddec.finished = is->auddec.pkt_serial;
#endif
		}
	} while (ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF);
the_end:
#if CONFIG_AVFILTER
	avfilter_graph_free(&is->agraph);
#endif
	av_frame_free(&frame);
	is->sampq.eof = 1;
	return ret;
}

static int video_thread(void *arg){
	VideoState *is = (VideoState *)arg;
	AVFrame *frame = av_frame_alloc();
	double pts;
	double duration;
	int ret;
	AVRational tb = is->video_st->time_base;
	AVRational frame_rate = av_guess_frame_rate(is->ic, is->video_st, NULL);

#if CONFIG_AVFILTER
	AVFilterGraph *graph = avfilter_graph_alloc();
	AVFilterContext *filt_out = NULL, *filt_in = NULL;
	int last_w = 0;
	int last_h = 0;
	enum AVPixelFormat last_format = (AVPixelFormat)(-2);
	int last_serial = -1;
	int last_vfilter_idx = 0;
	if (!graph) {
		av_frame_free(&frame);
		return AVERROR(ENOMEM);
	}

#endif

	if (!frame) {
#if CONFIG_AVFILTER
		avfilter_graph_free(&graph);
#endif
		return AVERROR(ENOMEM);
	}

	for (;;) {
     //   double tt = cc_clock();
		ret = get_video_frame(is, frame);
     //   My_log(0,0,"get_video_frame = %f",cc_clock()-tt);
        if (ret < 0){
      //      My_log(0,0,"ret < 0 !\n");
			goto the_end;
        }
        if (!ret){
      //      My_log(0,0,"continue ret==0\n");
			continue;
        }

#if CONFIG_AVFILTER
		if (last_w != frame->width
			|| last_h != frame->height
			|| last_format != frame->format
			|| last_serial != is->viddec.pkt_serial
			|| last_vfilter_idx != is->vfilter_idx) {
			My_log(NULL, AV_LOG_DEBUG,
				"Video frame changed from size:%dx%d format:%s serial:%d to size:%dx%d format:%s serial:%d\n",
				last_w, last_h,
				(const char *)av_x_if_null(av_get_pix_fmt_name(last_format), "none"), last_serial,
				frame->width, frame->height,
				(const char *)av_x_if_null(av_get_pix_fmt_name((AVPixelFormat)frame->format), "none"), is->viddec.pkt_serial);
			avfilter_graph_free(&graph);
			graph = avfilter_graph_alloc();
            
			if ((ret = configure_video_filters(graph, is, vfilters_list ? vfilters_list[is->vfilter_idx] : NULL, frame)) < 0) {
                /*
				Event event;
				event.type = FF_QUIT_EVENT;
				event.user.data1 = is;
				SDL_PushEvent(&event);
                */
                char errormsg[1024];
                av_strerror(ret,errormsg,1024);
                My_log(0,0,"configure_vide_filters failed errorcode = %d , errormsg = %s!\n",ret,errormsg);
				goto the_end;
			}
			filt_in = is->in_video_filter;
			filt_out = is->out_video_filter;
			last_w = frame->width;
			last_h = frame->height;
			last_format = (AVPixelFormat)frame->format;
			last_serial = is->viddec.pkt_serial;
			last_vfilter_idx = is->vfilter_idx;
			frame_rate = filt_out->inputs[0]->frame_rate;
		}

		ret = av_buffersrc_add_frame(filt_in, frame);
        if (ret < 0){
        //    My_log(0,0,"goto end!\n");
			goto the_end;
        }

		while (ret >= 0) {
			is->frame_last_returned_time = av_gettime_relative() / 1000000.0;

			ret = av_buffersink_get_frame_flags(filt_out, frame, 0);
			if (ret < 0) {
				if (ret == AVERROR_EOF)
					is->viddec.finished = is->viddec.pkt_serial;
				
            //    char errmsg[1024];
            //    av_strerror(ret, errmsg, 1024);
            //    My_log(0,0,"AVERROR_EOF %d %s!\n",ret,errmsg);
                ret = 0;
				break;
			}

			is->frame_last_filter_delay = av_gettime_relative() / 1000000.0 - is->frame_last_returned_time;
			if (fabs(is->frame_last_filter_delay) > AV_NOSYNC_THRESHOLD / 10.0)
				is->frame_last_filter_delay = 0;
			tb = filt_out->inputs[0]->time_base;
#endif
			//duration = (frame_rate.num && frame_rate.den ? av_q2d((AVRational){ frame_rate.den, frame_rate.num }) : 0);
			if (frame_rate.num && frame_rate.den){
				AVRational tmp;
				tmp.num = frame_rate.den;
				tmp.den = frame_rate.num;
				duration = av_q2d(tmp);
			}
			else
				duration = 0;
			pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
          //  double t0 = cc_clock();
			ret = queue_picture(is, frame, pts, duration, av_frame_get_pkt_pos(frame), is->viddec.pkt_serial);
          //  My_log(0,0,"call queue_picture %f ",cc_clock()-t0);
			av_frame_unref(frame);
#if CONFIG_AVFILTER
		}
#endif

		if (ret < 0)
			goto the_end;
	}
the_end:
#if CONFIG_AVFILTER
	avfilter_graph_free(&graph);
#endif
	av_frame_free(&frame);
	return 0;
}

static int subtitle_thread(void *arg){
	VideoState *is = (VideoState *)arg;
	Frame *sp;
	int got_subtitle;
	double pts;
	int i, j;
	int r, g, b, y, u, v, a;

	for (;;) {
		if (!(sp = frame_queue_peek_writable(&is->subpq,is)))
			return 0;

		if ((got_subtitle = decoder_decode_frame(&is->subdec, NULL, &sp->sub)) < 0)
			break;

		pts = 0;

		if (got_subtitle && sp->sub.format == 0) {
			if (sp->sub.pts != AV_NOPTS_VALUE)
				pts = sp->sub.pts / (double)AV_TIME_BASE;
			sp->pts = pts;
			sp->serial = is->subdec.pkt_serial;

			for (i = 0; i < (int)sp->sub.num_rects; i++)
			{
				for (j = 0; j < sp->sub.rects[i]->nb_colors; j++)
				{
					RGBA_IN(r, g, b, a, (uint32_t*)sp->sub.rects[i]->pict.data[1] + j);
					y = RGB_TO_Y_CCIR(r, g, b);
					u = RGB_TO_U_CCIR(r, g, b, 0);
					v = RGB_TO_V_CCIR(r, g, b, 0);
					YUVA_OUT((uint32_t*)sp->sub.rects[i]->pict.data[1] + j, y, u, v, a);
				}
			}

			/* now we can update the picture count */
			frame_queue_push(&is->subpq);
		}
		else if (got_subtitle) {
			avsubtitle_free(&sp->sub);
		}
	}
	return 0;
}

#if CONFIG_VIDEOTOOLBOX

const HWAccel hwaccels[] = {
#if HAVE_VDPAU_X11
    { "vdpau", vdpau_init, HWACCEL_VDPAU, AV_PIX_FMT_VDPAU },
#endif
#if HAVE_DXVA2_LIB
    { "dxva2", dxva2_init, HWACCEL_DXVA2, AV_PIX_FMT_DXVA2_VLD },
#endif
#if CONFIG_VDA
    { "vda", videotoolbox_init, HWACCEL_VDA, AV_PIX_FMT_VDA },
#endif
#if CONFIG_VIDEOTOOLBOX
    { "videotoolbox", videotoolbox_init, HWACCEL_VIDEOTOOLBOX, AV_PIX_FMT_VIDEOTOOLBOX },
#endif
#if CONFIG_LIBMFX
    { "qsv", qsv_init, HWACCEL_QSV, AV_PIX_FMT_QSV },
#endif
    { 0 },
};

static const HWAccel *get_hwaccel(enum AVPixelFormat pix_fmt)
{
    int i;
    for (i = 0; hwaccels[i].name; i++)
        if (hwaccels[i].pix_fmt == pix_fmt)
            return &hwaccels[i];
    return NULL;
}

static int get_buffer(struct AVCodecContext *s, AVFrame *frame, int flags)
{
    VideoState *ist = (VideoState *)s->opaque;
    
    if (ist->hwaccel_get_buffer && frame->format == ist->hwaccel_pix_fmt)
        return ist->hwaccel_get_buffer(s, frame, flags);
    
    return avcodec_default_get_buffer2(s, frame, flags);
}

static enum AVPixelFormat get_format(struct AVCodecContext *s, const enum AVPixelFormat * pix_fmts)
{
    VideoState *ist = (VideoState *)s->opaque;
    const enum AVPixelFormat *p;
    int ret;
    
    for (p = pix_fmts; *p != -1; p++) {
        const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get(*p);
        const HWAccel *hwaccel;
        
        if (!(desc->flags & AV_PIX_FMT_FLAG_HWACCEL))
            break;
        
        hwaccel = get_hwaccel(*p);
     //   if (!hwaccel ||
     //       (ist->active_hwaccel_id && ist->active_hwaccel_id != hwaccel->id) ||
     //       (ist->hwaccel_id != HWACCEL_AUTO && ist->hwaccel_id != hwaccel->id))
        if(!hwaccel)
            continue;
        
        ist->hwaccel_id = hwaccel->id;
        ret = hwaccel->init(s);
        if (ret < 0) {
            if (ist->hwaccel_id == hwaccel->id) {
                av_log(NULL, AV_LOG_FATAL,
                       "%s hwaccel requested for input stream #?:?, "
                       "but cannot be initialized.\n", hwaccel->name);
                return AV_PIX_FMT_NONE;
            }
            continue;
        }
        ist->active_hwaccel_id = hwaccel->id;
        ist->hwaccel_pix_fmt = *p;
        break;
    }
    
    return *p;
}
#endif

/* open a given stream. Return 0 if OK */
static int stream_component_open(VideoState *is, int stream_index)
{
	AVFormatContext *ic = is->ic;
	AVCodecContext *avctx;
	AVCodec *codec;
	const char *forced_codec_name = NULL;
	AVDictionary *opts;
	AVDictionaryEntry *t = NULL;
	int sample_rate, nb_channels;
	int64_t channel_layout;
	int ret = 0;
	int stream_lowres = lowres;

	if (stream_index < 0 || stream_index >= (int)ic->nb_streams)
		return -1;
	avctx = ic->streams[stream_index]->codec;

 
    codec = avcodec_find_decoder(avctx->codec_id);
    
	switch (avctx->codec_type){
	case AVMEDIA_TYPE_AUDIO: is->last_audio_stream = stream_index; forced_codec_name = audio_codec_name; break;
	case AVMEDIA_TYPE_SUBTITLE: is->last_subtitle_stream = stream_index; forced_codec_name = subtitle_codec_name; break;
	case AVMEDIA_TYPE_VIDEO: is->last_video_stream = stream_index; forced_codec_name = video_codec_name; break;
	}
	if (forced_codec_name)
		codec = avcodec_find_decoder_by_name(forced_codec_name);
	if (!codec) {
		if (forced_codec_name) My_log(NULL, AV_LOG_WARNING,
			"No codec could be found with name '%s'\n", forced_codec_name);
		else                   My_log(NULL, AV_LOG_WARNING,
			"No codec could be found with id %d\n", avctx->codec_id);
		return -1;
	}

	avctx->codec_id = codec->id;
	if (stream_lowres > av_codec_get_max_lowres(codec)){
		My_log(avctx, AV_LOG_WARNING, "The maximum value for lowres supported by the decoder is %d\n",
			av_codec_get_max_lowres(codec));
		stream_lowres = av_codec_get_max_lowres(codec);
	}
	av_codec_set_lowres(avctx, stream_lowres);

	if (stream_lowres) avctx->flags |= CODEC_FLAG_EMU_EDGE;
	if (fast)   avctx->flags2 |= CODEC_FLAG2_FAST;
	if (codec->capabilities & CODEC_CAP_DR1)
		avctx->flags |= CODEC_FLAG_EMU_EDGE;

	opts = filter_codec_opts(codec_opts, avctx->codec_id, ic, ic->streams[stream_index], codec);
	if (!av_dict_get(opts, "threads", NULL, 0))
		av_dict_set(&opts, "threads", "auto", 0);
	if (stream_lowres)
		av_dict_set_int(&opts, "lowres", stream_lowres, 0);
	if (avctx->codec_type == AVMEDIA_TYPE_VIDEO || avctx->codec_type == AVMEDIA_TYPE_AUDIO)
		av_dict_set(&opts, "refcounted_frames", "1", 0);
    
    /*
     * 打开iOS VideoToolbox视频加速功能
     */
#if CONFIG_VIDEOTOOLBOX
    if(avctx->codec_type==AVMEDIA_TYPE_VIDEO){
        avctx->opaque = is;
        avctx->get_buffer2 = get_buffer;
        avctx->get_format = get_format;
        avctx->thread_safe_callbacks = 1;
    }
#endif
	if ((ret = avcodec_open2(avctx, codec, &opts)) < 0) {
		goto fail;
	}
    
	if ((t = av_dict_get(opts, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
		My_log(NULL, AV_LOG_ERROR, "Option %s not found.\n", t->key);
		ret = AVERROR_OPTION_NOT_FOUND;
		goto fail;
	}

	is->eof = 0;
	ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;
	switch (avctx->codec_type) {
	case AVMEDIA_TYPE_AUDIO:
#if CONFIG_AVFILTER
	{
							   AVFilterLink *link;

							   is->audio_filter_src.freq = avctx->sample_rate;
							   is->audio_filter_src.channels = avctx->channels;
							   is->audio_filter_src.channel_layout = get_valid_channel_layout(avctx->channel_layout, avctx->channels);
							   is->audio_filter_src.fmt = avctx->sample_fmt;
							   if ((ret = configure_audio_filters(is, afilters, 0)) < 0)
								   goto fail;
							   link = is->out_audio_filter->inputs[0];
							   sample_rate = link->sample_rate;
							   nb_channels = link->channels;
							   channel_layout = link->channel_layout;
	}
#else
		sample_rate = avctx->sample_rate;
		nb_channels = avctx->channels;
		channel_layout = avctx->channel_layout;
#endif

		/* prepare audio output */
		if ((ret = audio_open(is, channel_layout, nb_channels, sample_rate, &is->audio_tgt)) < 0)
			goto fail;
		is->audio_hw_buf_size = ret;
		is->audio_src = is->audio_tgt;
		is->audio_buf_size = 0;
		is->audio_buf_index = 0;

		/* init averaging filter */
		is->audio_diff_avg_coef = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
		is->audio_diff_avg_count = 0;
		/* since we do not have a precise anough audio fifo fullness,
		we correct audio sync only if larger than this threshold */
		is->audio_diff_threshold = (double)(is->audio_hw_buf_size) / is->audio_tgt.bytes_per_sec;

		is->audio_stream = stream_index;
		is->audio_st = ic->streams[stream_index];

		decoder_init(&is->auddec, avctx, &is->audioq, is->continue_read_thread);
		if ((is->ic->iformat->flags & (AVFMT_NOBINSEARCH | AVFMT_NOGENSEARCH | AVFMT_NO_BYTE_SEEK)) && !is->ic->iformat->read_seek) {
			is->auddec.start_pts = is->audio_st->start_time;
			is->auddec.start_pts_tb = is->audio_st->time_base;
		}
		decoder_start(&is->auddec, audio_thread, is);
		//PauseAudio(0);
		break;
	case AVMEDIA_TYPE_VIDEO:
		is->video_stream = stream_index;
		is->video_st = ic->streams[stream_index];

		decoder_init(&is->viddec, avctx, &is->videoq, is->continue_read_thread);
		decoder_start(&is->viddec, video_thread, is);
		is->queue_attachments_req = 1;
		break;
	case AVMEDIA_TYPE_SUBTITLE:
		is->subtitle_stream = stream_index;
		is->subtitle_st = ic->streams[stream_index];

		decoder_init(&is->subdec, avctx, &is->subtitleq, is->continue_read_thread);
		decoder_start(&is->subdec, subtitle_thread, is);
		break;
	default:
		break;
	}

fail:
	av_dict_free(&opts);

	return ret;
}

/* seek in the stream */
void stream_seek(VideoState *is, int64_t pos, int64_t rel, int seek_by_bytes)
{
	if (!is->seek_req) {

		is->seek_pos = pos;
		is->seek_rel = rel;
		is->seek_flags &= ~AVSEEK_FLAG_BYTE;
		if (seek_by_bytes)
			is->seek_flags |= AVSEEK_FLAG_BYTE;
		is->seek_req = 1;
		signalCond(is->continue_read_thread);
	}
}

void step_to_next_frame(VideoState *is)
{
	/* if the stream is paused unpause it, then step */
	if (is->paused)
		stream_toggle_pause(is);
	is->step = 1;
}

//static int64_t last_read_pts = 0;
static void avlog(void * l,int level,const char *fmt,va_list vl)
{
	char buf[1024];
//	va_list args;
//	va_start(args, fmt);
	vsnprintf(buf, 1024 - 3, fmt, vl);
//	va_end(args);
	CCLog(buf);
}
/* this thread gets the stream from the disk or the network */
static int read_thread(void *arg)
{
	VideoState *is = (VideoState *)arg;
	AVFormatContext *ic = NULL;
	int err, i, ret;
	int st_index[AVMEDIA_TYPE_NB];
	AVPacket pkt1, *pkt = &pkt1;
	int64_t stream_start_time;
	int pkt_in_play_range = 0;
	AVDictionaryEntry *t;
	AVDictionary **opts;
	int orig_nb_streams;
	mutex_t *wait_mutex = createMutex();
	int scan_all_pmts_set = 0;
	int64_t pkt_ts;

	memset(st_index, -1, sizeof(st_index));
	is->last_video_stream = is->video_stream = -1;
	is->last_audio_stream = is->audio_stream = -1;
	is->last_subtitle_stream = is->subtitle_stream = -1;
	is->eof = 0;

	ic = avformat_alloc_context();
	/*
    My_log(0,0,"filename = %d \n start_time = %d \n duration = %d \n bit_rate = %d \n packet_size=%d \n max_delay offset = %d",
           offsetof(AVFormatContext, filename),
           offsetof(AVFormatContext, start_time),
           offsetof(AVFormatContext, duration),
           offsetof(AVFormatContext, bit_rate),
           offsetof(AVFormatContext, packet_size),
           offsetof(AVFormatContext, max_delay));
    My_log(0,0,"duration = %d \n bit_rate = %d \n packet_size = %d \n max_delay = %d",
           sizeof(ic->duration),
           sizeof(ic->bit_rate),
           sizeof(ic->packet_size),
           sizeof(ic->max_delay));
	*/
	if (!ic) {
		//My_log(NULL, AV_LOG_FATAL, "Could not allocate context.\n");
		ret = AVERROR(ENOMEM);
		is->errmsg = "Could not allocate context.";
		is->errcode = ret;
		goto fail;
	}
	ic->interrupt_callback.callback = decode_interrupt_cb;
	ic->interrupt_callback.opaque = is;
	if (!av_dict_get(format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE)) {
		av_dict_set(&format_opts, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);
		scan_all_pmts_set = 1;
	}
	err = avformat_open_input(&ic, is->filename, is->iformat, &format_opts);
	if (err < 0) {
		char errmsg[1024];
		av_strerror(err, errmsg, 1024);
		My_log(NULL, AV_LOG_ERROR, "ffmpeg error msg:%s", errmsg);
		is->errmsg = "Could not open input file.";
		is->errcode = -1;
		ret = -1;
		goto fail;
	}
	
	//if (scan_all_pmts_set)
		av_dict_set(&format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE);

	if ((t = av_dict_get(format_opts, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
		My_log(NULL, AV_LOG_ERROR, "Option %s not found.\n", t->key);
		ret = AVERROR_OPTION_NOT_FOUND;
		is->errmsg = "Option not found";
		is->errcode = -1;
		goto fail;
	}
	is->ic = ic;

	if (genpts)
		ic->flags |= AVFMT_FLAG_GENPTS;

	av_format_inject_global_side_data(ic);

	opts = setup_find_stream_info_opts(ic, codec_opts);
	orig_nb_streams = ic->nb_streams;

	err = avformat_find_stream_info(ic, opts);

	for (i = 0; i < orig_nb_streams; i++)
		av_dict_free(&opts[i]);
	av_freep(&opts);

	if (err < 0) {
		My_log(NULL, AV_LOG_WARNING,
			"%s: could not find codec parameters\n", is->filename);
		is->errmsg = "Option not found";
		is->errcode = -1;
		ret = -1;
		goto fail;
	}

	if (ic->pb)
		ic->pb->eof_reached = 0; // FIXME hack, ffplay maybe should not use avio_feof() to test for the end

	if (seek_by_bytes < 0)
		seek_by_bytes = !!(ic->iformat->flags & AVFMT_TS_DISCONT) && strcmp("ogg", ic->iformat->name);

	is->max_frame_duration = (ic->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;

   // My_log(0,0,"ic=%X , metadata=%X ",ic,ic->metadata);

	if (!window_title && (t = av_dict_get(ic->metadata, "title", NULL, 0)))
		window_title = av_asprintf("%s - %s", t->value, is->filename);

	/* if seeking requested, we execute it */
	if (start_time != AV_NOPTS_VALUE) {
		int64_t timestamp;

		timestamp = start_time;
		/* add the stream start time */
		if (ic->start_time != AV_NOPTS_VALUE)
			timestamp += ic->start_time;
		ret = avformat_seek_file(ic, -1, INT64_MIN, timestamp, INT64_MAX, 0);
		if (ret < 0) {
			My_log(NULL, AV_LOG_WARNING, "%s: could not seek to position %0.3f\n",
				is->filename, (double)timestamp / AV_TIME_BASE);
		}
	}

	is->realtime = is_realtime(ic);

	if (show_status)
		av_dump_format(ic, 0, is->filename, 0);

	for (i = 0; i < (int)ic->nb_streams; i++) {
		AVStream *st = ic->streams[i];
		enum AVMediaType type = st->codec->codec_type;
		st->discard = AVDISCARD_ALL;
		if (wanted_stream_spec[type] && st_index[type] == -1)
		if (avformat_match_stream_specifier(ic, st, wanted_stream_spec[type]) > 0)
			st_index[type] = i;
	}
	for (i = 0; i < AVMEDIA_TYPE_NB; i++) {
		if (wanted_stream_spec[i] && st_index[i] == -1) {
			My_log(NULL, AV_LOG_ERROR, "Stream specifier %s does not match any %s stream\n", wanted_stream_spec[i], av_get_media_type_string((AVMediaType)i));
			st_index[i] = INT_MAX;
		}
	}

	if (!video_disable)
		st_index[AVMEDIA_TYPE_VIDEO] =
		av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO,
		st_index[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);
	if (!audio_disable)
		st_index[AVMEDIA_TYPE_AUDIO] =
		av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO,
		st_index[AVMEDIA_TYPE_AUDIO],
		st_index[AVMEDIA_TYPE_VIDEO],
		NULL, 0);
	if (!video_disable && !subtitle_disable)
		st_index[AVMEDIA_TYPE_SUBTITLE] =
		av_find_best_stream(ic, AVMEDIA_TYPE_SUBTITLE,
		st_index[AVMEDIA_TYPE_SUBTITLE],
		(st_index[AVMEDIA_TYPE_AUDIO] >= 0 ?
		st_index[AVMEDIA_TYPE_AUDIO] :
		st_index[AVMEDIA_TYPE_VIDEO]),
		NULL, 0);

	is->show_mode = show_mode;
	if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
		AVStream *st = ic->streams[st_index[AVMEDIA_TYPE_VIDEO]];
		AVCodecContext *avctx = st->codec;
		AVRational sar = av_guess_sample_aspect_ratio(ic, st, NULL);
		if (avctx->width)
			set_default_window_size(avctx->width, avctx->height, sar);
	}

	/* open the streams */
	if (st_index[AVMEDIA_TYPE_AUDIO] >= 0) {
		stream_component_open(is, st_index[AVMEDIA_TYPE_AUDIO]);
	}

	ret = -1;
	if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
		ret = stream_component_open(is, st_index[AVMEDIA_TYPE_VIDEO]);
	}
	if (is->show_mode == SHOW_MODE_NONE)
		is->show_mode = ret >= 0 ? SHOW_MODE_VIDEO : SHOW_MODE_RDFT;

	if (st_index[AVMEDIA_TYPE_SUBTITLE] >= 0) {
		stream_component_open(is, st_index[AVMEDIA_TYPE_SUBTITLE]);
	}

	if (is->video_stream < 0 && is->audio_stream < 0) {
		My_log(NULL, AV_LOG_FATAL, "Failed to open file '%s' or configure filtergraph\n",
			is->filename);
		ret = -1;
		is->errmsg = "Failed to open file or configure filtergraph";
		is->errcode = -1;
		goto fail;
	}

	if (infinite_buffer < 0 && is->realtime)
		infinite_buffer = 1;

	for (;;) {
		if (is->abort_request)
			break;
		if (is->paused != is->last_paused) {
			is->last_paused = is->paused;
			if (is->paused)
				is->read_pause_return = av_read_pause(ic);
			else
				av_read_play(ic);
		}
#if CONFIG_RTSP_DEMUXER || CONFIG_MMSH_PROTOCOL
		if (is->paused &&
			(!strcmp(ic->iformat->name, "rtsp") ||
			(ic->pb && !strncmp(is->filename, "mmsh:", 5)))) {
			/* wait 10 ms to avoid trying to get another packet */
			/* XXX: horrible */
			Delay(10);
			continue;
		}
#endif
		if (is->seek_req) {
			int64_t seek_target = is->seek_pos;
			int64_t seek_min = is->seek_rel > 0 ? seek_target - is->seek_rel + 2 : INT64_MIN;
			int64_t seek_max = is->seek_rel < 0 ? seek_target - is->seek_rel - 2 : INT64_MAX;
			// FIXME the +-2 is due to rounding being not done in the correct direction in generation
			//      of the seek_pos/seek_rel variables

			ret = avformat_seek_file(is->ic, -1, seek_min, seek_target, seek_max, is->seek_flags);
			if (ret < 0) {
				My_log(NULL, AV_LOG_ERROR,
					"%s: error while seeking\n", is->ic->filename);
			}
			else {
				if (is->audio_stream >= 0) {
					packet_queue_flush(&is->audioq);
					packet_queue_put(&is->audioq, &flush_pkt);
				}
				if (is->subtitle_stream >= 0) {
					packet_queue_flush(&is->subtitleq);
					packet_queue_put(&is->subtitleq, &flush_pkt);
				}
				if (is->video_stream >= 0) {
					packet_queue_flush(&is->videoq);
					packet_queue_put(&is->videoq, &flush_pkt);
				}
				if (is->seek_flags & AVSEEK_FLAG_BYTE) {
				//	My_log(0, 0, "is->seek_flags & AVSEEK_FLAG_BYTE");
					set_clock(&is->extclk, NAN, 0);
				}
				else {
				//	My_log(0, 0, "is->seek_flags & AVSEEK_FLAG_BYTE 2");
					set_clock(&is->extclk, seek_target / (double)AV_TIME_BASE, 0);
				}
			}
			is->seek_req = 0;
			is->queue_attachments_req = 1;
			is->eof = 0;
			if (is->paused)
				step_to_next_frame(is);
		}
		if (is->queue_attachments_req) {
			if (is->video_st && is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC) {
				AVPacket copy;
				if ((ret = av_copy_packet(&copy, &is->video_st->attached_pic)) < 0)
					goto fail;
				packet_queue_put(&is->videoq, &copy);
				packet_queue_put_nullpacket(&is->videoq, is->video_stream);
			}
			is->queue_attachments_req = 0;
		}

		/* if the queue are full, no need to read more */
		if (infinite_buffer<1 &&
			(is->audioq.size + is->videoq.size + is->subtitleq.size > MAX_QUEUE_SIZE
			|| ((is->audioq.nb_packets > is->nMIN_FRAMES || is->audio_stream < 0 || is->audioq.abort_request)
			&& (is->videoq.nb_packets > is->nMIN_FRAMES || is->video_stream < 0 || is->videoq.abort_request
			|| (is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC))
			&& (is->subtitleq.nb_packets > is->nMIN_FRAMES || is->subtitle_stream < 0 || is->subtitleq.abort_request)))) {
			/* wait 10 ms */
			//lockMutex(wait_mutex);
			{
				std::unique_lock<mutex_t> lk(*wait_mutex);
				//waitCondTimeout(is->continue_read_thread, wait_mutex, 10);
				is->continue_read_thread->wait_for(lk, std::chrono::milliseconds(10));
			}
			//unlockMutex(wait_mutex);
			continue;
		}
		if (!is->paused &&
			(!is->audio_st || (is->auddec.finished == is->audioq.serial && frame_queue_nb_remaining(&is->sampq) == 0)) &&
			(!is->video_st || (is->viddec.finished == is->videoq.serial && frame_queue_nb_remaining(&is->pictq) == 0))) {
			if (loop != 1 && (!loop || --loop)) {
				stream_seek(is, start_time != AV_NOPTS_VALUE ? start_time : 0, 0, 0);
			}
			else if (autoexit) {
				ret = AVERROR_EOF;
				goto fail;
			}
		}
		ret = av_read_frame(ic, pkt);
		if (ret < 0) {
			if ((ret == AVERROR_EOF || avio_feof(ic->pb)) && !is->eof) {
				if (is->video_stream >= 0)
					packet_queue_put_nullpacket(&is->videoq, is->video_stream);
				if (is->audio_stream >= 0)
				{
					packet_queue_put_nullpacket(&is->audioq, is->audio_stream);
					is->audioq.eof = 1;
				}
				if (is->subtitle_stream >= 0)
					packet_queue_put_nullpacket(&is->subtitleq, is->subtitle_stream);
				is->eof = 1;
			}
			if (ic->pb && ic->pb->error)
				break;
			//lockMutex(wait_mutex);
			{
				std::unique_lock<mutex_t> lk(*wait_mutex);
				//waitCondTimeout(is->continue_read_thread, wait_mutex, 10);
				is->continue_read_thread->wait_for(lk, std::chrono::milliseconds(10));
			}
			//unlockMutex(wait_mutex);
			continue;
		}
		else {
			is->eof = 0;
			is->audioq.eof = 0;
		}
		//if (pkt->stream_index == is->audio_stream){
		//	if (last_read_pts == 0){
		//		last_read_pts = pkt->pts;
		//		AllocConsole();
		//		freopen("CONIN$", "r", stdin);
		//		freopen("CONOUT$", "w", stdout);
		//		freopen("CONOUT$", "w", stderr);
		//	}
			//if (pkt->pts - last_read_pts>2100)
			//	My_log(0, 0, "1read_thread pts = %I64d,read_thread_diff= %I64d",pkt->pts,pkt->pts-last_read_pts);
			//else
			//	My_log(0, 0, "2read_thread pts = %I64d,read_thread_diff= %I64d", pkt->pts, pkt->pts - last_read_pts);
		//	last_read_pts = pkt->pts;
		//}

		/* check if packet is in play range specified by user, then queue, otherwise discard */
		stream_start_time = ic->streams[pkt->stream_index]->start_time;
		pkt_ts = pkt->pts == AV_NOPTS_VALUE ? pkt->dts : pkt->pts;
		pkt_in_play_range = duration == AV_NOPTS_VALUE ||
			(pkt_ts - (stream_start_time != AV_NOPTS_VALUE ? stream_start_time : 0)) *
			av_q2d(ic->streams[pkt->stream_index]->time_base) -
			(double)(start_time != AV_NOPTS_VALUE ? start_time : 0) / 1000000
			<= ((double)duration / 1000000);
		if (pkt->stream_index == is->audio_stream && pkt_in_play_range) {
			packet_queue_put(&is->audioq, pkt);
		}
		else if (pkt->stream_index == is->video_stream && pkt_in_play_range
			&& !(is->video_st->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
			packet_queue_put(&is->videoq, pkt);
		}
		else if (pkt->stream_index == is->subtitle_stream && pkt_in_play_range) {
			packet_queue_put(&is->subtitleq, pkt);
		}
		else {
			av_free_packet(pkt);
		}
	}
	/* wait until the end */
	while (!is->abort_request) {
		Delay(100);
	}

	ret = 0;
fail:
	/* close each stream */
	if (is->audio_stream >= 0)
		stream_component_close(is, is->audio_stream);
	if (is->video_stream >= 0)
		stream_component_close(is, is->video_stream);
	if (is->subtitle_stream >= 0)
		stream_component_close(is, is->subtitle_stream);
	if (ic) {
		avformat_close_input(&ic);
		is->ic = NULL;
	}
	/*
	if (ret != 0) {
		Event event;

		event.type = FF_QUIT_EVENT;
		event.user.data1 = is;
		PushEvent(&event);
	}
	*/
	destroyMutex(wait_mutex);
	return 0;
}

VideoState *stream_open(const char *filename, AVInputFormat *iformat)
{
	VideoState *is;

	is = (VideoState *)av_mallocz(sizeof(VideoState));
	if (!is)
		return NULL;
	av_strlcpy(is->filename, filename, sizeof(is->filename));
	is->iformat = iformat;
	is->ytop = 0;
	is->xleft = 0;
	is->nMIN_FRAMES = MIN_FRAMES;
	is->errcode = 0;
//	is->pscreen = nullptr;
//	is->pscreen2 = nullptr;
    is->isVideoOpen = 0;
	is->current = -1;
	is->step = 1;
	is->errmsg = nullptr;
    is->_currentFrame = av_frame_alloc();
    is->isNewFrame = 0;
    is->pyuv420p.w = -10;
    is->pyuv420p.h = -10;
	do 
	{
		/* start video display */
		if (frame_queue_init(&is->pictq, &is->videoq, VIDEO_PICTURE_QUEUE_SIZE, 1) < 0)
			break;
		if (frame_queue_init(&is->subpq, &is->subtitleq, SUBPICTURE_QUEUE_SIZE, 0) < 0)
			break;
		if (frame_queue_init(&is->sampq, &is->audioq, SAMPLE_QUEUE_SIZE, 1) < 0)
			break;
		packet_queue_init(&is->videoq);
		packet_queue_init(&is->audioq);
		packet_queue_init(&is->subtitleq);

		is->continue_read_thread = createCond();

		init_clock(&is->vidclk, &is->videoq.serial);
		init_clock(&is->audclk, &is->audioq.serial);
		init_clock(&is->extclk, &is->extclk.serial);
		is->audio_clock_serial = -1;
		is->av_sync_type = av_sync_type;
		is->read_tid = createThread(read_thread, is);
		if (!is->read_tid) 
			break;
		return is;
	} while (false); 
	stream_close(is);
	return NULL;
}

static int lockmgr(void **mtx, enum AVLockOp op)
{
	mutex_t **ppmtx = (mutex_t **)mtx;
	switch (op) {
	case AV_LOCK_CREATE:
		*mtx = createMutex();
		if (!*mtx)
			return 1;
		return 0;
	case AV_LOCK_OBTAIN:
		return !!lockMutex(*ppmtx);
	case AV_LOCK_RELEASE:
		return !!unlockMutex(*ppmtx);
	case AV_LOCK_DESTROY:
		destroyMutex(*ppmtx);
		return 0;
	}
	return 1;
}

static int opt_codec(void *optctx, const char *opt, const char *arg)
{
	const char *spec = strchr(opt, ':');
	if (!spec) {
		My_log(NULL, AV_LOG_ERROR,
			"No media specifier was specified in '%s' in option '%s'\n",
			arg, opt);
		return AVERROR(EINVAL);
	}
	spec++;
	switch (spec[0]) {
	case 'a':    audio_codec_name = arg; break;
	case 's': subtitle_codec_name = arg; break;
	case 'v':    video_codec_name = arg; break;
	default:
		My_log(NULL, AV_LOG_ERROR,
			"Invalid media specifier '%s' in option '%s'\n", spec, opt);
		return AVERROR(EINVAL);
	}
	return 0;
}

void stream_cycle_channel(VideoState *is, int codec_type)
{
	AVFormatContext *ic = is->ic;
	int start_index, stream_index;
	int old_index;
	AVStream *st;
	AVProgram *p = NULL;
	int nb_streams = is->ic->nb_streams;

	if (codec_type == AVMEDIA_TYPE_VIDEO) {
		start_index = is->last_video_stream;
		old_index = is->video_stream;
	}
	else if (codec_type == AVMEDIA_TYPE_AUDIO) {
		start_index = is->last_audio_stream;
		old_index = is->audio_stream;
	}
	else {
		start_index = is->last_subtitle_stream;
		old_index = is->subtitle_stream;
	}
	stream_index = start_index;

	if (codec_type != AVMEDIA_TYPE_VIDEO && is->video_stream != -1) {
		p = av_find_program_from_stream(ic, NULL, is->video_stream);
		if (p) {
			nb_streams = p->nb_stream_indexes;
			for (start_index = 0; start_index < nb_streams; start_index++)
			if (p->stream_index[start_index] == stream_index)
				break;
			if (start_index == nb_streams)
				start_index = -1;
			stream_index = start_index;
		}
	}

	for (;;) {
		if (++stream_index >= nb_streams)
		{
			if (codec_type == AVMEDIA_TYPE_SUBTITLE)
			{
				stream_index = -1;
				is->last_subtitle_stream = -1;
				goto the_end;
			}
			if (start_index == -1)
				return;
			stream_index = 0;
		}
		if (stream_index == start_index)
			return;
		st = is->ic->streams[p ? p->stream_index[stream_index] : stream_index];
		if (st->codec->codec_type == codec_type) {
			/* check that parameters are OK */
			switch (codec_type) {
			case AVMEDIA_TYPE_AUDIO:
				if (st->codec->sample_rate != 0 &&
					st->codec->channels != 0)
					goto the_end;
				break;
			case AVMEDIA_TYPE_VIDEO:
			case AVMEDIA_TYPE_SUBTITLE:
				goto the_end;
			default:
				break;
			}
		}
	}
the_end:
	if (p && stream_index != -1)
		stream_index = p->stream_index[stream_index];
	My_log(NULL, AV_LOG_INFO, "Switch %s stream from #%d to #%d\n",
		av_get_media_type_string((AVMediaType)codec_type),
		old_index,
		stream_index);

	stream_component_close(is, old_index);
	stream_component_open(is, stream_index);
}

void toggle_audio_display(VideoState *is)
{
	int next = is->show_mode;
	do {
		next = (next + 1) % SHOW_MODE_NB;
	} while (next != is->show_mode && (next == SHOW_MODE_VIDEO && !is->video_st || next != SHOW_MODE_VIDEO && !is->audio_st));
	if (is->show_mode != next) {
		is->force_refresh = 1;
		is->show_mode = (ShowMode)next;
	}
}

void refresh_loop_wait_event(VideoState *is) {
	double remaining_time = 0.0;
//	if (!cursor_hidden && av_gettime_relative() - cursor_last_shown > CURSOR_HIDE_DELAY) {
//		ShowCursor(0);
//		cursor_hidden = 1;
//	}
	if (remaining_time > 0.0)
		av_usleep((int64_t)(remaining_time * 1000000.0));
	remaining_time = REFRESH_RATE;
	if (is->show_mode != SHOW_MODE_NONE && (!is->paused || is->force_refresh))
		video_refresh(is, &remaining_time);
}

void seek_chapter(VideoState *is, int incr)
{
	int64_t pos = get_master_clock(is) * AV_TIME_BASE;
	int i;
	//AV_TIME_BASE_q∫ÕAV_TIME_BASE_Qœ‡Õ¨
	AVRational AV_TIME_BASE_q;
	AV_TIME_BASE_q.num = 1;
	AV_TIME_BASE_q.den = AV_TIME_BASE;

	if (!is->ic->nb_chapters)
		return;

	/* find the current chapter */
	for (i = 0; i < (int)is->ic->nb_chapters; i++) {
		AVChapter *ch = is->ic->chapters[i];
		if (av_compare_ts(pos, AV_TIME_BASE_q, ch->start, ch->time_base) < 0) {
			i--;
			break;
		}
	}
	
	i += incr;
	i = FFMAX(i, 0);
	if (i >= (int)is->ic->nb_chapters)
		return;

	My_log(NULL, AV_LOG_VERBOSE, "Seeking to chapter %d.\n", i);
	stream_seek(is, av_rescale_q(is->ic->chapters[i]->start, is->ic->chapters[i]->time_base,
		AV_TIME_BASE_q), 0, 0);
}

void initFF()
{
	/* register all codecs, demux and protocols */
#if CONFIG_AVDEVICE
	avdevice_register_all();
#endif
#if CONFIG_AVFILTER
	avfilter_register_all();
#endif
	av_register_all();
	avformat_network_init();
	//œ¬√Êµƒ¥˙¬Î“≤–Ì∫Õµ•∏ˆ ”∆µµƒ≤•∑≈œ‡πÿ
	if (av_lockmgr_register(lockmgr)) {
		My_log(NULL, AV_LOG_FATAL, "Could not initialize lock manager!\n");
		do_exit(NULL);
	}

	av_init_packet(&flush_pkt);
	flush_pkt.data = (uint8_t *)&flush_pkt;

	//av_log_set_callback(avlog);
}
/*
* 取得视频文件的流信息
*/
int getVideoInfo(const char * filename, int *w, int *h)
{
	int err;
	unsigned int i;
	AVFormatContext *ic = NULL;
	ic = avformat_alloc_context();
	err = avformat_open_input(&ic, filename, NULL, &format_opts);
	if (err < 0) {
		char errmsg[1024];
		av_strerror(err, errmsg, 1024);
		My_log(NULL, AV_LOG_ERROR, "ffmpeg error msg:%s", errmsg);
		return 0;
	}
	for (i = 0; i < ic->nb_streams; i++) {
		AVStream *st = ic->streams[i];
		enum AVMediaType type = st->codec->codec_type;
		if (type == AVMEDIA_TYPE_VIDEO){
			*w = st->codec->width;
			*h = st->codec->height;
			avformat_close_input(&ic);
			return 1;
		}
	}
	avformat_close_input(&ic);
	return 0;
}

}