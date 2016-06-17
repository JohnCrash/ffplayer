#include "ff.h"
#include "ffmpeg_cxx.h"
#include <thread>
#include "cocos2d.h"
namespace ff
{
	static std::thread * _thread = NULL;
	static char * _cmd = NULL;
	static int (*_tpc)(TranCode tc, float p) = NULL;

	static int stpc(int tc, float p)
	{
		if (_tpc)
			return _tpc((TranCode)tc, p);
		return 0;
	}

	static void ffmpeg_exit(int r)
	{
		if (r != 0)
			stpc(TC_ERROR, 0);
		stpc(TC_END, 1.0f);
		_thread = NULL;
		free(_cmd);
		_cmd = NULL;
		_tpc = NULL;
	}

	void ffmpeg_cmd_proc()
	{
		char * argv[64] = { 0 };
		int argn = 0;
		int len = strlen(_cmd);

		for (int i = 0; i < len; i++){
			if (argn == 64)break;
			if (_cmd[i] == ' '){
				_cmd[i] = '\0';
				if(argv[argn])
					argn++;
			}
			else if (!argv[argn]){
				argv[argn] = _cmd + i;
			}
		}
		if (argv[argn])argn++;
		register_exit_callback(ffmpeg_exit);
		set_transcode_callback(stpc);
		stpc(TC_BEGIN, 0.0f);
		ffmpeg_main(argn, argv);
	}

	//ffmpeg_main();
	int ffmpeg(const char *cmd, int (* tpc)(TranCode tc, float p))
	{
		if (!cmd)return -1;
		if (_thread)return -2;
		_tpc = tpc;
		_cmd = strdup(cmd);
		_thread = new std::thread(ffmpeg_cmd_proc);
		return _thread ? 0 : -3;
	}
}
