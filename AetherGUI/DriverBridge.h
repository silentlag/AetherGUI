#pragma once
#include "Framework.h"
#include <cstdio>
#include <cstdarg>


class DriverBridge {
public:
	HANDLE hProcess = nullptr;
	HANDLE hStdinWrite = nullptr;
	HANDLE hStdoutRead = nullptr;
	HANDLE hReadThread = nullptr;

	std::atomic<bool> isRunning{false};
	std::atomic<bool> isConnected{false};

	std::mutex logMutex;
	std::vector<std::string> logLines;
	static const int MAX_LOG_LINES = 500;

	// Persistent diagnostic log file (next to AetherGUI.exe, fallback to %TEMP%).
	// Captures everything: bridge-side events (CreateProcess failures, exit codes)
	// AND every line from the service. Lets us debug "Console is empty" cases
	// where the service never even started.
	std::mutex debugLogMutex;
	FILE* debugLogFile = nullptr;
	bool debugLogOpened = false;
	std::wstring debugLogPath;

	std::string tabletName = "Not connected";
	float tabletWidth = 0;
	float tabletHeight = 0;
	int maxX = 0, maxY = 0;
	int maxPressure = 0;

	
	std::atomic<float> penX{0};
	std::atomic<float> penY{0};
	std::atomic<float> penPressure{0};
	std::atomic<float> penHz{0};
	std::atomic<bool> penActive{false};

	// Internal driver latency, measured by the service from "HID packet read"
	// to "VMulti / SendInput dispatched". Updated ~2x/second through a
	// [STATUS] LATENCY avgMs p99Ms maxMs samples line. Does NOT include USB
	// polling, kernel HID layer, or the game's own input handling.
	std::atomic<float> latencyAvgMs{0};
	std::atomic<float> latencyP99Ms{0};
	std::atomic<float> latencyMaxMs{0};
	std::atomic<int>   latencySamples{0};

	
	struct TrailPoint { float x, y; float pressure; float age; };
	std::mutex trailMutex;
	std::vector<TrailPoint> trail;
	static const int MAX_TRAIL = 512;

	DriverBridge();
	~DriverBridge();

	
	bool Start(const std::wstring& exePath, const std::string& configFile = "init.cfg");
	
	void Stop();
	
	void SendCommand(const std::string& command);

	
	std::vector<std::string> GetLogLines();
	
	void ClearLog();

	// Open the diagnostic log file. Safe to call multiple times.
	void OpenDebugLog();

	// Close the diagnostic log file (called automatically on destruction).
	void CloseDebugLog();

	// Append a tagged, timestamped line. Thread-safe. Tag examples: "BRIDGE", "APP", "SVC".
	void DebugLog(const char* tag, const char* fmt, ...);

	// Append a raw, already-formatted service line with timestamp + tag.
	void DebugLogRaw(const char* tag, const std::string& line);

	// Public so the GUI can show the user where the log file lives.
	const std::wstring& GetDebugLogPath() const { return debugLogPath; }

	// Called from the GUI main thread once per frame. Drains the shared-
	// memory ring (if available) into the public penX/Y/pressure/hz atomics
	// and trail buffer. Cheap when nothing happens.
	void PollShmem();

private:
	static DWORD WINAPI ReadThreadProc(LPVOID param);
	void ReadLoop();
	
	void ParseStatusLine(const std::string& line);

	// Shared-memory fast path. Pointer so the .h doesn't need to pull in the
	// full reader/header; concrete type lives in the .cpp.
	class StatusSharedReader* shmem = nullptr;
	bool shmemActive = false;
};
