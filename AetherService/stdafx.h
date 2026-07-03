
#pragma once
#include "targetver.h"

#if defined(_WIN32)
	#include <windows.h>
#endif

#include <stdio.h>
#include <math.h>

#if !defined(_WIN32)
  #include <cstdarg>
  #include <cstddef>
  #include <time.h>
  #define localtime_s(out_tm, in_t) (localtime_r((in_t), (out_tm)) ? 0 : 1)
  inline int sprintf_s(char* dest, size_t cap, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(dest, cap, fmt, ap);
    va_end(ap);
    return n;
  }
  template<size_t N> inline int sprintf_s(char (&dest)[N], const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(dest, N, fmt, ap);
    va_end(ap);
    return n;
  }
#endif
#include <chrono>
#include <thread>
#include <mutex>
#include <string>
#include <vector>
#include <sstream>

#include "VMulti.h"
#include "Tablet.h"
#include "ScreenMapper.h"

extern VMulti *vmulti;
extern Tablet *tablet;
extern ScreenMapper *mapper;
extern std::thread *tabletThread;
class HotkeyManager;
extern HotkeyManager *hotkeyManager;
extern void CleanupAndExit(int code);
extern std::mutex tabletStateMutex;
extern bool overclockActive;
extern double overclockTargetHz;
extern bool penRateLimitActive;
extern double penRateLimitHz;
extern void StartOverclockTimer(double targetHz);
extern void StopOverclockTimer();
extern void ResetPenRateLimiter();
extern int WritePenReport(bool force);
extern bool IsTimedOutputEnabled();
extern void RefreshTimedOutputTimer();
