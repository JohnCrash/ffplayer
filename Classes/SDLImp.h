#ifndef _SDLImp_H_
#define _SDLImp_H_
/*
	SDLImp 一个脱离SDL的本地实现
*/
/*
	使用std::thread 替代 SDL_Thread函数
*/
#include <mutex>
#include <condition_variable>
#include <thread>
#include <stdlib.h>

namespace ff{
	#define SDL_min(x, y) (((x) < (y)) ? (x) : (y))
	#define SDL_max(x, y) (((x) > (y)) ? (x) : (y))

	typedef int8_t		Sint8;
	typedef uint8_t		Uint8;
	typedef int16_t		Sint16;
	typedef uint16_t	Uint16;
	typedef int32_t		Sint32;
	typedef uint32_t	Uint32;
	typedef int64_t Sint64;

	inline Uint16 SDL_Swap16(Uint16 x)
	{
		return static_cast<Uint16>( ((x << 8) | (x >> 8)));
	}

	inline Uint32
		SDL_Swap32(Uint32 x)
	{
		return static_cast<Uint32>(((x << 24) | ((x << 8) & 0x00FF0000) |
			((x >> 8) & 0x0000FF00) | (x >> 24)));
	}

	inline float
		SDL_SwapFloat(float x)
	{
		union
		{
			float f;
			Uint32 ui32;
		} swapper;
		swapper.f = x;
		swapper.ui32 = SDL_Swap32(swapper.ui32);
		return swapper.f;
	}

	#if SDL_BYTEORDER == SDL_LIL_ENDIAN
	#define SDL_SwapLE16(X) (X)
	#define SDL_SwapLE32(X) (X)
	#define SDL_SwapLE64(X) (X)
	#define SDL_SwapFloatLE(X)  (X)
	#define SDL_SwapBE16(X) SDL_Swap16(X)
	#define SDL_SwapBE32(X) SDL_Swap32(X)
	#define SDL_SwapBE64(X) SDL_Swap64(X)
	#define SDL_SwapFloatBE(X)  SDL_SwapFloat(X)
	#else
	#define SDL_SwapLE16(X) SDL_Swap16(X)
	#define SDL_SwapLE32(X) SDL_Swap32(X)
	#define SDL_SwapLE64(X) SDL_Swap64(X)
	#define SDL_SwapFloatLE(X)  SDL_SwapFloat(X)
	#define SDL_SwapBE16(X) (X)
	#define SDL_SwapBE32(X) (X)
	#define SDL_SwapBE64(X) (X)
	#define SDL_SwapFloatBE(X)  (X)
	#endif

	typedef std::mutex mutex_t;
	typedef std::condition_variable cond_t;
	typedef std::thread thread_t;
	typedef std::thread::id threadid_t;

	void Delay(Uint32 ms);

	mutex_t* createMutex();
	int lockMutex(mutex_t *);
	int unlockMutex(mutex_t *);
	void destroyMutex(mutex_t *);
	threadid_t currentThreadId();

	cond_t * createCond();
	int waitCond(cond_t*, mutex_t *);
	int signalCond(cond_t*);
	void destroyCond(cond_t *);
	int waitCondTimeout(cond_t* p, mutex_t*, unsigned int ms);

	thread_t* createThread(int(*)(void*), void *);
	void waitThread(thread_t *, int* status);

	struct Rect {
		Sint16 x, y;
		Uint16 w, h;
	};

	#ifdef __cplusplus
	#define SDL_reinterpret_cast(type, expression) reinterpret_cast<type>(expression)
	#define SDL_static_cast(type, expression) static_cast<type>(expression)
	#define SDL_const_cast(type, expression) const_cast<type>(expression)
	#else
	#define SDL_reinterpret_cast(type, expression) ((type)(expression))
	#define SDL_static_cast(type, expression) ((type)(expression))
	#define SDL_const_cast(type, expression) ((type)(expression))
	#endif

	typedef Uint32 AudioDeviceID;
	#define SDL_arraysize(array)    (sizeof(array)/sizeof(array[0]))
	#define SDL_TABLESIZE(table)    SDL_arraysize(table)
	#define INIT_AUDIO          0x00000010
	#define WAVE_FORMAT_IEEE_FLOAT 0x0003
	#define NUM_BUFFERS 2
	#define AUDIO_MASK_BITSIZE       (0xFF)
	#define AUDIO_BITSIZE(x)         (x & AUDIO_MASK_BITSIZE)
	#define SDL_AUDIO_MASK_DATATYPE      (1<<8)
	#define SDL_AUDIO_ISFLOAT(x)         (x & SDL_AUDIO_MASK_DATATYPE)
	#define SDL_zero(x) memset(&(x), 0, sizeof((x)))
	#define SDL_zerop(x) memset((x), 0, sizeof(*(x)))

	#define SDL_AllocAudioMem   malloc
	#define SDL_FreeAudioMem    free

	#define DEFAULT_OUTPUT_DEVNAME "System audio output device"
	#define DEFAULT_INPUT_DEVNAME "System audio capture device"

	#define SDL_AUDIO_ALLOW_FREQUENCY_CHANGE    0x00000001
	#define SDL_AUDIO_ALLOW_FORMAT_CHANGE       0x00000002
	#define SDL_AUDIO_ALLOW_CHANNELS_CHANGE     0x00000004
	#define SDL_AUDIO_ALLOW_ANY_CHANGE          (SDL_AUDIO_ALLOW_FREQUENCY_CHANGE|SDL_AUDIO_ALLOW_FORMAT_CHANGE|SDL_AUDIO_ALLOW_CHANNELS_CHANGE)

	#define SDL_AUDIO_MASK_BITSIZE       (0xFF)
	#define SDL_AUDIO_MASK_DATATYPE      (1<<8)
	#define SDL_AUDIO_MASK_ENDIAN        (1<<12)
	#define SDL_AUDIO_MASK_SIGNED        (1<<15)
	#define SDL_AUDIO_BITSIZE(x)         (x & SDL_AUDIO_MASK_BITSIZE)
	#define SDL_AUDIO_ISFLOAT(x)         (x & SDL_AUDIO_MASK_DATATYPE)
	#define SDL_AUDIO_ISBIGENDIAN(x)     (x & SDL_AUDIO_MASK_ENDIAN)
	#define SDL_AUDIO_ISSIGNED(x)        (x & SDL_AUDIO_MASK_SIGNED)
	#define SDL_AUDIO_ISINT(x)           (!SDL_AUDIO_ISFLOAT(x))
	#define SDL_AUDIO_ISLITTLEENDIAN(x)  (!SDL_AUDIO_ISBIGENDIAN(x))
	#define SDL_AUDIO_ISUNSIGNED(x)      (!SDL_AUDIO_ISSIGNED(x))

	#define SDL_INIT_TIMER          0x00000001
	#define SDL_INIT_AUDIO          0x00000010

	/* Used by audio targets during DetectDevices() */
	typedef void(*AddAudioDevice)(const char *name);
	
	typedef Uint16 AudioFormat;

	struct AudioCVT;
	typedef void (* AudioFilter) (struct AudioCVT * cvt,AudioFormat format);
	struct AudioCVT
	{
		int needed;                 /**< Set to 1 if conversion possible */
		AudioFormat src_format; /**< Source audio format */
		AudioFormat dst_format; /**< Target audio format */
		double rate_incr;           /**< Rate conversion increment */
		Uint8 *buf;                 /**< Buffer to hold entire audio data */
		int len;                    /**< Length of original audio buffer */
		int len_cvt;                /**< Length of converted audio buffer */
		int len_mult;               /**< buffer must be len*len_mult big */
		double len_ratio;           /**< Given len, final size is len*len_ratio */
		AudioFilter filters[10];        /**< Filter list */
		int filter_index;           /**< Current audio conversion function */
	};
	/* Streamer */
	struct AudioStreamer
	{
		Uint8 *buffer;
		int max_len;                /* the maximum length in bytes */
		int read_pos, write_pos;    /* the position of the write and read heads in bytes */
	};
	/**
	*  This function is called when the audio device needs more data.
	*
	*  \param userdata An application-specific parameter saved in
	*                  the SDL_AudioSpec structure
	*  \param stream A pointer to the audio data buffer.
	*  \param len    The length of that buffer in bytes.
	*
	*  Once the callback returns, the buffer will no longer be valid.
	*  Stereo samples are stored in a LRLRLR ordering.
	*/
	typedef void (* AudioCallback) (void *userdata, Uint8 * stream,int len);
	/**
	*  The calculated values in this structure are calculated by SDL_OpenAudio().
	*/
	struct AudioSpec
	{
		int freq;                   /**< DSP frequency -- samples per second */
		AudioFormat format;     /**< Audio data format */
		Uint8 channels;             /**< Number of channels: 1 mono, 2 stereo */
		Uint8 silence;              /**< Audio buffer silence value (calculated) */
		Uint16 samples;             /**< Audio buffer size in samples (power of 2) */
		Uint16 padding;             /**< Necessary for some compile environments */
		Uint32 size;                /**< Audio buffer size in bytes (calculated) */
		AudioCallback callback;
		void *userdata;
	};

    struct PrivateAudioData;
	/* Define the SDL audio driver structure */
	struct AudioDevice
	{
		/* * * */
		/* Data common to all devices */

		/* The current audio specification (shared with audio thread) */
		AudioSpec spec;

		/* An audio conversion block for audio format emulation */
		AudioCVT convert;

		/* The streamer, if sample rate conversion necessitates it */
		int use_streamer;
		AudioStreamer streamer;

		/* Current state flags */
		int iscapture;
		int enabled;
		int paused;
		int opened;

		/* Fake audio buffer for when the audio hardware is busy */
		Uint8 *fake_stream;

		/* A semaphore for locking the mixing buffers */
		mutex_t *mixer_lock;

		/* A thread to feed the audio device */
		thread_t *thread;
		threadid_t threadid;

		/* * * */
		/* Data private to this driver */
		PrivateAudioData *hidden;
	};

	struct AudioDriverImpl
	{
		void(*DetectDevices) (int iscapture, AddAudioDevice addfn);
		int(*OpenDevice) (AudioDevice *_this, const char *devname, int iscapture);
		void(*ThreadInit) (AudioDevice *_this); /* Called by audio thread at start */
		void(*WaitDevice) (AudioDevice *_this);
		void(*PlayDevice) (AudioDevice *_this);
		Uint8 *(*GetDeviceBuf) (AudioDevice *_this);
		void(*WaitDone) (AudioDevice *_this);
		void(*CloseDevice) (AudioDevice *_this);
		void(*LockDevice) (AudioDevice *_this);
		void(*UnlockDevice) (AudioDevice *_this);
		void(*Deinitialize) (void);

		/* !!! FIXME: add pause(), so we can optimize instead of mixing silence. */

		/* Some flags to push duplicate code into the core and reduce #ifdefs. */
		int ProvidesOwnCallbackThread;
		int SkipMixerLock;  /* !!! FIXME: do we need this anymore? */
		int HasCaptureSupport;
		int OnlyHasDefaultOutputDevice;
		int OnlyHasDefaultInputDevice;
	};

	struct AudioDriver
	{
		/* * * */
		/* The name of this audio driver */
		const char *name;

		/* * * */
		/* The description of this audio driver */
		const char *desc;

		AudioDriverImpl impl;

		char **outputDevices;
		int outputDeviceCount;

		char **inputDevices;
		int inputDeviceCount;
	};

	struct AudioBootStrap
	{
		const char *name;
		const char *desc;
		int(*init) (AudioDriverImpl * impl);
		int demand_only;  /* 1==request explicitly, or it won't be available. */
	};

	struct SDL_AudioRateFilters
	{
		AudioFormat fmt;
		int channels;
		int upsample;
		int multiple;
		AudioFilter filter;
	} ;
	struct SDL_AudioTypeFilters
	{
		AudioFormat src_fmt;
		AudioFormat dst_fmt;
		AudioFilter filter;
	} ;
	extern const SDL_AudioRateFilters sdl_audio_rate_filters[];
	extern const SDL_AudioTypeFilters sdl_audio_type_filters[];
	extern AudioBootStrap audio_driver;
	/*
		从SDL_video.h复制而来
		*/
	#define SWSURFACE	0x00000000	/**< Surface is in system memory */
	#define HWSURFACE	0x00000001	/**< Surface is in video memory */
	#define ASYNCBLIT	0x00000004	/**< Use asynchronous blits if possible */
	#define HWACCEL	0x00000100	/**< Blit uses hardware acceleration */
	#define FULLSCREEN	0x80000000	/**< Surface is a full screen display */
	#define RESIZABLE 	0x00000010	/**< This video mode may be resized */

	#define YV12_OVERLAY  0x32315659	/**< Planar mode: Y + V + U  (3 planes) */
	#define IYUV_OVERLAY  0x56555949	/**< Planar mode: Y + U + V  (3 planes) */
	#define YUY2_OVERLAY  0x32595559	/**< Packed mode: Y0+U0+Y1+V0 (1 plane) */
	#define UYVY_OVERLAY  0x59565955	/**< Packed mode: U0+Y0+V0+Y1 (1 plane) */
	#define YVYU_OVERLAY  0x55595659	/**< Packed mode: Y0+V0+Y1+U0 (1 plane) */

	#define HWACCEL	0x00000100	/**< Blit uses hardware acceleration */
	#define SRCCOLORKEY	0x00001000	/**< Blit uses a source color key */
	#define RLEACCELOK	0x00002000	/**< Private flag */
	#define RLEACCEL	0x00004000	/**< Surface is RLE encoded */
	#define SRCALPHA	0x00010000	/**< Blit uses source alpha blending */
	#define PREALLOC	0x01000000	/**< Surface uses preallocated memory */

	#define ALPHA_OPAQUE 255
	#define ALPHA_TRANSPARENT 0
	
	/** Evaluates to true if the surface needs to be locked before access */
	#define MUSTLOCK(surface)	\
		(surface->offset || \
		((surface->flags & (HWSURFACE | ASYNCBLIT | RLEACCEL)) != 0))

	/* RGB conversion lookup tables */
	struct Surface;
	struct Overlay;
	struct Color;

	struct Palette {
		int       ncolors;
		Color *colors;
	};

	struct private_yuvhwfuncs {
		int(*Lock)(Overlay *overlay);
		void(*Unlock)(Overlay *overlay);
		int(*Display)(Overlay *overlay, Rect *src, Rect *dst);
		void(*FreeHW)(Overlay *overlay);
	};

	struct private_yuvhwdata {
		Surface *stretch;
		Surface *display;
		Uint8 *pixels;
		int *colortab;
		Uint32 *rgb_2_pix;
		void(*Display1X)(int *colortab, Uint32 *rgb_2_pix,
			unsigned char *lum, unsigned char *cr,
			unsigned char *cb, unsigned char *out,
			int rows, int cols, int mod);
		void(*Display2X)(int *colortab, Uint32 *rgb_2_pix,
			unsigned char *lum, unsigned char *cr,
			unsigned char *cb, unsigned char *out,
			int rows, int cols, int mod);

		/* These are just so we don't have to allocate them separately */
		Uint16 pitches[3];
		Uint8 *planes[3];
	};

	struct Overlay {
		Uint32 format;				/**< Read-only */
		int w, h;				/**< Read-only */
		int planes;				/**< Read-only */
		Uint16 *pitches;			/**< Read-only */
		Uint8 **pixels;				/**< Read-write */

		/** @name Hardware-specific surface info */
		/*@{*/
		struct private_yuvhwfuncs *hwfuncs;
		struct private_yuvhwdata *hwdata;
		/*@{*/

		/** @name Special flags */
		/*@{*/
		Uint32 hw_overlay : 1;	/**< Flag: This overlay hardware accelerated? */
		Uint32 UnusedBits : 31;
		/*@}*/
	};

	struct Color {
		Uint8 r;
		Uint8 g;
		Uint8 b;
		Uint8 unused;
	};

	/** Everything in the pixel format structure is read-only */
	struct PixelFormat {
		Palette *palette;
		Uint8  BitsPerPixel;
		Uint8  BytesPerPixel;
		Uint8  Rloss;
		Uint8  Gloss;
		Uint8  Bloss;
		Uint8  Aloss;
		Uint8  Rshift;
		Uint8  Gshift;
		Uint8  Bshift;
		Uint8  Ashift;
		Uint32 Rmask;
		Uint32 Gmask;
		Uint32 Bmask;
		Uint32 Amask;

		/** RGB color key information */
		Uint32 colorkey;
		/** Alpha value information (per-surface alpha) */
		Uint8  alpha;
	};

	struct BlitInfo{
		Uint8 *s_pixels;
		int s_width;
		int s_height;
		int s_skip;
		Uint8 *d_pixels;
		int d_width;
		int d_height;
		int d_skip;
		void *aux_data;
		PixelFormat *src;
		Uint8 *table;
		PixelFormat *dst;
	};
	typedef void(*loblit)(BlitInfo *info);
	/* This is the private info structure for software accelerated blits */
	struct private_swaccel {
		loblit blit;
		void *aux_data;
	};

	/** typedef for private surface blitting functions */
	typedef int(*SDL_blit)(Surface *src, Rect *srcrect,Surface *dst, Rect *dstrect);

	/* Blit mapping definition */
	struct BlitMap {
		Surface *dst;
		int identity;
		Uint8 *table;
		SDL_blit hw_blit;
		SDL_blit sw_blit;
		struct private_hwaccel *hw_data;
		struct private_swaccel *sw_data;

		/* the version count matches the destination; mismatch indicates
		an invalid mapping */
		unsigned int format_version;
	};

	struct Surface {
		Uint32 flags;				/**< Read-only */
		PixelFormat *format;		/**< Read-only */
		int w, h;				/**< Read-only */
		Uint16 pitch;				/**< Read-only */
		void *pixels;				/**< Read-write */
		int offset;				/**< Private */

		/** Hardware-specific surface info */
		struct private_hwdata *hwdata;

		/** clipping information */
		Rect clip_rect;			/**< Read-only */
		Uint32 unused1;				/**< for binary compatibility */

		/** Allow recursive locks */
		Uint32 locked;				/**< Private */

		/** info for fast blit mapping to other surfaces */
		struct BlitMap *map;		/**< Private */

		/** format version, bumped at every change to invalidate blit maps */
		unsigned int format_version;		/**< Private */

		/** Reference count -- used when freeing surface */
		int refcount;				/**< Read-mostly */
	};

	int SDLog(const char *log);

	/*
		SDL Video替代函数
		*/
	int LockYUVOverlay(Overlay *overlay);
	void UnlockYUVOverlay(Overlay *overlay);
	int DisplayYUVOverlay(Overlay *overlay, Rect *dstrect);
	void FreeYUVOverlay(Overlay *overlay);
	Overlay * CreateYUVOverlay(int width, int height,
		Uint32 format, Surface *display);
	Overlay * CreateYUV_SW(int width, int height,
		Uint32 format, Surface *display);

	Surface * CreateRGBSurface(Uint32 flags,
		int width, int height, int depth,
		Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
	void FreeSurface(Surface *);
	void UnlockSurface(Surface *surface);
	int LockSurface(Surface *surface);

	/*
		SDL 辅助函数
		*/
	void WM_SetCaption(const char *title, const char *icon);
	void Quit(void);
	const char *GetError(void);
	char *my_getenv(const char *name);

	/*
		SDL Audio替代函数
		*/
#define AUDIO_U8	0x0008	/**< Unsigned 8-bit samples */
#define AUDIO_S8	0x8008	/**< Signed 8-bit samples */
#define AUDIO_U16LSB	0x0010	/**< Unsigned 16-bit samples */
#define AUDIO_S16LSB	0x8010	/**< Signed 16-bit samples */
#define AUDIO_U16MSB	0x1010	/**< As above, but big-endian byte order */
#define AUDIO_S16MSB	0x9010	/**< As above, but big-endian byte order */
#define AUDIO_U16	AUDIO_U16LSB
#define AUDIO_S16	AUDIO_S16LSB


#if SDL_BYTEORDER == SDL_LIL_ENDIAN
#define AUDIO_U16SYS	AUDIO_U16LSB
#define AUDIO_S16SYS	AUDIO_S16LSB
#else
#define AUDIO_U16SYS	AUDIO_U16MSB
#define AUDIO_S16SYS	AUDIO_S16MSB
#endif

#define AUDIO_S32LSB    0x8020  /**< 32-bit integer samples */
#define AUDIO_S32MSB    0x9020  /**< As above, but big-endian byte order */
#define AUDIO_S32       AUDIO_S32LSB

#define AUDIO_F32LSB    0x8120  /**< 32-bit floating point samples */
#define AUDIO_F32MSB    0x9120  /**< As above, but big-endian byte order */
#define AUDIO_F32       AUDIO_F32LSB

	int OpenAudio(AudioSpec *desired, AudioSpec *obtained);
	void CloseAudio(void);
	void PauseAudio(int pause_on);
	int BuildAudioCVT(AudioCVT * cvt,
		AudioFormat src_fmt, Uint8 src_channels, int src_rate,
		AudioFormat dst_fmt, Uint8 dst_channels, int dst_rate);

	/*
		SDL Event
		*/
#define EVENTMASK(X)	(1<<(X))
#define USEREVENT 24
#define FF_ALLOC_EVENT   (USEREVENT)
#define FF_QUIT_EVENT    (USEREVENT + 2)
#define ALLEVENTS		0xFFFFFFFF
	typedef enum {
		ADDEVENT,
		PEEKEVENT,
		GETEVENT
	} eventaction;

	/** A user-defined event type */
	struct UserEvent {
		Uint8 type;	/**< SDL_USEREVENT through SDL_NUMEVENTS-1 */
		int code;	/**< User defined event code */
		void *data1;	/**< User defined data pointer */
		void *data2;	/**< User defined data pointer */
	};

	union Event {
		Uint8 type;
		UserEvent user;
	};

	int PushEvent(Event *event);
	int PeepEvents(Event *events, int numevents,
		eventaction action, Uint32 mask);
	void PumpEvents(void);
	int OutOfMemory();
	
	#define SDL_InvalidParamError(param)    CCLog("Parameter '%s' is invalid", (param))

	int CCLog(const char* fmt,...);

	Uint32 SDL_WasInit(Uint32 flags);
	int SDL_InitSubSystem(Uint32 flags);
	int SDL_AudioInit(const char *driver_name);
	void SDL_AudioQuit(void);
	int SDL_ConvertAudio(AudioCVT * cvt);
	int SDL_strncasecmp(const char *str1, const char *str2, size_t maxlen);
	AudioFormat FirstAudioFormat(AudioFormat format);
	AudioFormat NextAudioFormat(void);
	void CalculateAudioSpec(AudioSpec * spec);
}
#endif