#pragma once
#include "Framework.h"

/// UI color palette, sizing constants, font settings, and animation speeds
namespace Theme {

	// =============================================
	// Full theme data — background, text, borders
	// =============================================
	namespace Base {
		inline float BgDeepR = 0.020f, BgDeepG = 0.020f, BgDeepB = 0.024f;
		inline float BgBaseR = 0.035f, BgBaseG = 0.035f, BgBaseB = 0.043f;
		inline float BgSurfR = 0.055f, BgSurfG = 0.055f, BgSurfB = 0.063f;
		inline float BgElevR = 0.086f, BgElevG = 0.086f, BgElevB = 0.094f;
		inline float BgHoverR = 0.122f, BgHoverG = 0.122f, BgHoverB = 0.141f;

		inline float TextPriR = 0.871f, TextPriG = 0.871f, TextPriB = 0.871f;
		inline float TextSecR = 0.620f, TextSecG = 0.620f, TextSecB = 0.620f;
		inline float TextMutR = 0.420f, TextMutG = 0.420f, TextMutB = 0.420f;

		inline float BorderSubR = 0.145f, BorderSubG = 0.145f, BorderSubB = 0.145f, BorderSubA = 0.5f;
		inline float BorderNormR = 0.231f, BorderNormG = 0.231f, BorderNormB = 0.231f, BorderNormA = 0.3f;

		inline float SuccessR = 0.439f, SuccessG = 0.831f, SuccessB = 0.627f;
		inline float WarningR = 0.910f, WarningG = 0.784f, WarningB = 0.251f;
		inline float ErrorR = 0.878f, ErrorG = 0.376f, ErrorB = 0.376f;
	}

	namespace Custom {
		inline float AccentR = 0.498f;
		inline float AccentG = 0.608f;
		inline float AccentB = 0.831f;

		inline void SetAccent(float r, float g, float b) {
			AccentR = r; AccentG = g; AccentB = b;
		}

		inline D2D1_COLOR_F Accent(float alpha = 1.0f) {
			return D2D1::ColorF(AccentR, AccentG, AccentB, alpha);
		}

		inline D2D1_COLOR_F AccentLight(float alpha = 1.0f) {
			float f = 0.3f;
			return D2D1::ColorF(
				AccentR + (1.0f - AccentR) * f,
				AccentG + (1.0f - AccentG) * f,
				AccentB + (1.0f - AccentB) * f, alpha);
		}

		inline D2D1_COLOR_F AccentDark(float alpha = 1.0f) {
			return D2D1::ColorF(AccentR * 0.7f, AccentG * 0.7f, AccentB * 0.7f, alpha);
		}
	}

	// Background colors — now use Base:: values
	inline D2D1_COLOR_F BgDeep()       { return D2D1::ColorF(Base::BgDeepR, Base::BgDeepG, Base::BgDeepB); }
	inline D2D1_COLOR_F BgBase()       { return D2D1::ColorF(Base::BgBaseR, Base::BgBaseG, Base::BgBaseB); }
	inline D2D1_COLOR_F BgSurface()    { return D2D1::ColorF(Base::BgSurfR, Base::BgSurfG, Base::BgSurfB); }
	inline D2D1_COLOR_F BgElevated()   { return D2D1::ColorF(Base::BgElevR, Base::BgElevG, Base::BgElevB); }
	inline D2D1_COLOR_F BgHover()      { return D2D1::ColorF(Base::BgHoverR, Base::BgHoverG, Base::BgHoverB); }

	inline D2D1_COLOR_F AccentPrimary()   { return Custom::Accent(); }
	inline D2D1_COLOR_F AccentSecondary() { return Custom::AccentLight(); }
	inline D2D1_COLOR_F BrandHot()        { return Custom::AccentDark(); }
	inline D2D1_COLOR_F AccentGlow()      { return Custom::Accent(0.22f); }
	inline D2D1_COLOR_F AccentDim()       { return Custom::Accent(0.12f); }

	inline D2D1_COLOR_F TextPrimary()    { return D2D1::ColorF(Base::TextPriR, Base::TextPriG, Base::TextPriB); }
	inline D2D1_COLOR_F TextSecondary()  { return D2D1::ColorF(Base::TextSecR, Base::TextSecG, Base::TextSecB); }
	inline D2D1_COLOR_F TextMuted()      { return D2D1::ColorF(Base::TextMutR, Base::TextMutG, Base::TextMutB); }
	inline D2D1_COLOR_F TextAccent()     { return Custom::AccentLight(0.85f); }

	inline D2D1_COLOR_F Success()  { return D2D1::ColorF(Base::SuccessR, Base::SuccessG, Base::SuccessB); }
	inline D2D1_COLOR_F Warning()  { return D2D1::ColorF(Base::WarningR, Base::WarningG, Base::WarningB); }
	inline D2D1_COLOR_F Error()    { return D2D1::ColorF(Base::ErrorR, Base::ErrorG, Base::ErrorB); }

	inline D2D1_COLOR_F BorderSubtle()  { return D2D1::ColorF(Base::BorderSubR, Base::BorderSubG, Base::BorderSubB, Base::BorderSubA); }
	inline D2D1_COLOR_F BorderNormal()  { return D2D1::ColorF(Base::BorderNormR, Base::BorderNormG, Base::BorderNormB, Base::BorderNormA); }
	inline D2D1_COLOR_F BorderAccent()  { return Custom::Accent(0.35f); }

	// =============================================
	// Full theme struct for presets
	// =============================================
	struct ThemeData {
		const wchar_t* name;
		// Background
		float bgDeep[3], bgBase[3], bgSurface[3], bgElevated[3], bgHover[3];
		// Text
		float textPri[3], textSec[3], textMut[3];
		// Borders
		float borderSub[4], borderNorm[4];
		// Accent
		float accent[3];
	};

	inline void ApplyTheme(const ThemeData& t) {
		Base::BgDeepR = t.bgDeep[0]; Base::BgDeepG = t.bgDeep[1]; Base::BgDeepB = t.bgDeep[2];
		Base::BgBaseR = t.bgBase[0]; Base::BgBaseG = t.bgBase[1]; Base::BgBaseB = t.bgBase[2];
		Base::BgSurfR = t.bgSurface[0]; Base::BgSurfG = t.bgSurface[1]; Base::BgSurfB = t.bgSurface[2];
		Base::BgElevR = t.bgElevated[0]; Base::BgElevG = t.bgElevated[1]; Base::BgElevB = t.bgElevated[2];
		Base::BgHoverR = t.bgHover[0]; Base::BgHoverG = t.bgHover[1]; Base::BgHoverB = t.bgHover[2];
		Base::TextPriR = t.textPri[0]; Base::TextPriG = t.textPri[1]; Base::TextPriB = t.textPri[2];
		Base::TextSecR = t.textSec[0]; Base::TextSecG = t.textSec[1]; Base::TextSecB = t.textSec[2];
		Base::TextMutR = t.textMut[0]; Base::TextMutG = t.textMut[1]; Base::TextMutB = t.textMut[2];
		Base::BorderSubR = t.borderSub[0]; Base::BorderSubG = t.borderSub[1]; Base::BorderSubB = t.borderSub[2]; Base::BorderSubA = t.borderSub[3];
		Base::BorderNormR = t.borderNorm[0]; Base::BorderNormG = t.borderNorm[1]; Base::BorderNormB = t.borderNorm[2]; Base::BorderNormA = t.borderNorm[3];
		Custom::SetAccent(t.accent[0], t.accent[1], t.accent[2]);
	}

	// =============================================
	// Built-in themes
	// =============================================
	namespace Themes {
		// Midnight — the original dark theme
		inline const ThemeData Midnight = {
			L"Midnight",
			{0.020f, 0.020f, 0.024f}, {0.035f, 0.035f, 0.043f}, {0.055f, 0.055f, 0.063f}, {0.086f, 0.086f, 0.094f}, {0.122f, 0.122f, 0.141f},
			{0.871f, 0.871f, 0.871f}, {0.620f, 0.620f, 0.620f}, {0.420f, 0.420f, 0.420f},
			{0.145f, 0.145f, 0.145f, 0.5f}, {0.231f, 0.231f, 0.231f, 0.3f},
			{0.498f, 0.608f, 0.831f}
		};

		// Abyss — ultra dark with deep blue tint
		inline const ThemeData Abyss = {
			L"Abyss",
			{0.008f, 0.012f, 0.028f}, {0.016f, 0.024f, 0.048f}, {0.028f, 0.038f, 0.068f}, {0.045f, 0.058f, 0.095f}, {0.065f, 0.082f, 0.130f},
			{0.820f, 0.855f, 0.920f}, {0.500f, 0.545f, 0.640f}, {0.320f, 0.360f, 0.460f},
			{0.080f, 0.100f, 0.170f, 0.6f}, {0.120f, 0.150f, 0.240f, 0.4f},
			{0.350f, 0.520f, 0.920f}
		};

		// Rosé — warm pink/mauve tones
		inline const ThemeData Rose = {
			L"Ros\x00E9",
			{0.030f, 0.020f, 0.025f}, {0.050f, 0.035f, 0.042f}, {0.075f, 0.055f, 0.065f}, {0.105f, 0.080f, 0.092f}, {0.150f, 0.115f, 0.130f},
			{0.920f, 0.860f, 0.880f}, {0.660f, 0.580f, 0.610f}, {0.440f, 0.380f, 0.410f},
			{0.180f, 0.130f, 0.150f, 0.5f}, {0.260f, 0.200f, 0.220f, 0.35f},
			{0.886f, 0.490f, 0.620f}
		};

		// Nord — cool grey-blue Scandinavian palette
		inline const ThemeData Nord = {
			L"Nord",
			{0.020f, 0.024f, 0.030f}, {0.040f, 0.047f, 0.058f}, {0.060f, 0.070f, 0.086f}, {0.090f, 0.102f, 0.122f}, {0.130f, 0.145f, 0.170f},
			{0.850f, 0.870f, 0.895f}, {0.580f, 0.610f, 0.660f}, {0.380f, 0.410f, 0.460f},
			{0.150f, 0.170f, 0.200f, 0.5f}, {0.220f, 0.245f, 0.280f, 0.35f},
			{0.530f, 0.750f, 0.820f}
		};

		// Ember — warm charcoal with orange glow
		inline const ThemeData Ember = {
			L"Ember",
			{0.028f, 0.018f, 0.012f}, {0.050f, 0.035f, 0.025f}, {0.078f, 0.058f, 0.042f}, {0.110f, 0.084f, 0.062f}, {0.155f, 0.120f, 0.090f},
			{0.930f, 0.880f, 0.840f}, {0.660f, 0.600f, 0.540f}, {0.450f, 0.400f, 0.350f},
			{0.180f, 0.140f, 0.100f, 0.5f}, {0.260f, 0.200f, 0.150f, 0.35f},
			{0.920f, 0.520f, 0.220f}
		};

		// Void — pure black OLED theme
		inline const ThemeData Void = {
			L"Void",
			{0.000f, 0.000f, 0.000f}, {0.012f, 0.012f, 0.012f}, {0.030f, 0.030f, 0.030f}, {0.055f, 0.055f, 0.055f}, {0.085f, 0.085f, 0.085f},
			{0.900f, 0.900f, 0.900f}, {0.580f, 0.580f, 0.580f}, {0.360f, 0.360f, 0.360f},
			{0.110f, 0.110f, 0.110f, 0.5f}, {0.180f, 0.180f, 0.180f, 0.3f},
			{0.700f, 0.700f, 0.700f}
		};

		// Matcha — earthy green tea tones
		inline const ThemeData Matcha = {
			L"Matcha",
			{0.016f, 0.025f, 0.016f}, {0.030f, 0.045f, 0.032f}, {0.048f, 0.068f, 0.050f}, {0.072f, 0.098f, 0.076f}, {0.105f, 0.140f, 0.110f},
			{0.860f, 0.910f, 0.860f}, {0.580f, 0.650f, 0.580f}, {0.380f, 0.440f, 0.380f},
			{0.120f, 0.165f, 0.125f, 0.5f}, {0.180f, 0.240f, 0.185f, 0.35f},
			{0.420f, 0.780f, 0.460f}
		};

		// Lavender — soft purple haze
		inline const ThemeData Lavender = {
			L"Lavender",
			{0.024f, 0.018f, 0.032f}, {0.042f, 0.034f, 0.055f}, {0.065f, 0.054f, 0.080f}, {0.092f, 0.078f, 0.112f}, {0.130f, 0.112f, 0.158f},
			{0.880f, 0.860f, 0.920f}, {0.610f, 0.580f, 0.660f}, {0.410f, 0.385f, 0.450f},
			{0.150f, 0.130f, 0.185f, 0.5f}, {0.220f, 0.195f, 0.265f, 0.35f},
			{0.690f, 0.460f, 0.860f}
		};

		// ============ LIGHT THEMES ============

		// Snow — clean white minimal
		inline const ThemeData Snow = {
			L"Snow",
			{0.940f, 0.940f, 0.945f}, {0.960f, 0.960f, 0.965f}, {0.975f, 0.975f, 0.980f}, {1.000f, 1.000f, 1.000f}, {0.920f, 0.925f, 0.935f},
			{0.120f, 0.120f, 0.140f}, {0.400f, 0.400f, 0.440f}, {0.600f, 0.600f, 0.640f},
			{0.850f, 0.855f, 0.870f, 0.6f}, {0.800f, 0.805f, 0.825f, 0.4f},
			{0.300f, 0.480f, 0.860f}
		};

		// Linen — warm off-white with subtle beige
		inline const ThemeData Linen = {
			L"Linen",
			{0.930f, 0.920f, 0.900f}, {0.950f, 0.940f, 0.920f}, {0.965f, 0.958f, 0.940f}, {0.985f, 0.980f, 0.965f}, {0.910f, 0.900f, 0.880f},
			{0.180f, 0.150f, 0.120f}, {0.420f, 0.390f, 0.350f}, {0.600f, 0.575f, 0.540f},
			{0.840f, 0.825f, 0.800f, 0.5f}, {0.790f, 0.775f, 0.750f, 0.35f},
			{0.720f, 0.520f, 0.300f}
		};

		// Frost — icy blue-white
		inline const ThemeData Frost = {
			L"Frost",
			{0.920f, 0.935f, 0.950f}, {0.940f, 0.952f, 0.968f}, {0.955f, 0.966f, 0.980f}, {0.978f, 0.985f, 0.995f}, {0.900f, 0.918f, 0.940f},
			{0.100f, 0.140f, 0.200f}, {0.350f, 0.400f, 0.480f}, {0.550f, 0.590f, 0.650f},
			{0.830f, 0.850f, 0.885f, 0.6f}, {0.780f, 0.805f, 0.845f, 0.4f},
			{0.220f, 0.520f, 0.820f}
		};

		// Blossom — light pink/sakura white
		inline const ThemeData Blossom = {
			L"Blossom",
			{0.945f, 0.925f, 0.935f}, {0.960f, 0.942f, 0.950f}, {0.975f, 0.960f, 0.966f}, {0.992f, 0.980f, 0.985f}, {0.925f, 0.905f, 0.915f},
			{0.200f, 0.120f, 0.150f}, {0.450f, 0.370f, 0.400f}, {0.620f, 0.560f, 0.580f},
			{0.870f, 0.840f, 0.855f, 0.5f}, {0.825f, 0.795f, 0.810f, 0.35f},
			{0.850f, 0.400f, 0.550f}
		};

		// Custom — user-editable slot (mutable, not const)
		inline ThemeData Custom = {
			L"Custom",
			{0.020f, 0.020f, 0.024f}, {0.035f, 0.035f, 0.043f}, {0.055f, 0.055f, 0.063f}, {0.086f, 0.086f, 0.094f}, {0.122f, 0.122f, 0.141f},
			{0.871f, 0.871f, 0.871f}, {0.620f, 0.620f, 0.620f}, {0.420f, 0.420f, 0.420f},
			{0.145f, 0.145f, 0.145f, 0.5f}, {0.231f, 0.231f, 0.231f, 0.3f},
			{0.498f, 0.608f, 0.831f}
		};
	}

	/// Helper: is current theme light? (for DWM titlebar)
	inline bool IsLightTheme() {
		return (Base::BgBaseR + Base::BgBaseG + Base::BgBaseB) / 3.0f > 0.5f;
	}

	/// Snapshot current Base values into the Custom theme slot
	inline void SaveCurrentToCustom() {
		auto& c = Themes::Custom;
		c.bgDeep[0] = Base::BgDeepR; c.bgDeep[1] = Base::BgDeepG; c.bgDeep[2] = Base::BgDeepB;
		c.bgBase[0] = Base::BgBaseR; c.bgBase[1] = Base::BgBaseG; c.bgBase[2] = Base::BgBaseB;
		c.bgSurface[0] = Base::BgSurfR; c.bgSurface[1] = Base::BgSurfG; c.bgSurface[2] = Base::BgSurfB;
		c.bgElevated[0] = Base::BgElevR; c.bgElevated[1] = Base::BgElevG; c.bgElevated[2] = Base::BgElevB;
		c.bgHover[0] = Base::BgHoverR; c.bgHover[1] = Base::BgHoverG; c.bgHover[2] = Base::BgHoverB;
		c.textPri[0] = Base::TextPriR; c.textPri[1] = Base::TextPriG; c.textPri[2] = Base::TextPriB;
		c.textSec[0] = Base::TextSecR; c.textSec[1] = Base::TextSecG; c.textSec[2] = Base::TextSecB;
		c.textMut[0] = Base::TextMutR; c.textMut[1] = Base::TextMutG; c.textMut[2] = Base::TextMutB;
		c.borderSub[0] = Base::BorderSubR; c.borderSub[1] = Base::BorderSubG; c.borderSub[2] = Base::BorderSubB; c.borderSub[3] = Base::BorderSubA;
		c.borderNorm[0] = Base::BorderNormR; c.borderNorm[1] = Base::BorderNormG; c.borderNorm[2] = Base::BorderNormB; c.borderNorm[3] = Base::BorderNormA;
		c.accent[0] = Custom::AccentR; c.accent[1] = Custom::AccentG; c.accent[2] = Custom::AccentB;
	}

	namespace Size {
		constexpr float WindowWidth = 860.0f;
		constexpr float WindowHeight = 720.0f;
		constexpr float SidebarWidth = 64.0f;
		constexpr float HeaderHeight = 48.0f;
		constexpr float Padding = 16.0f;
		constexpr float PaddingSmall = 8.0f;
		constexpr float CornerRadius = 10.0f;
		constexpr float CornerRadiusSmall = 6.0f;
		constexpr float SliderHeight = 6.0f;
		constexpr float SliderThumbRadius = 8.0f;
		constexpr float ToggleWidth = 40.0f;
		constexpr float ToggleHeight = 22.0f;
		constexpr float ButtonHeight = 36.0f;
	}

	namespace Runtime {
		inline float WindowWidth = Size::WindowWidth;
		inline float WindowHeight = Size::WindowHeight;

		inline void SetWindowSize(float width, float height) {
			WindowWidth = (width < 320.0f) ? 320.0f : ((width > 8192.0f) ? 8192.0f : width);
			WindowHeight = (height < 240.0f) ? 240.0f : ((height > 8192.0f) ? 8192.0f : height);
		}
	}

	namespace Font {
		constexpr wchar_t FamilyBrand[] = L"badcache";
		constexpr wchar_t Family[] = L"Segoe UI";
		constexpr wchar_t FamilyMono[] = L"Consolas";
		constexpr float SizeTitle = 22.0f;
		constexpr float SizeHeading = 14.0f;
		constexpr float SizeBody = 12.0f;
		constexpr float SizeSmall = 10.0f;
		constexpr float SizeCaption = 9.0f;
	}

	namespace Anim {
		constexpr float Speed = 8.0f;
		constexpr float SpeedFast = 14.0f;
		constexpr float SpeedSlow = 4.0f;
	}
}

/// Linear interpolation between a and b by factor t (0..1)
inline float Lerp(float a, float b, float t) {
	return a + (b - a) * t;
}

/// Interpolate each RGBA channel between two colors by factor t
inline D2D1_COLOR_F LerpColor(D2D1_COLOR_F a, D2D1_COLOR_F b, float t) {
	return D2D1::ColorF(
		Lerp(a.r, b.r, t),
		Lerp(a.g, b.g, t),
		Lerp(a.b, b.b, t),
		Lerp(a.a, b.a, t)
	);
}

/// Constrain value v to the range [lo, hi]
inline float Clamp(float v, float lo, float hi) {
	return (v < lo) ? lo : (v > hi) ? hi : v;
}
