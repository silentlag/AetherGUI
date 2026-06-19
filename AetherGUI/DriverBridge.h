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

	void OpenDebugLog();

	void CloseDebugLog();

	void DebugLog(const char* tag, const char* fmt, ...);

	void DebugLogRaw(const char* tag, const std::string& line);

	const std::wstring& GetDebugLogPath() const { return debugLogPath; }

	void PollShmem();

private:
	static DWORD WINAPI ReadThreadProc(LPVOID param);
	void ReadLoop();

	void ParseStatusLine(const std::string& line);

	class StatusSharedReader* shmem = nullptr;
	bool shmemActive = false;
};
