#pragma once

#include <atomic>
#include <thread>

class TabletFilter {
public:
	virtual void SetTarget(Vector2D vector, double h) = 0;
	virtual void Reset(Vector2D vector) = 0;
	virtual void SetPosition(Vector2D vector, double h) = 0;
	virtual void SetReportState(BYTE buttons, double pressure, double hoverDistance);
	virtual bool GetPosition(Vector2D *vector) = 0;
	virtual void Update() = 0;

	unsigned int uTimerID;
#if defined(_WIN32)
	HANDLE timer;
	LPTIMECALLBACK callback;
#else
	void* timer;
	typedef void (*FilterCallback)(unsigned int wTimerID, unsigned int msg,
								   unsigned long dwUser, unsigned long dw1, unsigned long dw2);
	FilterCallback callback;
#endif
	double timerInterval;

	bool isEnabled;
	bool isValid;

	double hostDtSec;
	double hostRawSpeed;
	bool   hasHostTiming;
	void SetFrameTiming(double dtSec, double rawSpeedMmPerSec);

	TabletFilter();

	bool StartTimer();
	bool StopTimer();

private:

	std::atomic<bool> timerRunning{false};
	std::thread       timerThread;
};
