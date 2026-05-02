#include "stdafx.h"
#include "TabletFilter.h"


TabletFilter::TabletFilter() {
	timer = NULL;
	callback = NULL;
	timerInterval = 2;
	isValid = false;
	isEnabled = false;
}


void TabletFilter::SetReportState(BYTE buttons, double pressure, double hoverDistance) {
}




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
