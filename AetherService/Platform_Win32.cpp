
#include "stdafx.h"
#if defined(_WIN32)
#include "Platform.h"
#define LOG_MODULE "Timer"
#include "Logger.h"

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
	HANDLE    mmcss;
	int       oldPri;
	DWORD_PTR oldAffinity; // 0 = affinity was not changed
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

// NtSetTimerResolution: undocumented but stable since XP. timeBeginPeriod(1)
// bottoms out at ~1ms; this reaches 0.5ms where the kernel allows it.
void RequestFineTimerResolution() {
	typedef long (NTAPI *NtSetTimerResolution_t)(unsigned long, unsigned char, unsigned long*);
	typedef long (NTAPI *NtQueryTimerResolution_t)(unsigned long*, unsigned long*, unsigned long*);
	HMODULE ntdll = GetModuleHandleA("ntdll.dll");
	if (ntdll == NULL) return;
	auto ntSet = (NtSetTimerResolution_t)GetProcAddress(ntdll, "NtSetTimerResolution");
	auto ntQuery = (NtQueryTimerResolution_t)GetProcAddress(ntdll, "NtQueryTimerResolution");
	if (ntSet == NULL) return;
	unsigned long minRes = 0, maxRes = 0, curRes = 0;
	if (ntQuery != NULL) ntQuery(&minRes, &maxRes, &curRes);
	unsigned long desired = maxRes; // finest the kernel allows (typically 5000 = 0.5ms)
	if (desired == 0) desired = 5000;
	unsigned long actual = 0;
	if (ntSet(desired, 1, &actual) == 0) {
		LOG_INFO("Timer resolution: %.2f ms requested (was %.2f ms)\n",
			actual / 10000.0, curRes / 10000.0);
	}
}

void GlobalInit() {

	timeBeginPeriod(1);
	RequestFineTimerResolution();
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

	// keep pipeline threads off core 0: it services most DPCs and IRQs,
	// and its deferred work is what our waits queue behind
	boost->oldAffinity = 0;
	DWORD_PTR procMask = 0, sysMask = 0;
	if (GetProcessAffinityMask(GetCurrentProcess(), &procMask, &sysMask)) {
		DWORD_PTR altMask = procMask & ~(DWORD_PTR)1;
		if (altMask != 0) {
			boost->oldAffinity = SetThreadAffinityMask(GetCurrentThread(), altMask);
		}
	}

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
	if (boost->oldAffinity != 0) {
		SetThreadAffinityMask(GetCurrentThread(), boost->oldAffinity);
	}
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
