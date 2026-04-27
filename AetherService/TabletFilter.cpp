#include "stdafx.h"
#include "TabletFilter.h"


TabletFilter::TabletFilter() {
	timer = NULL;
	callback = NULL;
	timerInterval = 2;
	isValid = false;
	isEnabled = false;
}

//
// Start Timer
//
bool TabletFilter::StartTimer() {
	if (timer == NULL) {
		MMRESULT result = timeSetEvent(
			(UINT)timerInterval,//UINT           uDelay,
			1,//UINT           uResolution,
			callback, //LPTIMECALLBACK lpTimeProc,
			NULL, //DWORD_PTR      dwUser,
			TIME_PERIODIC | TIME_KILL_SYNCHRONOUS //UINT           fuEvent
		);
		if (result == NULL) {
			return false;
		}
		else {
			timer = (HANDLE)1; // for code compatibility purposes
			uTimerID = result;
		}
	}
	return true;
}


//
// Stop Timer
//
bool TabletFilter::StopTimer() {
	if (timer == NULL) return false;

	MMRESULT result = timeKillEvent(uTimerID);
	// Always reset timer handle so StartTimer() can create a new one
	timer = NULL;
	return (result == TIMERR_NOERROR);
}
