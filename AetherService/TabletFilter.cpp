#include "stdafx.h"
#include "TabletFilter.h"
#include "Platform.h"

TabletFilter::TabletFilter() {
	timer = NULL;
	callback = NULL;
	timerInterval = 2;
	isValid = false;
	isEnabled = false;
	uTimerID = 0;
	hostDtSec = 0.0;
	hostRawSpeed = 0.0;
	hasHostTiming = false;
}

void TabletFilter::SetReportState(BYTE buttons, double pressure, double hoverDistance) {
}

void TabletFilter::SetFrameTiming(double dtSec, double rawSpeedMmPerSec) {
	hostDtSec = dtSec;
	hostRawSpeed = rawSpeedMmPerSec;
	hasHostTiming = true;
}

bool TabletFilter::StartTimer() {
	if (timer != nullptr) return true;
	if (callback == nullptr) return false;

	double intervalMs = timerInterval > 0.5 ? timerInterval : 0.5;
	int64_t intervalNs = (int64_t)(intervalMs * 1000000.0);

	timerRunning.store(true);
	timer = (void*)1;
	FilterCallback cb = callback;

	timerThread = std::thread([this, cb, intervalNs]() {
		platform::ThreadBoostHandle boost =
			platform::BoostCurrentThread(platform::ThreadBoostTier::Timer);

		int64_t spin = intervalNs / 4;
		if (spin > 100000) spin = 100000;
		if (spin < 5000)   spin = 5000;

		int64_t next = platform::MonotonicNs();
		while (timerRunning.load()) {
			cb(uTimerID, 0, 0, 0, 0);
			next += intervalNs;

			int64_t now = platform::MonotonicNs();
			if (now > next + intervalNs * 4) next = now + intervalNs;

			platform::SleepUntilNs(next, spin);
		}

		platform::RestoreCurrentThread(boost);
	});
	return true;
}

bool TabletFilter::StopTimer() {
	if (timer == nullptr) return false;
	timerRunning.store(false);
	if (timerThread.joinable()) {
		timerThread.join();
	}
	timer = nullptr;
	return true;
}
