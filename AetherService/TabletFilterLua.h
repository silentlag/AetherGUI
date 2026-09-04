#pragma once

#include "TabletFilterPlugin.h"
#include "lua/AetherLua.h"

class TabletFilterLua : public TabletFilterPlugin {
public:
	TabletFilterLua();
	virtual ~TabletFilterLua();

	bool LoadScript(const std::wstring& scriptPath);
	void UnloadScript();

	virtual void SetTarget(Vector2D vector, double h);
	virtual void SetPosition(Vector2D vector, double h);
	virtual void SetReportState(BYTE buttons, double pressure, double hoverDistance);
	virtual bool GetPosition(Vector2D *outputVector);
	virtual void Update();
	virtual void Reset(Vector2D position);
	virtual bool SetDoubleOption(const std::string& key, double value);
	virtual bool SetStringOption(const std::string& key, const std::string& value);

	bool ApplyOptionNumber(const std::string& key, double value);

	lua_State* luaState;
	int stateRef;
};
