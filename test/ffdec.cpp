#include "ffdec.h"

/*
* ����һ�������������ģ�����Ƶ�ļ����н������
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
	 * ʧ������
	 */
	ffCloseDecodeContext(pdc);
	return NULL;
}

void ffCloseDecodeContext(AVDecodeContex *pdc)
{

}

/*
* ȡ����Ƶ��֡�ʣ���ȣ��߶�
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
* ����Ƶ�ļ���ȡ��һ֡��������ͼ��֡��Ҳ������һ����Ƶ��
*/
AVRaw * ffReadFrame(AVDecodeContex *pdc)
{

}