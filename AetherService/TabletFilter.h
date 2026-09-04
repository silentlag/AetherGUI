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
void* timer;
#if defined(_WIN32)
	typedef void (CALLBACK *FilterCallback)(UINT wTimerID, UINT msg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2);
#else
	typedef void (*FilterCallback)(unsigned int wTimerID, unsigned int msg,
								   unsigned long dwUser, unsigned long dw1, unsigned long dw2);
#endif
	FilterCallback callback;
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
