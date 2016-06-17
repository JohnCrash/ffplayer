#ifndef __FFMPEG_H__
#define __FFMPEG_H__

namespace ff
{
    enum VideoPixelFormat
    {
        VIDEO_PIX_RGB = 1,
        VIDEO_PIX_YUV420P,
    };
    
    typedef struct YUV420P{
        int w,h;
        unsigned char * data[3];
        int linesize[3];
    } YUV420P;
    
	class FFVideo
	{
	public:
		FFVideo();
		virtual ~FFVideo();
		bool open(const char *url);
		void seek(double t); //跳的指定位置进行播放，单位秒
		double cur() const; //视频当前播放位置,单位秒
		double cur_clock() const; //视频内部时钟
		double length() const; //视频时间长度,单位秒
		bool isPause() const; //视频是否在暂停状态
		bool isOpen() const; //视频是否打开
		bool isPlaying() const; //视频是否在播放
		bool isSeeking() const; //是否正在seek中
		bool hasVideo() const; //是否有视频流
		bool hasAudio() const; //是否有音频流

		bool isError() const; //如果在打开时发生错误返回true
		const char * errorMsg() const; //取得错误字符串
		/*
		 *	播放到结尾了返回true.
		 */
		bool isEnd() const;
		void pause(); //暂停视频
		void play(); //播放视频
		void close(); //关闭视频，并释放内存
		int width() const; //视频的宽度
		int height() const; //视频的高度
		int codec_width() const; //解码器视频的宽度
		int codec_height() const; //解码器视频的高度
		/*
		 *	刷新,播放程序需要以一定的帧率调用该函数。比如1/30s
		 *	函数成功返回一个RGB raw指针，格式为Texture2D::PixelFormat::RGB888
		 *	因此你可以直接用来作为材质使用
		 */
		void *refresh();
        
        VideoPixelFormat getPixelFormat();
        void *allocRgbBufferFormYuv420p(void *pyuv);
        void freeRgbBuffer(void * prgb);
        
        /*
		 *	网络预加载进度
		 *	set_preload_nb,
		 *	不设置使用默认值50
		 *	下载包的速度大于播放速度时视频就可以连续播放，并且这时可以下载更多包进行缓冲。
		 *	preload_packet_nb,返回缓冲的包数，如果该值在0的位置徘徊视频将是不流畅的。
		 */
		void set_preload_nb(int n);
		int preload_packet_nb() const;

		/*
		 *	set_preload_time 设置预加载时间单位秒
		 *	注：仅当视频打开后才能设置
		 */
		bool set_preload_time(double t);
		/*
		 *	preload_time 取得预加载视频的长度单位(秒)
		 *	函数失败返回-1
		 */
		double preload_time();
	private:
		void* _ctx;
		bool _first;
	};
	
	/*
	* 取得视频文件的流信息
	*/
	int getVideoInfo(const char * filename, int *w, int *h);

	enum TranCode
	{
		TC_BEGIN = 1,	//开始转码
		TC_END = 2,		//结束转码
		TC_PROGRESS = 3,//转码进度
		TC_ERROR = 4,	//转码错误
	};
	/*
	 * 执行ffmpeg命令
	 * 回调函数用来通知转码进度，tc表示状态，p是进度值0-1
	 * 回调返回0继续转码，非0将终止转码
	 */
	int ffmpeg(const char *cmd,int (*)(TranCode tc,float p));
}
#endif