#include "SDLImp.h"
#if __cplusplus
extern "C" {
#endif
#include <SDL.h>
#if __cplusplus
}
#endif

namespace ff{
	/*
		����ʵ��SDL Audio�Ĳ��ֺ���
		*/

	int OpenAudio(AudioSpec *desired, AudioSpec *obtained)
	{
		return SDL_OpenAudio((SDL_AudioSpec*)desired, (SDL_AudioSpec*)obtained);
	}

	/*
		SDL_CloseAudio
		*/
	void CloseAudio(void)
	{
		SDL_CloseAudio();
	}

	void PauseAudio(int pause_on)
	{
		SDL_PauseAudio(pause_on);
	}
}