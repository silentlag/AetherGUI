#pragma once
#include "Framework.h"
#include "Theme.h"


class Renderer {
public:
	ID2D1Factory* pFactory = nullptr;
	ID2D1HwndRenderTarget* pRT = nullptr;
	IDWriteFactory* pDWriteFactory = nullptr;
	ID2D1StrokeStyle* pStrokeRound = nullptr;
	IWICImagingFactory* pWICFactory = nullptr;
	ID2D1Bitmap* pLogoBitmap = nullptr;

	IDWriteTextFormat* pFontTitle = nullptr;
	IDWriteTextFormat* pFontHeading = nullptr;
	IDWriteTextFormat* pFontBody = nullptr;
	IDWriteTextFormat* pFontSmall = nullptr;
	IDWriteTextFormat* pFontMono = nullptr;
	IDWriteTextFormat* pFontIcon = nullptr;

	ID2D1SolidColorBrush* pBrush = nullptr;
	std::wstring privateFontPath;
	bool privateFontLoaded = false;
	bool comInitialized = false;

	HWND hWnd = nullptr;
	float dpiScale = 1.0f;
	float deltaTime = 0.016f;

	
	bool Initialize(HWND hwnd);
	
	void Shutdown();
	
	void Resize(UINT width, UINT height);

	bool SetDpiScale(float scale);

	
	void BeginFrame();
	
	void EndFrame();

	void FillRect(float x, float y, float w, float h, D2D1_COLOR_F color);
	void FillRoundedRect(float x, float y, float w, float h, float radius, D2D1_COLOR_F color);
	void DrawRoundedRect(float x, float y, float w, float h, float radius, D2D1_COLOR_F color, float strokeWidth = 1.0f);
	void FillCircle(float cx, float cy, float r, D2D1_COLOR_F color);
	void DrawCircle(float cx, float cy, float r, D2D1_COLOR_F color, float strokeWidth = 1.0f);
	void DrawLine(float x1, float y1, float x2, float y2, D2D1_COLOR_F color, float strokeWidth = 1.0f);
	void DrawBitmap(ID2D1Bitmap* bitmap, float x, float y, float w, float h, float opacity = 1.0f);
	void DrawBitmapTinted(ID2D1Bitmap* bitmap, float x, float y, float w, float h, D2D1_COLOR_F tint, float opacity = 1.0f);

	enum TextAlign { AlignLeft, AlignCenter, AlignRight };
	void DrawText(const wchar_t* text, float x, float y, float w, float h,
		D2D1_COLOR_F color, IDWriteTextFormat* font, TextAlign align = AlignLeft);
	void DrawTextSimple(const wchar_t* text, float x, float y,
		D2D1_COLOR_F color, IDWriteTextFormat* font);

	void FillRectGradientV(float x, float y, float w, float h,
		D2D1_COLOR_F topColor, D2D1_COLOR_F bottomColor);
	void FillRectGradientH(float x, float y, float w, float h,
		D2D1_COLOR_F leftColor, D2D1_COLOR_F rightColor);
	void FillRoundedRectGradientH(float x, float y, float w, float h, float radius,
		D2D1_COLOR_F leftColor, D2D1_COLOR_F rightColor);

private:
	
	void TryLoadPrivateFont();
	
	void TryLoadLogoBitmap();
	bool LoadBitmapFromFile(const std::wstring& path, ID2D1Bitmap** bitmap, UINT maxSize = 0);
	
	bool CreateTextFormats();
};
