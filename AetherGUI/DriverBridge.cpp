#include "DriverBridge.h"
#include <ctime>

DriverBridge::DriverBridge() {}

DriverBridge::~DriverBridge() {
	try { Stop(); } catch (...) {}
	try { CloseDebugLog(); } catch (...) {}
}

// ---------------------------------------------------------------------------
// Diagnostic log file (next to AetherGUI.exe)
// ---------------------------------------------------------------------------

static std::wstring BridgeDirNextToExe() {
	wchar_t exePath[MAX_PATH] = {};
	DWORD n = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
	if (n == 0 || n >= MAX_PATH) return L"";
	std::wstring dir(exePath);
	size_t slash = dir.find_last_of(L'\\');
	if (slash == std::wstring::npos) return L"";
	return dir.substr(0, slash + 1);
}

static std::wstring BridgeFallbackTempDir() {
	wchar_t tmp[MAX_PATH] = {};
	DWORD n = GetTempPathW(MAX_PATH, tmp);
	if (n == 0 || n >= MAX_PATH) return L"";
	return std::wstring(tmp);
}

void DriverBridge::OpenDebugLog() {
	std::lock_guard<std::mutex> lock(debugLogMutex);
	if (debugLogOpened) return;

	std::wstring dir = BridgeDirNextToExe();
	std::wstring file = dir + L"AetherGUI.log";

	FILE* fp = nullptr;
	// Plain binary append - we write raw UTF-8 ourselves. The "ccs=UTF-8"
	// mode has had append-mode quirks in some MSVC builds.
	errno_t err = _wfopen_s(&fp, file.c_str(), L"ab");
	if (err != 0 || fp == nullptr) {
		// Fall back to %TEMP% if exe directory isn't writable (Program Files etc.)
		std::wstring tmpDir = BridgeFallbackTempDir();
		if (!tmpDir.empty()) {
			file = tmpDir + L"AetherGUI.log";
			err = _wfopen_s(&fp, file.c_str(), L"ab");
		}
	}

	if (fp != nullptr) {
		debugLogFile = fp;
		debugLogOpened = true;
		debugLogPath = file;

		// Write UTF-8 BOM once if the file is empty.
		long pos = ftell(fp);
		if (pos == 0) {
			const unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
			fwrite(bom, 1, 3, fp);
		}

		time_t t = time(nullptr);
		tm lt;
		localtime_s(&lt, &t);
		char stamp[64];
		strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &lt);
		fprintf(debugLogFile,
			"\n========================================\n"
			"AetherGUI session start %s\n"
			"========================================\n",
			stamp);
		fflush(debugLogFile);
	}
}

void DriverBridge::CloseDebugLog() {
	std::lock_guard<std::mutex> lock(debugLogMutex);
	if (debugLogFile != nullptr) {
		try { fclose(debugLogFile); } catch (...) {}
		debugLogFile = nullptr;
	}
	debugLogOpened = false;
}

static void BridgeFormatTimestamp(char* buf, size_t bufSize) {
	time_t t = time(nullptr);
	tm lt;
	localtime_s(&lt, &t);
	strftime(buf, bufSize, "%Y-%m-%d %H:%M:%S", &lt);
}

void DriverBridge::DebugLog(const char* tag, const char* fmt, ...) {
	char message[2048];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(message, sizeof(message), fmt, ap);
	va_end(ap);
	message[sizeof(message) - 1] = '\0';

	// Strip a trailing newline so we control formatting consistently.
	size_t len = strlen(message);
	while (len > 0 && (message[len - 1] == '\n' || message[len - 1] == '\r')) {
		message[--len] = '\0';
	}

	try {
		if (!debugLogOpened) OpenDebugLog();

		std::lock_guard<std::mutex> lock(debugLogMutex);
		if (debugLogFile == nullptr) return;

		char stamp[32];
		BridgeFormatTimestamp(stamp, sizeof(stamp));
		fprintf(debugLogFile, "[%s] [%s] %s\n", stamp, tag ? tag : "", message);
		fflush(debugLogFile);
	}
	catch (...) {}
}

void DriverBridge::DebugLogRaw(const char* tag, const std::string& line) {
	try {
		if (!debugLogOpened) OpenDebugLog();

		std::lock_guard<std::mutex> lock(debugLogMutex);
		if (debugLogFile == nullptr) return;

		char stamp[32];
		BridgeFormatTimestamp(stamp, sizeof(stamp));
		fprintf(debugLogFile, "[%s] [%s] %s\n", stamp, tag ? tag : "SVC", line.c_str());
		fflush(debugLogFile);
	}
	catch (...) {}
}

static std::string FormatLastErrorA(DWORD err) {
	char* msg = nullptr;
	DWORD n = FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&msg, 0, nullptr);
	std::string out;
	if (n > 0 && msg != nullptr) {
		out.assign(msg, n);
		while (!out.empty() && (out.back() == '\r' || out.back() == '\n' || out.back() == ' '))
			out.pop_back();
	}
	if (msg) LocalFree(msg);
	return out;
}

static std::string WideToUtf8(const std::wstring& w) {
	if (w.empty()) return std::string();
	int cb = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (cb <= 1) return std::string();
	std::string out(cb - 1, '\0');
	WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, out.data(), cb, nullptr, nullptr);
	return out;
}

bool DriverBridge::Start(const std::wstring& exePath, const std::string& configFile) {
	OpenDebugLog();

	if (isRunning) {
		DebugLog("BRIDGE", "Start() called but already running");
		return true;
	}

	std::string exePathUtf8 = WideToUtf8(exePath);
	DebugLog("BRIDGE", "Starting service: exe='%s' config='%s'",
		exePathUtf8.c_str(), configFile.c_str());

	DWORD attrs = GetFileAttributesW(exePath.c_str());
	if (attrs == INVALID_FILE_ATTRIBUTES) {
		DWORD err = GetLastError();
		DebugLog("BRIDGE", "Service exe NOT FOUND at '%s' (error 0x%08X: %s)",
			exePathUtf8.c_str(), err, FormatLastErrorA(err).c_str());
		return false;
	}
	if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
		DebugLog("BRIDGE", "Service exe path is a directory: '%s'", exePathUtf8.c_str());
		return false;
	}

	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = nullptr;

	HANDLE hStdinRead, hStdoutWrite;

	if (!CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0)) {
		DWORD err = GetLastError();
		DebugLog("BRIDGE", "CreatePipe(stdin) failed: 0x%08X %s", err, FormatLastErrorA(err).c_str());
		return false;
	}
	if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) {
		DWORD err = GetLastError();
		DebugLog("BRIDGE", "CreatePipe(stdout) failed: 0x%08X %s", err, FormatLastErrorA(err).c_str());
		CloseHandle(hStdinRead);
		CloseHandle(hStdinWrite);
		return false;
	}

	SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFOW si = {};
	si.cb = sizeof(si);
	si.hStdInput = hStdinRead;
	si.hStdOutput = hStdoutWrite;
	si.hStdError = hStdoutWrite;
	si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;

	PROCESS_INFORMATION pi = {};

	// Build wide command line: "<quoted exe>" <config>
	std::wstring cmdLine;
	cmdLine.reserve(exePath.size() + configFile.size() + 8);
	cmdLine.push_back(L'"');
	cmdLine += exePath;
	cmdLine.push_back(L'"');
	if (!configFile.empty()) {
		cmdLine.push_back(L' ');
		int cb = MultiByteToWideChar(CP_UTF8, 0, configFile.c_str(), -1, nullptr, 0);
		if (cb > 1) {
			std::wstring wcfg(cb - 1, L'\0');
			MultiByteToWideChar(CP_UTF8, 0, configFile.c_str(), -1, wcfg.data(), cb);
			cmdLine += wcfg;
		}
	}

	// CreateProcessW requires a writable buffer for lpCommandLine.
	std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
	cmdBuf.push_back(L'\0');

	if (!CreateProcessW(
		exePath.c_str(), cmdBuf.data(),
		nullptr, nullptr, TRUE,
		CREATE_NO_WINDOW | HIGH_PRIORITY_CLASS,
		nullptr, nullptr, &si, &pi)) {
		DWORD err = GetLastError();
		DebugLog("BRIDGE", "CreateProcess FAILED for '%s' (error 0x%08X: %s)",
			exePathUtf8.c_str(), err, FormatLastErrorA(err).c_str());
		switch (err) {
		case ERROR_FILE_NOT_FOUND:
			DebugLog("BRIDGE", "  -> Hint: AetherService.exe is missing. Reinstall or rebuild.");
			break;
		case ERROR_ACCESS_DENIED:
			DebugLog("BRIDGE", "  -> Hint: Antivirus / SmartScreen quarantine? Try unblocking the exe.");
			break;
		case ERROR_BAD_EXE_FORMAT:
			DebugLog("BRIDGE", "  -> Hint: 32/64-bit mismatch or corrupted exe.");
			break;
		case ERROR_INVALID_NAME:
			DebugLog("BRIDGE", "  -> Hint: Path contains characters that became invalid after ANSI conversion.");
			break;
		}
		CloseHandle(hStdinRead);
		CloseHandle(hStdinWrite);
		CloseHandle(hStdoutRead);
		CloseHandle(hStdoutWrite);
		hStdinWrite = nullptr;
		hStdoutRead = nullptr;
		return false;
	}

	DebugLog("BRIDGE", "Service started, PID=%lu", (unsigned long)pi.dwProcessId);

	hProcess = pi.hProcess;
	CloseHandle(pi.hThread);
	CloseHandle(hStdinRead);
	CloseHandle(hStdoutWrite);

	
	
	{
		struct { ULONG Version; ULONG ControlMask; ULONG StateMask; } throttling = {};
		throttling.Version = 1;
		throttling.ControlMask = 0x1; 
		throttling.StateMask = 0;     
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
			DWORD err = GetLastError();
			if (isRunning) {
				DebugLog("BRIDGE", "ReadFile from service stopped (bytes=%lu, error=0x%08X). Pipe closed.",
					(unsigned long)bytesRead, err);
			}
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

			// High-rate position updates drive the live cursor/Hz meter, but should not spam the Console tab.
			bool isPosStatus = (line.find("[STATUS] POS ") != std::string::npos);

			if (!isPosStatus) {
				// Mirror every non-POS service line to the persistent log file.
				DebugLogRaw("SVC", line);

				std::lock_guard<std::mutex> lock(logMutex);
				logLines.push_back(line);
				if ((int)logLines.size() > MAX_LOG_LINES) {
					logLines.erase(logLines.begin());
				}
			}
		}
	}

	// Capture exit code so we can tell crash vs. clean shutdown.
	if (hProcess != nullptr) {
		DWORD exitCode = 0;
		if (GetExitCodeProcess(hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
			DebugLog("BRIDGE", "Service process exited with code %lu (0x%08X)",
				(unsigned long)exitCode, (unsigned long)exitCode);
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
	else if (status.find("LATENCY ") == 0) {
		float avg = 0, p99 = 0, mx = 0;
		int samples = 0;
		if (sscanf_s(status.c_str() + 8, "%f %f %f %d", &avg, &p99, &mx, &samples) >= 3) {
			latencyAvgMs.store(avg);
			latencyP99Ms.store(p99);
			latencyMaxMs.store(mx);
			latencySamples.store(samples);
		}
		return;
	}
	else if (status.find("POS ") == 0) {
		
		float px = 0, py = 0, pp = 0, phz = 0;
		if (sscanf_s(status.c_str() + 4, "%f %f %f %f", &px, &py, &pp, &phz) >= 2) {
			penX.store(px);
			penY.store(py);
			penPressure.store(pp);
			if (phz > 0.1f) penHz.store(phz);
			penActive.store(true);

			
			{
				std::lock_guard<std::mutex> lock(trailMutex);
				TrailPoint tp;
				tp.x = px; tp.y = py; tp.pressure = pp; tp.age = 0;
				trail.push_back(tp);
				if ((int)trail.size() > MAX_TRAIL)
					trail.erase(trail.begin());
			}
		}
		return; 
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
