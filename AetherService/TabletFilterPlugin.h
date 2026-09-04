#pragma once

#include <chrono>
#include <mutex>
#include <string>

#include "AetherPluginApi.h"
#include "Platform.h"
#include "TabletFilter.h"
#include "Vector2D.h"

class TabletFilterPlugin : public TabletFilter {
public:
	TabletFilterPlugin();
	virtual ~TabletFilterPlugin();

	bool Load(const std::wstring& dllPath);
	void Unload();

	virtual void SetTarget(Vector2D vector, double h);
	virtual void SetPosition(Vector2D vector, double h);
	virtual void SetReportState(BYTE buttons, double pressure, double hoverDistance);
	virtual bool GetPosition(Vector2D *outputVector);
	virtual void Update();
	virtual void Reset(Vector2D position);
	virtual bool SetDoubleOption(const std::string& key, double value);
	virtual bool SetStringOption(const std::string& key, const std::string& value);

	std::wstring path;
	std::string name;
	std::string description;
	std::mutex pluginMutex;

protected:

	platform::DynLib module;
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
