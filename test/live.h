#ifndef _PUBLISHER_H_
#define _PUBLISHER_H_

namespace ff
{
	struct liveState
	{

	};

	typedef int(*liveCB)(liveState * pls);

	/*
	 * 选择视频和音频俘获设备进行在线直播
	 */
	void liveOnRtmp(
		const char * rtmp_publisher,
		const char * camera_name, int w, int h, int fps, int vbitRate,
		const char * phone_name, int ch, int bit, int rate, int abitRate,
		liveCB cb);

#define MAX_DEVICE_NAME_LENGTH 256
#define MAX_FORMAT_LENGTH 32
#define MAX_CAPABILITY_COUNT 128

	enum AVDeviceType{
		AV_DEVICE_NONE = 0,
		AV_DEVICE_VIDEO = 1,
		AV_DEVICE_AUDIO = 2,
	};

	union AVDeviceCap{
		struct {
			int min_w, min_h;
			int max_w, max_h;
			double min_fps, max_fps;
			char pix_format[MAX_FORMAT_LENGTH];
			char codec_name[MAX_FORMAT_LENGTH];
		} video;
		struct {
			int min_ch, min_bit, min_rate;
			int max_ch, max_bit, max_rate;
			char sample_format[MAX_FORMAT_LENGTH];
			char codec_name[MAX_FORMAT_LENGTH];
		} audio;
	};

	struct AVDevice{
		char name[MAX_DEVICE_NAME_LENGTH];
		char alternative_name[MAX_DEVICE_NAME_LENGTH];
		AVDeviceType type;
		AVDeviceCap capability[MAX_CAPABILITY_COUNT];
		int capability_count;
	};

	/**
	* 列出俘获设备
	* 成功返回设备数量，失败返回-1
	*/
	int ffCapDevicesList(AVDevice *pdevices, int nmax);
}
#endif