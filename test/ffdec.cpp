#include "ffdec.h"

/*
* 创建一个解码器上下文，对视频文件进行解码操作
*/
AVDecodeContex *ffCreateDecodeContext(
	const char * filename, AVDictionary *opt_arg
	)
{
	int i,ret;
	AVInputFormat *file_iformat = NULL;
	AVDecodeContex * pdc;
	AVDictionary * opt;

	pdc = (AVDecodeContex *)malloc(sizeof(AVDecodeContex));
	while (pdc)
	{
		pdc->_fileName = strdup(filename);
		pdc->_ctx = avformat_alloc_context();
		if (!pdc->_ctx)
		{
			ffLog("ffCreateDecodeContext : could not allocate context.\n");
			break;
		}
		
		av_dict_copy(&opt, opt_arg, 0);
		ret = avformat_open_input(&pdc->_ctx, filename, file_iformat, &opt);
		av_dict_free(&opt);
		if (ret < 0)
		{
			char errmsg[ERROR_BUFFER_SIZE];
			av_strerror(ret, errmsg, ERROR_BUFFER_SIZE);
			ffLog("ffCreateDecodeContext %s.\n",errmsg);
			break;
		}
		av_format_inject_global_side_data(pdc->_ctx);
		av_dict_copy(&opt, opt_arg, 0);
		ret = avformat_find_stream_info(pdc->_ctx, &opt);
		av_dict_free(&opt);
		if (ret < 0)
		{
			char errmsg[ERROR_BUFFER_SIZE];
			av_strerror(ret, errmsg, ERROR_BUFFER_SIZE);
			ffLog("ffCreateDecodeContext %s.\n", errmsg);
			break;
		}
		return pdc;
	}

	/*
	 * 失败清理
	 */
	ffCloseDecodeContext(pdc);
	return NULL;
}

void ffCloseDecodeContext(AVDecodeContex *pdc)
{

}

/*
* 取得视频的帧率，宽度，高度
*/
int ffGetFrameRate(AVDecodeContex *pdc)
{

}

int ffGetFrameWidth(AVDecodeContex *pdc)
{

}

int ffGetFrameHeight(AVDecodeContex *pdc)
{

}

/*
* 从视频文件中取得一帧，可以是图像帧，也可以是一个音频包
*/
AVRaw * ffReadFrame(AVDecodeContex *pdc)
{

}