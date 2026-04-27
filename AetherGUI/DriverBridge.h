#pragma once
#include "Framework.h"

/// Manages AetherService.exe subprocess via stdin/stdout pipes
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

	std::string tabletName = "Not connected";
	float tabletWidth = 0;
	float tabletHeight = 0;
	int maxX = 0, maxY = 0;
	int maxPressure = 0;

	// Live pen position (updated ~60Hz from service)
	std::atomic<float> penX{0};
	std::atomic<float> penY{0};
	std::atomic<float> penPressure{0};
	std::atomic<float> penHz{0};
	std::atomic<bool> penActive{false};

	// Input trail history for visualizer
	struct TrailPoint { float x, y; float pressure; float age; };
	std::mutex trailMutex;
	std::vector<TrailPoint> trail;
	static const int MAX_TRAIL = 512;

	DriverBridge();
	~DriverBridge();

	/// Launch the driver service process and begin reading its output
	bool Start(const std::string& exePath, const std::string& configFile = "init.cfg");
	/// Terminate the driver service process and close all handles
	void Stop();
	/// Send a command string to the driver via stdin pipe
	void SendCommand(const std::string& command);

	/// Get a thread-safe copy of all buffered log lines
	std::vector<std::string> GetLogLines();
	/// Clear all buffered log lines
	void ClearLog();

private:
	static DWORD WINAPI ReadThreadProc(LPVOID param);
	void ReadLoop();
	/// Parse [STATUS] lines to extract tablet name, dimensions, etc.
	void ParseStatusLine(const std::string& line);
};
