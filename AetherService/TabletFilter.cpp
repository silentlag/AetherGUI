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
}

void TabletFilter::SetReportState(BYTE buttons, double pressure, double hoverDistance) {
}

#if defined(_WIN32)

bool TabletFilter::StartTimer() {
	if (timer == NULL) {
		MMRESULT result = timeSetEvent(
			(UINT)timerInterval,
			1,
			callback,
			NULL,
			TIME_PERIODIC | TIME_KILL_SYNCHRONOUS
		);
		if (result == NULL) {
			return false;
		}
		else {
			timer = (HANDLE)1;
			uTimerID = result;
		}
	}
	return true;
}

bool TabletFilter::StopTimer() {
	if (timer == NULL) return false;

	MMRESULT result = timeKillEvent(uTimerID);

	timer = NULL;
	return (result == TIMERR_NOERROR);
}
#else

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
#endif
