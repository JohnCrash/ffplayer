/*
 * 俘获视频的vfw实现
 */

#include "cap.h"
#include "ffenc.h"
#include <Windows.h>
#include <Vfw.h>

#pragma comment (lib,"vfw32")
#pragma comment (lib,"winmm.lib")

namespace cv
{

	static BITMAPINFOHEADER icvBitmapHeader(int width, int height, int bpp, int compression = BI_RGB)
	{
		BITMAPINFOHEADER bmih;
		memset(&bmih, 0, sizeof(bmih));
		bmih.biSize = sizeof(bmih);
		bmih.biWidth = width;
		bmih.biHeight = height;
		bmih.biBitCount = (WORD)bpp;
		bmih.biCompression = compression;
		bmih.biPlanes = 1;
		bmih.biSizeImage = width * height * bpp/8;
		return bmih;
	}

	class CvCaptureCAM_VFW : public CvCapture
	{
	public:
		CvCaptureCAM_VFW() { init(); }
		virtual ~CvCaptureCAM_VFW() { close(); }

		virtual bool open(int index);
		virtual void close();
		virtual double getProperty(int) const;
		virtual bool setProperty(int, double);
		virtual bool grabFrame();
		virtual AVRaw * retrieveFrame(int);
		virtual int getCaptureDomain() { return CV_CAP_VFW; } // Return the type of the capture object: CV_CAP_VFW, etc...

	protected:
		void init();
		void closeHIC();
		static LRESULT PASCAL frameCallback(HWND hWnd, VIDEOHDR* hdr);

		CAPDRIVERCAPS caps;
		HWND   capWnd;
		VIDEOHDR* hdr;
		DWORD  fourcc;
		int width, height;
		int widthSet, heightSet;
		HIC    hic;
		AVRaw * frame;
	};


	void CvCaptureCAM_VFW::init()
	{
		memset(&caps, 0, sizeof(caps));
		capWnd = 0;
		hdr = 0;
		fourcc = 0;
		hic = 0;
		frame = 0;
		width = height = -1;
		widthSet = heightSet = 0;
	}

	void CvCaptureCAM_VFW::closeHIC()
	{
		if (hic)
		{
			ICDecompressEnd(hic);
			ICClose(hic);
			hic = 0;
		}
	}


	LRESULT PASCAL CvCaptureCAM_VFW::frameCallback(HWND hWnd, VIDEOHDR* hdr)
	{
		CvCaptureCAM_VFW* capture = 0;

		if (!hWnd) return FALSE;

		capture = (CvCaptureCAM_VFW*)capGetUserData(hWnd);
		capture->hdr = hdr;

		return (LRESULT)TRUE;
	}


	// Initialize camera input
	bool CvCaptureCAM_VFW::open(int wIndex)
	{
		TCHAR szDeviceName[80];
		TCHAR szDeviceVersion[80];
		HWND hWndC = 0;

		close();

		if ((unsigned)wIndex >= 10)
			wIndex = 0;

		for (; wIndex < 10; wIndex++)
		{
			if (capGetDriverDescription(wIndex, szDeviceName,
				sizeof (szDeviceName), szDeviceVersion,
				sizeof (szDeviceVersion)))
			{
				hWndC = capCreateCaptureWindow(TEXT("My Own Capture Window"),
					WS_POPUP | WS_CHILD, 0, 0, 320, 240, 0, 0);
				if (capDriverConnect(hWndC, wIndex))
					break;
				DestroyWindow(hWndC);
				hWndC = 0;
			}
		}

		if (hWndC)
		{
			capWnd = hWndC;
			hdr = 0;
			hic = 0;
			fourcc = (DWORD)-1;

			memset(&caps, 0, sizeof(caps));
			capDriverGetCaps(hWndC, &caps, sizeof(caps));
			CAPSTATUS status = {};
			capGetStatus(hWndC, &status, sizeof(status));
			::SetWindowPos(hWndC, NULL, 0, 0, status.uiImageWidth, status.uiImageHeight, SWP_NOZORDER | SWP_NOMOVE);
			capSetUserData(hWndC, (size_t)this);
			capSetCallbackOnFrame(hWndC, frameCallback);
			CAPTUREPARMS p;
			capCaptureGetSetup(hWndC, &p, sizeof(CAPTUREPARMS));
			p.dwRequestMicroSecPerFrame = 66667 / 2; // 30 FPS
			capCaptureSetSetup(hWndC, &p, sizeof(CAPTUREPARMS));
			//capPreview( hWndC, 1 );
			capPreviewScale(hWndC, FALSE);
			capPreviewRate(hWndC, 1);

			// Get frame initial parameters.
			const DWORD size = capGetVideoFormatSize(capWnd);
			if (size > 0)
			{
				unsigned char *pbi = new unsigned char[size];
				if (pbi)
				{
					if (capGetVideoFormat(capWnd, pbi, size) == size)
					{
						BITMAPINFOHEADER& vfmt = ((BITMAPINFO*)pbi)->bmiHeader;
						widthSet = vfmt.biWidth;
						heightSet = vfmt.biHeight;
						fourcc = vfmt.biCompression;
					}
					delete[]pbi;
				}
			}
			// And alternative way in case of failure.
			if (widthSet == 0 || heightSet == 0)
			{
				widthSet = status.uiImageWidth;
				heightSet = status.uiImageHeight;
			}

		}
		return capWnd != 0;
	}


	void CvCaptureCAM_VFW::close()
	{
		if (capWnd)
		{
			capSetCallbackOnFrame(capWnd, NULL);
			capDriverDisconnect(capWnd);
			DestroyWindow(capWnd);
			closeHIC();
		}

		release_raw(frame);

		init();
	}


	bool CvCaptureCAM_VFW::grabFrame()
	{
		if (capWnd)
			return capGrabFrameNoStop(capWnd) == TRUE;

		return false;
	}

	/*
	 * 将图像进行上下翻转
	 */
	static void flip_rgb24_raw(AVRaw * praw)
	{
		int linesize = praw->linesize[0];
		uint8_t * tmp = (uint8_t *)malloc(linesize);
		if (tmp == 0)
			return;
		for (int y = 0; y < praw->height / 2; y++)
		{
			uint8_t * top = praw->data[0] + y*linesize;
			uint8_t * bottom = praw->data[0] + (praw->height-y-1)*linesize;
			memcpy(tmp,top, linesize);
			memcpy(top, bottom, linesize);
			memcpy(bottom, tmp, linesize);
		}
		free(tmp);
	}

	AVRaw * CvCaptureCAM_VFW::retrieveFrame(int)
	{
		BITMAPINFO vfmt;
		memset(&vfmt, 0, sizeof(vfmt));
		BITMAPINFOHEADER& vfmt0 = vfmt.bmiHeader;

		if (!capWnd)
			return 0;

		const DWORD sz = capGetVideoFormat(capWnd, &vfmt, sizeof(vfmt));
		const int prevWidth = frame ? frame->width : 0;
		const int prevHeight = frame ? frame->height : 0;

		if (!hdr || hdr->lpData == 0 || sz == 0)
			return 0;

		release_raw(frame);
		frame = make_image_raw((int)AV_PIX_FMT_BGR24, vfmt0.biWidth, vfmt0.biHeight);
		if (!frame)
		{
			return 0;
		}
		retain_raw(frame);
		/*
			FIXME:实现YUV420P编码
		if (vfmt0.biCompression == MAKEFOURCC('N', 'V', '1', '2'))
		{
			return 0;
		}
		else */
		if (vfmt0.biCompression != BI_RGB ||
			vfmt0.biBitCount != 24)
		{
			BITMAPINFOHEADER vfmt1 = icvBitmapHeader(vfmt0.biWidth, vfmt0.biHeight, 24);

			if (hic == 0 || fourcc != vfmt0.biCompression ||
				prevWidth != vfmt0.biWidth || prevHeight != vfmt0.biHeight)
			{
				closeHIC();
				hic = ICOpen(MAKEFOURCC('V', 'I', 'D', 'C'),
					vfmt0.biCompression, ICMODE_DECOMPRESS);
				if (hic)
				{
					if (ICDecompressBegin(hic, &vfmt0, &vfmt1) != ICERR_OK)
					{
						closeHIC();
						return 0;
					}
				}
			}

			memset(frame->data[0], 128, frame->size); 
			if (!hic || ICDecompress(hic, 0, &vfmt0, hdr->lpData,
				&vfmt1, frame->data[0]) != ICERR_OK)
			{
				closeHIC();
				return 0;
			}
			flip_rgb24_raw(frame);
		}
		else
		{
			/*
			 * 确保BITMAP和frame的对齐方式相同就可以使用内存复制操作
			 */
			memcpy(frame->data[0], hdr->lpData, frame->linesize[0] * frame->height);
		}
		return frame;
	}


	double CvCaptureCAM_VFW::getProperty(int property_id) const
	{
		switch (property_id)
		{
		case CV_CAP_PROP_FRAME_WIDTH:
			return widthSet;
		case CV_CAP_PROP_FRAME_HEIGHT:
			return heightSet;
		case CV_CAP_PROP_FOURCC:
			return fourcc;
		case CV_CAP_PROP_FPS:
		{
								CAPTUREPARMS params = {};
								if (capCaptureGetSetup(capWnd, &params, sizeof(params)))
									return 1e6 / params.dwRequestMicroSecPerFrame;
		}
			break;
		default:
			break;
		}
		return 0;
	}

	bool CvCaptureCAM_VFW::setProperty(int property_id, double value)
	{
		bool handledSize = false;

		switch (property_id)
		{
		case CV_CAP_PROP_FRAME_WIDTH:
			width = cvRound(value);
			handledSize = true;
			break;
		case CV_CAP_PROP_FRAME_HEIGHT:
			height = cvRound(value);
			handledSize = true;
			break;
		case CV_CAP_PROP_FOURCC:
			break;
		case CV_CAP_PROP_FPS:
			if (value > 0)
			{
				CAPTUREPARMS params;
				if (capCaptureGetSetup(capWnd, &params, sizeof(params)))
				{
					params.dwRequestMicroSecPerFrame = cvRound(1e6 / value);
					return capCaptureSetSetup(capWnd, &params, sizeof(params)) == TRUE;
				}
			}
			break;
		default:
			break;
		}

		if (handledSize)
		{
			// If both width and height are set then change frame size.
			if (width > 0 && height > 0)
			{
				const DWORD size = capGetVideoFormatSize(capWnd);
				if (size == 0)
					return false;

				unsigned char *pbi = new unsigned char[size];
				if (!pbi)
					return false;

				if (capGetVideoFormat(capWnd, pbi, size) != size)
				{
					delete[]pbi;
					return false;
				}

				BITMAPINFOHEADER& vfmt = ((BITMAPINFO*)pbi)->bmiHeader;
				bool success = true;
				if (width != vfmt.biWidth || height != vfmt.biHeight)
				{
					// Change frame size.
					vfmt.biWidth = width;
					vfmt.biHeight = height;
					vfmt.biSizeImage = height * ((width * vfmt.biBitCount + 31) / 32) * 4;
					vfmt.biCompression = BI_RGB;
					success = capSetVideoFormat(capWnd, pbi, size) == TRUE;
				}
				if (success)
				{
					// Adjust capture window size.
					CAPSTATUS status = {};
					capGetStatus(capWnd, &status, sizeof(status));
					::SetWindowPos(capWnd, NULL, 0, 0, status.uiImageWidth, status.uiImageHeight, SWP_NOZORDER | SWP_NOMOVE);
					// Store frame size.
					widthSet = width;
					heightSet = height;
				}
				delete[]pbi;
				width = height = -1;

				return success;
			}

			return true;
		}

		return false;
	}

	CvCapture* cvCreateCameraCapture_VFW(int index)
	{
		CvCaptureCAM_VFW* capture = new CvCaptureCAM_VFW;

		if (capture->open(index))
			return capture;

		delete capture;
		return 0;
	}
} //namespace cv