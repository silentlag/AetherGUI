

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace platform {

struct ThreadBoostHandle {
	void* impl;
};

enum class ThreadBoostTier {

	Producer,

	Timer,
};

ThreadBoostHandle BoostCurrentThread(ThreadBoostTier tier);

void RestoreCurrentThread(ThreadBoostHandle handle);

inline void CpuPause();

int64_t MonotonicNs();

int64_t SleepUntilNs(int64_t deadlineNs, int64_t spinThresholdNs);

void GlobalInit();
void GlobalShutdown();

void SleepMs(unsigned ms);

uint32_t LastErrorCode();

uint32_t ErrorDeviceNotConnected();
uint32_t ErrorAccessDenied();

struct MonitorInfo {
	double primaryWidth;
	double primaryHeight;
	double virtualWidth;
	double virtualHeight;
	double virtualX;
	double virtualY;
};

MonitorInfo QueryMonitorInfo();

struct DynLib {
	void* handle;
};

DynLib DynLibLoad(const wchar_t* path);

bool DynLibValid(const DynLib& lib);

void* DynLibSymbol(const DynLib& lib, const char* name);

void DynLibUnload(DynLib& lib);

std::string DynLibLastError();

bool PathExists(const wchar_t* path);
bool PathIsDirectory(const wchar_t* path);

bool MakeDirectory(const wchar_t* path);

bool ListDirectory(const wchar_t* path, std::vector<std::wstring>* out);

}

#if defined(_WIN32)
	#include <intrin.h>
	inline void platform::CpuPause() { _mm_pause(); }
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
	inline void platform::CpuPause() { __builtin_ia32_pause(); }
#elif defined(__aarch64__) || defined(__arm__)
	inline void platform::CpuPause() { __asm__ __volatile__("yield"); }
#else
	inline void platform::CpuPause() {}
#endif
