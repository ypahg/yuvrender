// D3D11YUVRendering.cpp : Defines the entry point for the application.
//


#include "D3D11YUVRendering.h"
#include "OutputManager.h"
#include <thread>
#define MAX_LOADSTRING 100

char buf[1024];

struct YUVFrame
{
	UINT width;
	UINT height;
	UINT pitch;
	BYTE* Y;
	BYTE* U;
	BYTE* V;
};

enum YUV
{
	YUV444,
	YUV420
};

enum YUV yuv = YUV444; // Change it accordingly

//
// Globals
//
OUTPUTMANAGER OutMgr;

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
HWND hWnd;										// The window handle created

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);



int test();
void WriteYUV444ToTexture(YUVFrame *yuvFrame);
void WriteYUV420ToTexture(YUVFrame *yuvFrame);
YUVFrame* ReadYUV420FromFile();
YUVFrame* ReadYUV444FromFile();

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.
	ReadYUV420FromFile();
    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_D3D11YUVRENDERING, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_D3D11YUVRENDERING));

	RECT DeskBounds;
	bool FirstTime = true;
	bool Occluded = true;

	std::thread([]() {test(); }).detach();

	MSG msg = { 0 };

	while (WM_QUIT != msg.message)
	{
		DUPL_RETURN Ret = DUPL_RETURN_SUCCESS;
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == OCCLUSION_STATUS_MSG)
			{
				// Present may not be occluded now so try again
				Occluded = false;
			}
			else
			{
				// Process window messages
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else if (FirstTime)
		{

			// First time through the loop so nothing to clean up
			FirstTime = false;

			// Re-initialize
			Ret = OutMgr.InitOutput(hWnd, &DeskBounds);

			// We start off in occluded state and we should immediate get a occlusion status window message
			Occluded = true;
		}
		else
		{
			//// Nothing else to do, so try to present to write out to window if not occluded
			//if (!Occluded)
			//{
			//	YUVFrame *yuvFrame = (yuv == YUV444) ? ReadYUV444FromFile() : ReadYUV420FromFile();
			//	(yuv == YUV444) ? WriteYUV444ToTexture(yuvFrame) : WriteYUV420ToTexture(yuvFrame);
			//	free(yuvFrame->Y);
			//	free(yuvFrame->U);
			//	free(yuvFrame->V);
			//	free(yuvFrame);

			//	Ret = OutMgr.UpdateApplicationWindow(&Occluded);
			//}
		}
	}

	if (msg.message == WM_QUIT)
	{
		OutMgr.CleanRefs();
		// For a WM_QUIT message we should return the wParam value
		return static_cast<INT>(msg.wParam);
	}

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_D3D11YUVRENDERING));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= nullptr;//MAKEINTRESOURCEW(IDC_D3D11YUVRENDERING);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY:
	{
		PostQuitMessage(0);
		break;
	}
	case WM_SIZE:
	{
		// Tell output manager that window size has changed
		OutMgr.WindowResize();
		break;
	}
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}



YUVFrame* ReadYUV444FromFile()
{

	FILE *file = nullptr;
	sprintf_s(buf, "content\\yuv444.bmp");
	fopen_s(&file, buf, "rb");
	int size = sizeof(YUVFrame);
	YUVFrame *yuvFrame = (YUVFrame*)malloc(sizeof(YUVFrame));
	int readBytes = fread(yuvFrame, sizeof(YUVFrame), 1, file);

	size = yuvFrame->pitch * yuvFrame->height;
	yuvFrame->Y = (BYTE *)malloc(size);
	readBytes = fread(yuvFrame->Y, 1, size, file);

	size = (yuvFrame->pitch * yuvFrame->height);
	yuvFrame->U = (BYTE *)malloc(size);
	readBytes = fread(yuvFrame->U, 1, size, file);

	size = (yuvFrame->pitch * yuvFrame->height);
	yuvFrame->V = (BYTE *)malloc(size);
	readBytes = fread(yuvFrame->V, 1, size, file);

	fclose(file);

	return yuvFrame;
}

YUVFrame* ReadYUV420FromFile()
{

	FILE *file = nullptr;
	sprintf_s(buf, "content\\yuv420.bmp");
	fopen_s(&file, buf, "rb");

	int size = sizeof(YUVFrame);
	YUVFrame *yuvFrame = (YUVFrame*)malloc(size);
	int readBytes = fread(yuvFrame, size, 1, file);

	size = yuvFrame->pitch * yuvFrame->height;
	yuvFrame->Y = (BYTE *)malloc(size);
	readBytes = fread(yuvFrame->Y, size, 1, file);

	size = yuvFrame->pitch/2 * yuvFrame->height / 2;
	yuvFrame->U = (BYTE *)malloc(size);
	readBytes = fread(yuvFrame->U, size, 1, file);

	size = yuvFrame->pitch/2 * yuvFrame->height / 2;
	yuvFrame->V = (BYTE *)malloc(size);
	readBytes = fread(yuvFrame->V, size, 1, file);

	fclose(file);

	return yuvFrame;
}

void WriteYUV444ToTexture(YUVFrame *yuvFrame)
{
	// Copy image into CPU access texture
	OutMgr.m_DeviceContext->CopyResource(OutMgr.m_AccessibleSurf, OutMgr.m_SharedSurf);

	// Copy from CPU access texture to bitmap buffer
	D3D11_MAPPED_SUBRESOURCE resource;
	UINT subresource = D3D11CalcSubresource(0, 0, 0);
	OutMgr.m_DeviceContext->Map(OutMgr.m_AccessibleSurf, subresource, D3D11_MAP_WRITE, 0, &resource);

	BYTE* dptr = reinterpret_cast<BYTE*>(resource.pData);

	int dist = 0;
	uint8_t u = 0, v = 0;
	for (int h = 0; h < yuvFrame->height; h++)
	{
		for (int w = 0; w < yuvFrame->width; w++)
		{
			dist = resource.RowPitch * (yuvFrame->height - 1 - h);

			dptr[dist + w * 4 + 2] = yuvFrame->Y[yuvFrame->pitch * h + w];
			dptr[dist + w * 4 + 1] = yuvFrame->U[yuvFrame->pitch * h + w];
			dptr[dist + w * 4]     = yuvFrame->V[yuvFrame->pitch * h + w];
		}
	}

	OutMgr.m_DeviceContext->Unmap(OutMgr.m_AccessibleSurf, subresource);
	OutMgr.m_DeviceContext->CopyResource(OutMgr.m_SharedSurf, OutMgr.m_AccessibleSurf);
}

void WriteYUV420ToTexture(YUVFrame *yuvFrame)
{
	// Copy image into CPU access texture
	OutMgr.m_DeviceContext->CopyResource(OutMgr.m_AccessibleSurf, OutMgr.m_SharedSurf);

	// Copy from CPU access texture to bitmap buffer
	D3D11_MAPPED_SUBRESOURCE resource;
	UINT subresource = D3D11CalcSubresource(0, 0, 0);
	OutMgr.m_DeviceContext->Map(OutMgr.m_AccessibleSurf, subresource, D3D11_MAP_WRITE, 0, &resource);

	BYTE* dptr = reinterpret_cast<BYTE*>(resource.pData);

	int dist = 0, dd = yuvFrame->pitch / 2;
	uint8_t u = 0, v = 0;
	for (int h = 0; h < yuvFrame->height; h += 2)
	{
		for (int w = 0; w < yuvFrame->width; w += 2)
		{
			u = yuvFrame->U[yuvFrame->pitch/2 * h / 2 + w / 2];
			v = yuvFrame->V[yuvFrame->pitch/2 * h / 2 + w / 2];

			for (int i = 0; i < 2; i++)
			{
				for (int j = 0; j < 2; j++)
				{
					dist = resource.RowPitch * (yuvFrame->height - 1 - (h + i));
					dptr[dist + (w + j) * 4 + 2] = yuvFrame->Y[yuvFrame->pitch * (h + i) + (w + j)];
					dptr[dist + (w + j) * 4 + 1] = u;
					dptr[dist + (w + j) * 4] = v;
				}
			}
		}
	}

	OutMgr.m_DeviceContext->Unmap(OutMgr.m_AccessibleSurf, subresource);
	OutMgr.m_DeviceContext->CopyResource(OutMgr.m_SharedSurf, OutMgr.m_AccessibleSurf);
}


#define __STDC_CONSTANT_MACROS

//Windows
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include  "libavdevice/avdevice.h"
};

#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avdevice.lib")

int test()
{
	AVFormatContext* pFormatCtx;
	int             i, videoindex;
	AVCodecContext* pCodecCtx;
	AVCodec* pCodec;
	AVFrame* pFrame, * pFrameYUV;
	uint8_t* out_buffer;
	AVPacket* packet;
	int y_size;
	int ret;
	struct SwsContext* img_convert_ctx;

	char filepath[] = "G:\\Code\\D3D11YUVRendering\\D3D11YUVRendering\\1.mp4";

	FILE* fp_yuv = fopen("output.yuv", "wb+");
	FILE* fp_h264 = fopen("output.h264", "wb+");
	
	
	avdevice_register_all();//注册所有组件
	avformat_network_init();//初始化网络
	pFormatCtx = avformat_alloc_context();//初始化一个AVFormatContext

	if (avformat_open_input(&pFormatCtx, filepath, NULL, NULL) != 0) {//打开输入的视频文件
		printf("Couldn't open input stream.\n");
		return -1;
	}
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {//获取视频文件信息
		printf("Couldn't find stream information.\n");
		return -1;
	}
	videoindex = -1;
	for (i = 0; i < pFormatCtx->nb_streams; i++)
		if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoindex = i;
			break;
		}

	if (videoindex == -1) {
		printf("Didn't find a video stream.\n");
		return -1;
	}




	pCodecCtx = avcodec_alloc_context3(NULL);
	if (pCodecCtx == NULL)
	{
		printf("Could not allocate AVCodecContext \n");
		return -1;
	}
	if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
		printf("Couldn't find audio stream information \n");
	}
	avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[videoindex]->codecpar);

	//pCodecCtx = pFormatCtx->streams[videoindex]->codecpar;
	pCodec = (AVCodec*)avcodec_find_decoder(pCodecCtx->codec_id);//查找解码器
	if (pCodec == NULL) {
		printf("Codec not found.\n");
		return -1;
	}
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {//打开解码器
		printf("Could not open codec.\n");
		return -1;
	}


	packet = av_packet_alloc();
	pFrame = av_frame_alloc();
	pFrameYUV = av_frame_alloc();
	ret = av_read_frame(pFormatCtx, packet);
	img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
		pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P,
		SWS_BICUBIC, NULL, NULL, NULL);

	int videoIndex = 0;
	int frame_cnt = 0;
	while (av_read_frame(pFormatCtx, packet) >= 0) {
		if (packet->stream_index == videoIndex) {
			ret = avcodec_send_packet(pCodecCtx, packet);
			if (ret < 0) {
				return -1;
			}
			ret = avcodec_receive_frame(pCodecCtx, pFrame);
			if (ret != 0) {
				// TODO 第一次调用avcodec_receive_frame()返回ret = -11，原因不明，先continue吧
				continue;
			}
			
			
			// sws_scale() 主要工作是进行图像转换
			if (sws_scale(img_convert_ctx, pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
				pFrameYUV->data, pFrameYUV->linesize) > 0) {




				y_size = pCodecCtx->width * pCodecCtx->height;
				fwrite(pFrameYUV->data[0], 1, y_size, fp_yuv);      // Y
				fwrite(pFrameYUV->data[1], 1, y_size / 4, fp_yuv);  // U
				fwrite(pFrameYUV->data[2], 1, y_size / 4, fp_yuv);  // V

				//Output info
				char pictype_str[10] = { 0 };
				switch (pFrame->pict_type) {
				case AV_PICTURE_TYPE_I:
					strcpy(pictype_str, "I");
					break;
				case AV_PICTURE_TYPE_P:
					strcpy(pictype_str, "P");
					break;
				case AV_PICTURE_TYPE_B:
					strcpy(pictype_str, "B");
					break;
				default:
					strcpy(pictype_str, "Other");
				}
				frame_cnt++;
			}
		}
		av_packet_unref(packet);
		while (av_read_frame(pFormatCtx, packet) >= 0) {
			if (packet->stream_index == videoIndex) {
				ret = avcodec_send_packet(pCodecCtx, packet);
				if (ret < 0) {
					return -1;
				}
				ret = avcodec_receive_frame(pCodecCtx, pFrame);
				if (ret != 0) {
					// TODO 第一次调用avcodec_receive_frame()返回ret = -11，原因不明，先continue吧
					continue;
				}
				
				bool  Occluded = true;
				YUVFrame  f;
				f.width = pFrame->width;
				f.height = pFrame->height;
				f.pitch = pFrame->linesize[0];
				f.Y = pFrame->data[0];
				f.U = pFrame->data[1];
				f.V = pFrame->data[2];
				WriteYUV420ToTexture(&f);
				OutMgr.UpdateApplicationWindow(&Occluded);

				// sws_scale() 主要工作是进行图像转换
				if (sws_scale(img_convert_ctx, pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
					pFrameYUV->data, pFrameYUV->linesize) > 0) {




					y_size = pCodecCtx->width * pCodecCtx->height;
					fwrite(pFrameYUV->data[0], 1, y_size, fp_yuv);      // Y
					fwrite(pFrameYUV->data[1], 1, y_size / 4, fp_yuv);  // U
					fwrite(pFrameYUV->data[2], 1, y_size / 4, fp_yuv);  // V

					//Output info
					char pictype_str[10] = { 0 };
					switch (pFrame->pict_type) {
					case AV_PICTURE_TYPE_I:
						strcpy(pictype_str, "I");
						break;
					case AV_PICTURE_TYPE_P:
						strcpy(pictype_str, "P");
						break;
					case AV_PICTURE_TYPE_B:
						strcpy(pictype_str, "B");
						break;
					default:
						strcpy(pictype_str, "Other");
					}
					frame_cnt++;
				}
			}
			av_packet_unref(packet);
		}
	}

	pFrameYUV = av_frame_alloc();
	out_buffer = (uint8_t*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 0));
	av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, out_buffer, AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 0);
	
	//Output Info-----------------------------
	printf("--------------- File Information ----------------\n");
	av_dump_format(pFormatCtx, 0, filepath, 0);
	printf("-------------------------------------------------\n");
	img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
		500, 500, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

	while (av_read_frame(pFormatCtx, packet) >= 0) {//读取一帧压缩数据
		if (packet->stream_index == videoindex) {

			fwrite(packet->data, 1, packet->size, fp_h264); //把H264数据写入fp_h264文件

			ret = avcodec_send_packet(pCodecCtx, packet);//解码一帧压缩数据
			if (ret < 0) {
				printf("Decode Error.\n");
				return -1;
			}
			sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
				pFrameYUV->data, pFrameYUV->linesize);

			y_size = pCodecCtx->width * pCodecCtx->height;
			fwrite(pFrameYUV->data[0], 1, y_size, fp_yuv);    //Y 
			fwrite(pFrameYUV->data[1], 1, y_size / 4, fp_yuv);  //U
			fwrite(pFrameYUV->data[2], 1, y_size / 4, fp_yuv);  //V
			printf("Succeed to decode 1 frame!\n");
		}
		av_packet_unref(packet);
	}
	//flush decoder
	/*当av_read_frame()循环退出的时候，实际上解码器中可能还包含剩余的几帧数据。
	因此需要通过“flush_decoder”将这几帧数据输出。
   “flush_decoder”功能简而言之即直接调用avcodec_decode_video2()获得AVFrame，而不再向解码器传递AVPacket。*/
	while (1) {
		ret = avcodec_send_packet(pCodecCtx, packet);
		if (ret < 0)
			break;
		sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
			pFrameYUV->data, pFrameYUV->linesize);

		int y_size = pCodecCtx->width * pCodecCtx->height;
		fwrite(pFrameYUV->data[0], 1, y_size, fp_yuv);    //Y 
		fwrite(pFrameYUV->data[1], 1, y_size / 4, fp_yuv);  //U
		fwrite(pFrameYUV->data[2], 1, y_size / 4, fp_yuv);  //V

		printf("Flush Decoder: Succeed to decode 1 frame!\n");
	}

	sws_freeContext(img_convert_ctx);

	//关闭文件以及释放内存
	fclose(fp_yuv);
	fclose(fp_h264);

	av_frame_free(&pFrameYUV);
	av_frame_free(&pFrame);
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);

	return 0;
}
