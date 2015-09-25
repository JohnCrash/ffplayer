#include "ffenc.h"

int read_media_file(const char *filename)
{
	AVFormatContext *ic;
	AVInputFormat *file_iformat = NULL;
	AVDictionary *opts;

	ic = avformat_alloc_context();

	avformat_open_input(&ic, filename, file_iformat, &opts);
	return 0;
}