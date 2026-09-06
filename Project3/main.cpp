
// +++ main.cpp +++

#define STRICT
#define _CRT_NON_CONFORMING_SWPRINTFS
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <tchar.h>

#include <gl\\gl.h>
#include <gl\\glu.h>

#pragma comment (linker, "/defaultlib:opengl32.lib")
#pragma comment (linker, "/defaultlib:glu32.lib")

#include "resource.h"

#ifndef _countof
#define _countof (a) (sizeof (a) / sizeof (a [0]))
#endif

HINSTANCE g_hApp = nullptr;
LPCTSTR g_szAppName = _T("Пользовательское сечение с диалогом свойств");

HWND g_hWindow = nullptr;
HDC g_hDC = nullptr;
HGLRC g_hGLRC = nullptr;
LPCTSTR g_szWndClass = _T("WcOglBoxpipe"),
g_szTitle = g_szAppName;

int g_wndWidth = -1, g_wndHeight = -1;

double g_angle = 0;
const double g_angIncr = 1;

GLUquadricObj* g_pGluQuadObj = nullptr;

#define M_PI 3.1415926

//
// свойства
//

GLubyte pattern[128] = { 0 };

struct Color
{
	LPCTSTR szName;
	COLORREF value;

	Color() : szName(_T("")), value(0) {}
	Color(LPCTSTR szName_, COLORREF value_) : szName(szName_), value(value_) {}
};

Color g_colors[] =
{
	Color(_T("- Красный"), RGB(255, 0, 0)),  // 0x0000FF
	Color(_T("- Зелёный"), RGB(0, 255, 0)),  // 0x00FF00
	Color(_T("- Жёлтый"),  RGB(255, 255, 0)),  // 0x00FFFF
};

struct BoxpipeProps
{
	double W, H, T, L;
	int iColor;

	BoxpipeProps() : W(15.), H(10.), T(1.), L(30.), iColor(1) {}

	void CorrectValues()
	{
		if (W < 1e-5)
			W = 1e-5;
		if (H < 1e-5)
			H = 1e-5;
		if (L < 1e-5)
			L = 1e-5;
		if (T < 1e-5)
			T = 1e-5;
		if (T * 2 > W)
			T = W / 2;
		if (T * 2 > H)
			T = H / 2;

		if (iColor < 0 || iColor >= _countof(g_colors))
			iColor = 0;
	}
};

BoxpipeProps g_boxpipeProps;


// размер сцены
double g_sceneWidth = 50.;


LRESULT CALLBACK MainWindowProc(HWND, UINT, WPARAM, LPARAM);
BOOL MainOnCreate(HWND, LPCREATESTRUCT);
BOOL MainOnCommand(int, HWND, UINT);
BOOL MainOnSize(int width, int height);
BOOL MainOnPaint();
BOOL MainOnSize(UINT, int, int);
BOOL MainOnDestroy();

INT_PTR CALLBACK DlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

BOOL InitApp(void);
void UninitApp(void);

int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR, int);


// установка формата пикселей
BOOL SetPixelFormat(HDC dc)
{
	PIXELFORMATDESCRIPTOR pfd;

	ZeroMemory(&pfd, sizeof(PIXELFORMATDESCRIPTOR));

	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;
	pfd.cAlphaBits = 24;
	pfd.cStencilBits = 0;
	pfd.cDepthBits = 24;
	pfd.cAccumBits = 0;
	pfd.iLayerType = PFD_MAIN_PLANE;

	int pf = ChoosePixelFormat(dc, &pfd);
	SetPixelFormat(dc, pf, &pfd);

	return !(pfd.dwFlags & PFD_NEED_PALETTE);
}  //SetPixelFormat

 //
 // рисование трубы
 //

void create_pattern() {
	for (int row = 0; row < 32; row++) {
		for (int col = 0; col < 32; col++) {
			if ((row + col) % 4 < 2) {  // Линии под 45 градусов
				int byteIndex = row * 4 + (col / 8);
				int bitIndex = 7 - (col % 8);
				pattern[byteIndex] |= (1 << bitIndex);
			}
		}
	}
}

void create_hatching(bool fFill, double w, double w1, double h, double t, double l, int mode, double color) {
	
	l *= mode;
	GLfloat c = color;
	glBegin(GL_QUAD_STRIP);
	glColor3f(color, color, color);
	glVertex3d(-w, h, l);
	glVertex3d(-w1, h, l);
	glVertex3d(-w, -h, l);
	glVertex3d(-w1, -h, l);
	glEnd();

	glBegin(GL_QUAD_STRIP);
	glColor3f(color, color, color);
	glVertex3d(w1, h, l);
	glVertex3d(w, h, l);
	glVertex3d(w1, -h, l);
	glVertex3d(w, -h, l);
	glEnd();

	glBegin(GL_QUAD_STRIP);
	glColor3f(color, color, color);
	glVertex3d(-w1, -t, l);
	glVertex3d(-w1, t, l);
	glVertex3d(w1, -t, l);
	glVertex3d(w1, t, l);
	glEnd();

	glBegin(GL_QUAD_STRIP);
	glColor3f(color, color, color);
	glVertex3d(-t, h / 2, l);
	glVertex3d(t, h / 2, l);
	glVertex3d(-t, -h / 2, l);
	glVertex3d(t, -h / 2, l);
	glEnd();

}

void DrawBoxpipe(bool fFill = true)
{
	float r = GetRValue(g_colors[g_boxpipeProps.iColor].value) / 255.f / 2,
		g = GetGValue(g_colors[g_boxpipeProps.iColor].value) / 255.f / 2,
		b = GetBValue(g_colors[g_boxpipeProps.iColor].value) / 255.f / 2;

	double w = g_boxpipeProps.W / 2,
		h = g_boxpipeProps.H / 2,
		l = g_boxpipeProps.L / 2,
		t = g_boxpipeProps.T / 2;

	double w1 = w - g_boxpipeProps.T,
		h1 = h - g_boxpipeProps.T;

	glPolygonMode(GL_FRONT, fFill ? GL_FILL : GL_LINE);
	glPolygonMode(GL_BACK, GL_LINE);
	glEnable(GL_DEPTH_TEST);

	// Оболочка
	glBegin(GL_QUADS);

	glColor3d(r * 1.5, g * 1.5, b * 1.5);
	glVertex3d(-w1, h, l);
	glVertex3d(-w1, h, -l);           // 1
	glVertex3d(-w, h, -l);
	glVertex3d(-w, h, l);

	glColor3d(r, g, b);
	glVertex3d(-w, h, l);
	glVertex3d(-w, h, -l);           // 2
	glVertex3d(-w, -h, -l);
	glVertex3d(-w, -h, l);

	glColor3d(r * 1.5, g * 1.5, b * 1.5);
	glVertex3d(-w, -h, l);
	glVertex3d(-w, -h, -l);           // 3
	glVertex3d(-w1, -h, -l);
	glVertex3d(-w1, -h, l);

	glColor3d(r, g, b);
	glVertex3d(-w1, -h, l);
	glVertex3d(-w1, -h, -l);           // 4
	glVertex3d(-w1, -t, -l);
	glVertex3d(-w1, -t, l);
	
	glColor3d(r * 1.5, g * 1.5, b * 1.5);
	glVertex3d(-w1, -t, l);
	glVertex3d(-w1, -t, -l);           // 5
	glVertex3d(-t, -t, -l);
	glVertex3d(-t, -t, l);

	glColor3d(r, g, b);
	glVertex3d(-t, -t, l);
	glVertex3d(-t, -t, -l);           // 6
	glVertex3d(-t, -h/2, -l);
	glVertex3d(-t, -h/2, l);

	glColor3d(r * 1.5, g * 1.5, b * 1.5);
	glVertex3d(-t, -h/2, l);
	glVertex3d(-t, -h/2, -l);           // 7
	glVertex3d(t, -h/2, -l);
	glVertex3d(t, -h/2, l);

	glColor3d(r, g, b);
	glVertex3d(t, -h/2, l);
	glVertex3d(t, -h/2, -l);           // 8
	glVertex3d(t, -t, -l);
	glVertex3d(t, -t, l);

	glColor3d(r * 1.5, g * 1.5, b * 1.5);
	glVertex3d(t, -t, l);
	glVertex3d(t, -t, -l);           // 9
	glVertex3d(w1, -t, -l);
	glVertex3d(w1, -t, l);

	glColor3d(r, g, b);
	glVertex3d(w1, -t, l);
	glVertex3d(w1, -t, -l);           // 10
	glVertex3d(w1, -h, -l);
	glVertex3d(w1, -h, l);

	glColor3d(r * 1.5, g * 1.5, b * 1.5);
	glVertex3d(w1, -h, l);
	glVertex3d(w1, -h, -l);           // 11
	glVertex3d(w, -h, -l);
	glVertex3d(w, -h, l);

	glColor3d(r, g, b);
	glVertex3d(w, -h, l);
	glVertex3d(w, -h, -l);           // 12
	glVertex3d(w, h, -l);
	glVertex3d(w, h, l);

	glColor3d(r * 1.5, g * 1.5, b * 1.5);
	glVertex3d(w, h, l);
	glVertex3d(w, h, -l);           // 13
	glVertex3d(w1, h, -l);
	glVertex3d(w1, h, l);

	glColor3d(r, g, b);
	glVertex3d(w1, h, l);
	glVertex3d(w1, h, -l);           // 14
	glVertex3d(w1, t, -l);
	glVertex3d(w1, t, l);

	glColor3d(r * 1.5, g * 1.5, b * 1.5);
	glVertex3d(w1, t, l);
	glVertex3d(w1, t, -l);           // 15
	glVertex3d(t, t, -l);
	glVertex3d(t, t, l);

	glColor3d(r, g, b);
	glVertex3d(t, t, l);
	glVertex3d(t, t, -l);           // 16
	glVertex3d(t, h/2, -l);
	glVertex3d(t, h/2, l);

	glColor3d(r * 1.5, g * 1.5, b * 1.5);
	glVertex3d(t, h/2, l);
	glVertex3d(t, h/2, -l);           // 17
	glVertex3d(-t, h/2, -l);
	glVertex3d(-t, h/2, l);

	glColor3d(r, g, b);
	glVertex3d(-t, h / 2, l);
	glVertex3d(-t, h / 2, -l);           // 18
	glVertex3d(-t, t, -l);
	glVertex3d(-t, t, l);

	glColor3d(r * 1.5, g * 1.5, b * 1.5);
	glVertex3d(-t, t, l);
	glVertex3d(-t, t, -l);           // 19
	glVertex3d(-w1, t, -l);
	glVertex3d(-w1, t, l);

	glColor3d(r, g, b);
	glVertex3d(-w1, t, l);
	glVertex3d(-w1, t, -l);           // 20
	glVertex3d(-w1, h, -l);
	glVertex3d(-w1, h, l);	

	glEnd();

	// сечение 1

	glPolygonMode(GL_FRONT, GL_LINE);
	glPolygonMode(GL_BACK, fFill ? GL_FILL : GL_LINE);
	glEnable(GL_POLYGON_STIPPLE);
	glPolygonStipple(pattern);
	create_hatching(fFill, w, w1, h, t, l, 1, 0);   // штриховка
	glDisable(GL_POLYGON_STIPPLE);
	create_hatching(fFill, w, w1, h, t, l, 1, 0.5); // фон

	// сечение 2
	glPolygonMode(GL_BACK, GL_LINE);
	glPolygonMode(GL_FRONT, fFill ? GL_FILL : GL_LINE);
	glEnable(GL_POLYGON_STIPPLE);
	glPolygonStipple(pattern);
	create_hatching(fFill, w, w1, h, t, l, -1, 0);  // штриховка
	glDisable(GL_POLYGON_STIPPLE);
	create_hatching(fFill, w, w1, h, t, l, -1, 0.5); // фон
	

}  //DrawBoxpipe

 //
 // рисование осей
 //
void DrawAxes(double dAxisSize)
{
	glPolygonMode(GL_FRONT, GL_FILL);
	glPolygonMode(GL_BACK, GL_LINE);
	gluQuadricDrawStyle(g_pGluQuadObj, GLU_FILL);
	glDisable(GL_DEPTH_TEST);

	glBegin(GL_LINES);
	glColor3d(1, 0, 0);
	glVertex3d(-dAxisSize / 2, 0, 0);
	glVertex3d(dAxisSize, 0, 0);
	glColor3d(0, 1, 0);
	glVertex3d(0, -dAxisSize / 2, 0);
	glVertex3d(0, dAxisSize, 0);
	glColor3d(0, 0, 1);
	glVertex3d(0, 0, -dAxisSize / 2);
	glVertex3d(0, 0, dAxisSize);
	glEnd();

	glPushMatrix();
	glColor3d(1, 0, 0);
	glTranslated(dAxisSize, 0, 0);
	glRotated(90, 0, 1, 0);
	gluCylinder(g_pGluQuadObj, dAxisSize / 10, 0, dAxisSize / 5, 32, 1);
	glPopMatrix();

	glPushMatrix();
	glColor3d(0, 1, 0);
	glTranslated(0, dAxisSize, 0);
	glRotated(-90, 1, 0, 0);
	gluCylinder(g_pGluQuadObj, dAxisSize / 10, 0, dAxisSize / 5, 32, 1);
	glPopMatrix();

	glPushMatrix();
	glColor3d(0, 0, 1);
	glTranslated(0, 0, dAxisSize);
	gluCylinder(g_pGluQuadObj, dAxisSize / 10, 0, dAxisSize / 5, 32, 1);
	glPopMatrix();

}  //DrawAxes

 //
 // рисование
 //
void Draw()
{
	double maxWH = g_boxpipeProps.W;
	if (maxWH < g_boxpipeProps.H)
		maxWH = g_boxpipeProps.H;

	double minWHL = g_boxpipeProps.W;
	if (minWHL > g_boxpipeProps.H)
		minWHL = g_boxpipeProps.H;
	if (minWHL > g_boxpipeProps.L)
		minWHL = g_boxpipeProps.L;

	double maxWHL = g_boxpipeProps.L;
	if (maxWHL < g_boxpipeProps.W)
		maxWHL = g_boxpipeProps.W;
	if (maxWHL < g_boxpipeProps.H)
		maxWHL = g_boxpipeProps.H;

	GLsizei viewportSize = g_wndHeight; // g_wndWidth > wndHeight ? wndHeight : g_wndWidth;

	glViewport(0, 0, viewportSize, viewportSize);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glOrtho(-g_sceneWidth / 2, g_sceneWidth / 2, -g_sceneWidth / 2, g_sceneWidth / 2, -g_sceneWidth / 2, g_sceneWidth / 2);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// основной
	glPushMatrix();
	glRotated(30., 1., 0., 0.);
	glRotated(g_angle, 0., 1., 0.);
	DrawBoxpipe(); // труба
	DrawAxes(minWHL / 2); // оси
	glPopMatrix();
	//

	glViewport(viewportSize, 0, viewportSize, viewportSize);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	double gap = maxWH / 10,
		size = maxWH + maxWHL + gap;

	double scw2 = size / 2 * 1.2;
	glOrtho(-scw2, scw2, -scw2, scw2, -scw2, scw2);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// нарисовать проекции трубы

	double _05size = size / 2,
		_05maxWH = maxWH / 2,
		_05L = maxWHL / 2;

	glPushMatrix();        // вид спереди
	glTranslated(-_05size + _05maxWH, _05size - _05maxWH, 0.);
	DrawBoxpipe();
	DrawAxes(minWHL / 2);
	glPopMatrix();

	glPushMatrix();       // вид сверху
	glTranslated(-_05size + _05maxWH, _05size - maxWH - _05L - gap, 0.);
	glRotated(90., 1., 0., 0.);
	DrawBoxpipe();
	DrawAxes(minWHL / 2);
	glPopMatrix();

	glPushMatrix();       // вид сбоку
	glTranslated(-_05size + maxWH + _05L + gap, _05size - _05maxWH, 0.);
	glRotated(90., 0., 1., 0.);
	DrawBoxpipe();
	DrawAxes(minWHL / 2);
	glPopMatrix();

	glPushMatrix();      // дополнительный вид
	glTranslated(-_05size + maxWH + _05L + gap, _05size - maxWH - _05L - gap, 0.);
	glRotated(30., 1., 0., 0.);
	glRotated(int(g_angle / 22.5) * 22.5, 0, -1., 0.);
	glScaled(0.7, 0.7, 0.7);
	DrawBoxpipe(false);
	DrawAxes(minWHL * 0.7 / 2);
	glPopMatrix();

	glFinish();
	SwapBuffers(g_hDC);
}  //Draw


LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CREATE:
		return MainOnCreate(hwnd, (LPCREATESTRUCT)lParam);

	case WM_SIZE:
		return MainOnSize(LOWORD(lParam), HIWORD(lParam));

	case WM_PAINT:
		return MainOnPaint();

	case WM_DESTROY:
		return MainOnDestroy();

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case ID_MENU_DIALOG:
			DialogBox(g_hApp, MAKEINTRESOURCE(IDD_DIALOG1), hwnd, DLGPROC(DlgProc));
			return 0L;
		}
		return 0L;

	case WM_TIMER:
		if ((g_angle = g_angle + g_angIncr) >= 360)
			g_angle -= 360;
		InvalidateRect(g_hWindow, nullptr, FALSE);
		break;

	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0L;
}  //MainWindowProc

 //
 // WM_CREATE
 //
BOOL MainOnCreate(HWND hwnd, LPCREATESTRUCT p_cs)
{
	// получить контекст окна
	// (значение дескриптора не меняется, т.к. класс окна имеет стиль CS_OWNDC)
	g_hDC = GetDC(hwnd);

	// установить формат пикселей
	SetPixelFormat(g_hDC);

	// создать контекст воспроизведения OpenGL, сделать его текущим
	g_hGLRC = wglCreateContext(g_hDC);
	wglMakeCurrent(g_hDC, g_hGLRC);

	// установить таймер
	SetTimer(hwnd, 0, 10, 0);

	// создать объект OpenGL для рисования осей координат
	g_pGluQuadObj = gluNewQuadric();
	assert(g_pGluQuadObj);

	create_pattern();

	CreateDialog(g_hApp, MAKEINTRESOURCE(IDD_DIALOG1), hwnd, DlgProc);

	return TRUE;
}  //MainOnCreate

 //
 // WM_SIZE
 //
BOOL MainOnSize(int width, int height)
{
	g_wndWidth = width;
	g_wndHeight = height;
	return TRUE;
}

//
// WM_PAINT
//
BOOL MainOnPaint()
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(g_hWindow, &ps);
	Draw();
	EndPaint(g_hWindow, &ps);
	return TRUE;
}


BOOL MainOnDestroy()
{
	gluDeleteQuadric(g_pGluQuadObj);
	g_pGluQuadObj = nullptr;

	// удалить контекст воспроизведения OpenGL
	wglMakeCurrent(nullptr, nullptr);
	if (g_hGLRC)
		wglDeleteContext(g_hGLRC);

	PostQuitMessage(0);

	return TRUE;
}


void SetDlgItemReal(HWND hDlg, int id, double val)
{
	TCHAR buff[256];
	_stprintf(buff, _T("%g"), val);
	SetDlgItemText(hDlg, id, buff);
}

bool GetDlgItemReal(HWND hDlg, int id, double& val)
{
	TCHAR buff[512];
	GetDlgItemText(hDlg, id, buff, _countof(buff));

	LPTSTR err = nullptr;
	val = _tcstod(buff, &err);
	return nullptr == err;
}

INT_PTR CALLBACK DlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	static bool s_fInit;
	double w = 0, h = 0, t = 0, l = 0;
	double k = 0; // масштабный коэффициент

	switch (message)
	{
	case WM_INITDIALOG:
	{
		s_fInit = true;

		SetDlgItemReal(hDlg, IDC_EDIT1, g_boxpipeProps.W);
		SetDlgItemReal(hDlg, IDC_EDIT2, g_boxpipeProps.H);
		SetDlgItemReal(hDlg, IDC_EDIT3, g_boxpipeProps.T);
		SetDlgItemReal(hDlg, IDC_EDIT4, g_boxpipeProps.L);

		for (int i = 0; i < _countof(g_colors); i++)
			SendDlgItemMessage(hDlg, IDC_LIST1, LB_ADDSTRING, 0, (LPARAM)g_colors[i].szName);

		SendDlgItemMessage(hDlg, IDC_LIST1, LB_SETCURSEL, g_boxpipeProps.iColor, 0);

		s_fInit = false;
	}
	return (INT_PTR)TRUE;

	case WM_COMMAND:
	{
		if (s_fInit)
			return (INT_PTR)FALSE;

		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}

		if (HIWORD(wParam) == EN_CHANGE
			|| HIWORD(wParam) == LBN_SELCHANGE)
		{
			GetDlgItemReal(hDlg, IDC_EDIT1, w);
			GetDlgItemReal(hDlg, IDC_EDIT2, h);
			GetDlgItemReal(hDlg, IDC_EDIT3, t);
			GetDlgItemReal(hDlg, IDC_EDIT4, l);

			if (l > 30) {
				k = 30 / l;
				g_boxpipeProps.W = w * k;
				g_boxpipeProps.H = h * k;
				g_boxpipeProps.T = t * k;
				g_boxpipeProps.L = l * k;
			}
			else {
				g_boxpipeProps.W = w;
				g_boxpipeProps.H = h;
				g_boxpipeProps.T = t;
				g_boxpipeProps.L = l;
			}

			g_boxpipeProps.iColor = SendDlgItemMessage(hDlg, IDC_LIST1, LB_GETCURSEL, 0, 0);

			g_boxpipeProps.CorrectValues();

			InvalidateRect(g_hWindow, nullptr, FALSE);
		}
	}
	break;

	case WM_CLOSE:
		DestroyWindow(g_hWindow);
		break;
	}
	return (INT_PTR)FALSE;
}


BOOL InitApp()
{
	WNDCLASSEX wce;
	ZeroMemory(&wce, sizeof(WNDCLASSEX));
	wce.cbSize = sizeof(WNDCLASSEX);
	wce.hInstance = g_hApp;
	wce.style = CS_VREDRAW | CS_HREDRAW | CS_OWNDC;
	wce.lpfnWndProc = MainWindowProc;
	wce.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wce.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wce.lpszClassName = g_szWndClass;
	if (!RegisterClassEx(&wce))
		return FALSE;

	SetLastError(0);
	g_hWindow = CreateWindow(g_szWndClass, g_szTitle,
		WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX
		| WS_THICKFRAME | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
		50, 50, 900, 400, nullptr, LoadMenu(g_hApp, MAKEINTRESOURCE(IDR_MENU1)), g_hApp, nullptr);
	if (!g_hWindow)
	{
		DWORD err = GetLastError();
		return FALSE;
	}

	ShowWindow(g_hWindow, SW_SHOW);
	UpdateWindow(g_hWindow);

	return TRUE;
}


void UninitApp()
{
	UnregisterClass(g_szWndClass, g_hApp);
}

int APIENTRY WinMain(HINSTANCE hApp_, HINSTANCE, LPSTR, int)
{
	g_hApp = hApp_;

	if (InitApp())
	{
		MSG msg;
		while (GetMessage(&msg, nullptr, 0, 0))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	UninitApp();

	return 0;
}

// --- main.cpp ---