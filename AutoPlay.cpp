// AutoPlay.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "resource.h"

#define WINDOW_WIDTH 500
#define WINDOW_HEIGHT 500
#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// The title bar text

// customer define
// 目前已知
// 1. diamond 消除後有機會按錯 因為畫面會進入動畫階段
// 2. buffer 從畫面更新時沒有發火 按到一半發火 influence window 會變化 但是用到沒發火階段的 window size
// 3. 剛消完掉下來又剛好跟旁邊顏色一樣 消掉後 buffer 更新會有預期以外的問題
bool game_loop = FALSE;
bool game_status = FALSE;
bool game_check_diamond = TRUE;
unsigned char game_delay_control = 6;
HWND hWnd_target;
TCHAR SearchText[MAX_LOADSTRING] = _T("Diamond");
#define GAME_START_X 24//79
#define GAME_START_Y 28//93
#define DIAMOND_X_NUM 10
#define DIAMOND_Y_NUM 9
#define DIAMOND_SIZE 12//40

unsigned int img_game_width = DIAMOND_X_NUM*DIAMOND_SIZE+DIAMOND_SIZE;
unsigned int img_game_height = DIAMOND_Y_NUM*DIAMOND_SIZE+DIAMOND_SIZE;
RECT GAME_START;
RECT target_rc, img_rc;
BITMAPINFO bmi;

struct BlockSampleInfo
{
	double HSI_H_min;
	double HSI_H_max;
	double HSI_H_min_step2;
	double HSI_H_max_step2;
};
BlockSampleInfo block_sample_info[7];
struct MappingInfo
{
	COLORREF mapping_color;
	unsigned char group_set_number; /* from 0 to DIAMOND_X_NUM*DIAMOND_Y_NUM-1 */
	unsigned char block_type; // 0:not block, 1:red, 2:yellow, 3:green, 4:blue, 5:purple, 6:diamond
	double HSI_H;
	double HSI_S;
	double HSI_I;
	double HSV_H;
	double HSV_S;
	double HSV_V;
	bool influence_area;
	bool searched;
	bool re_check;
};
MappingInfo mapping_info[DIAMOND_Y_NUM][DIAMOND_X_NUM];
unsigned int same_color_count_data[DIAMOND_Y_NUM*DIAMOND_X_NUM]; // index is group set number
POINT node_visit[DIAMOND_Y_NUM*DIAMOND_X_NUM];
int node_visit_position = 0;
bool MUST_UPDATE_DATA_FROM_DC_PIC;
POINT cur_click, pre_click;
void MainLoop(HWND hWnd);

// unicode reference
// http://www.ucancode.net/faq/Visual_c_character_sets-Unicode-MBCS.htm
// http://blog.csdn.net/yanonsoftware/article/details/544428
TCHAR WindowsList_Text[50];
HWND WindowsList_hWnd;

// Foward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
 	// TODO: Place code here.
	MSG msg;
	HACCEL hAccelTable;

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_AUTOPLAY, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow)) 
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)IDC_AUTOPLAY);

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0)) 
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return msg.wParam;
}

void ProcessMessages()
{
   MSG msg;
   while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
   {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
   }
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage is only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX); 

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= (WNDPROC)WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, (LPCTSTR)IDI_AUTOPLAY);
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= (LPCWSTR)IDC_AUTOPLAY;
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, (LPCTSTR)IDI_AUTOPLAY);

	return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HANDLE, int)
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
   HWND hWnd;

   hInst = hInstance; // Store instance handle in our global variable

   hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

unsigned char checkNode(int y, int x)
{
	int sum = 0;

	if (mapping_info[y][x].searched == 0)
	{
		mapping_info[y][x].searched = 1;
		// which node already visit
		node_visit[node_visit_position].x = x;
		node_visit[node_visit_position].y = y;
		node_visit_position++;

		// check right
		if (x+1 < DIAMOND_X_NUM)
		{
			//if (abs(mapping_info[y][x].HSI_H - mapping_info[y][x+1].HSI_H) < 2)
			if (mapping_info[y][x].block_type == mapping_info[y][x+1].block_type)
			{
				sum += checkNode(y, x+1);
			}
		}

		// check top
		if (y-1 >= 0)
		{
			//if (abs(mapping_info[y][x].HSI_H - mapping_info[y-1][x].HSI_H) < 2)
			if (mapping_info[y][x].block_type == mapping_info[y-1][x].block_type)
			{
				sum += checkNode(y-1, x);
			}
		}

		// check left
		if (x-1 >= 0)
		{
			//if (abs(mapping_info[y][x].HSI_H - mapping_info[y][x-1].HSI_H) < 2)
			if (mapping_info[y][x].block_type == mapping_info[y][x-1].block_type)
			{
				sum += checkNode(y, x-1);
			}
		}

		// check bottom
		if (y+1 < DIAMOND_Y_NUM)
		{
			//if (abs(mapping_info[y][x].HSI_H - mapping_info[y+1][x].HSI_H) < 2)
			if (mapping_info[y][x].block_type == mapping_info[y+1][x].block_type)
			{
				sum += checkNode(y+1, x);
			}
		}
		
		return (1+sum);
	}
	else
		return 0;
}

unsigned char checkNode_RE(int y, int x)
{
	int sum = 0;

	
	if (mapping_info[y][x].re_check == 0)
	{
		mapping_info[y][x].re_check = 1;
		// which node already visit
		node_visit[node_visit_position].x = x;
		node_visit[node_visit_position].y = y;
		node_visit_position++;

		// check right
		if (x+1 < DIAMOND_X_NUM)
		{
			//if (abs(mapping_info[y][x].HSI_H - mapping_info[y][x+1].HSI_H) < 2)
			if (mapping_info[y][x].block_type == mapping_info[y][x+1].block_type)
			{
				sum += checkNode_RE(y, x+1);
			}
		}

		// check top
		if (y-1 >= 0)
		{
			//if (abs(mapping_info[y][x].HSI_H - mapping_info[y-1][x].HSI_H) < 2)
			if (mapping_info[y][x].block_type == mapping_info[y-1][x].block_type)
			{
				sum += checkNode_RE(y-1, x);
			}
		}

		// check left
		if (x-1 >= 0)
		{
			//if (abs(mapping_info[y][x].HSI_H - mapping_info[y][x-1].HSI_H) < 2)
			if (mapping_info[y][x].block_type == mapping_info[y][x-1].block_type)
			{
				sum += checkNode_RE(y, x-1);
			}
		}

		// check bottom
		if (y+1 < DIAMOND_Y_NUM)
		{
			//if (abs(mapping_info[y][x].HSI_H - mapping_info[y+1][x].HSI_H) < 2)
			if (mapping_info[y][x].block_type == mapping_info[y+1][x].block_type)
			{
				sum += checkNode_RE(y+1, x);
			}
		}
		
		return (1+sum);
	}
	else
		return 0;
}

BOOL CALLBACK WorkerProc(HWND hwnd, LPARAM lParam)
{
	TCHAR buffer[50];

	GetWindowText(hwnd, buffer, 50);
	if(_tcsstr(buffer, SearchText))
	{
		//MessageBox(NULL, buffer, NULL, MB_OK);
		_tcscpy(WindowsList_Text, buffer);
		WindowsList_hWnd = hwnd;
		return FALSE;
	}

	return TRUE;
}

BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam)
{
	RECT rc;

	//GetWindowRect((HWND)0x0013031E, &rc);
	GetWindowRect(hwnd, &rc);

	if (rc.top >= 0 && rc.bottom >= 0 && rc.left >=0 && rc.right >= 0)
	{
		GAME_START = rc;
		GAME_START.left += GAME_START_X;
		GAME_START.top += GAME_START_Y;
	}

	return TRUE; // must return TRUE; If return is FALSE it stops the recursion
}

void Sample()
{
	int x, y; 
	HDC hdcTemp, hdcSrc;
	HBITMAP hbmpTemp;
	BITMAP bm;

	GetWindowRect(hWnd_target, &target_rc);
	//GetClientRect(hWnd_target, &rc);
	img_rc.left = GAME_START.left - target_rc.left;
	img_rc.top = GAME_START.top - target_rc.top;

	// 需要更新
	hdcSrc = GetWindowDC(hWnd_target);

	// 取得目標應用程式 DC
	hdcTemp = CreateCompatibleDC(hdcSrc);
	hbmpTemp = CreateCompatibleBitmap(hdcSrc, img_game_width, img_game_height);
	SelectObject(hdcTemp, hbmpTemp);
	StretchBlt(hdcTemp, 0, img_game_height, img_game_width, -img_game_height, hdcSrc, img_rc.left, img_rc.top, img_game_width, img_game_height, SRCCOPY);
	
	GetObject(hbmpTemp, sizeof(bm), &bm);
	COLORREF* pixel = new COLORREF[bm.bmWidth*bm.bmHeight];

	//Create and fill BITMAPINFO structure to pass to GetDIBits
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = bm.bmWidth;
	bmi.bmiHeader.biHeight = bm.bmHeight;
	bmi.bmiHeader.biPlanes = bm.bmPlanes; 
	bmi.bmiHeader.biBitCount = 32; 
	bmi.bmiHeader.biCompression = 0; 
	bmi.bmiHeader.biSizeImage = (bm.bmWidth+7)/8*bm.bmHeight*bm.bmBitsPixel;
	bmi.bmiHeader.biXPelsPerMeter = 0; 
	bmi.bmiHeader.biYPelsPerMeter = 0; 
	bmi.bmiHeader.biClrUsed = 0; 
	GetDIBits(hdcSrc, hbmpTemp, 0, bm.bmHeight, pixel, &bmi, DIB_RGB_COLORS);

	// algorithm start
	double R=0.0, G=0.0, B=0.0, H=0.0, S=0.0, I=0.0, Theta=0.0;
	double H_MAX=0.0, H_MIN=0.0;
	COLORREF define_color;

	// get each block info
	for (y=0; y<DIAMOND_Y_NUM; y++)
	{
		for (x=0; x<DIAMOND_X_NUM; x++)
		{
			// initial
			//define_color = pixel[y*(img_game_width*DIAMOND_SIZE)+16*img_game_width + x*DIAMOND_SIZE+ 30];
			define_color = pixel[y*(img_game_width*DIAMOND_SIZE)+5*img_game_width + x*DIAMOND_SIZE+ 9];
			
			B = GetRValue(define_color)/255.0;
			G = GetGValue(define_color)/255.0;
			R = GetBValue(define_color)/255.0;
			
			// get HSI
			Theta = acos((((R-G)+(R-B))/2.0)/(pow((pow(R-G,2)+(R-B)*(G-B)),0.5)))*180.0/3.14;

			if (B <= G)
				H = Theta;
			else
				H = 360.0-Theta;

			if (R == G && G == B)
				H = 999;

			S = 1.0 - ((3.0/(R+G+B))*min(R,min(G,B)));
			I = (R+G+B)/3.0;

			H_MAX = H+2;
			H_MIN = H-2;
			if ((H >= 351.6 && H <= 360.0) || (H >= 0.0 && H <= 5.0))
			{
				if (I > 0.2) // red
				{
					block_sample_info[1].HSI_H_max = H_MAX;
					block_sample_info[1].HSI_H_min = H_MIN;
					block_sample_info[1].HSI_H_max_step2 = H_MAX;
					block_sample_info[1].HSI_H_min_step2 = H_MIN;

					if (H_MAX > 360.0)
					{
						block_sample_info[1].HSI_H_max = 360.0;
						block_sample_info[1].HSI_H_min = H_MIN;
						block_sample_info[1].HSI_H_max_step2 = H_MAX-360.0;
						block_sample_info[1].HSI_H_min_step2 = 0.0;
					}
					else if (H_MIN < 0.0)
					{
						block_sample_info[1].HSI_H_max = H_MAX;
						block_sample_info[1].HSI_H_min = 0.0;
						block_sample_info[1].HSI_H_max_step2 = 360.0;
						block_sample_info[1].HSI_H_min_step2 = 360.0+H_MIN;
					}
				}
			}
			else if (H >= 30.8 && H <= 53.8) // yellow
			{
				block_sample_info[2].HSI_H_max = H_MAX;
				block_sample_info[2].HSI_H_min = H_MIN;
			}
			else if (H >= 100.1 && H <= 120.1) // green
			{
				block_sample_info[3].HSI_H_max = H_MAX;
				block_sample_info[3].HSI_H_min = H_MIN;
			}
			else if (H >= 212.2 && H <= 232.2) // blue
			{
				block_sample_info[4].HSI_H_max = H_MAX;
				block_sample_info[4].HSI_H_min = H_MIN;
			}
			else if (H >= 265.9 && H <= 280.9) // purple
			{
				block_sample_info[5].HSI_H_max = H_MAX;
				block_sample_info[5].HSI_H_min = H_MIN;
			}
		}
	}

	// release
	DeleteObject(hdcTemp);
	ReleaseDC(NULL, hdcSrc);
	DeleteObject(hbmpTemp);
	delete [] pixel;
}

void MainLoop(HWND hWnd)
{
	int i, x, y; 
	HDC hdc, hdcTemp, hdcSrc;
	HBITMAP hbmpTemp;
	BITMAP bm;

	/* need sample first if mark here
	GetWindowRect(hWnd_target, &target_rc);
	//GetClientRect(hWnd_target, &rc);
	img_rc.left = GAME_START.left - target_rc.left;
	img_rc.top = GAME_START.top - target_rc.top;
	*/

	// 取得桌面 hWnd
	//hWnd_target = GetDesktopWindow();

	// 需要更新
	if (MUST_UPDATE_DATA_FROM_DC_PIC)
	{
		hdcSrc = GetWindowDC(hWnd_target);

		// 取得目標應用程式 DC
		hdcTemp = CreateCompatibleDC(hdcSrc);
		hbmpTemp = CreateCompatibleBitmap(hdcSrc, img_game_width, img_game_height);
		SelectObject(hdcTemp, hbmpTemp);
		StretchBlt(hdcTemp, 0, img_game_height, img_game_width, -img_game_height, hdcSrc, img_rc.left, img_rc.top, img_game_width, img_game_height, SRCCOPY);
		
		GetObject(hbmpTemp, sizeof(bm), &bm);
		COLORREF* pixel = new COLORREF[bm.bmWidth*bm.bmHeight];

		//Create and fill BITMAPINFO structure to pass to GetDIBits
		/* need sample first if mark here
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = bm.bmWidth;
		bmi.bmiHeader.biHeight = bm.bmHeight;
		bmi.bmiHeader.biPlanes = bm.bmPlanes; 
		bmi.bmiHeader.biBitCount = 32; 
		bmi.bmiHeader.biCompression = 0; 
		bmi.bmiHeader.biSizeImage = (bm.bmWidth+7)/8*bm.bmHeight*bm.bmBitsPixel;
		bmi.bmiHeader.biXPelsPerMeter = 0; 
		bmi.bmiHeader.biYPelsPerMeter = 0; 
		bmi.bmiHeader.biClrUsed = 0; 
		*/
		GetDIBits(hdcSrc, hbmpTemp, 0, bm.bmHeight, pixel, &bmi, DIB_RGB_COLORS);

		/*hdc = GetDC(hWnd);
		for (i=0; i<img_game_width*img_game_height; i++)
		{
			SetPixel(hdc, i%img_game_width, i/img_game_width, RGB(GetBValue(pixel[i]), GetGValue(pixel[i]), GetRValue(pixel[i])));
			//BitBlt(hdc, 0, 0, game_width, game_height, hdcSrc, 0, 0, SRCCOPY);
		}*/

		// algorithm start
		double R=0.0, G=0.0, B=0.0, H=0.0, S=0.0, I=0.0, Theta=0.0, V=0.0;
		double MAX, MIN, X, r, g, b;
		COLORREF define_color;

		// get game status is normal or fire
		//define_color = pixel[9*(img_game_width*DIAMOND_SIZE)+12*img_game_width + 10*DIAMOND_SIZE+ 12];
		define_color = pixel[9*(img_game_width*DIAMOND_SIZE)+3*img_game_width + 10*DIAMOND_SIZE+ 3];

		B = GetRValue(define_color)/255.0;
		G = GetGValue(define_color)/255.0;
		R = GetBValue(define_color)/255.0;		

		Theta = acos((((R-G)+(R-B))/2.0)/(pow((pow(R-G,2)+(R-B)*(G-B)),0.5)))*180.0/3.14;
		if (B <= G)
			H = Theta;
		else
			H = 360.0-Theta;

		if (R == G && G == B)
			H = 999;

		if (H <= 140)
			game_status = 1;
		else
			game_status = 0;

		// get each block info
		for (y=0; y<DIAMOND_Y_NUM; y++)
		{
			for (x=0; x<DIAMOND_X_NUM; x++)
			{
				// initial
				//define_color = pixel[y*(img_game_width*DIAMOND_SIZE)+16*img_game_width + x*DIAMOND_SIZE+ 30];
				define_color = pixel[y*(img_game_width*DIAMOND_SIZE)+5*img_game_width + x*DIAMOND_SIZE+ 9];
				
				B = GetRValue(define_color)/255.0;
				G = GetGValue(define_color)/255.0;
				R = GetBValue(define_color)/255.0;
				
				// get HSI
				Theta = acos((((R-G)+(R-B))/2.0)/(pow((pow(R-G,2)+(R-B)*(G-B)),0.5)))*180.0/3.14;

				if (B <= G)
					H = Theta;
				else
					H = 360.0-Theta;

				if (R == G && G == B)
					H = 999;

				S = 1.0 - ((3.0/(R+G+B))*min(R,min(G,B)));
				I = (R+G+B)/3.0;

				// get HSV
				/*
				MAX = max(R, max(G, B));
				MIN = min(R, min(G, B));
				V = MAX;
				if (MAX-MIN == 0)
				{
					H = 0;
					S = 0;
				}
				else
				{
					X = MAX-MIN;
					S = X/V;
					r = ( ((MAX-R)/6)+(X/2) )/X;
					g = ( ((MAX-G)/6)+(X/2) )/X;
					b = ( ((MAX-B)/6)+(X/2) )/X;

					if (R == MAX)
						H = b-g;
					else if (G == MAX)
						H = (1.0/3.0)+r-b;
					else if (B == MAX)
						H = (2.0/3.0)+r-g;

					if (H > 1.0)
						H = 1.0-H;
					if (H < 0.0)
						H = 1.0+H;
				}
				*/

				mapping_info[y][x].mapping_color = define_color;
				mapping_info[y][x].group_set_number = y*DIAMOND_X_NUM+x;
				mapping_info[y][x].HSI_H = H;
				mapping_info[y][x].HSI_S = S;
				mapping_info[y][x].HSI_I = I;
				//mapping_info[y][x].HSV_H = H;
				//mapping_info[y][x].HSV_S = S;
				//mapping_info[y][x].HSV_V = V;
				//mapping_info[y][x].influence_area = 0;
				//mapping_info[y][x].searched = 0;
				//mapping_info[y][x].re_check = 0;

				//if ((H >= 351.6 && H <= 360.0) || (H >= 0.0 && H <= 5.0))
				if ((H >= block_sample_info[1].HSI_H_min && H <= block_sample_info[1].HSI_H_max) || (H >= block_sample_info[1].HSI_H_min_step2 && H <= block_sample_info[1].HSI_H_max_step2))
				{
					if (I > 0.2)
						mapping_info[y][x].block_type = 1; // red else maybe is background
				}
				else if (H >= block_sample_info[2].HSI_H_min && H <= block_sample_info[2].HSI_H_max)
					mapping_info[y][x].block_type = 2; // yellow
				else if (H >= block_sample_info[3].HSI_H_min && H <= block_sample_info[3].HSI_H_max)
					mapping_info[y][x].block_type = 3; // green
				else if (H >= block_sample_info[4].HSI_H_min && H <= block_sample_info[4].HSI_H_max)
					mapping_info[y][x].block_type = 4; // blue
				else if (H >= block_sample_info[5].HSI_H_min && H <= block_sample_info[5].HSI_H_max)
					mapping_info[y][x].block_type = 5; // purple
				//else if (H >= 195.0 && H <= 210 && S >= 0.45 && S <= 0.54)
				else if (H >= 120.0 && H <= 210.0 && S >= 0.45 && S <= 0.60 && I >= 0.3 && I <= 0.7)
					mapping_info[y][x].block_type = 6; // diamond
				else
					mapping_info[y][x].block_type = 0;
			}
		}
		
		// release
		DeleteObject(hdcTemp);
		//ReleaseDC(NULL, hdc);
		ReleaseDC(hWnd, hdcSrc);
		DeleteObject(hbmpTemp);
		delete [] pixel;

		// search
		//for (i=0; i<DIAMOND_Y_NUM*DIAMOND_X_NUM; i++)
		//	same_color_count_data[i] = 0;
		int counter = 0;
		for (y=0; y<DIAMOND_Y_NUM; y++)
		{
			for (x=0; x<DIAMOND_X_NUM; x++)
			{
				node_visit_position = 0;
				counter = checkNode(y, x);
				if (counter != 0)
					same_color_count_data[mapping_info[y][x].group_set_number] = counter;

				for (i=0; i<node_visit_position; i++) // 0 is itself
					mapping_info[node_visit[i].y][node_visit[i].x].group_set_number = mapping_info[y][x].group_set_number;
			}
		}
	}
	
	// from top click is better
	int counter = 0;
	bool diamond_find = FALSE, normal_find = FALSE;
	for (y=pre_click.y; y<DIAMOND_Y_NUM; y++)
	{
		for (x=0; x<DIAMOND_X_NUM; x++)
		{
			if (mapping_info[y][x].influence_area == 0)
			{
				if (game_check_diamond)
				{
					if (mapping_info[y][x].block_type == 6)
					{
						cur_click.x = x;
						cur_click.y = y;
						diamond_find = TRUE;
						break;
					}
				}
				if (same_color_count_data[mapping_info[y][x].group_set_number] >= 3 && mapping_info[y][x].block_type != 0)
				{
					cur_click.x = x;
					cur_click.y = y;
					
					// re-search and check
					node_visit_position = 0;
					counter = checkNode_RE(y, x);

					if (counter >= 3)
					{
						normal_find = TRUE;
						break;
					}
					else
						same_color_count_data[mapping_info[y][x].group_set_number] -= counter;
				}
			}
		}
		if (normal_find || diamond_find)
			break;
	}

	/*if (normal_find || diamond_find)
	{
		hdcSrc = GetWindowDC(hWnd_target);

		// 取得目標應用程式 DC
		hdcTemp = CreateCompatibleDC(hdcSrc);
		hbmpTemp = CreateCompatibleBitmap(hdcSrc, img_game_width, img_game_height);
		SelectObject(hdcTemp, hbmpTemp);
		StretchBlt(hdcTemp, 0, img_game_height, img_game_width, -img_game_height, hdcSrc, img_rc.left, img_rc.top, img_game_width, img_game_height, SRCCOPY);
		
		GetObject(hbmpTemp, sizeof(bm), &bm);
		COLORREF* pixel = new COLORREF[bm.bmWidth*bm.bmHeight];

		GetDIBits(hdcSrc, hbmpTemp, 0, bm.bmHeight, pixel, &bmi, DIB_RGB_COLORS);

		for (i=0; i<node_visit_position; i++)
		{
			// algorithm start
			double R=0.0, G=0.0, B=0.0, H=0.0, S=0.0, I=0.0, Theta=0.0, V=0.0;
			double MAX, MIN, X, r, g, b;
			COLORREF define_color;

			// initial
			define_color = pixel[node_visit[i].y*(img_game_width*DIAMOND_SIZE)+5*img_game_width + node_visit[i].x*DIAMOND_SIZE+ 9];
			
			B = GetRValue(define_color)/255.0;
			G = GetGValue(define_color)/255.0;
			R = GetBValue(define_color)/255.0;
			
			// get HSI
			Theta = acos((((R-G)+(R-B))/2.0)/(pow((pow(R-G,2)+(R-B)*(G-B)),0.5)))*180.0/3.14;

			if (B <= G)
				H = Theta;
			else
				H = 360.0-Theta;

			if (R == G && G == B)
				H = 999;

			S = 1.0 - ((3.0/(R+G+B))*min(R,min(G,B)));
			I = (R+G+B)/3.0;

			unsigned char block_type;
			if ((H >= block_sample_info[1].HSI_H_min && H <= block_sample_info[1].HSI_H_max) || (H >= block_sample_info[1].HSI_H_min_step2 && H <= block_sample_info[1].HSI_H_max_step2))
			{
				if (I > 0.2)
					block_type = 1; // red else maybe is background
			}
			else if (H >= block_sample_info[2].HSI_H_min && H <= block_sample_info[2].HSI_H_max)
				block_type = 2; // yellow
			else if (H >= block_sample_info[3].HSI_H_min && H <= block_sample_info[3].HSI_H_max)
				block_type = 3; // green
			else if (H >= block_sample_info[4].HSI_H_min && H <= block_sample_info[4].HSI_H_max)
				block_type = 4; // blue
			else if (H >= block_sample_info[5].HSI_H_min && H <= block_sample_info[5].HSI_H_max)
				block_type = 5; // purple
			//else if (H >= 195.0 && H <= 210 && S >= 0.45 && S <= 0.54)
			else if (H >= 120.0 && H <= 210.0 && S >= 0.45 && S <= 0.60 && I >= 0.3 && I <= 0.7)
				block_type = 6; // diamond
			else
				block_type = 0;

			if (block_type != mapping_info[node_visit[i].y][node_visit[i].x].block_type)
			{
				normal_find = FALSE;
				diamond_find = FALSE;
				break;
			}
		}
		// release
		DeleteObject(hdcTemp);
		//ReleaseDC(NULL, hdc);
		ReleaseDC(hWnd, hdcSrc);
		DeleteObject(hbmpTemp);
		delete [] pixel;
	}
	*/

	// diamond have high priority
	/*
	if (game_check_diamond)
	{
		for (y=pre_click.y; y<DIAMOND_Y_NUM; y++)
		{
			for (x=0; x<DIAMOND_X_NUM; x++)
			{
				if (mapping_info[y][x].influence_area == 0)
				{
					if (mapping_info[y][x].block_type == 6)
					{
						cur_click.x = x;
						cur_click.y = y;
						diamond_find = TRUE;
						break;
					}
				}
			}
			if (diamond_find)
				break;
		}
	}
	*/

	// using current click info to update same color count data
	//game_status = 1;
	if (normal_find)
	{
		int start_x, end_x; // active block will affect nearby block
		for (i=0; i<node_visit_position; i++) // 0 is itself
		{
			for (y=0; y<=node_visit[i].y; y++)
			{
				if (node_visit[i].x > 0 && game_status == 1) // game status is fire must have more range
					start_x = node_visit[i].x-1;
				else
					start_x = node_visit[i].x;
				
				if (node_visit[i].x < DIAMOND_X_NUM-1 && game_status == 1)
					end_x = node_visit[i].x+1;
				else
					end_x = node_visit[i].x;

				for (x=start_x; x<=end_x; x++)
				{
					if (mapping_info[y][x].influence_area == 0)
					{
						mapping_info[y][x].influence_area = 1;
						mapping_info[y][x].block_type = 0; // for click confirm re-search using
						same_color_count_data[mapping_info[y][x].group_set_number]--;
					}
				}
				if ((y+1)<DIAMOND_Y_NUM && game_status == 1) // add influence range under bottom line
				{
					if (mapping_info[y+1][node_visit[i].x].influence_area == 0)
					{
						mapping_info[y+1][node_visit[i].x].influence_area = 1;
						mapping_info[y+1][node_visit[i].x].block_type = 0; // for click confirm re-search using
						same_color_count_data[mapping_info[y+1][node_visit[i].x].group_set_number]--;
					}
				}
			}
		}
	}

	POINT newCursor;
	newCursor.x = 0;
	newCursor.y = 0;
	if (normal_find || diamond_find)
	{
		MUST_UPDATE_DATA_FROM_DC_PIC = FALSE;

		pre_click = cur_click;
		newCursor.x = cur_click.x*DIAMOND_SIZE+(DIAMOND_SIZE/2);
		newCursor.y = cur_click.y*DIAMOND_SIZE+(DIAMOND_SIZE/2);
		SetCursorPos(GAME_START.left+newCursor.x, GAME_START.top+newCursor.y);
		//mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, rc.left+GAME_START_X+newCursor.x, rc.top+GAME_START_Y+newCursor.y, 0, 0);
		mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
	}
	else
	{
		pre_click.x = 0;
		pre_click.y = 0;
		MUST_UPDATE_DATA_FROM_DC_PIC = TRUE;
		Sleep(game_delay_control);
	}

	if (diamond_find)
	{
		//Sleep(933); // about 28 frame of 30fps
		MUST_UPDATE_DATA_FROM_DC_PIC = TRUE;
		//Sleep(game_delay_control);
	}

	if (MUST_UPDATE_DATA_FROM_DC_PIC)
	{
		// reset
		for (y=0; y<DIAMOND_Y_NUM; y++)
		{
			for (x=0; x<DIAMOND_X_NUM; x++)
			{
				mapping_info[y][x].influence_area = 0;
				mapping_info[y][x].searched = 0;
				mapping_info[y][x].re_check = 0;
			}
		}

		for (i=0; i<DIAMOND_Y_NUM*DIAMOND_X_NUM; i++)
			same_color_count_data[i] = 0;
		// reset end
	}

	Sleep(2);
	ProcessMessages();
}

//
//  FUNCTION: WndProc(HWND, unsigned, WORD, LONG)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc, hdcSrc, hdcScreenshot;
	TCHAR szHello[MAX_LOADSTRING];
	LoadString(hInst, IDS_HELLO, szHello, MAX_LOADSTRING);
	HBITMAP hbmpTemp;
	int i;

	switch (message) 
	{
		case WM_CREATE:
			RECT rctDesktop;
			SystemParametersInfo(SPI_GETWORKAREA, NULL, &rctDesktop, NULL);
			MoveWindow(hWnd, (rctDesktop.right-rctDesktop.left)-WINDOW_WIDTH, (rctDesktop.bottom-rctDesktop.top)-WINDOW_HEIGHT, WINDOW_WIDTH, WINDOW_HEIGHT, TRUE);

			RegisterHotKey(hWnd, 100, NULL, VK_F1);
			RegisterHotKey(hWnd, 101, NULL, VK_F2);
			RegisterHotKey(hWnd, 102, NULL, VK_F3);
			RegisterHotKey(hWnd, 103, NULL, VK_F4);
			RegisterHotKey(hWnd, 104, NULL, VK_F8);

			GAME_START.left = -1;
			GAME_START.top = -1;
			GAME_START.right = -1;
			GAME_START.bottom = -1;

			// set golden default
			block_sample_info[1].HSI_H_max = 360.0;
			block_sample_info[1].HSI_H_min = 351.6;
			block_sample_info[1].HSI_H_max_step2 = 5.0;
			block_sample_info[1].HSI_H_min_step2 = 0.0;

			block_sample_info[2].HSI_H_max = 53.8;
			block_sample_info[2].HSI_H_min = 30.8;

			block_sample_info[3].HSI_H_max = 120.1;
			block_sample_info[3].HSI_H_min = 100.1;

			block_sample_info[4].HSI_H_max = 232.2;
			block_sample_info[4].HSI_H_min = 212.2;

			block_sample_info[5].HSI_H_max = 280.9;
			block_sample_info[5].HSI_H_min = 265.9;

			game_check_diamond = TRUE;
			CheckMenuItem(GetMenu(hWnd), IDM_CHECKDIANOND, MF_CHECKED);
			
			game_delay_control = 6;
			CheckMenuItem(GetMenu(hWnd), IDM_DELAYCONTROL_30MS, MF_CHECKED);
			break;
		case WM_KEYDOWN:
			switch (wParam)
			{
			case VK_F5:
				break;
			}
			break;

		case WM_HOTKEY:
			switch (wParam)
			{
			case 100:
				POINT pt;
				RECT rc;
				GetCursorPos(&pt);
				
				//EnumWindows(WorkerProc, NULL);
				//hWnd_target = WindowsList_hWnd;
				//GetWindowRect(hWnd_target, &rc);

				GAME_START.left = pt.x;// - rc.left;
				GAME_START.top = pt.y;// - rc.top;
				InvalidateRect(hWnd, NULL, false);
				break;

			case 101:
				EnumWindows(WorkerProc, NULL);

				hWnd_target = WindowsList_hWnd;
				EnumChildWindows(hWnd_target, EnumChildProc, NULL);
				if (GAME_START.left == -1 && GAME_START.top == -1 && GAME_START.right == -1 && GAME_START.bottom == -1)
					MessageBox(hWnd, _T("Must assign game window of left-top coordination."), _T("ERROR"), MB_OK);
				else if (hWnd_target == NULL)
					MessageBox(hWnd, _T("cant find any window"), _T("ERROR"), MB_OK);
				else
					Sample();
				break;

			case 102:
				game_loop = TRUE;

				//hWnd_target = FindWindow(TEXT("NotePad"), NULL); // the window can't be min // ::FindWindow( 0, _T( "Calculator" ));
				EnumWindows(WorkerProc, NULL);

				hWnd_target = WindowsList_hWnd;
				EnumChildWindows(hWnd_target, EnumChildProc, NULL);
				if (GAME_START.left == -1 && GAME_START.top == -1 && GAME_START.right == -1 && GAME_START.bottom == -1)
					MessageBox(hWnd, _T("Must assign game window of left-top coordination."), _T("ERROR"), MB_OK);
				else if (hWnd_target == NULL)
					MessageBox(hWnd, _T("cant find any window"), _T("ERROR"), MB_OK);
				else
				{
					MUST_UPDATE_DATA_FROM_DC_PIC = TRUE;
					while(game_loop)
					{
						MainLoop(hWnd);
					}
				}
				break;
			case 103:
				game_loop = FALSE;
				break;
			case 104:
				game_check_diamond = !game_check_diamond;
				if (game_check_diamond)
					CheckMenuItem(GetMenu(hWnd), IDM_CHECKDIANOND, MF_CHECKED);
				else
					CheckMenuItem(GetMenu(hWnd), IDM_CHECKDIANOND, MF_UNCHECKED);
				break;
			}
			break;
		case WM_COMMAND:
			wmId    = LOWORD(wParam); 
			wmEvent = HIWORD(wParam); 
			// Parse the menu selections:
			switch (wmId)
			{
				case IDM_SAMPLE:
					// http://msdn.microsoft.com/en-us/library/ybk95axf%28v=vs.80%29.aspx
					TCHAR buffer[100];
					_stprintf(buffer, _T("red: %.02f\nyellow: %.02f\ngreen: %.02f\nblue: %.02f\npurple: %.02f"), 
						block_sample_info[1].HSI_H_min, 
						block_sample_info[2].HSI_H_min, 
						block_sample_info[3].HSI_H_min, 
						block_sample_info[4].HSI_H_min, 
						block_sample_info[5].HSI_H_min);
					MessageBox(hWnd, buffer, _T("Info"), MB_OK);
					
					break;
				case IDM_CHECKDIANOND:
					game_check_diamond = !game_check_diamond;
					if (game_check_diamond)
						CheckMenuItem(GetMenu(hWnd), IDM_CHECKDIANOND, MF_CHECKED);
					else
						CheckMenuItem(GetMenu(hWnd), IDM_CHECKDIANOND, MF_UNCHECKED);

					break;
				case IDM_DELAYCONTROL_5MS:
				case IDM_DELAYCONTROL_10MS:
				case IDM_DELAYCONTROL_15MS:
				case IDM_DELAYCONTROL_20MS:
				case IDM_DELAYCONTROL_25MS:
				case IDM_DELAYCONTROL_30MS:
					game_delay_control = wmId - 32774;
					CheckMenuItem(GetMenu(hWnd), IDM_DELAYCONTROL_5MS, MF_UNCHECKED);
					CheckMenuItem(GetMenu(hWnd), IDM_DELAYCONTROL_10MS, MF_UNCHECKED);
					CheckMenuItem(GetMenu(hWnd), IDM_DELAYCONTROL_15MS, MF_UNCHECKED);
					CheckMenuItem(GetMenu(hWnd), IDM_DELAYCONTROL_20MS, MF_UNCHECKED);
					CheckMenuItem(GetMenu(hWnd), IDM_DELAYCONTROL_25MS, MF_UNCHECKED);
					CheckMenuItem(GetMenu(hWnd), IDM_DELAYCONTROL_30MS, MF_UNCHECKED);

					if (game_delay_control == 1)
						CheckMenuItem(GetMenu(hWnd), IDM_DELAYCONTROL_5MS, MF_CHECKED);
					else if (game_delay_control == 2)
						CheckMenuItem(GetMenu(hWnd), IDM_DELAYCONTROL_10MS, MF_CHECKED);
					else if (game_delay_control == 3)
						CheckMenuItem(GetMenu(hWnd), IDM_DELAYCONTROL_15MS, MF_CHECKED);
					else if (game_delay_control == 4)
						CheckMenuItem(GetMenu(hWnd), IDM_DELAYCONTROL_20MS, MF_CHECKED);
					else if (game_delay_control == 5)
						CheckMenuItem(GetMenu(hWnd), IDM_DELAYCONTROL_25MS, MF_CHECKED);
					else if (game_delay_control == 6)
						CheckMenuItem(GetMenu(hWnd), IDM_DELAYCONTROL_30MS, MF_CHECKED);
					game_delay_control *= 5;

					break;

				case IDM_ABOUT:
				   DialogBox(hInst, (LPCTSTR)IDD_ABOUTBOX, hWnd, (DLGPROC)About);
				   break;
				case IDM_EXIT:
				   DestroyWindow(hWnd);
				   break;
				default:
				   return DefWindowProc(hWnd, message, wParam, lParam);
			}
			break;
		case WM_PAINT:
			{
			hdc = BeginPaint(hWnd, &ps);
			// TODO: Add any drawing code here...
			//hdcSrc = GetWindowDC(hWnd_target);
			//HDC hdcMem = CreateCompatibleDC(hdcSrc);
			//HBITMAP bmpMem = CreateCompatibleBitmap(hdcSrc, game_width, game_height);
			//SelectObject(hdcMem, bmpMem);
			//BitBlt(hdcMem, 0, 0, game_width, game_height, hdcSrc, GAME_START.left, GAME_START.top, SRCCOPY);

			//RECT rt;
			//GetClientRect(hWnd, &rt);
			//DrawText(hdc, szHello, _tcslen(szHello), &rt, DT_CENTER);
			//BitBlt(hdc, 0, 0, game_width, game_height, hdcMem, 0, 0, SRCCOPY);
			EndPaint(hWnd, &ps);
			}
			break;
		case WM_DESTROY:
			UnregisterHotKey(hWnd, 100);
			UnregisterHotKey(hWnd, 101);
			UnregisterHotKey(hWnd, 102);
			UnregisterHotKey(hWnd, 103);
			UnregisterHotKey(hWnd, 104);
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
   }
   return 0;
}

// Mesage handler for about box.
LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_INITDIALOG:
				return TRUE;

		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
			{
				EndDialog(hDlg, LOWORD(wParam));
				return TRUE;
			}
			break;
	}
    return FALSE;
}
