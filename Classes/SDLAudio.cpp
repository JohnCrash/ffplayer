#include "SDLImp.h"
#include <stdlib.h>

/*
#ifdef WIN32
	#if __cplusplus
	extern "C" {
	#endif
	#include <SDL.h>
	#if __cplusplus
	}
	#endif
*/

namespace ff{
	/*
		重新实现SDL Audio的部分函数
	*/

	static AudioDriver current_audio;
	static AudioDevice *open_devices[16];

	void SDL_CalculateAudioSpec(AudioSpec * spec)
	{
			switch (spec->format) {
			case AUDIO_U8:
				spec->silence = 0x80;
				break;
			default:
				spec->silence = 0x00;
				break;
			}
			spec->size = SDL_AUDIO_BITSIZE(spec->format) / 8;
			spec->size *= spec->channels;
			spec->size *= spec->samples;
	}

	static int prepare_audiospec(const AudioSpec * orig, AudioSpec * prepared)
	{
			memcpy(prepared, orig, sizeof(AudioSpec));

			if (orig->callback == NULL) {
				SDLog("SDL_OpenAudio() passed a NULL callback");
				return 0;
			}

			if (orig->freq == 0) {
				prepared->freq = 22050;     /* a reasonable default */
			}

			if (orig->format == 0) {
				prepared->format = AUDIO_S16;       /* a reasonable default */
			}

			switch (orig->channels) {
			case 0:{
						   prepared->channels = 2; /* a reasonable default */
					   break;
			}
			case 1:                    /* Mono */
			case 2:                    /* Stereo */
			case 4:                    /* surround */
			case 6:                    /* surround with center and lfe */
				break;
			default:
				SDLog("Unsupported number of audio channels.");
				return 0;
			}

			if (orig->samples == 0) {
				/* Pick a default of ~46 ms at desired frequency */
				/* !!! FIXME: remove this when the non-Po2 resampling is in. */
				const int samples = (prepared->freq / 1000) * 46;
				int power2 = 1;
				while (power2 < samples) {
					power2 *= 2;
				}
				prepared->samples = power2;
			}

			/* Calculate the silence and size of the audio specification */
			SDL_CalculateAudioSpec(prepared);

			return 1;
		}

	static void
		close_audio_device(AudioDevice * device)
	{
			device->enabled = 0;
			if (device->thread != NULL) {
				waitThread(device->thread, NULL);
			}
			if (device->mixer_lock != NULL) {
				destroyMutex(device->mixer_lock);
			}
			SDL_FreeAudioMem(device->fake_stream);
			if (device->convert.needed) {
				SDL_FreeAudioMem(device->convert.buf);
			}
			if (device->opened) {
				current_audio.impl.CloseDevice(device);
				device->opened = 0;
			}
			SDL_FreeAudioMem(device);
		}

	static
		void SDL_AddCaptureAudioDevice(const char *_name)
	{
			char *name = NULL;
			void *ptr = realloc(current_audio.inputDevices,
				(current_audio.inputDeviceCount + 1) * sizeof(char*));
			if (ptr == NULL) {
				return;  /* oh well. */
			}

			current_audio.inputDevices = (char **)ptr;
			name = strdup(_name);  /* if this returns NULL, that's okay. */
			current_audio.inputDevices[current_audio.inputDeviceCount++] = name;
		}

	static
		void SDL_AddOutputAudioDevice(const char *_name)
	{
			char *name = NULL;
			void *ptr = realloc(current_audio.outputDevices,
				(current_audio.outputDeviceCount + 1) * sizeof(char*));
			if (ptr == NULL) {
				return;  /* oh well. */
			}

			current_audio.outputDevices = (char **)ptr;
			name = strdup(_name);  /* if this returns NULL, that's okay. */
			current_audio.outputDevices[current_audio.outputDeviceCount++] = name;
		}

	static void
		free_device_list(char ***devices, int *devCount)
	{
			int i = *devCount;
			if ((i > 0) && (*devices != NULL)) {
				while (i--) {
					free((*devices)[i]);
				}
			}

			free(*devices);

			*devices = NULL;
			*devCount = 0;
		}

	int
		SDL_GetNumAudioDevices(int iscapture)
	{
			int retval = 0;

			if (!SDL_WasInit(SDL_INIT_AUDIO)) {
				return -1;
			}

			if ((iscapture) && (!current_audio.impl.HasCaptureSupport)) {
				return 0;
			}

			if ((iscapture) && (current_audio.impl.OnlyHasDefaultInputDevice)) {
				return 1;
			}

			if ((!iscapture) && (current_audio.impl.OnlyHasDefaultOutputDevice)) {
				return 1;
			}

			if (iscapture) {
				free_device_list(&current_audio.inputDevices,
					&current_audio.inputDeviceCount);
				current_audio.impl.DetectDevices(iscapture, SDL_AddCaptureAudioDevice);
				retval = current_audio.inputDeviceCount;
			}
			else {
				free_device_list(&current_audio.outputDevices,
					&current_audio.outputDeviceCount);
				current_audio.impl.DetectDevices(iscapture, SDL_AddOutputAudioDevice);
				retval = current_audio.outputDeviceCount;
			}

			return retval;
		}

	static AudioDevice *
		get_audio_device(AudioDeviceID id)
	{
			id--;
			if ((id >= SDL_arraysize(open_devices)) || (open_devices[id] == NULL)) {
				SDLog("Invalid audio device ID");
				return NULL;
			}

			return open_devices[id];
		}

	void
		SDL_CloseAudioDevice(AudioDeviceID devid)
	{
			AudioDevice *device = get_audio_device(devid);
			if (device) {
				close_audio_device(device);
				open_devices[devid - 1] = NULL;
			}
		}

	/* Deinitialize the stream simply by freeing the buffer */
	static void
		SDL_StreamDeinit(AudioStreamer * stream)
	{
			free(stream->buffer);
		}

	static int
		SDL_StreamLength(AudioStreamer * stream)
	{
			return (stream->write_pos - stream->read_pos) % stream->max_len;
		}

	/* Streaming functions (for when the input and output buffer sizes are different) */
	/* Write [length] bytes from buf into the streamer */
	static void
		SDL_StreamWrite(AudioStreamer * stream, Uint8 * buf, int length)
	{
			int i;

			for (i = 0; i < length; ++i) {
				stream->buffer[stream->write_pos] = buf[i];
				++stream->write_pos;
			}
		}

	/* Read [length] bytes out of the streamer into buf */
	static void
		SDL_StreamRead(AudioStreamer * stream, Uint8 * buf, int length)
	{
			int i;

			for (i = 0; i < length; ++i) {
				buf[i] = stream->buffer[stream->read_pos];
				++stream->read_pos;
			}
		}

	/* The general mixing thread function */
	int  SDL_RunAudio(void *devicep)
	{
		AudioDevice *device = (AudioDevice *)devicep;
		Uint8 *stream;
		int stream_len;
		void *udata;
		void ( * fill) (void *userdata, Uint8 * stream, int len);
		Uint32 delay;
		/* For streaming when the buffer sizes don't match up */
		Uint8 *istream;
		int istream_len = 0;

		/* The audio mixing is always a high priority thread */
		//SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);
		//std::thread库不支持线程优先级

		/* Perform any thread setup */
		device->threadid = currentThreadId();
		current_audio.impl.ThreadInit(device);

		/* Set up the mixing function */
		fill = device->spec.callback;
		udata = device->spec.userdata;

		/* By default do not stream */
		device->use_streamer = 0;

		if (device->convert.needed) {
#if 0                           /* !!! FIXME: I took len_div out of the structure. Use rate_incr instead? */
			/* If the result of the conversion alters the length, i.e. resampling is being used, use the streamer */
			if (device->convert.len_mult != 1 || device->convert.len_div != 1) {
				/* The streamer's maximum length should be twice whichever is larger: spec.size or len_cvt */
				stream_max_len = 2 * device->spec.size;
				if (device->convert.len_mult > device->convert.len_div) {
					stream_max_len *= device->convert.len_mult;
					stream_max_len /= device->convert.len_div;
				}
				if (SDL_StreamInit(&device->streamer, stream_max_len, silence) <
					0)
					return -1;
				device->use_streamer = 1;

				/* istream_len should be the length of what we grab from the callback and feed to conversion,
				so that we get close to spec_size. I.e. we want device.spec_size = istream_len * u / d
				*/
				istream_len =
					device->spec.size * device->convert.len_div /
					device->convert.len_mult;
			}
#endif
			stream_len = device->convert.len;
		}
		else {
			stream_len = device->spec.size;
		}

		/* Calculate the delay while paused */
		delay = ((device->spec.samples * 1000) / device->spec.freq);

		/* Determine if the streamer is necessary here */
		if (device->use_streamer == 1) {
			/* This code is almost the same as the old code. The difference is, instead of reading
			directly from the callback into "stream", then converting and sending the audio off,
			we go: callback -> "istream" -> (conversion) -> streamer -> stream -> device.
			However, reading and writing with streamer are done separately:
			- We only call the callback and write to the streamer when the streamer does not
			contain enough samples to output to the device.
			- We only read from the streamer and tell the device to play when the streamer
			does have enough samples to output.
			This allows us to perform resampling in the conversion step, where the output of the
			resampling process can be any number. We will have to see what a good size for the
			stream's maximum length is, but I suspect 2*max(len_cvt, stream_len) is a good figure.
			*/
			while (device->enabled) {

				if (device->paused) {
					Delay(delay);
					continue;
				}

				/* Only read in audio if the streamer doesn't have enough already (if it does not have enough samples to output) */
				if (SDL_StreamLength(&device->streamer) < stream_len) {
					/* Set up istream */
					if (device->convert.needed) {
						if (device->convert.buf) {
							istream = device->convert.buf;
						}
						else {
							continue;
						}
					}
					else {
						/* FIXME: Ryan, this is probably wrong.  I imagine we don't want to get
						* a device buffer both here and below in the stream output.
						*/
						istream = current_audio.impl.GetDeviceBuf(device);
						if (istream == NULL) {
							istream = device->fake_stream;
						}
					}

					/* Read from the callback into the _input_ stream */
					lockMutex(device->mixer_lock);
					(*fill) (udata, istream, istream_len);
					unlockMutex(device->mixer_lock);

					/* Convert the audio if necessary and write to the streamer */
					if (device->convert.needed) {
						SDL_ConvertAudio(&device->convert);
						if (istream == NULL) {
							istream = device->fake_stream;
						}
						/* SDL_memcpy(istream, device->convert.buf, device->convert.len_cvt); */
						SDL_StreamWrite(&device->streamer, device->convert.buf,
							device->convert.len_cvt);
					}
					else {
						SDL_StreamWrite(&device->streamer, istream, istream_len);
					}
				}

				/* Only output audio if the streamer has enough to output */
				if (SDL_StreamLength(&device->streamer) >= stream_len) {
					/* Set up the output stream */
					if (device->convert.needed) {
						if (device->convert.buf) {
							stream = device->convert.buf;
						}
						else {
							continue;
						}
					}
					else {
						stream = current_audio.impl.GetDeviceBuf(device);
						if (stream == NULL) {
							stream = device->fake_stream;
						}
					}

					/* Now read from the streamer */
					SDL_StreamRead(&device->streamer, stream, stream_len);

					/* Ready current buffer for play and change current buffer */
					if (stream != device->fake_stream) {
						current_audio.impl.PlayDevice(device);
						/* Wait for an audio buffer to become available */
						current_audio.impl.WaitDevice(device);
					}
					else {
						Delay(delay);
					}
				}

			}
		}
		else {
			/* Otherwise, do not use the streamer. This is the old code. */
			const int silence = (int)device->spec.silence;

			/* Loop, filling the audio buffers */
			while (device->enabled) {

				/* Fill the current buffer with sound */
				if (device->convert.needed) {
					if (device->convert.buf) {
						stream = device->convert.buf;
					}
					else {
						continue;
					}
				}
				else {
					stream = current_audio.impl.GetDeviceBuf(device);
					if (stream == NULL) {
						stream = device->fake_stream;
						return -1;
					}
				}

				lockMutex(device->mixer_lock);
				if (device->paused) {
					memset(stream, silence, stream_len);
				}
				else {
					(*fill) (udata, stream, stream_len);
				}
				unlockMutex(device->mixer_lock);

				/* Convert the audio if necessary */
				if (device->convert.needed) {
					SDL_ConvertAudio(&device->convert);
					stream = current_audio.impl.GetDeviceBuf(device);
					if (stream == NULL) {
						stream = device->fake_stream;
					}
					memcpy(stream, device->convert.buf,
						device->convert.len_cvt);
				}

				/* Ready current buffer for play and change current buffer */
				if (stream != device->fake_stream) {
					current_audio.impl.PlayDevice(device);
					/* Wait for an audio buffer to become available */
					current_audio.impl.WaitDevice(device);
				}
				else {
					Delay(delay);
				}
			}
		}

		/* Wait for the audio to drain.. */
		current_audio.impl.WaitDone(device);

		/* If necessary, deinit the streamer */
		if (device->use_streamer == 1)
			SDL_StreamDeinit(&device->streamer);

		return (0);
	}

	static AudioDeviceID
		open_audio_device(const char *devname, int iscapture,
		const AudioSpec * desired, AudioSpec * obtained,
		int allowed_changes, int min_id)
	{
			AudioDeviceID id = 0;
			AudioSpec _obtained;
			AudioDevice *device;
			bool build_cvt;
			int i = 0;

			if (!SDL_WasInit(SDL_INIT_AUDIO)) {
				SDLog("Audio subsystem is not initialized");
				return 0;
			}

			if ((iscapture) && (!current_audio.impl.HasCaptureSupport)) {
				SDLog("No capture support");
				return 0;
			}

			if (!obtained) {
				obtained = &_obtained;
			}
			if (!prepare_audiospec(desired, obtained)) {
				return 0;
			}

			/* If app doesn't care about a specific device, let the user override. */
			if (devname == NULL) {
				devname = getenv("SDL_AUDIO_DEVICE_NAME");
			}

			/*
			* Catch device names at the high level for the simple case...
			* This lets us have a basic "device enumeration" for systems that
			*  don't have multiple devices, but makes sure the device name is
			*  always NULL when it hits the low level.
			*
			* Also make sure that the simple case prevents multiple simultaneous
			*  opens of the default system device.
			*/

			if ((iscapture) && (current_audio.impl.OnlyHasDefaultInputDevice)) {
				if ((devname) && (strcmp(devname, DEFAULT_INPUT_DEVNAME) != 0)) {
					SDLog("No such device");
					return 0;
				}
				devname = NULL;

				for (i = 0; i < SDL_arraysize(open_devices); i++) {
					if ((open_devices[i]) && (open_devices[i]->iscapture)) {
						SDLog("Audio device already open");
						return 0;
					}
				}
			}

			if ((!iscapture) && (current_audio.impl.OnlyHasDefaultOutputDevice)) {
				if ((devname) && (strcmp(devname, DEFAULT_OUTPUT_DEVNAME) != 0)) {
					SDLog("No such device");
					return 0;
				}
				devname = NULL;

				for (i = 0; i < SDL_arraysize(open_devices); i++) {
					if ((open_devices[i]) && (!open_devices[i]->iscapture)) {
						SDLog("Audio device already open");
						return 0;
					}
				}
			}

			device = (AudioDevice *)SDL_AllocAudioMem(sizeof(AudioDevice));
			if (device == NULL) {
				OutOfMemory();
				return 0;
			}
			memset(device, '\0', sizeof(AudioDevice));
			device->spec = *obtained;
			device->enabled = 1;
			device->paused = 1;
			device->iscapture = iscapture;

			/* Create a semaphore for locking the sound buffers */
			if (!current_audio.impl.SkipMixerLock) {
				device->mixer_lock = createMutex();
				if (device->mixer_lock == NULL) {
					close_audio_device(device);
					SDLog("Couldn't create mixer lock");
					return 0;
				}
			}

			/* force a device detection if we haven't done one yet. */
			if (((iscapture) && (current_audio.inputDevices == NULL)) ||
				((!iscapture) && (current_audio.outputDevices == NULL)))
				SDL_GetNumAudioDevices(iscapture);

			if (current_audio.impl.OpenDevice(device, devname, iscapture) < 0) {
				close_audio_device(device);
				return 0;
			}
			device->opened = 1;

			/* Allocate a fake audio memory buffer */
			device->fake_stream = (Uint8 *)SDL_AllocAudioMem(device->spec.size);
			if (device->fake_stream == NULL) {
				close_audio_device(device);
				OutOfMemory();
				return 0;
			}

			/* See if we need to do any conversion */
			build_cvt = false;
			if (obtained->freq != device->spec.freq) {
				if (allowed_changes & SDL_AUDIO_ALLOW_FREQUENCY_CHANGE) {
					obtained->freq = device->spec.freq;
				}
				else {
					build_cvt = true;
				}
			}
			if (obtained->format != device->spec.format) {
				if (allowed_changes & SDL_AUDIO_ALLOW_FORMAT_CHANGE) {
					obtained->format = device->spec.format;
				}
				else {
					build_cvt = true;
				}
			}
			if (obtained->channels != device->spec.channels) {
				if (allowed_changes & SDL_AUDIO_ALLOW_CHANNELS_CHANGE) {
					obtained->channels = device->spec.channels;
				}
				else {
					build_cvt = true;
				}
			}

			/* If the audio driver changes the buffer size, accept it.
			This needs to be done after the format is modified above,
			otherwise it might not have the correct buffer size.
			*/
			if (device->spec.samples != obtained->samples) {
				obtained->samples = device->spec.samples;
				SDL_CalculateAudioSpec(obtained);
			}

			if (build_cvt) {
				/* Build an audio conversion block */
				if (BuildAudioCVT(&device->convert,
					obtained->format, obtained->channels,
					obtained->freq,
					device->spec.format, device->spec.channels,
					device->spec.freq) < 0) {
					close_audio_device(device);
					return 0;
				}
				if (device->convert.needed) {
					device->convert.len = (int)(((double)device->spec.size) /
						device->convert.len_ratio);

					device->convert.buf =
						(Uint8 *)SDL_AllocAudioMem(device->convert.len *
						device->convert.len_mult);
					if (device->convert.buf == NULL) {
						close_audio_device(device);
						OutOfMemory();
						return 0;
					}
				}
			}

			/* Find an available device ID and store the structure... */
			for (id = min_id - 1; id < SDL_arraysize(open_devices); id++) {
				if (open_devices[id] == NULL) {
					open_devices[id] = device;
					break;
				}
			}

			if (id == SDL_arraysize(open_devices)) {
				SDLog("Too many open audio devices");
				close_audio_device(device);
				return 0;
			}

			/* Start the audio thread if necessary */
			if (!current_audio.impl.ProvidesOwnCallbackThread) {
				/* Start the audio thread */
				char name[64];
#ifdef WIN32
				_snprintf(name, sizeof (name), "SDLAudioDev%d", (int)(id + 1));
#else
				snprintf(name, sizeof (name), "SDLAudioDev%d", (int)(id + 1));
#endif
				/* !!! FIXME: this is nasty. */
#if defined(__WIN32__) && !defined(HAVE_LIBC)
#undef createThread
#if SDL_DYNAMIC_API
				device->thread = createThread_REAL(SDL_RunAudio, name, device, NULL, NULL);
#else
				device->thread = createThread(SDL_RunAudio, name, device, NULL, NULL);
#endif
#else
				device->thread = createThread(SDL_RunAudio, device); //简单的取消线程名称
#endif
				if (device->thread == NULL) {
					SDL_CloseAudioDevice(id + 1);
					SDLog("Couldn't create audio thread");
					return 0;
				}
			}

			return id + 1;
		}

	int OpenAudio(AudioSpec *desired, AudioSpec *obtained)
	{
		AudioDeviceID id = 0;

		/* Start up the audio driver, if necessary. This is legacy behaviour! */
		if (!SDL_WasInit(SDL_INIT_AUDIO)) {
			if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
				return (-1);
			}
		}

		/* SDL_OpenAudio() is legacy and can only act on Device ID #1. */
		if (open_devices[0] != NULL) {
			SDLog("Audio device is already opened");
			return (-1);
		}

		if (obtained) {
			id = open_audio_device(NULL, 0, desired, obtained,
				SDL_AUDIO_ALLOW_ANY_CHANGE, 1);
		}
		else {
			id = open_audio_device(NULL, 0, desired, desired, 0, 1);
		}

		//SDL_assert((id == 0) || (id == 1));
		return ((id == 0) ? -1 : 0);
	}

	/*
		SDL_CloseAudio
	*/
	void CloseAudio(void)
	{
		SDL_CloseAudioDevice(1);
	}

	void SDL_PauseAudioDevice(AudioDeviceID devid, int pause_on)
	{
			AudioDevice *device = get_audio_device(devid);
			if (device) {
				current_audio.impl.LockDevice(device);
				device->paused = pause_on;
				current_audio.impl.UnlockDevice(device);
			}
	}

	void PauseAudio(int pause_on)
	{
		SDL_PauseAudioDevice(1, pause_on);
	}

	void
		SDL_AudioQuit(void)
	{
			AudioDeviceID i;

			if (!current_audio.name) {  /* not initialized?! */
				return;
			}

			for (i = 0; i < SDL_arraysize(open_devices); i++) {
				if (open_devices[i] != NULL) {
					SDL_CloseAudioDevice(i + 1);
				}
			}

			/* Free the driver data */
			current_audio.impl.Deinitialize();
			free_device_list(&current_audio.outputDevices,
				&current_audio.outputDeviceCount);
			free_device_list(&current_audio.inputDevices,
				&current_audio.inputDeviceCount);
			memset(&current_audio, '\0', sizeof(current_audio));
			memset(open_devices, '\0', sizeof(open_devices));
		}

	/*
	* Not all of these will be compiled and linked in, but it's convenient
	*  to have a complete list here and saves yet-another block of #ifdefs...
	*  Please see bootstrap[], below, for the actual #ifdef mess.
	*/
	extern AudioBootStrap BSD_AUDIO_bootstrap;
	extern AudioBootStrap DSP_bootstrap;
	extern AudioBootStrap ALSA_bootstrap;
	extern AudioBootStrap PULSEAUDIO_bootstrap;
	extern AudioBootStrap QSAAUDIO_bootstrap;
	extern AudioBootStrap SUNAUDIO_bootstrap;
	extern AudioBootStrap ARTS_bootstrap;
	extern AudioBootStrap ESD_bootstrap;
	extern AudioBootStrap NAS_bootstrap;
	extern AudioBootStrap XAUDIO2_bootstrap;
	extern AudioBootStrap DSOUND_bootstrap;
	extern AudioBootStrap WINMM_bootstrap;
	extern AudioBootStrap PAUDIO_bootstrap;
	extern AudioBootStrap HAIKUAUDIO_bootstrap;
	extern AudioBootStrap COREAUDIO_bootstrap;
	extern AudioBootStrap SNDMGR_bootstrap;
	extern AudioBootStrap DISKAUD_bootstrap;
	extern AudioBootStrap DUMMYAUD_bootstrap;
	extern AudioBootStrap DCAUD_bootstrap;
	extern AudioBootStrap DART_bootstrap;
	extern AudioBootStrap NDSAUD_bootstrap;
	extern AudioBootStrap FUSIONSOUND_bootstrap;
	extern AudioBootStrap ANDROIDAUD_bootstrap;
	extern AudioBootStrap PSPAUD_bootstrap;
	extern AudioBootStrap SNDIO_bootstrap;

#ifdef WIN32
#define SDL_AUDIO_DRIVER_WINMM 1
#elif __ANDROID__
#define SDL_AUDIO_DRIVER_ANDROID 1
#else
	//iOS
#define SDL_AUDIO_DRIVER_COREAUDIO 1
#endif

	static const AudioBootStrap *const bootstrap[] = {
#if SDL_AUDIO_DRIVER_PULSEAUDIO
		&PULSEAUDIO_bootstrap,
#endif
#if SDL_AUDIO_DRIVER_ALSA
		&ALSA_bootstrap,
#endif
#if SDL_AUDIO_DRIVER_SNDIO
		&SNDIO_bootstrap,
#endif
#if SDL_AUDIO_DRIVER_BSD
		&BSD_AUDIO_bootstrap,
#endif
#if SDL_AUDIO_DRIVER_OSS
		&DSP_bootstrap,
#endif
#if SDL_AUDIO_DRIVER_QSA
		&QSAAUDIO_bootstrap,
#endif
#if SDL_AUDIO_DRIVER_SUNAUDIO
		&SUNAUDIO_bootstrap,
#endif
#if SDL_AUDIO_DRIVER_ARTS
		&ARTS_bootstrap,
#endif
#if SDL_AUDIO_DRIVER_ESD
		&ESD_bootstrap,
#endif
#if SDL_AUDIO_DRIVER_NAS
		&NAS_bootstrap,
#endif
#if SDL_AUDIO_DRIVER_XAUDIO2
		&XAUDIO2_bootstrap,
#endif
#if SDL_AUDIO_DRIVER_DSOUND
		&DSOUND_bootstrap,
#endif
#if SDL_AUDIO_DRIVER_WINMM
		&WINMM_bootstrap,
#endif
#if SDL_AUDIO_DRIVER_PAUDIO
		&PAUDIO_bootstrap,
#endif
#if SDL_AUDIO_DRIVER_HAIKU
		&HAIKUAUDIO_bootstrap,
#endif
#if SDL_AUDIO_DRIVER_COREAUDIO
		&COREAUDIO_bootstrap,
#endif
#if SDL_AUDIO_DRIVER_DISK
		&DISKAUD_bootstrap,
#endif
#if SDL_AUDIO_DRIVER_DUMMY
		&DUMMYAUD_bootstrap,
#endif
#if SDL_AUDIO_DRIVER_FUSIONSOUND
		&FUSIONSOUND_bootstrap,
#endif
#if SDL_AUDIO_DRIVER_ANDROID
		&ANDROIDAUD_bootstrap,
#endif
#if SDL_AUDIO_DRIVER_PSP
		&PSPAUD_bootstrap,
#endif
		NULL
	};

	/* stubs for audio drivers that don't need a specific entry point... */
	static void
		SDL_AudioDetectDevices_Default(int iscapture, AddAudioDevice addfn)
	{                               /* no-op. */
		}

	static void
		SDL_AudioThreadInit_Default(AudioDevice *_this)
	{                               /* no-op. */
		}

	static void
		SDL_AudioWaitDevice_Default(AudioDevice *_this)
	{                               /* no-op. */
		}

	static void
		SDL_AudioPlayDevice_Default(AudioDevice *_this)
	{                               /* no-op. */
		}

	static Uint8 *
		SDL_AudioGetDeviceBuf_Default(AudioDevice *_this)
	{
			return NULL;
		}

	static void
		SDL_AudioWaitDone_Default(AudioDevice *_this)
	{                               /* no-op. */
		}

	static void
		SDL_AudioCloseDevice_Default(AudioDevice *_this)
	{                               /* no-op. */
		}

	static void
		SDL_AudioDeinitialize_Default(void)
	{                               /* no-op. */
		}

	static int
		SDL_AudioOpenDevice_Default(AudioDevice *_this, const char *devname, int iscapture)
	{
			return -1;
	}

	static void
		SDL_AudioLockDevice_Default(AudioDevice * device)
	{
			if (device->thread && (currentThreadId() == device->threadid)) {
				return;
			}
			lockMutex(device->mixer_lock);
		}

	static void
		SDL_AudioUnlockDevice_Default(AudioDevice * device)
	{
			if (device->thread && (currentThreadId() == device->threadid)) {
				return;
			}
			unlockMutex(device->mixer_lock);
		}

	static void
		finalize_audio_entry_points(void)
	{
			/*
			* Fill in stub functions for unused driver entry points. This lets us
			*  blindly call them without having to check for validity first.
			*/

#define FILL_STUB(x) \
			if (current_audio.impl.x == NULL) {\
				current_audio.impl.x = SDL_Audio##x##_Default; \
			}
			FILL_STUB(DetectDevices);
			FILL_STUB(OpenDevice);
			FILL_STUB(ThreadInit);
			FILL_STUB(WaitDevice);
			FILL_STUB(PlayDevice);
			FILL_STUB(GetDeviceBuf);
			FILL_STUB(WaitDone);
			FILL_STUB(CloseDevice);
			FILL_STUB(LockDevice);
			FILL_STUB(UnlockDevice);
			FILL_STUB(Deinitialize);
#undef FILL_STUB
	}

	int SDL_AudioInit(const char *driver_name)
	{
		int i = 0;
		int initialized = 0;
		int tried_to_init = 0;

		if (SDL_WasInit(SDL_INIT_AUDIO)) {
			SDL_AudioQuit();        /* shutdown driver if already running. */
		}

		memset(&current_audio, '\0', sizeof(current_audio));
		memset(open_devices, '\0', sizeof(open_devices));

		/* Select the proper audio driver */
		if (driver_name == NULL) {
			driver_name = getenv("SDL_AUDIODRIVER");
		}

		for (i = 0; (!initialized) && (bootstrap[i]); ++i) {
			/* make sure we should even try this driver before doing so... */
			const AudioBootStrap *backend = bootstrap[i];
			if ((driver_name && (SDL_strncasecmp(backend->name, driver_name, strlen(driver_name)) != 0)) ||
				(!driver_name && backend->demand_only)) {
				continue;
			}

			tried_to_init = 1;
			memset(&current_audio, 0, sizeof(current_audio));
			current_audio.name = backend->name;
			current_audio.desc = backend->desc;
			initialized = backend->init(&current_audio.impl);
		}

		if (!initialized) {
			/* specific drivers will set the error message if they fail... */
			if (!tried_to_init) {
				if (driver_name) {
					CCLog("Audio target '%s' not available", driver_name);
				}
				else {
					SDLog("No available audio device");
				}
			}

			memset(&current_audio, 0, sizeof(current_audio));
			return (-1);            /* No driver was available, so fail. */
		}

		finalize_audio_entry_points();

		return (0);

	}

	#define NUM_FORMATS 10
	static int format_idx;
	static int format_idx_sub;
	static AudioFormat format_list[NUM_FORMATS][NUM_FORMATS] = {
		{ AUDIO_U8, AUDIO_S8, AUDIO_S16LSB, AUDIO_S16MSB, AUDIO_U16LSB,
		AUDIO_U16MSB, AUDIO_S32LSB, AUDIO_S32MSB, AUDIO_F32LSB, AUDIO_F32MSB },
		{ AUDIO_S8, AUDIO_U8, AUDIO_S16LSB, AUDIO_S16MSB, AUDIO_U16LSB,
		AUDIO_U16MSB, AUDIO_S32LSB, AUDIO_S32MSB, AUDIO_F32LSB, AUDIO_F32MSB },
		{ AUDIO_S16LSB, AUDIO_S16MSB, AUDIO_U16LSB, AUDIO_U16MSB, AUDIO_S32LSB,
		AUDIO_S32MSB, AUDIO_F32LSB, AUDIO_F32MSB, AUDIO_U8, AUDIO_S8 },
		{ AUDIO_S16MSB, AUDIO_S16LSB, AUDIO_U16MSB, AUDIO_U16LSB, AUDIO_S32MSB,
		AUDIO_S32LSB, AUDIO_F32MSB, AUDIO_F32LSB, AUDIO_U8, AUDIO_S8 },
		{ AUDIO_U16LSB, AUDIO_U16MSB, AUDIO_S16LSB, AUDIO_S16MSB, AUDIO_S32LSB,
		AUDIO_S32MSB, AUDIO_F32LSB, AUDIO_F32MSB, AUDIO_U8, AUDIO_S8 },
		{ AUDIO_U16MSB, AUDIO_U16LSB, AUDIO_S16MSB, AUDIO_S16LSB, AUDIO_S32MSB,
		AUDIO_S32LSB, AUDIO_F32MSB, AUDIO_F32LSB, AUDIO_U8, AUDIO_S8 },
		{ AUDIO_S32LSB, AUDIO_S32MSB, AUDIO_F32LSB, AUDIO_F32MSB, AUDIO_S16LSB,
		AUDIO_S16MSB, AUDIO_U16LSB, AUDIO_U16MSB, AUDIO_U8, AUDIO_S8 },
		{ AUDIO_S32MSB, AUDIO_S32LSB, AUDIO_F32MSB, AUDIO_F32LSB, AUDIO_S16MSB,
		AUDIO_S16LSB, AUDIO_U16MSB, AUDIO_U16LSB, AUDIO_U8, AUDIO_S8 },
		{ AUDIO_F32LSB, AUDIO_F32MSB, AUDIO_S32LSB, AUDIO_S32MSB, AUDIO_S16LSB,
		AUDIO_S16MSB, AUDIO_U16LSB, AUDIO_U16MSB, AUDIO_U8, AUDIO_S8 },
		{ AUDIO_F32MSB, AUDIO_F32LSB, AUDIO_S32MSB, AUDIO_S32LSB, AUDIO_S16MSB,
		AUDIO_S16LSB, AUDIO_U16MSB, AUDIO_U16LSB, AUDIO_U8, AUDIO_S8 },
	};

	void CalculateAudioSpec(AudioSpec * spec)
	{
		switch (spec->format) {
		case AUDIO_U8:
			spec->silence = 0x80;
			break;
		default:
			spec->silence = 0x00;
			break;
		}
		spec->size = AUDIO_BITSIZE(spec->format) / 8;
		spec->size *= spec->channels;
		spec->size *= spec->samples;
	}

	AudioFormat NextAudioFormat(void)
	{
		if ((format_idx == NUM_FORMATS) || (format_idx_sub == NUM_FORMATS)) {
			return (0);
		}
		return (format_list[format_idx][format_idx_sub++]);
	}

	AudioFormat FirstAudioFormat(AudioFormat format)
	{
		for (format_idx = 0; format_idx < NUM_FORMATS; ++format_idx) {
			if (format_list[format_idx][0] == format) {
				break;
			}
		}
		format_idx_sub = 0;
		return (NextAudioFormat());
	}
}