#ifndef __FFMPEG_H__
#define __FFMPEG_H__

namespace ff
{
	class FFVideo
	{
	public:
		FFVideo();
		virtual ~FFVideo();
		bool open(const char *url);
		void seek(double t); //����ָ��λ�ý��в��ţ���λ��
		double cur() const; //��Ƶ��ǰ����λ��,��λ��
		double cur_clock() const; //��Ƶ�ڲ�ʱ��
		double length() const; //��Ƶʱ�䳤��,��λ��
		bool isPause() const; //��Ƶ�Ƿ�����ͣ״̬
		bool isOpen() const; //��Ƶ�Ƿ��
		bool isPlaying() const; //��Ƶ�Ƿ��ڲ���
		bool isSeeking() const; //�Ƿ�����seek��
		bool hasVideo() const; //�Ƿ�����Ƶ��
		bool hasAudio() const; //�Ƿ�����Ƶ��

		bool isError() const; //����ڴ�ʱ�������󷵻�true
		const char * errorMsg() const; //ȡ�ô����ַ���
		/*
		 *	���ŵ���β�˷���true.
		 */
		bool isEnd() const;
		void pause(); //��ͣ��Ƶ
		void play(); //������Ƶ
		void close(); //�ر���Ƶ�����ͷ��ڴ�
		int width() const; //��Ƶ�Ŀ��
		int height() const; //��Ƶ�ĸ߶�
		int codec_width() const; //��������Ƶ�Ŀ��
		int codec_height() const; //��������Ƶ�ĸ߶�
		/*
		 *	ˢ��,���ų�����Ҫ��һ����֡�ʵ��øú���������1/30s
		 *	�����ɹ�����һ��RGB rawָ�룬��ʽΪTexture2D::PixelFormat::RGB888
		 *	��������ֱ��������Ϊ����ʹ��
		 */
		void *refresh();

		/*
		 *	����Ԥ���ؽ���
		 *	set_preload_nb,
		 *	������ʹ��Ĭ��ֵ50
		 *	���ذ����ٶȴ��ڲ����ٶ�ʱ��Ƶ�Ϳ����������ţ�������ʱ�������ظ�������л��塣
		 *	preload_packet_nb,���ػ���İ����������ֵ��0��λ���ǻ���Ƶ���ǲ������ġ�
		 */
		void set_preload_nb(int n);
		int preload_packet_nb() const;

		/*
		 *	set_preload_time ����Ԥ����ʱ�䵥λ��
		 *	ע��������Ƶ�򿪺��������
		 */
		bool set_preload_time(double t);
		/*
		 *	preload_time ȡ��Ԥ������Ƶ�ĳ��ȵ�λ(��)
		 *	����ʧ�ܷ���-1
		 */
		double preload_time();
	private:
		void* _ctx;
		bool _first;
	};
}
#endif