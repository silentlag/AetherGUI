#pragma once
#include "Framework.h"


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

	
	std::atomic<float> penX{0};
	std::atomic<float> penY{0};
	std::atomic<float> penPressure{0};
	std::atomic<float> penHz{0};
	std::atomic<bool> penActive{false};

	
	struct TrailPoint { float x, y; float pressure; float age; };
	std::mutex trailMutex;
	std::vector<TrailPoint> trail;
	static const int MAX_TRAIL = 512;

	DriverBridge();
	~DriverBridge();

	
	bool Start(const std::string& exePath, const std::string& configFile = "init.cfg");
	
	void Stop();
	
	void SendCommand(const std::string& command);

	
	std::vector<std::string> GetLogLines();
	
	void ClearLog();

private:
	static DWORD WINAPI ReadThreadProc(LPVOID param);
	void ReadLoop();
	
	void ParseStatusLine(const std::string& line);
};
