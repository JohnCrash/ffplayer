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
		double length() const; //��Ƶʱ�䳤��,��λ��
		bool isPause() const; //��Ƶ�Ƿ�����ͣ״̬
		bool isOpen() const; //��Ƶ�Ƿ��
		bool isPlaying() const; //��Ƶ�Ƿ��ڲ���
		bool hasVideo() const; //�Ƿ�����Ƶ��
		bool hasAudio() const; //�Ƿ�����Ƶ��
		/*
			���ŵ���β�˷���true.
			Ҳ����ͨ��cur,��ȷ���Ƿ񵽽�β��.
		*/
		bool isEnd() const;
		void pause(); //��ͣ��Ƶ
		void play(); //������Ƶ
		void close(); //�ر���Ƶ�����ͷ��ڴ�
		int width() const; //��Ƶ�Ŀ��
		int height() const; //��Ƶ�ĸ߶�

		/*
			ˢ��,���ų�����Ҫ��һ����֡�ʵ��øú���������1/30s
			�����ɹ�����һ��RGB rawָ�룬���Ը�ʽΪTexture2D::PixelFormat::RGB888
			��������ֱ��������Ϊ����ʹ��
		*/
		void *refresh() const;
	private:
		void* _ctx;
	};
}
#endif