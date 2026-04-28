#include "Renderer.h"

static bool FileExistsW(const std::wstring& path) {
	DWORD attrs = GetFileAttributesW(path.c_str());
	return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

static std::wstring GetExecutableDirectoryW() {
	wchar_t exePath[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, exePath, MAX_PATH);
	std::wstring dir(exePath);
	size_t slash = dir.find_last_of(L"\\/");
	if (slash != std::wstring::npos)
		dir = dir.substr(0, slash + 1);
	return dir;
}

void Renderer::TryLoadPrivateFont() {
	std::wstring exeDir = GetExecutableDirectoryW();
	std::wstring candidates[] = {
		exeDir + L"font.ttf",
		exeDir + L"..\\font.ttf",
		exeDir + L"..\\..\\font.ttf",
		exeDir + L"..\\..\\..\\font.ttf",
		L"C:\\Users\\tsukuyomi\\Desktop\\font.ttf"
	};

	for (const auto& candidate : candidates) {
		if (FileExistsW(candidate) && AddFontResourceExW(candidate.c_str(), FR_PRIVATE, nullptr) > 0) {
			privateFontPath = candidate;
			privateFontLoaded = true;
			break;
		}
	}
}

bool Renderer::LoadBitmapFromFile(const std::wstring& path, ID2D1Bitmap** bitmap, UINT maxSize) {
	if (!pWICFactory || !pRT || !bitmap)
		return false;

	IWICBitmapDecoder* decoder = nullptr;
	IWICBitmapFrameDecode* frame = nullptr;
	IWICBitmapScaler* scaler = nullptr;
	IWICFormatConverter* converter = nullptr;

	HRESULT hr = pWICFactory->CreateDecoderFromFilename(
		path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
	if (SUCCEEDED(hr))
		hr = decoder->GetFrame(0, &frame);

	
	IWICBitmapSource* scaledSource = nullptr;
	if (SUCCEEDED(hr) && maxSize > 0) {
		UINT srcW = 0, srcH = 0;
		frame->GetSize(&srcW, &srcH);
		if (srcW > maxSize || srcH > maxSize) {
			UINT dstW = maxSize, dstH = maxSize;
			if (srcW > srcH) { dstH = (UINT)((float)maxSize * srcH / srcW); }
			else if (srcH > srcW) { dstW = (UINT)((float)maxSize * srcW / srcH); }
			hr = pWICFactory->CreateBitmapScaler(&scaler);
			if (SUCCEEDED(hr))
				hr = scaler->Initialize(frame, dstW, dstH, WICBitmapInterpolationModeHighQualityCubic);
			if (SUCCEEDED(hr))
				scaledSource = scaler;
		}
	}
	if (!scaledSource) scaledSource = frame;

	if (SUCCEEDED(hr))
		hr = pWICFactory->CreateFormatConverter(&converter);
	if (SUCCEEDED(hr)) {
		hr = converter->Initialize(
			scaledSource, GUID_WICPixelFormat32bppPBGRA,
			WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeMedianCut);
	}
	if (SUCCEEDED(hr))
		hr = pRT->CreateBitmapFromWicBitmap(converter, nullptr, bitmap);

	SafeRelease(&converter);
	SafeRelease(&scaler);
	SafeRelease(&frame);
	SafeRelease(&decoder);
	return SUCCEEDED(hr);
}

void Renderer::TryLoadLogoBitmap() {
	std::wstring exeDir = GetExecutableDirectoryW();
	std::wstring candidates[] = {
		exeDir + L"aether_logo.png",
		exeDir + L"assets\\aether_logo.png",
		exeDir + L"..\\..\\assets\\aether_logo.png",
		exeDir + L"..\\..\\AetherGUI\\assets\\aether_logo.png",
		exeDir + L"..\\..\\..\\AetherGUI\\assets\\aether_logo.png"
	};

	for (const auto& candidate : candidates) {
		
		if (FileExistsW(candidate) && LoadBitmapFromFile(candidate, &pLogoBitmap, 128))
			break;
	}
}

bool Renderer::Initialize(HWND hwnd) {
	hWnd = hwnd;
	TryLoadPrivateFont();

	HRESULT coHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	comInitialized = SUCCEEDED(coHr);
	if (FAILED(coHr) && coHr != RPC_E_CHANGED_MODE)
		return false;

	HDC hdc = GetDC(hwnd);
	dpiScale = GetDeviceCaps(hdc, LOGPIXELSX) / 96.0f;
	ReleaseDC(hwnd, hdc);

	HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory);
	if (FAILED(hr)) return false;

	RECT rc;
	GetClientRect(hwnd, &rc);
	D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

	hr = pFactory->CreateHwndRenderTarget(
		D2D1::RenderTargetProperties(),
		D2D1::HwndRenderTargetProperties(hwnd, size),
		&pRT);
	if (FAILED(hr)) return false;

	pRT->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
	pRT->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);

	D2D1_STROKE_STYLE_PROPERTIES roundStroke = D2D1::StrokeStyleProperties(
		D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
		D2D1_LINE_JOIN_ROUND, 10.0f, D2D1_DASH_STYLE_SOLID, 0.0f);
	hr = pFactory->CreateStrokeStyle(roundStroke, nullptr, 0, &pStrokeRound);
	if (FAILED(hr)) return false;

	hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
		__uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&pDWriteFactory));
	if (FAILED(hr)) return false;

	hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pWICFactory));
	if (FAILED(hr)) return false;

	hr = pRT->CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF), &pBrush);
	if (FAILED(hr)) return false;

	if (!CreateTextFormats()) return false;
	TryLoadLogoBitmap();

	return true;
}

bool Renderer::CreateTextFormats() {
	HRESULT hr;

	hr = pDWriteFactory->CreateTextFormat(
		Theme::Font::FamilyBrand, nullptr,
		DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
		Theme::Font::SizeTitle * dpiScale, L"", &pFontTitle);
	if (FAILED(hr)) {
		hr = pDWriteFactory->CreateTextFormat(
			Theme::Font::Family, nullptr,
			DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
			Theme::Font::SizeTitle * dpiScale, L"", &pFontTitle);
	}
	if (FAILED(hr)) return false;

	hr = pDWriteFactory->CreateTextFormat(
		Theme::Font::Family, nullptr,
		DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
		Theme::Font::SizeHeading * dpiScale, L"", &pFontHeading);
	if (FAILED(hr)) return false;

	hr = pDWriteFactory->CreateTextFormat(
		Theme::Font::Family, nullptr,
		DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
		Theme::Font::SizeBody * dpiScale, L"", &pFontBody);
	if (FAILED(hr)) return false;

	hr = pDWriteFactory->CreateTextFormat(
		Theme::Font::Family, nullptr,
		DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
		Theme::Font::SizeSmall * dpiScale, L"", &pFontSmall);
	if (FAILED(hr)) return false;

	hr = pDWriteFactory->CreateTextFormat(
		Theme::Font::FamilyMono, nullptr,
		DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
		Theme::Font::SizeBody * dpiScale, L"", &pFontMono);
	if (FAILED(hr)) return false;

	hr = pDWriteFactory->CreateTextFormat(
		L"Segoe MDL2 Assets", nullptr,
		DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
		16.0f * dpiScale, L"", &pFontIcon);
	if (FAILED(hr)) return false;

	return true;
}

void Renderer::Shutdown() {
	SafeRelease(&pLogoBitmap);
	SafeRelease(&pFontTitle);
	SafeRelease(&pFontHeading);
	SafeRelease(&pFontBody);
	SafeRelease(&pFontSmall);
	SafeRelease(&pFontMono);
	SafeRelease(&pFontIcon);
	SafeRelease(&pBrush);
	SafeRelease(&pRT);
	SafeRelease(&pStrokeRound);
	SafeRelease(&pWICFactory);
	SafeRelease(&pDWriteFactory);
	SafeRelease(&pFactory);
	if (privateFontLoaded) {
		RemoveFontResourceExW(privateFontPath.c_str(), FR_PRIVATE, nullptr);
		privateFontLoaded = false;
		privateFontPath.clear();
	}
	if (comInitialized) {
		CoUninitialize();
		comInitialized = false;
	}
}

void Renderer::Resize(UINT width, UINT height) {
	if (pRT && width > 0 && height > 0) {
		pRT->Resize(D2D1::SizeU(width, height));
	}
}

void Renderer::BeginFrame() {
	if (!pRT) return;
	pRT->BeginDraw();
	pRT->Clear(Theme::BgDeep());
}

void Renderer::EndFrame() {
	if (!pRT) return;
	HRESULT hr = pRT->EndDraw();
	if (hr == D2DERR_RECREATE_TARGET) {
		Shutdown();
		Initialize(hWnd);
	}
}

void Renderer::FillRect(float x, float y, float w, float h, D2D1_COLOR_F color) {
	pBrush->SetColor(color);
	pRT->FillRectangle(D2D1::RectF(x, y, x + w, y + h), pBrush);
}

void Renderer::FillRoundedRect(float x, float y, float w, float h, float radius, D2D1_COLOR_F color) {
	pBrush->SetColor(color);
	D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(D2D1::RectF(x, y, x + w, y + h), radius, radius);
	pRT->FillRoundedRectangle(rr, pBrush);
}

void Renderer::DrawRoundedRect(float x, float y, float w, float h, float radius, D2D1_COLOR_F color, float strokeWidth) {
	pBrush->SetColor(color);
	D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(D2D1::RectF(x, y, x + w, y + h), radius, radius);
	pRT->DrawRoundedRectangle(rr, pBrush, strokeWidth);
}

void Renderer::FillCircle(float cx, float cy, float r, D2D1_COLOR_F color) {
	pBrush->SetColor(color);
	pRT->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), pBrush);
}

void Renderer::DrawCircle(float cx, float cy, float r, D2D1_COLOR_F color, float strokeWidth) {
	pBrush->SetColor(color);
	pRT->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), r, r), pBrush, strokeWidth);
}

void Renderer::DrawLine(float x1, float y1, float x2, float y2, D2D1_COLOR_F color, float strokeWidth) {
	pBrush->SetColor(color);
	pRT->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), pBrush, strokeWidth);
}

void Renderer::DrawBitmap(ID2D1Bitmap* bitmap, float x, float y, float w, float h, float opacity) {
	if (!bitmap) return;
	pRT->DrawBitmap(bitmap, D2D1::RectF(x, y, x + w, y + h), opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
}

void Renderer::DrawBitmapTinted(ID2D1Bitmap* bitmap, float x, float y, float w, float h, D2D1_COLOR_F tint, float opacity) {
	if (!bitmap || !pRT) return;

	
	D2D1_RECT_F destRect = D2D1::RectF(x, y, x + w, y + h);
	pRT->DrawBitmap(bitmap, destRect, opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);

	
	
	ID2D1BitmapBrush* pBmpBrush = nullptr;
	HRESULT hr = pRT->CreateBitmapBrush(bitmap, &pBmpBrush);
	if (SUCCEEDED(hr)) {
		
		D2D1_SIZE_F bmpSize = bitmap->GetSize();
		float sx = w / bmpSize.width;
		float sy = h / bmpSize.height;
		pBmpBrush->SetTransform(D2D1::Matrix3x2F::Scale(sx, sy) * D2D1::Matrix3x2F::Translation(x, y));
		pBmpBrush->SetOpacity(opacity);

		ID2D1Layer* pLayer = nullptr;
		hr = pRT->CreateLayer(nullptr, &pLayer);
		if (SUCCEEDED(hr)) {
			
			pRT->PushLayer(
				D2D1::LayerParameters(destRect, nullptr, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
					D2D1::Matrix3x2F::Identity(), 1.0f, pBmpBrush),
				pLayer);

			
			pBrush->SetColor(tint);
			pRT->FillRectangle(destRect, pBrush);

			pRT->PopLayer();
			SafeRelease(&pLayer);
		}
		SafeRelease(&pBmpBrush);
	}
}

void Renderer::DrawText(const wchar_t* text, float x, float y, float w, float h,
	D2D1_COLOR_F color, IDWriteTextFormat* font, TextAlign align) {
	pBrush->SetColor(color);

	switch (align) {
	case AlignCenter: font->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER); break;
	case AlignRight:  font->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING); break;
	default:          font->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING); break;
	}
	font->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

	D2D1_RECT_F rect = D2D1::RectF(x, y, x + w, y + h);
	pRT->DrawText(text, (UINT32)wcslen(text), font, rect, pBrush);
}

void Renderer::DrawTextSimple(const wchar_t* text, float x, float y,
	D2D1_COLOR_F color, IDWriteTextFormat* font) {
	DrawText(text, x, y, 500, 30, color, font, AlignLeft);
}

void Renderer::FillRectGradientV(float x, float y, float w, float h,
	D2D1_COLOR_F topColor, D2D1_COLOR_F bottomColor) {
	ID2D1GradientStopCollection* pStops = nullptr;
	D2D1_GRADIENT_STOP stops[2];
	stops[0] = { 0.0f, topColor };
	stops[1] = { 1.0f, bottomColor };

	HRESULT hr = pRT->CreateGradientStopCollection(stops, 2, &pStops);
	if (SUCCEEDED(hr)) {
		ID2D1LinearGradientBrush* pGradBrush = nullptr;
		hr = pRT->CreateLinearGradientBrush(
			D2D1::LinearGradientBrushProperties(D2D1::Point2F(x, y), D2D1::Point2F(x, y + h)),
			pStops, &pGradBrush);
		if (SUCCEEDED(hr)) {
			pRT->FillRectangle(D2D1::RectF(x, y, x + w, y + h), pGradBrush);
			SafeRelease(&pGradBrush);
		}
		SafeRelease(&pStops);
	}
}

void Renderer::FillRectGradientH(float x, float y, float w, float h,
	D2D1_COLOR_F leftColor, D2D1_COLOR_F rightColor) {
	ID2D1GradientStopCollection* pStops = nullptr;
	D2D1_GRADIENT_STOP stops[2];
	stops[0] = { 0.0f, leftColor };
	stops[1] = { 1.0f, rightColor };

	HRESULT hr = pRT->CreateGradientStopCollection(stops, 2, &pStops);
	if (SUCCEEDED(hr)) {
		ID2D1LinearGradientBrush* pGradBrush = nullptr;
		hr = pRT->CreateLinearGradientBrush(
			D2D1::LinearGradientBrushProperties(D2D1::Point2F(x, y), D2D1::Point2F(x + w, y)),
			pStops, &pGradBrush);
		if (SUCCEEDED(hr)) {
			pRT->FillRectangle(D2D1::RectF(x, y, x + w, y + h), pGradBrush);
			SafeRelease(&pGradBrush);
		}
		SafeRelease(&pStops);
	}
}

void Renderer::FillRoundedRectGradientH(float x, float y, float w, float h, float radius,
	D2D1_COLOR_F leftColor, D2D1_COLOR_F rightColor) {
	ID2D1RoundedRectangleGeometry* pGeometry = nullptr;
	D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(D2D1::RectF(x, y, x + w, y + h), radius, radius);
	HRESULT hr = pFactory->CreateRoundedRectangleGeometry(rr, &pGeometry);
	if (FAILED(hr)) return;

	ID2D1Layer* pLayer = nullptr;
	hr = pRT->CreateLayer(nullptr, &pLayer);
	if (SUCCEEDED(hr)) {
		pRT->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), pGeometry), pLayer);

		ID2D1GradientStopCollection* pStops = nullptr;
		D2D1_GRADIENT_STOP stops[2];
		stops[0] = { 0.0f, leftColor };
		stops[1] = { 1.0f, rightColor };

		hr = pRT->CreateGradientStopCollection(stops, 2, &pStops);
		if (SUCCEEDED(hr)) {
			ID2D1LinearGradientBrush* pGradBrush = nullptr;
			hr = pRT->CreateLinearGradientBrush(
				D2D1::LinearGradientBrushProperties(D2D1::Point2F(x, y), D2D1::Point2F(x + w, y)),
				pStops, &pGradBrush);
			if (SUCCEEDED(hr)) {
				pRT->FillRectangle(D2D1::RectF(x, y, x + w, y + h), pGradBrush);
				SafeRelease(&pGradBrush);
			}
			SafeRelease(&pStops);
		}

		pRT->PopLayer();
		SafeRelease(&pLayer);
	}
	SafeRelease(&pGeometry);
}
