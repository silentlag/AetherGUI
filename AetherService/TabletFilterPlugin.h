#pragma once

#include <chrono>
#include <string>

#include "AetherPluginApi.h"
#include "TabletFilter.h"
#include "Vector2D.h"

class TabletFilterPlugin : public TabletFilter {
public:
	TabletFilterPlugin();
	~TabletFilterPlugin();

	bool Load(const std::wstring& dllPath);
	void Unload();

	void SetTarget(Vector2D vector, double h);
	void SetPosition(Vector2D vector, double h);
	void SetReportState(BYTE buttons, double pressure, double hoverDistance);
	bool GetPosition(Vector2D *outputVector);
	void Update();
	void Reset(Vector2D position);
	bool SetDoubleOption(const std::string& key, double value);
	bool SetStringOption(const std::string& key, const std::string& value);

	std::wstring path;
	std::string name;
	std::string description;

private:
	HMODULE module;
	void* instance;
	AetherPluginDestroyFn destroyFn;
	AetherPluginResetFn resetFn;
	AetherPluginProcessFn processFn;
	AetherPluginSetDoubleFn setDoubleFn;
	AetherPluginSetStringFn setStringFn;

	Vector2D position;
	Vector2D target;
	double z;
	BYTE buttons;
	double pressure;
	double hoverDistance;
	bool firstUpdate;
	std::chrono::high_resolution_clock::time_point lastTime;
};
