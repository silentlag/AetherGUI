#pragma once

#include <windows.h>
#include "AetherShared.h"

class StatusSharedWriter {
public:
	StatusSharedWriter();
	~StatusSharedWriter();

	
	bool Initialize();
	void Shutdown();

	bool IsActive() const { return header != nullptr; }

	void PushPos(uint64_t tsNs, float x, float y, float pressure, float hz, bool tipDown);

	
	void PushLatency(float avgMs, float p99Ms, float maxMs, int samples);

private:
	HANDLE                   hMapping = nullptr;
	AetherShared::Header*    header   = nullptr;
	AetherShared::PosSlot*   ring     = nullptr;
};
