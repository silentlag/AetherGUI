
#include "stdafx.h"
#if defined(_WIN32)
#include "Platform.h"

#include <windows.h>
#include <avrt.h>
#include <chrono>

#pragma comment(lib, "avrt.lib")
#pragma comment(lib, "winmm.lib")

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace platform {

namespace {
struct WinBoost {
	HANDLE mmcss;
	int    oldPri;
};

thread_local HANDLE tlsWaitTimer = nullptr;

HANDLE GetOrCreateThreadTimer() {
	if (tlsWaitTimer == nullptr) {
		tlsWaitTimer = CreateWaitableTimerExW(
			NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
		if (tlsWaitTimer == NULL) {

			tlsWaitTimer = CreateWaitableTimerW(NULL, FALSE, NULL);
		}
	}
	return tlsWaitTimer;
}
}

void GlobalInit() {

	timeBeginPeriod(1);
}

void GlobalShutdown() {
	timeEndPeriod(1);
}

MonitorInfo QueryMonitorInfo() {
	MonitorInfo m{};
	m.primaryWidth  = GetSystemMetrics(SM_CXSCREEN);
	m.primaryHeight = GetSystemMetrics(SM_CYSCREEN);
	m.virtualWidth  = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	m.virtualHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
	m.virtualX      = GetSystemMetrics(SM_XVIRTUALSCREEN);
	m.virtualY      = GetSystemMetrics(SM_YVIRTUALSCREEN);
	return m;
}

void SleepMs(unsigned ms) {
	Sleep(ms);
}

uint32_t LastErrorCode() {
	return (uint32_t)GetLastError();
}

uint32_t ErrorDeviceNotConnected() { return (uint32_t)ERROR_DEVICE_NOT_CONNECTED; }
uint32_t ErrorAccessDenied()       { return (uint32_t)ERROR_ACCESS_DENIED; }

DynLib DynLibLoad(const wchar_t* path) {
	DynLib lib{};

	DWORD oldMode = SetErrorMode(SEM_FAILCRITICALERRORS);
	HMODULE module = LoadLibraryExW(path, nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR);
	if (module == nullptr) {
		module = LoadLibraryW(path);
	}
	SetErrorMode(oldMode);
	lib.handle = (void*)module;
	return lib;
}

bool DynLibValid(const DynLib& lib) { return lib.handle != nullptr; }

void* DynLibSymbol(const DynLib& lib, const char* name) {
	if (lib.handle == nullptr) return nullptr;
	return (void*)GetProcAddress((HMODULE)lib.handle, name);
}

void DynLibUnload(DynLib& lib) {
	if (lib.handle != nullptr) {
		FreeLibrary((HMODULE)lib.handle);
		lib.handle = nullptr;
	}
}

std::string DynLibLastError() {
	DWORD code = GetLastError();
	if (code == 0) return "";
	char buf[256] = {0};
	DWORD n = FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, code, 0, buf, sizeof(buf), nullptr);
	std::string out(buf, n);
	while (!out.empty() && (out.back() == '\r' || out.back() == '\n' || out.back() == ' ')) {
		out.pop_back();
	}
	return out;
}

bool PathExists(const wchar_t* path) {
	DWORD attrs = GetFileAttributesW(path);
	return attrs != INVALID_FILE_ATTRIBUTES;
}

bool PathIsDirectory(const wchar_t* path) {
	DWORD attrs = GetFileAttributesW(path);
	return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

bool MakeDirectory(const wchar_t* path) {
	if (CreateDirectoryW(path, nullptr)) return true;
	return GetLastError() == ERROR_ALREADY_EXISTS;
}

bool ListDirectory(const wchar_t* path, std::vector<std::wstring>* out) {
	if (out == nullptr) return false;
	out->clear();
	std::wstring pattern(path);
	if (!pattern.empty() && pattern.back() != L'\\' && pattern.back() != L'/') {
		pattern.push_back(L'\\');
	}
	pattern.append(L"*");

	WIN32_FIND_DATAW data{};
	HANDLE find = FindFirstFileW(pattern.c_str(), &data);
	if (find == INVALID_HANDLE_VALUE) return false;
	do {
		if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0)
			continue;
		out->emplace_back(data.cFileName);
	} while (FindNextFileW(find, &data));
	FindClose(find);
	return true;
}

ThreadBoostHandle BoostCurrentThread(ThreadBoostTier tier) {
	WinBoost* boost = new WinBoost{};
	boost->oldPri = GetThreadPriority(GetCurrentThread());

	int targetPri = THREAD_PRIORITY_NORMAL;
	AVRT_PRIORITY mmcssPri = AVRT_PRIORITY_NORMAL;

	switch (tier) {
		case ThreadBoostTier::Producer:

			targetPri = THREAD_PRIORITY_TIME_CRITICAL;
			mmcssPri = AVRT_PRIORITY_CRITICAL;
			break;

		case ThreadBoostTier::Timer:

			targetPri = THREAD_PRIORITY_ABOVE_NORMAL;
			mmcssPri = AVRT_PRIORITY_NORMAL;
			break;
	}

	SetThreadPriority(GetCurrentThread(), targetPri);

	DWORD taskIndex = 0;
	HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
	if (mmcss != NULL) {
		AvSetMmThreadPriority(mmcss, mmcssPri);
	}
	boost->mmcss = mmcss;

	return ThreadBoostHandle{ boost };
}

void RestoreCurrentThread(ThreadBoostHandle handle) {
	WinBoost* boost = static_cast<WinBoost*>(handle.impl);
	if (boost == nullptr) return;
	if (boost->mmcss != NULL) {
		AvRevertMmThreadCharacteristics(boost->mmcss);
	}
	SetThreadPriority(GetCurrentThread(), boost->oldPri);
	delete boost;
}

int64_t MonotonicNs() {
	using namespace std::chrono;
	return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

int64_t SleepUntilNs(int64_t deadlineNs, int64_t spinThresholdNs) {
	int64_t now = MonotonicNs();
	while (now < deadlineNs) {
		int64_t remainingNs = deadlineNs - now;
		if (remainingNs > spinThresholdNs) {
			HANDLE waitTimer = GetOrCreateThreadTimer();
			if (waitTimer != NULL) {

				int64_t sleepUntilNs = deadlineNs - spinThresholdNs;
				int64_t sleep100ns = (sleepUntilNs - now) / 100;
				if (sleep100ns < 1) sleep100ns = 1;

				LARGE_INTEGER dueTime;
				dueTime.QuadPart = -sleep100ns;
				if (SetWaitableTimer(waitTimer, &dueTime, 0, NULL, NULL, FALSE)) {
					WaitForSingleObject(waitTimer, INFINITE);
				}
				else {
					Sleep(0);
				}
			}
			else {
				Sleep(0);
			}
		}
		else {
			CpuPause();
		}
		now = MonotonicNs();
	}
	return now;
}

}
#endif
