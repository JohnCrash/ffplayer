#include "SDLImp.h"
#include "SDLAudio/SDL_audio.h"
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
		����ʵ��SDL Audio�Ĳ��ֺ���
		*/
	static AudioDriver current_audio;

	int OpenAudio(AudioSpec *desired, AudioSpec *obtained)
	{
		audio_driver.init(&audio);
		return 0;
	}

	/*
		SDL_CloseAudio
		*/
	void CloseAudio(void)
	{
	}

	void PauseAudio(int pause_on)
	{
	}
}