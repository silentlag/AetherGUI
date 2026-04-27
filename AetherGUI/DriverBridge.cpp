#include "DriverBridge.h"

DriverBridge::DriverBridge() {}

DriverBridge::~DriverBridge() {
	Stop();
}

bool DriverBridge::Start(const std::string& exePath, const std::string& configFile) {
	if (isRunning) return true;

	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = nullptr;

	HANDLE hStdinRead, hStdoutWrite;

	if (!CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0)) return false;
	if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) {
		CloseHandle(hStdinRead);
		CloseHandle(hStdinWrite);
		return false;
	}

	SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFOA si = {};
	si.cb = sizeof(si);
	si.hStdInput = hStdinRead;
	si.hStdOutput = hStdoutWrite;
	si.hStdError = hStdoutWrite;
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;

	PROCESS_INFORMATION pi = {};

	std::string cmdLine = exePath + " " + configFile;

	if (!CreateProcessA(
		nullptr, (LPSTR)cmdLine.c_str(),
		nullptr, nullptr, TRUE,
		CREATE_NO_WINDOW | HIGH_PRIORITY_CLASS,
		nullptr, nullptr, &si, &pi)) {
		CloseHandle(hStdinRead);
		CloseHandle(hStdinWrite);
		CloseHandle(hStdoutRead);
		CloseHandle(hStdoutWrite);
		hStdinWrite = nullptr;
		hStdoutRead = nullptr;
		return false;
	}

	hProcess = pi.hProcess;
	CloseHandle(pi.hThread);
	CloseHandle(hStdinRead);
	CloseHandle(hStdoutWrite);

	// Disable power throttling on the driver service process
	// Prevents Windows 11 from reducing timer resolution when GUI loses focus
	{
		struct { ULONG Version; ULONG ControlMask; ULONG StateMask; } throttling = {};
		throttling.Version = 1;
		throttling.ControlMask = 0x1; // PROCESS_POWER_THROTTLING_EXECUTION_SPEED
		throttling.StateMask = 0;     // disable throttling
		SetProcessInformation(hProcess, ProcessPowerThrottling, &throttling, sizeof(throttling));
	}

	isRunning = true;
	isConnected = true;

	hReadThread = CreateThread(nullptr, 0, ReadThreadProc, this, 0, nullptr);
	return true;
}

void DriverBridge::Stop() {
	if (!isRunning) return;

	isRunning = false;
	SendCommand("Exit");
	Sleep(200);

	if (hProcess) {
		TerminateProcess(hProcess, 0);
		WaitForSingleObject(hProcess, 1000);
		CloseHandle(hProcess);
		hProcess = nullptr;
	}

	if (hReadThread) {
		WaitForSingleObject(hReadThread, 1000);
		CloseHandle(hReadThread);
		hReadThread = nullptr;
	}

	if (hStdinWrite) { CloseHandle(hStdinWrite); hStdinWrite = nullptr; }
	if (hStdoutRead) { CloseHandle(hStdoutRead); hStdoutRead = nullptr; }

	isConnected = false;
}

void DriverBridge::SendCommand(const std::string& command) {
	if (!hStdinWrite || !isRunning) return;

	std::string line = command + "\n";
	DWORD written;
	WriteFile(hStdinWrite, line.c_str(), (DWORD)line.size(), &written, nullptr);
}

DWORD WINAPI DriverBridge::ReadThreadProc(LPVOID param) {
	DriverBridge* bridge = (DriverBridge*)param;
	bridge->ReadLoop();
	return 0;
}

void DriverBridge::ReadLoop() {
	char buffer[4096];
	DWORD bytesRead;
	std::string lineBuffer;

	while (isRunning) {
		if (!ReadFile(hStdoutRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) || bytesRead == 0) {
			break;
		}

		buffer[bytesRead] = '\0';
		lineBuffer += buffer;

		size_t pos;
		while ((pos = lineBuffer.find('\n')) != std::string::npos) {
			std::string line = lineBuffer.substr(0, pos);
			if (!line.empty() && line.back() == '\r') {
				line.pop_back();
			}
			lineBuffer = lineBuffer.substr(pos + 1);

			ParseStatusLine(line);

			{
				std::lock_guard<std::mutex> lock(logMutex);
				logLines.push_back(line);
				if ((int)logLines.size() > MAX_LOG_LINES) {
					logLines.erase(logLines.begin());
				}
			}
		}
	}

	isConnected = false;
}

void DriverBridge::ParseStatusLine(const std::string& line) {
	if (line.find("[STATUS]") == std::string::npos) return;

	size_t pos = line.find("[STATUS]");
	std::string status = line.substr(pos + 9);

	if (status.find("TABLET ") == 0) {
		tabletName = status.substr(7);
	}
	else if (status.find("WIDTH ") == 0) {
		tabletWidth = (float)atof(status.substr(6).c_str());
	}
	else if (status.find("HEIGHT ") == 0) {
		tabletHeight = (float)atof(status.substr(7).c_str());
	}
	else if (status.find("MAX_X ") == 0) {
		maxX = atoi(status.substr(6).c_str());
	}
	else if (status.find("MAX_Y ") == 0) {
		maxY = atoi(status.substr(6).c_str());
	}
	else if (status.find("MAX_PRESSURE ") == 0) {
		maxPressure = atoi(status.substr(13).c_str());
	}
	else if (status.find("POS ") == 0) {
		// Parse "POS x y pressure hz"
		float px = 0, py = 0, pp = 0, phz = 0;
		if (sscanf_s(status.c_str() + 4, "%f %f %f %f", &px, &py, &pp, &phz) >= 2) {
			penX.store(px);
			penY.store(py);
			penPressure.store(pp);
			if (phz > 0.1f) penHz.store(phz);
			penActive.store(true);

			// Add to trail
			{
				std::lock_guard<std::mutex> lock(trailMutex);
				TrailPoint tp;
				tp.x = px; tp.y = py; tp.pressure = pp; tp.age = 0;
				trail.push_back(tp);
				if ((int)trail.size() > MAX_TRAIL)
					trail.erase(trail.begin());
			}
		}
		return; // Don't log POS lines to console
	}
}

std::vector<std::string> DriverBridge::GetLogLines() {
	std::lock_guard<std::mutex> lock(logMutex);
	return logLines;
}

void DriverBridge::ClearLog() {
	std::lock_guard<std::mutex> lock(logMutex);
	logLines.clear();
}
