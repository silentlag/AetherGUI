



#pragma once
#include "targetver.h"

#include <windows.h>
#include <stdio.h>
#include <math.h>
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
extern void CleanupAndExit(int code);
extern std::mutex tabletStateMutex;
extern bool overclockActive;
extern double overclockTargetHz;
extern void StartOverclockTimer(double targetHz);
extern void StopOverclockTimer();



