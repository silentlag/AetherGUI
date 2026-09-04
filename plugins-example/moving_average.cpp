#include <cmath>
#include <cstring>
#include <vector>

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

struct MovingAverageState {
	std::vector<double> xs;
	std::vector<double> ys;
	std::vector<double> ps;
	int window = 5;
};

extern "C" {

__declspec(dllexport) int AETHER_PLUGIN_CALL AetherPluginGetInfo(AetherPluginInfo* info) {
	if (!info) return 0;
	info->apiVersion = AETHER_PLUGIN_API_VERSION;
	info->name = "Example Moving Average";
	info->description = "Averages the last N pen positions. Reference Aether native plugin.";
	return 1;
}

__declspec(dllexport) void* AETHER_PLUGIN_CALL AetherPluginCreate() {
	return new MovingAverageState();
}

__declspec(dllexport) void AETHER_PLUGIN_CALL AetherPluginDestroy(void* instance) {
	delete (MovingAverageState*)instance;
}

__declspec(dllexport) void AETHER_PLUGIN_CALL AetherPluginReset(void* instance, const AetherPluginPoint* point) {
	MovingAverageState* s = (MovingAverageState*)instance;
	if (!s) return;
	s->xs.clear();
	s->ys.clear();
	s->ps.clear();
	if (point) {
		s->xs.push_back(point->x);
		s->ys.push_back(point->y);
		s->ps.push_back(point->pressure);
	}
}

__declspec(dllexport) void AETHER_PLUGIN_CALL AetherPluginProcess(void* instance, AetherPluginPoint* point) {
	MovingAverageState* s = (MovingAverageState*)instance;
	if (!s || !point) return;

	s->xs.push_back(point->x);
	s->ys.push_back(point->y);
	s->ps.push_back(point->pressure);
	while ((int)s->xs.size() > s->window) {
		s->xs.erase(s->xs.begin());
		s->ys.erase(s->ys.begin());
		s->ps.erase(s->ps.begin());
	}

	double sx = 0.0, sy = 0.0, sp = 0.0;
	for (size_t i = 0; i < s->xs.size(); i++) {
		sx += s->xs[i];
		sy += s->ys[i];
		sp += s->ps[i];
	}
	double n = (double)s->xs.size();
	if (n > 0.0) {
		point->x = sx / n;
		point->y = sy / n;
		point->pressure = sp / n;
	}
}

__declspec(dllexport) int AETHER_PLUGIN_CALL AetherPluginSetDouble(void* instance, const char* key, double value) {
	MovingAverageState* s = (MovingAverageState*)instance;
	if (!s || !key) return 0;
	if (strcmp(key, "window") == 0) {
		int w = (int)value;
		if (w < 2) w = 2;
		if (w > 20) w = 20;
		s->window = w;
		return 1;
	}
	return 0;
}

__declspec(dllexport) int AETHER_PLUGIN_CALL AetherPluginGetOptionCount() {
	return 1;
}

__declspec(dllexport) int AETHER_PLUGIN_CALL AetherPluginGetOptionInfo(int index, AetherPluginOptionInfo* info) {
	if (index != 0 || !info) return 0;
	info->apiVersion = AETHER_PLUGIN_API_VERSION;
	info->key = "window";
	info->label = "Window";
	info->type = AETHER_PLUGIN_OPTION_SLIDER;
	info->minValue = 2.0;
	info->maxValue = 20.0;
	info->defaultValue = 5.0;
	info->format = "%.0f";
	info->description = "Number of positions to average";
	return 1;
}

}
