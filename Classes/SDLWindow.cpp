#include "SDLImp.h"
#include <stdlib.h>

namespace ff{
	char *my_getenv(const char *name)
	{
		return getenv(name);
	}

	void WM_SetCaption(const char *title, const char *icon)
	{
	}

	void Quit(void)
	{
	}

	// µœ÷SDL_GetError()
	const char *GetError(void)
	{
		return "";
	}

	int OutOfMemory()
	{
		return 0;
	}
}
