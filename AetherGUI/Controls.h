#pragma once
#include "Framework.h"
#include "Theme.h"
#include "Renderer.h"


inline bool PointInRect(float px, float py, float x, float y, float w, float h) {
	return px >= x && px <= x + w && py >= y && py <= y + h;
}


struct Tooltip {
	static inline const wchar_t* text = nullptr;
	static inline const wchar_t* pendingTip = nullptr;
	static inline float x = 0, y = 0;
	static inline bool visible = false;
	static inline float timer = 0;
	static inline bool hoveredThisFrame = false;

	static void Show(float mx, float my, const wchar_t* tip, float dt) {
		if (!tip || !tip[0]) return;
		hoveredThisFrame = true;
		
		if (pendingTip != tip) {
			pendingTip = tip;
			timer = 0;
			visible = false;
		}
		timer += dt;
		if (timer > 0.35f) {
			text = tip;
			x = mx + 12;
			y = my + 16;
			visible = true;
		}
	}
	static void Reset() {
		
		if (!hoveredThisFrame) {
			timer = 0;
			visible = false;
			text = nullptr;
			pendingTip = nullptr;
		}
		hoveredThisFrame = false;
	}
	static void Draw(Renderer& r) {
		if (!visible || !text) return;
		float maxW = 280.0f;
		float rawW = (float)wcslen(text) * 5.6f + 16.0f;
		float w = (rawW < maxW) ? rawW : maxW;
		
		float h = (rawW > maxW) ? 36.0f : 22.0f;
		float dx = x, dy = y;
		
		if (dx + w > Theme::Runtime::WindowWidth - 8) dx = Theme::Runtime::WindowWidth - w - 8;
		if (dx < 4) dx = 4;
		if (dy + h > Theme::Runtime::WindowHeight - 8) dy -= h + 8;
		if (dy < 4) dy = 4;
		r.FillRoundedRect(dx, dy, w, h, 4, Theme::BgElevated());
		r.DrawRoundedRect(dx, dy, w, h, 4, Theme::BorderSubtle());
		r.DrawText(text, dx + 8, dy, w - 16, h, Theme::TextSecondary(), r.pFontSmall);
	}
};


struct Toggle {
	float x = 0, y = 0;
	bool value = false;
	float animT = 0.0f;
	float hoverT = 0.0f;
	bool isHovered = false;
	const wchar_t* label = L"";
	const wchar_t* tooltip = L"";

	
	void Layout(float px, float py, const wchar_t* lbl, const wchar_t* tip = L"") {
		x = px; y = py; label = lbl; tooltip = tip;
	}

	
	bool Update(float mx, float my, bool clicked, float dt) {
		isHovered = PointInRect(mx, my, x, y, Theme::Size::ToggleWidth + 120, Theme::Size::ToggleHeight);
		hoverT = Lerp(hoverT, isHovered ? 1.0f : 0.0f, dt * Theme::Anim::SpeedFast);
		animT = Lerp(animT, value ? 1.0f : 0.0f, dt * Theme::Anim::Speed);
		if (isHovered && tooltip[0]) Tooltip::Show(mx, my, tooltip, dt);
		if (isHovered && clicked) { value = !value; return true; }
		return false;
	}

	void Draw(Renderer& r) {
		float tw = Theme::Size::ToggleWidth;
		float th = Theme::Size::ToggleHeight;
		float radius = th * 0.5f;

		D2D1_COLOR_F trackColor = LerpColor(Theme::BgHover(), Theme::AccentPrimary(), animT);
		r.FillRoundedRect(x, y, tw, th, radius, trackColor);
		D2D1_COLOR_F border = LerpColor(Theme::BorderSubtle(), Theme::BorderAccent(), animT * 0.5f);
		r.DrawRoundedRect(x, y, tw, th, radius, border);

		float thumbX = Lerp(x + radius, x + tw - radius, animT);
		r.FillCircle(thumbX, y + radius, radius - 3.0f, Theme::TextPrimary());

		D2D1_COLOR_F labelCol = LerpColor(Theme::TextSecondary(), Theme::TextPrimary(), hoverT * 0.5f);
		r.DrawText(label, x + tw + 10, y, 200, th, labelCol, r.pFontBody);
	}
};


struct Slider {
	float x = 0, y = 0, width = 0;
	float minVal = 0, maxVal = 1;
	float value = 0;
	float animValue = 0;
	float hoverT = 0.0f;
	bool isDragging = false;
	bool isHovered = false;
	const wchar_t* label = L"";
	const wchar_t* tooltip = L"";
	const wchar_t* format = L"%.2f";

	bool editMode = false;
	wchar_t editBuffer[32] = {};
	int editCursor = 0;
	int selectionStart = 0;
	int selectionEnd = 0;
	float editBlinkT = 0;
	bool isMouseDraggingText = false;

	
	void Layout(float px, float py, float w, const wchar_t* lbl, float mn, float mx, float initial, const wchar_t* tip = L"") {
		x = px; y = py; width = w; label = lbl;
		minVal = mn; maxVal = mx; value = initial; animValue = initial; tooltip = tip;
	}

	int BufferLength() const { return (int)wcslen(editBuffer); }
	void ClearSelection() { selectionStart = selectionEnd = editCursor; }

	bool GetSelectionRange(int& start, int& end) const {
		start = selectionStart < selectionEnd ? selectionStart : selectionEnd;
		end = selectionStart > selectionEnd ? selectionStart : selectionEnd;
		return end > start;
	}

	
	void BeginEdit() {
		if (!editMode) {
			memset(editBuffer, 0, sizeof(editBuffer));
			swprintf_s(editBuffer, format, value);
			TrimTrailingZeros(editBuffer);
			editCursor = BufferLength();
			selectionStart = 0; selectionEnd = editCursor;
			isMouseDraggingText = false;
			editBlinkT = 0; editMode = true;
		}
	}

	bool DeleteSelection() {
		int start = 0, end = 0;
		if (!GetSelectionRange(start, end)) return false;
		int len = BufferLength();
		for (int i = start; i <= len - (end - start); i++) editBuffer[i] = editBuffer[i + (end - start)];
		editCursor = start; ClearSelection();
		return true;
	}

	bool IsNumericChar(wchar_t ch) const { return (ch >= L'0' && ch <= L'9') || ch == L'.' || ch == L'-'; }

	void InsertChar(wchar_t ch) {
		if (!IsNumericChar(ch)) return;
		DeleteSelection();
		int len = BufferLength();
		if (len >= 30) return;
		for (int i = len + 1; i > editCursor; i--) editBuffer[i] = editBuffer[i - 1];
		editBuffer[editCursor] = ch; editCursor++; ClearSelection();
	}

	void InsertText(const wchar_t* text) {
		if (!text) return;
		DeleteSelection();
		for (int i = 0; text[i]; i++) if (IsNumericChar(text[i])) InsertChar(text[i]);
	}

	void SelectAll() { selectionStart = 0; selectionEnd = BufferLength(); editCursor = selectionEnd; }

	void MoveCursor(int nc, bool shift) {
		int len = BufferLength();
		if (nc < 0) nc = 0; if (nc > len) nc = len;
		int old = editCursor; editCursor = nc;
		if (shift) { if (selectionStart == selectionEnd) selectionStart = old; selectionEnd = editCursor; }
		else ClearSelection();
	}

	void PasteClipboard() {
		if (!OpenClipboard(nullptr)) return;
		HANDLE data = GetClipboardData(CF_UNICODETEXT);
		if (data) { const wchar_t* t = (const wchar_t*)GlobalLock(data); if (t) { InsertText(t); GlobalUnlock(data); } }
		CloseClipboard();
	}

	void CopySelection() {
		int start = 0, end = 0;
		if (!GetSelectionRange(start, end)) return;
		if (!OpenClipboard(nullptr)) return;
		int count = end - start;
		HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, (count + 1) * sizeof(wchar_t));
		if (mem) {
			wchar_t* out = (wchar_t*)GlobalLock(mem);
			if (out) { for (int i = 0; i < count; i++) out[i] = editBuffer[start + i]; out[count] = 0; GlobalUnlock(mem); EmptyClipboard(); if (!SetClipboardData(CF_UNICODETEXT, mem)) GlobalFree(mem); } else GlobalFree(mem);
		}
		CloseClipboard();
	}

	
	bool Update(float mx, float my, bool mouseDown, bool clicked, float dt) {
		float trackY = y + 20;
		float trackH = Theme::Size::SliderHeight;
		float thumbR = Theme::Size::SliderThumbRadius;
		isHovered = PointInRect(mx, my, x, trackY - thumbR, width, trackH + thumbR * 2);
		bool valueHovered = PointInRect(mx, my, x + width * 0.6f, y, width * 0.4f, 18);
		hoverT = Lerp(hoverT, isHovered || isDragging ? 1.0f : 0.0f, dt * Theme::Anim::SpeedFast);
		animValue = Lerp(animValue, value, dt * Theme::Anim::SpeedFast);
		editBlinkT += dt;
		if (isHovered && tooltip[0]) Tooltip::Show(mx, my, tooltip, dt);

		
		if (editMode) {
			float editBoxX = x + width * 0.6f;
			float charW = 7.2f;
			if (clicked && valueHovered) {
				int pos = (int)((mx - editBoxX - 2.0f) / charW + 0.5f);
				if (pos < 0) pos = 0; if (pos > BufferLength()) pos = BufferLength();
				bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
				MoveCursor(pos, shift);
				isMouseDraggingText = true;
				editBlinkT = 0;
			} else if (clicked && !valueHovered) {
				isMouseDraggingText = false;
				return CommitEdit();
			}
			if (isMouseDraggingText && mouseDown && !clicked) {
				int pos = (int)((mx - editBoxX - 2.0f) / charW + 0.5f);
				if (pos < 0) pos = 0; if (pos > BufferLength()) pos = BufferLength();
				MoveCursor(pos, true);
			}
			if (!mouseDown) isMouseDraggingText = false;
			return false;
		}

		if (valueHovered && clicked) { BeginEdit(); return false; }
		if (isHovered && clicked) isDragging = true;
		if (!mouseDown) isDragging = false;
		if (isDragging) {
			float norm = Clamp((mx - x) / width, 0.0f, 1.0f);
			float nv = minVal + norm * (maxVal - minVal);
			if (nv != value) { value = nv; return true; }
		}
		return false;
	}

	
	bool UpdateInput(float mx, float my, bool mouseDown, bool clicked, float dt) {
		float inputY = y + 16.0f, inputH = 26.0f;
		bool hovered = PointInRect(mx, my, x, inputY, width, inputH);
		isHovered = hovered;
		hoverT = Lerp(hoverT, hovered ? 1.0f : 0.0f, dt * Theme::Anim::SpeedFast);
		editBlinkT += dt;
		if (hovered && tooltip[0]) Tooltip::Show(mx, my, tooltip, dt);

		float charW = 7.2f;
		float boxPad = 8.0f;

		if (editMode) {
			
			if (clicked && hovered) {
				int pos = (int)((mx - x - boxPad) / charW + 0.5f);
				if (pos < 0) pos = 0; if (pos > BufferLength()) pos = BufferLength();
				bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
				MoveCursor(pos, shift);
				isMouseDraggingText = true;
				editBlinkT = 0;
				return false;
			}
			
			if (clicked && !hovered) {
				isMouseDraggingText = false;
				return CommitEdit();
			}
			
			if (isMouseDraggingText && mouseDown && !clicked) {
				int pos = (int)((mx - x - boxPad) / charW + 0.5f);
				if (pos < 0) pos = 0; if (pos > BufferLength()) pos = BufferLength();
				MoveCursor(pos, true);
			}
			if (!mouseDown) isMouseDraggingText = false;
			return false;
		}

		
		if (clicked && hovered) {
			BeginEdit();
			int pos = (int)((mx - x - boxPad) / charW + 0.5f);
			if (pos < 0) pos = 0; if (pos > BufferLength()) pos = BufferLength();
			MoveCursor(pos, false);
			isMouseDraggingText = true;
			return false;
		}
		return false;
	}

	void OnChar(wchar_t ch) {
		if (!editMode) return;
		if (ch == L'\r' || ch == L'\n') { CommitEdit(); return; }
		if (ch == 27) { editMode = false; return; }
		if (ch < 32 && ch != L'\b') return;
		if (ch == L'\b') { if (!DeleteSelection() && editCursor > 0) { for (int i = editCursor - 1; editBuffer[i]; i++) editBuffer[i] = editBuffer[i + 1]; editCursor--; ClearSelection(); } return; }
		InsertChar(ch);
	}

	bool OnKeyDown(int vk, bool ctrl, bool shift) {
		if (!editMode) return false;
		if (ctrl && vk == 'A') { SelectAll(); return true; }
		if (ctrl && vk == 'C') { CopySelection(); return true; }
		if (ctrl && vk == 'X') { CopySelection(); DeleteSelection(); return true; }
		if (ctrl && vk == 'V') { PasteClipboard(); return true; }
		switch (vk) {
		case VK_LEFT: MoveCursor(editCursor - 1, shift); return true;
		case VK_RIGHT: MoveCursor(editCursor + 1, shift); return true;
		case VK_HOME: MoveCursor(0, shift); return true;
		case VK_END: MoveCursor(BufferLength(), shift); return true;
		case VK_DELETE: if (!DeleteSelection()) { int len = BufferLength(); if (editCursor < len) { for (int i = editCursor; editBuffer[i]; i++) editBuffer[i] = editBuffer[i + 1]; } } return true;
		}
		return false;
	}

	
	bool CommitEdit() {
		editMode = false;
		float v = Clamp((float)_wtof(editBuffer), minVal, maxVal);
		bool changed = fabsf(v - value) > 0.0001f;
		value = v; animValue = v; ClearSelection();
		return changed;
	}

	
	static void TrimTrailingZeros(wchar_t* buf) {
		int len = (int)wcslen(buf);
		bool hasDot = false;
		for (int i = 0; i < len; i++) if (buf[i] == L'.') { hasDot = true; break; }
		if (hasDot) {
			while (len > 1 && buf[len - 1] == L'0') buf[--len] = 0;
			if (len > 0 && buf[len - 1] == L'.') buf[--len] = 0;
		}
	}

	void Draw(Renderer& r) {
		float trackY = y + 22, trackH = Theme::Size::SliderHeight, thumbR = Theme::Size::SliderThumbRadius;
		r.DrawText(label, x, y, width * 0.6f, 18, Theme::TextSecondary(), r.pFontSmall);

		if (editMode) {
			float editBoxX = x + width * 0.6f - 4;
			float editBoxW = width * 0.4f + 4;
			r.FillRoundedRect(editBoxX, y - 2, editBoxW, 20, 3, Theme::BgElevated());
			r.DrawRoundedRect(editBoxX, y - 2, editBoxW, 20, 3, Theme::AccentPrimary());

			float charW = 7.2f;
			float textX = editBoxX + 4;

			
			int selS = 0, selE = 0;
			if (GetSelectionRange(selS, selE)) {
				D2D1_COLOR_F sel = Theme::AccentPrimary(); sel.a = 0.3f;
				r.FillRoundedRect(textX + selS * charW, y, (float)(selE - selS) * charW, 16, 2, sel);
			}

			
			r.DrawText(editBuffer, textX, y, editBoxW - 8, 18, Theme::TextPrimary(), r.pFontMono);

			
			if (fmod(editBlinkT, 1.0f) < 0.5f)
				r.DrawLine(textX + editCursor * charW, y + 1, textX + editCursor * charW, y + 15, Theme::AccentPrimary(), 1.0f);
		} else {
			wchar_t valBuf[32]; swprintf_s(valBuf, format, value);
			TrimTrailingZeros(valBuf);
			r.DrawText(valBuf, x + width * 0.6f, y, width * 0.4f, 18, Theme::TextAccent(), r.pFontSmall, Renderer::AlignRight);
		}

		r.FillRoundedRect(x, trackY, width, trackH, trackH * 0.5f, Theme::BgElevated());
		float norm = (animValue - minVal) / (maxVal - minVal);
		float filledW = width * norm;
		if (filledW > 2) r.FillRoundedRect(x, trackY, filledW, trackH, trackH * 0.5f, Theme::AccentPrimary());

		float thumbX = x + filledW, thumbY = trackY + trackH * 0.5f;
		float tr = Lerp(thumbR * 0.85f, thumbR, hoverT);
		r.FillCircle(thumbX, thumbY, tr, Theme::TextPrimary());
		if (isDragging) r.FillCircle(thumbX, thumbY, tr * 0.35f, Theme::AccentPrimary());
	}

	void DrawInput(Renderer& r) {
		float inputY = y + 16.0f, inputH = 26.0f, radius = 6.0f, boxPad = 8.0f;
		r.DrawText(label, x, y, width, 14, Theme::TextSecondary(), r.pFontSmall);

		D2D1_COLOR_F bg = LerpColor(Theme::BgSurface(), Theme::BgElevated(), hoverT * 0.5f);
		r.FillRoundedRect(x, inputY, width, inputH, radius, bg);
		D2D1_COLOR_F border = editMode ? Theme::AccentPrimary() : LerpColor(Theme::BorderSubtle(), Theme::BorderNormal(), hoverT * 0.5f);
		r.DrawRoundedRect(x, inputY, width, inputH, radius, border);

		wchar_t valueBuf[32]; swprintf_s(valueBuf, format, value);
		if (!editMode) TrimTrailingZeros(valueBuf);
		const wchar_t* text = editMode ? editBuffer : valueBuf;
		float textX = x + boxPad, textW = width - boxPad * 2.0f, charW = 7.2f;

		if (editMode) {
			int start = 0, end = 0;
			if (GetSelectionRange(start, end)) {
				D2D1_COLOR_F sel = Theme::AccentPrimary(); sel.a = 0.25f;
				r.FillRoundedRect(textX + start * charW, inputY + 4, (float)(end - start) * charW, inputH - 8, 2, sel);
			}
		}
		
		IDWriteTextFormat* inputFont = editMode ? r.pFontMono : r.pFontSmall;
		r.DrawText(text, textX, inputY, textW, inputH, Theme::TextPrimary(), inputFont);
		if (editMode && fmod(editBlinkT, 1.0f) < 0.5f)
			r.DrawLine(textX + editCursor * charW, inputY + 5, textX + editCursor * charW, inputY + inputH - 5, Theme::AccentPrimary(), 1.0f);
	}
};


struct Button {
	float x = 0, y = 0, width = 0, height = 0;
	const wchar_t* label = L"";
	const wchar_t* tooltip = L"";
	float hoverT = 0.0f, pressT = 0.0f;
	bool isHovered = false, isPrimary = false;

	void Layout(float px, float py, float w, float h, const wchar_t* lbl, bool primary = false, const wchar_t* tip = L"") {
		x = px; y = py; width = w; height = h; label = lbl; isPrimary = primary; tooltip = tip;
	}

	bool Update(float mx, float my, bool clicked, float dt) {
		isHovered = PointInRect(mx, my, x, y, width, height);
		hoverT = Lerp(hoverT, isHovered ? 1.0f : 0.0f, dt * Theme::Anim::SpeedFast);
		pressT = Lerp(pressT, (isHovered && clicked) ? 1.0f : 0.0f, dt * Theme::Anim::SpeedFast * 2);
		if (isHovered && tooltip[0]) Tooltip::Show(mx, my, tooltip, dt);
		return isHovered && clicked;
	}

	void Draw(Renderer& r) {
		float radius = 6.0f;
		float scale = Lerp(1.0f, 0.97f, pressT);
		float ox = x + width * (1 - scale) * 0.5f, oy = y + height * (1 - scale) * 0.5f;
		float sw = width * scale, sh = height * scale;

		if (isPrimary) {
			D2D1_COLOR_F bg = LerpColor(Theme::AccentPrimary(), Theme::AccentSecondary(), hoverT * 0.3f);
			r.FillRoundedRect(ox, oy, sw, sh, radius, bg);
			r.DrawText(label, ox, oy, sw, sh, D2D1::ColorF(0xFFFFFF), r.pFontBody, Renderer::AlignCenter);
		} else {
			D2D1_COLOR_F bg = LerpColor(Theme::BgElevated(), Theme::BgHover(), hoverT);
			r.FillRoundedRect(ox, oy, sw, sh, radius, bg);
			D2D1_COLOR_F border = LerpColor(Theme::BorderSubtle(), Theme::BorderNormal(), hoverT * 0.5f);
			r.DrawRoundedRect(ox, oy, sw, sh, radius, border);
			D2D1_COLOR_F textCol = LerpColor(Theme::TextSecondary(), Theme::TextPrimary(), hoverT);
			r.DrawText(label, ox, oy, sw, sh, textCol, r.pFontBody, Renderer::AlignCenter);
		}
	}
};


struct TabBar {
	struct Tab { const wchar_t* label; const wchar_t* icon; float hoverT = 0.0f; };
	std::vector<Tab> tabs;
	int activeIndex = 0;
	float x = 0, y = 0;

	void AddTab(const wchar_t* label, const wchar_t* icon = L"") { tabs.push_back({ label, icon, 0.0f }); }

	int Update(float mx, float my, bool clicked, float dt) {
		int result = -1;
		float tabW = Theme::Size::SidebarWidth, tabH = 48.0f;
		for (int i = 0; i < (int)tabs.size(); i++) {
			float ty = y + i * tabH;
			bool hovered = PointInRect(mx, my, x, ty, tabW, tabH);
			tabs[i].hoverT = Lerp(tabs[i].hoverT, hovered ? 1.0f : (i == activeIndex ? 0.5f : 0.0f), dt * Theme::Anim::Speed);
			if (hovered && clicked) { activeIndex = i; result = i; }
		}
		return result;
	}

	void Draw(Renderer& r) {
		float tabW = Theme::Size::SidebarWidth, tabH = 48.0f;
		r.FillRect(x, 0, tabW, Theme::Runtime::WindowHeight, Theme::BgSurface());
		D2D1_COLOR_F divider = Theme::BorderSubtle();
		r.DrawLine(x + tabW, 0, x + tabW, Theme::Runtime::WindowHeight, divider);

		for (int i = 0; i < (int)tabs.size(); i++) {
			float ty = y + i * tabH;
			if (i == activeIndex) {
				D2D1_COLOR_F activeBg = Theme::AccentDim();
				r.FillRoundedRect(x + 8, ty + 6, tabW - 16, tabH - 12, 6.0f, activeBg);
				r.FillRect(x + 2, ty + 14, 3, tabH - 28, Theme::AccentPrimary());
			} else if (tabs[i].hoverT > 0.05f) {
				D2D1_COLOR_F hbg = Theme::BgHover(); hbg.a = tabs[i].hoverT * 0.5f;
				r.FillRoundedRect(x + 8, ty + 6, tabW - 16, tabH - 12, 6.0f, hbg);
			}

			D2D1_COLOR_F textCol = (i == activeIndex) ? Theme::AccentPrimary() : Theme::TextMuted();
			textCol = LerpColor(textCol, Theme::TextPrimary(), tabs[i].hoverT * 0.4f);
			const wchar_t* iconText = tabs[i].icon;
			if (iconText && iconText[0])
				r.DrawText(iconText, x, ty, tabW, tabH, textCol, r.pFontIcon, Renderer::AlignCenter);
			else {
				wchar_t buf[2] = { tabs[i].label[0], 0 };
				r.DrawText(buf, x, ty, tabW, tabH, textCol, r.pFontHeading, Renderer::AlignCenter);
			}
		}
	}
};


struct SectionHeader {
	const wchar_t* title = L"";
	float x = 0, y = 0, width = 0;

	void Layout(float px, float py, float w, const wchar_t* t) { x = px; y = py; width = w; title = t; }

	float Draw(Renderer& r) {
		r.DrawText(title, x + 4, y, 200, 18, Theme::AccentPrimary(), r.pFontSmall);
		float lineY = y + 19;
		float titleW = (float)wcslen(title) * 6.0f + 16.0f;
		D2D1_COLOR_F lineCol = Theme::BorderSubtle();
		r.DrawLine(x + titleW, lineY, x + width, lineY, lineCol);
		return 26.0f;
	}
};


struct RadioGroup {
	float x = 0, y = 0;
	int selected = 0;
	struct Option { const wchar_t* text; float hoverT = 0.0f; };
	Option options[8];
	int optionCount = 0;

	void AddOption(const wchar_t* text) { if (optionCount < 8) options[optionCount++] = { text, 0.0f }; }

	int Update(float mx, float my, bool clicked, float dt, float btnW = 100) {
		float btnH = 28, gap = 6; int result = -1;
		for (int i = 0; i < optionCount; i++) {
			float bx = x + i * (btnW + gap);
			bool hovered = PointInRect(mx, my, bx, y, btnW, btnH);
			options[i].hoverT = Lerp(options[i].hoverT, hovered ? 1.0f : 0.0f, dt * Theme::Anim::SpeedFast);
			if (hovered && clicked) { selected = i; result = i; }
		}
		return result;
	}

	void Draw(Renderer& r, float btnW = 100) {
		float btnH = 28, gap = 6, radius = 6.0f;
		for (int i = 0; i < optionCount; i++) {
			float bx = x + i * (btnW + gap);
			if (i == selected) {
				r.FillRoundedRect(bx, y, btnW, btnH, radius, Theme::AccentPrimary());
				r.DrawText(options[i].text, bx, y, btnW, btnH, D2D1::ColorF(0xFFFFFF), r.pFontSmall, Renderer::AlignCenter);
			} else {
				D2D1_COLOR_F bg = LerpColor(Theme::BgElevated(), Theme::BgHover(), options[i].hoverT);
				r.FillRoundedRect(bx, y, btnW, btnH, radius, bg);
				r.DrawRoundedRect(bx, y, btnW, btnH, radius, Theme::BorderSubtle());
				D2D1_COLOR_F text = LerpColor(Theme::TextMuted(), Theme::TextPrimary(), options[i].hoverT);
				r.DrawText(options[i].text, bx, y, btnW, btnH, text, r.pFontSmall, Renderer::AlignCenter);
			}
		}
	}
};


struct CycleSelector {
	float x = 0, y = 0, width = 0;
	int selected = 0;
	const wchar_t* label = L"";
	const wchar_t* tooltip = L"";
	float hoverT = 0;
	const wchar_t* options[16];
	int optionCount = 0;

	void AddOption(const wchar_t* opt) { if (optionCount < 16) options[optionCount++] = opt; }
	void Layout(float px, float py, float w, const wchar_t* lbl, const wchar_t* tip = L"") { x = px; y = py; width = w; label = lbl; tooltip = tip; }

	bool Update(float mx, float my, bool clicked, float dt) {
		float valX = x + width * 0.45f, valW = width * 0.55f;
		bool hovered = PointInRect(mx, my, valX, y, valW, 28);
		hoverT = Lerp(hoverT, hovered ? 1.0f : 0.0f, dt * Theme::Anim::SpeedFast);
		if (hovered && tooltip[0]) Tooltip::Show(mx, my, tooltip, dt);
		if (hovered && clicked && optionCount > 0) { selected = (selected + 1) % optionCount; return true; }
		return false;
	}

	void Draw(Renderer& r) {
		float h = 28, radius = 6.0f;
		r.DrawText(label, x, y, width * 0.45f, h, Theme::TextSecondary(), r.pFontSmall);
		float valX = x + width * 0.45f, valW = width * 0.55f;
		D2D1_COLOR_F bg = LerpColor(Theme::BgElevated(), Theme::BgHover(), hoverT);
		r.FillRoundedRect(valX, y, valW, h, radius, bg);
		r.DrawRoundedRect(valX, y, valW, h, radius, Theme::BorderSubtle());
		const wchar_t* text = (selected >= 0 && selected < optionCount) ? options[selected] : L"?";
		D2D1_COLOR_F textCol = LerpColor(Theme::TextPrimary(), Theme::AccentPrimary(), hoverT * 0.3f);
		r.DrawText(text, valX, y, valW, h, textCol, r.pFontSmall, Renderer::AlignCenter);
	}
};


struct ColorPicker {
	float x = 0, y = 0, width = 200, height = 120;
	float hue = 0.62f, sat = 0.4f, val = 0.83f;
	bool draggingSV = false, draggingH = false;
	float hoverT = 0;

	void Layout(float px, float py, float w) { x = px; y = py; width = w; height = w * 0.55f; }

	static void HSVtoRGB(float h, float s, float v, float& r, float& g, float& b) {
		int i = (int)(h * 6.0f); float f = h * 6.0f - i;
		float p = v * (1.0f - s), q = v * (1.0f - f * s), t = v * (1.0f - (1.0f - f) * s);
		switch (i % 6) {
		case 0: r=v; g=t; b=p; break; case 1: r=q; g=v; b=p; break;
		case 2: r=p; g=v; b=t; break; case 3: r=p; g=q; b=v; break;
		case 4: r=t; g=p; b=v; break; default: r=v; g=p; b=q; break;
		}
	}

	static void RGBtoHSV(float r, float g, float b, float& h, float& s, float& v) {
		float mx = (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b);
		float mn = (r < g) ? ((r < b) ? r : b) : ((g < b) ? g : b);
		float d = mx - mn; v = mx;
		s = (mx > 0.001f) ? d / mx : 0;
		if (d < 0.001f) { h = 0; return; }
		if (r >= mx) h = (g - b) / d; else if (g >= mx) h = 2.0f + (b - r) / d; else h = 4.0f + (r - g) / d;
		h /= 6.0f; if (h < 0) h += 1.0f;
	}

	void SetRGB(float r, float g, float b) { RGBtoHSV(r, g, b, hue, sat, val); }
	void GetRGB(float& r, float& g, float& b) const { HSVtoRGB(hue, sat, val, r, g, b); }

	bool Update(float mx, float my, bool mouseDown, bool clicked, float dt) {
		float svW = width - 28, svH = height;
		float hBarX = x + svW + 8, hBarW = 16;
		bool inSV = PointInRect(mx, my, x, y, svW, svH);
		bool inH = PointInRect(mx, my, hBarX, y, hBarW, svH);
		if (clicked && inSV) draggingSV = true;
		if (clicked && inH) draggingH = true;
		if (!mouseDown) { draggingSV = false; draggingH = false; }
		bool changed = false;
		if (draggingSV) {
			sat = Clamp((mx - x) / svW, 0, 1);
			val = Clamp(1.0f - (my - y) / svH, 0, 1);
			changed = true;
		}
		if (draggingH) {
			hue = Clamp((my - y) / svH, 0, 0.999f);
			changed = true;
		}
		return changed;
	}

	void Draw(Renderer& r) {
		float svW = width - 28, svH = height;
		float hBarX = x + svW + 8, hBarW = 16;

		float pureR, pureG, pureB;
		HSVtoRGB(hue, 1, 1, pureR, pureG, pureB);

		r.FillRoundedRect(x, y, svW, svH, 4, D2D1::ColorF(pureR, pureG, pureB));
		r.FillRectGradientH(x, y, svW, svH, D2D1::ColorF(1,1,1,1), D2D1::ColorF(1,1,1,0));
		r.FillRectGradientV(x, y, svW, svH, D2D1::ColorF(0,0,0,0), D2D1::ColorF(0,0,0,1));
		r.DrawRoundedRect(x, y, svW, svH, 4, Theme::BorderSubtle());

		float cx = x + sat * svW, cy = y + (1 - val) * svH;
		r.DrawCircle(cx, cy, 5, D2D1::ColorF(1,1,1), 1.5f);
		r.DrawCircle(cx, cy, 4, D2D1::ColorF(0,0,0,0.5f), 1.0f);

		int hueSteps = 6;
		float stepH = svH / hueSteps;
		for (int i = 0; i < hueSteps; i++) {
			float h1 = (float)i / hueSteps, h2 = (float)(i + 1) / hueSteps;
			float r1, g1, b1, r2, g2, b2;
			HSVtoRGB(h1, 1, 1, r1, g1, b1);
			HSVtoRGB(h2, 1, 1, r2, g2, b2);
			r.FillRectGradientV(hBarX, y + i * stepH, hBarW, stepH + 1,
				D2D1::ColorF(r1, g1, b1), D2D1::ColorF(r2, g2, b2));
		}
		r.DrawRoundedRect(hBarX, y, hBarW, svH, 4, Theme::BorderSubtle());

		float hueY = y + hue * svH;
		r.FillRoundedRect(hBarX - 1, hueY - 2, hBarW + 2, 5, 2, D2D1::ColorF(1,1,1));
		r.DrawRoundedRect(hBarX - 1, hueY - 2, hBarW + 2, 5, 2, D2D1::ColorF(0,0,0,0.4f));

		float previewX = x, previewY = y + svH + 8;
		float curR, curG, curB; GetRGB(curR, curG, curB);
		r.FillRoundedRect(previewX, previewY, svW + hBarW + 8, 24, 4, D2D1::ColorF(curR, curG, curB));
		r.DrawRoundedRect(previewX, previewY, svW + hBarW + 8, 24, 4, Theme::BorderSubtle());

		wchar_t hexBuf[16];
		int ri = (int)(curR * 255), gi = (int)(curG * 255), bi = (int)(curB * 255);
		swprintf_s(hexBuf, L"#%02X%02X%02X", ri, gi, bi);
		r.DrawText(hexBuf, previewX, previewY, svW + hBarW + 8, 24,
			(val > 0.5f && sat < 0.5f) ? Theme::BgBase() : D2D1::ColorF(1,1,1),
			r.pFontSmall, Renderer::AlignCenter);
	}

	float GetTotalHeight() const { return height + 40; }
};


struct TextInput {
	float x = 0, y = 0, width = 0;
	wchar_t buffer[256] = {};
	int cursor = 0;
	int selStart = 0, selEnd = 0;
	bool focused = false;
	float blinkT = 0, hoverT = 0;
	const wchar_t* placeholder = L"";
	bool isDraggingText = false;

	void Layout(float px, float py, float w, const wchar_t* ph) { x = px; y = py; width = w; placeholder = ph; }

	int BufLen() const { return (int)wcslen(buffer); }
	void ClearSel() { selStart = selEnd = cursor; }

	bool GetSelRange(int& s, int& e) const {
		s = selStart < selEnd ? selStart : selEnd;
		e = selStart > selEnd ? selStart : selEnd;
		return e > s;
	}

	bool DeleteSel() {
		int s = 0, e = 0;
		if (!GetSelRange(s, e)) return false;
		int len = BufLen();
		for (int i = s; i <= len - (e - s); i++) buffer[i] = buffer[i + (e - s)];
		cursor = s; ClearSel();
		return true;
	}

	void MoveCur(int nc, bool shift) {
		int len = BufLen();
		if (nc < 0) nc = 0; if (nc > len) nc = len;
		int old = cursor; cursor = nc;
		if (shift) { if (selStart == selEnd) selStart = old; selEnd = cursor; }
		else ClearSel();
	}

	bool Update(float mx, float my, bool mouseDown, bool clicked, float dt) {
		bool hovered = PointInRect(mx, my, x, y, width, 28);
		hoverT = Lerp(hoverT, hovered ? 1.0f : 0.0f, dt * Theme::Anim::SpeedFast);
		blinkT += dt;

		float charW = 7.2f;
		float pad = 8.0f;

		if (clicked) {
			if (hovered) {
				bool wasFocused = focused;
				focused = true;
				if (!wasFocused) blinkT = 0;
				int pos = (int)((mx - x - pad) / charW + 0.5f);
				if (pos < 0) pos = 0; if (pos > BufLen()) pos = BufLen();
				bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
				MoveCur(pos, shift);
				isDraggingText = true;
				blinkT = 0;
			} else {
				focused = false;
				isDraggingText = false;
			}
		}

		
		if (isDraggingText && mouseDown && !clicked && focused) {
			int pos = (int)((mx - x - pad) / charW + 0.5f);
			if (pos < 0) pos = 0; if (pos > BufLen()) pos = BufLen();
			MoveCur(pos, true);
		}
		if (!mouseDown) isDraggingText = false;

		return false;
	}

	bool OnChar(wchar_t ch) {
		if (!focused) return false;
		if (ch == L'\r' || ch == L'\n') return true;
		if (ch == 27) { focused = false; return false; }
		if (ch == L'\b') {
			if (!DeleteSel() && cursor > 0) {
				for (int i = cursor - 1; buffer[i]; i++) buffer[i] = buffer[i + 1];
				cursor--; ClearSel();
			}
			return false;
		}
		if (ch < 32) return false;
		DeleteSel();
		int len = BufLen();
		if (len < 254) {
			for (int i = len + 1; i > cursor; i--) buffer[i] = buffer[i - 1];
			buffer[cursor] = ch; cursor++; ClearSel();
		}
		return false;
	}

	bool OnKeyDown(int vk) {
		if (!focused) return false;
		bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
		bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
		if (ctrl && vk == 'A') { selStart = 0; selEnd = BufLen(); cursor = selEnd; return true; }
		if (ctrl && vk == 'C') { CopyToClipboard(); return true; }
		if (ctrl && vk == 'X') { CopyToClipboard(); DeleteSel(); return true; }
		if (ctrl && vk == 'V') { PasteFromClipboard(); return true; }
		switch (vk) {
		case VK_LEFT: MoveCur(cursor - 1, shift); return true;
		case VK_RIGHT: MoveCur(cursor + 1, shift); return true;
		case VK_HOME: MoveCur(0, shift); return true;
		case VK_END: MoveCur(BufLen(), shift); return true;
		case VK_DELETE:
			if (!DeleteSel()) {
				int len = BufLen();
				if (cursor < len) { for (int i = cursor; buffer[i]; i++) buffer[i] = buffer[i + 1]; }
			}
			return true;
		}
		return false;
	}

	void CopyToClipboard() {
		int s = 0, e = 0;
		if (!GetSelRange(s, e)) return;
		if (!OpenClipboard(nullptr)) return;
		int count = e - s;
		HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, (count + 1) * sizeof(wchar_t));
		if (mem) {
			wchar_t* out = (wchar_t*)GlobalLock(mem);
			if (out) { for (int i = 0; i < count; i++) out[i] = buffer[s + i]; out[count] = 0; GlobalUnlock(mem); EmptyClipboard(); if (!SetClipboardData(CF_UNICODETEXT, mem)) GlobalFree(mem); } else GlobalFree(mem);
		}
		CloseClipboard();
	}

	void PasteFromClipboard() {
		if (!OpenClipboard(nullptr)) return;
		HANDLE data = GetClipboardData(CF_UNICODETEXT);
		if (data) {
			const wchar_t* t = (const wchar_t*)GlobalLock(data);
			if (t) {
				DeleteSel();
				for (int i = 0; t[i] && BufLen() < 254; i++) {
					if (t[i] >= 32) {
						int len = BufLen();
						for (int j = len + 1; j > cursor; j--) buffer[j] = buffer[j - 1];
						buffer[cursor] = t[i]; cursor++;
					}
				}
				ClearSel();
				GlobalUnlock(data);
			}
		}
		CloseClipboard();
	}

	std::wstring GetText() const { return std::wstring(buffer); }
	void Clear() { buffer[0] = 0; cursor = 0; ClearSel(); }

	void Draw(Renderer& r) {
		float h = 28, radius = 6.0f, pad = 8.0f;
		D2D1_COLOR_F bg = focused ? Theme::BgElevated() : Theme::BgSurface();
		r.FillRoundedRect(x, y, width, h, radius, bg);
		D2D1_COLOR_F border = focused ? Theme::AccentPrimary() : Theme::BorderSubtle();
		r.DrawRoundedRect(x, y, width, h, radius, border);

		if (buffer[0] == 0 && !focused) {
			r.DrawText(placeholder, x + pad, y, width - pad * 2, h, Theme::TextMuted(), r.pFontSmall);
			return;
		}

		float charW = 7.2f;
		float textX = x + pad;

		
		if (focused) {
			int s = 0, e = 0;
			if (GetSelRange(s, e)) {
				D2D1_COLOR_F sel = Theme::AccentPrimary(); sel.a = 0.3f;
				r.FillRoundedRect(textX + s * charW, y + 4, (float)(e - s) * charW, h - 8, 2, sel);
			}
		}

		
		IDWriteTextFormat* textFont = focused ? r.pFontMono : r.pFontSmall;
		r.DrawText(buffer, textX, y, width - pad * 2, h, Theme::TextPrimary(), textFont);

		
		if (focused && fmod(blinkT, 1.0f) < 0.5f)
			r.DrawLine(textX + cursor * charW, y + 5, textX + cursor * charW, y + h - 5, Theme::AccentPrimary(), 1.0f);
	}
};
