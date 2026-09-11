// Radial Follow Smoothing - native Aether plugin.
// Port of AbstractQbit's RadialFollowCore (C#). Units: the host pushes
// tablet.mmScaleX / tablet.mmScaleY (tablet units -> mm); all filter math
// runs in millimetres.

#include <cmath>
#include <cstdio>
#include <windows.h>
#include <cstring>

#define AETHER_PLUGIN_CALL __cdecl
#define AETHER_PLUGIN_API_VERSION 1

typedef struct AetherPluginInfo {
	int apiVersion;
	const char* name;
	const char* description;
} AetherPluginInfo;

enum AetherPluginOptionType {
	AETHER_PLUGIN_OPTION_SLIDER = 0,
	AETHER_PLUGIN_OPTION_TOGGLE = 1
};

typedef struct AetherPluginOptionInfo {
	int apiVersion;
	const char* key;
	const char* label;
	int type;
	double minValue;
	double maxValue;
	double defaultValue;
	const char* format;
	const char* description;
} AetherPluginOptionInfo;

typedef struct AetherPluginPoint {
	double x;
	double y;
	double z;
	double dt;
	int isValid;
	int buttons;
	int tipDown;
	double pressure;
	double hoverDistance;
	double tiltX;
	double tiltY;
} AetherPluginPoint;

static double clampd(double v, double lo, double hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}

struct RadialFollowState {
	double rOuter = 1.0;
	double rInner = 0.0;
	double smoothCoef = 0.95;
	double knScale = 1.0;
	double leakCoef = 0.0;

	double mmScaleX = 1.0;
	double mmScaleY = 1.0;

	double cursorX = 0.0;
	double cursorY = 0.0;
	bool hasCursor = false;

	// derived knee params
	double xOffset = -1.0;
	double scaleComp = 1.0;

	RadialFollowState() { updateDerivedParams(); }

	void updateDerivedParams() {
		if (knScale > 0.0001) {
			xOffset = getXOffset();
			scaleComp = derivKneeScaled(xOffset);
		} else {
			xOffset = -1.0;
			scaleComp = 1.0;
		}
	}

	// kneeFunc(x): x < -3 -> x; x < 3 -> ln(tanh(e^x)); else 0
	static double kneeFunc(double x) {
		if (x < -3.0) return x;
		if (x < 3.0) return log(tanh(exp(x)));
		return 0.0;
	}

	double kneeScaled(double x) const {
		if (knScale > 0.0001)
			return knScale * kneeFunc(x / knScale) + 1.0;
		return x > 0.0 ? 1.0 : 1.0 + x;
	}

	static double inverseTanh(double x) {
		return log((1.0 + x) / (1.0 - x)) / 2.0;
	}

	double inverseKneeScaled(double x) const {
		return knScale * log(inverseTanh(exp((x - 1.0) / knScale)));
	}

	double derivKneeScaled(double x) const {
		double e = exp(x / knScale);
		double t = tanh(e);
		return (e - e * t * t) / t;
	}

	double getXOffset() const { return inverseKneeScaled(0.0); }

	double rOuterAdjusted() const {
		double ro = rOuter > rInner + 0.0001 ? rOuter : rInner + 0.0001;
		return ro; // gridScale = 1
	}
	double rInnerAdjusted() const { return rInner; }

	double leakedFn(double x) const {
		return kneeScaled(x + xOffset) * (1.0 - leakCoef) + x * leakCoef * scaleComp;
	}

	double smoothedFn(double x) const {
		return leakedFn(x * smoothCoef / scaleComp);
	}

	double scaleToOuter(double x) const {
		double range = rOuterAdjusted() - rInnerAdjusted();
		return range * smoothedFn(x / range);
	}

	double deltaFn(double x) const {
		if (x > rInnerAdjusted())
			return x - scaleToOuter(x - rInnerAdjusted()) - rInnerAdjusted();
		return 0.0;
	}
};

extern "C" {

__declspec(dllexport) int AETHER_PLUGIN_CALL AetherPluginGetInfo(AetherPluginInfo* info) {
	if (!info) return 0;
	info->apiVersion = AETHER_PLUGIN_API_VERSION;
	info->name = "Radial Follow Smoothing";
	info->description = "The cursor follows the pen with a radial smoothing profile: a deadzone (inner radius), soft clamping (outer radius) and tunable softness/leak. Positions are filtered in millimetres.";
	return 1;
}

__declspec(dllexport) void* AETHER_PLUGIN_CALL AetherPluginCreate() {
	return new RadialFollowState();
}

__declspec(dllexport) void AETHER_PLUGIN_CALL AetherPluginDestroy(void* instance) {
	delete (RadialFollowState*)instance;
}

__declspec(dllexport) void AETHER_PLUGIN_CALL AetherPluginReset(void* instance, const AetherPluginPoint* point) {
	RadialFollowState* s = (RadialFollowState*)instance;
	if (!s || !point) return;
	s->cursorX = point->x * s->mmScaleX;
	s->cursorY = point->y * s->mmScaleY;
	s->hasCursor = true;
}

__declspec(dllexport) void AETHER_PLUGIN_CALL AetherPluginProcess(void* instance, AetherPluginPoint* point) {
	RadialFollowState* s = (RadialFollowState*)instance;
	if (!s || !point) return;

	double tx = point->x * s->mmScaleX;
	double ty = point->y * s->mmScaleY;

	if (!s->hasCursor) {
		s->cursorX = tx;
		s->cursorY = ty;
		s->hasCursor = true;
	}

	double dx = tx - s->cursorX;
	double dy = ty - s->cursorY;
	double dist = sqrt(dx * dx + dy * dy);
	double distToMove = s->deltaFn(dist);
	if (dist > 1e-12) {
		s->cursorX += dx / dist * distToMove;
		s->cursorY += dy / dist * distToMove;
	}

	double dtMs = point->dt * 1000.0;
	// debug trace: every 50th report
	{
		static int traceCounter = 0;
		if (++traceCounter >= 50) {
			traceCounter = 0;
			char dbg[256];
			sprintf_s(dbg, "RF: dist=%.4f inner=%.6f outer=%.6f coef=%.6f mmX=%.6f dt=%.3fms"
				" move=%.6f cur=(%.2f,%.2f) tgt=(%.2f,%.2f)",
				dist, s->rInner, s->rOuter, s->smoothCoef, s->mmScaleX, dtMs,
				distToMove, s->cursorX, s->cursorY, tx, ty);
			OutputDebugStringA(dbg);
		}
	}

	// NaN guard + pen redetection: snap when the gap between reports is too large
	if (!(s->cursorX > -1e30 && s->cursorX < 1e30 &&
	      s->cursorY > -1e30 && s->cursorY < 1e30) ||
	    dtMs > 50.0) {
		s->cursorX = tx;
		s->cursorY = ty;
	}

	point->x = s->cursorX / s->mmScaleX;
	point->y = s->cursorY / s->mmScaleY;
}

__declspec(dllexport) int AETHER_PLUGIN_CALL AetherPluginSetDouble(void* instance, const char* key, double value) {
	RadialFollowState* s = (RadialFollowState*)instance;
	if (!s || !key) return 0;
	if (strcmp(key, "outerRadius") == 0) {
		s->rOuter = clampd(value, 0.0, 1000.0);
		return 1;
	}
	if (strcmp(key, "innerRadius") == 0) {
		s->rInner = clampd(value, 0.0, 1000.0);
		return 1;
	}
	if (strcmp(key, "smoothingCoefficient") == 0) {
		s->smoothCoef = clampd(value, 0.0001, 1.0);
		return 1;
	}
	if (strcmp(key, "softKneeScale") == 0) {
		s->knScale = clampd(value, 0.0, 100.0);
		s->updateDerivedParams();
		return 1;
	}
	if (strcmp(key, "smoothingLeak") == 0) {
		s->leakCoef = clampd(value, 0.0, 1.0);
		return 1;
	}
	// host-provided unit scale (tablet units -> mm); rebuild cursor when it changes
	if (strcmp(key, "tablet.mmScaleX") == 0) {
		if (value > 0.000001) { s->mmScaleX = value; s->hasCursor = false; }
		return 1;
	}
	if (strcmp(key, "tablet.mmScaleY") == 0) {
		if (value > 0.000001) { s->mmScaleY = value; s->hasCursor = false; }
		return 1;
	}
	return 0;
}

__declspec(dllexport) int AETHER_PLUGIN_CALL AetherPluginGetOptionCount() {
	return 5;
}

__declspec(dllexport) int AETHER_PLUGIN_CALL AetherPluginGetOptionInfo(int index, AetherPluginOptionInfo* info) {
	static const char* keys[5] = { "outerRadius", "innerRadius", "smoothingCoefficient", "softKneeScale", "smoothingLeak" };
	static const char* labels[5] = { "Outer Radius", "Inner Radius", "Smoothing", "Soft Knee", "Smoothing Leak" };
	static const double mins[5] = { 0.0, 0.0, 0.0001, 0.0, 0.0 };
	static const double maxs[5] = { 30.0, 10.0, 1.0, 100.0, 1.0 };
	static const double defs[5] = { 1.0, 0.0, 0.95, 1.0, 0.0 };
	static const char* fmts[5] = { "%.6g mm", "%.6g mm", "%.7g", "%.6g", "%.6g" };
	static const char* descs[5] = {
		"Max distance the cursor can lag behind the pen, in mm",
		"Deadzone: pen movement inside this radius does not move the cursor",
		"How fast the cursor descends from outer to inner radius; higher = smoother",
		"Softness of the transition at the outer radius; higher = softer",
		"How much smoothing still applies beyond the outer radius"
	};
	if (index < 0 || index >= 5 || !info) return 0;
	info->apiVersion = AETHER_PLUGIN_API_VERSION;
	info->key = keys[index];
	info->label = labels[index];
	info->type = AETHER_PLUGIN_OPTION_SLIDER;
	info->minValue = mins[index];
	info->maxValue = maxs[index];
	info->defaultValue = defs[index];
	info->format = fmts[index];
	info->description = descs[index];
	return 1;
}

}
