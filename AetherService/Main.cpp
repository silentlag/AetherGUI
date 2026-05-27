#include "stdafx.h"

#include <csignal>

#include "HIDDevice.h"
#include "USBDevice.h"
#include "VMulti.h"
#include "Tablet.h"
#include "ScreenMapper.h"
#include "CommandLine.h"
#include "ProcessCommand.h"
#include "EmbeddedConfig.h"
#include "StatusSharedWriter.h"

// Shared-memory writer for high-rate pen-status updates. Initialized in
// main() if the OS lets us; if creation fails the service falls back to
// emitting only the rate-limited [STATUS] POS text lines.
StatusSharedWriter g_statusShmem;
#include "AetherPluginManager.h"

#include <atomic>
#include <algorithm>
#ifdef _WIN32
  #include <io.h>
  #include <fcntl.h>
#else
  #include <unistd.h>

  #define _write  ::write
  #define _fileno ::fileno
#endif

#include "Platform.h"

#define LOG_MODULE ""
#include "Logger.h"

#ifndef PROCESS_POWER_THROTTLING_EXECUTION_SPEED
#define PROCESS_POWER_THROTTLING_EXECUTION_SPEED 0x1
#endif

#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "winusb.lib")

Tablet *tablet;
VMulti *vmulti;
ScreenMapper *mapper;
thread *tabletThread;
chrono::high_resolution_clock::time_point timeBegin = chrono::high_resolution_clock::now();
chrono::high_resolution_clock::time_point lastMovement = chrono::high_resolution_clock::now();
Vector2D prevPos;

static atomic<bool> timedOutputEnabledCache{false};

static atomic<int> activeOutputMode{0};

struct TabletStateSnapshot {
	double posX;
	double posY;
	double pressure;
	double z;
	unsigned long long sequence;
	BYTE buttons;
	bool isValid;
};

static atomic<uint64_t> stateSeq{0};
static TabletStateSnapshot stateSnapshot{};

static inline void PublishTabletState(double px, double py,
									  double pressure, double z,
									  BYTE buttons, bool isValid,
									  unsigned long long sequence) {
	uint64_t seq = stateSeq.load(memory_order_relaxed);

	stateSeq.store(seq + 1, memory_order_relaxed);
	atomic_thread_fence(memory_order_release);

	stateSnapshot.posX = px;
	stateSnapshot.posY = py;
	stateSnapshot.pressure = pressure;
	stateSnapshot.z = z;
	stateSnapshot.buttons = buttons;
	stateSnapshot.isValid = isValid;
	stateSnapshot.sequence = sequence;

	atomic_thread_fence(memory_order_release);

	stateSeq.store(seq + 2, memory_order_relaxed);
}

static inline TabletStateSnapshot ReadTabletState() {
	TabletStateSnapshot snap;
	for (;;) {
		uint64_t s1 = stateSeq.load(memory_order_acquire);
		if (s1 & 1) {

			platform::CpuPause();
			continue;
		}
		snap = stateSnapshot;
		atomic_thread_fence(memory_order_acquire);
		uint64_t s2 = stateSeq.load(memory_order_relaxed);
		if (s1 == s2) {
			return snap;
		}
		platform::CpuPause();
	}
}

chrono::high_resolution_clock::time_point lastPosReport = chrono::high_resolution_clock::now();
bool livePosEnabled = true;

mutex tabletStateMutex;

bool overclockActive = false;
double overclockTargetHz = 1000.0;
static atomic<bool> overclockTimerRunning(false);
static thread *overclockTimerThread = NULL;

bool penRateLimitActive = false;
double penRateLimitHz = 133.0;
static mutex penRateLimitMutex;
static chrono::steady_clock::time_point lastPenOutputTime = chrono::steady_clock::now() - chrono::seconds(1);
static chrono::steady_clock::time_point outputHzWindowStart = chrono::steady_clock::now();
static int outputHzPacketCount = 0;
static double measuredOutputRate = 0;
static atomic<bool> penRateTimerRunning(false);
static thread *penRateTimerThread = NULL;

atomic<unsigned long long> tabletReportSequence{0};

double hzAccumulator = 0;
int hzPacketCount = 0;
chrono::high_resolution_clock::time_point hzWindowStart = chrono::high_resolution_clock::now();
double measuredReportRate = 0;

struct OverclockInterp {
	double prevX, prevY, prevP;
	double currX, currY, currP;
	chrono::high_resolution_clock::time_point lastReportTime;
	double reportMsAvg;
	bool initialized;

	OverclockInterp() : prevX(0), prevY(0), prevP(0),
		currX(0), currY(0), currP(0),
		reportMsAvg(5.0), initialized(false) {}

	void OnNewReport(double x, double y, double pressure) {
		auto now = chrono::high_resolution_clock::now();
		if (initialized) {
			double deltaMs = (now - lastReportTime).count() / 1000000.0;
			if (deltaMs > 0.5 && deltaMs < 150.0)
				reportMsAvg += (deltaMs - reportMsAvg) * 0.1;
		}
		lastReportTime = now;

		prevX = currX;  prevY = currY;  prevP = currP;
		currX = x;      currY = y;      currP = pressure;

		if (!initialized) {
			prevX = x; prevY = y; prevP = pressure;
			initialized = true;
		}
	}

	void Evaluate(double *outX, double *outY, double *outP) {
		auto now = chrono::high_resolution_clock::now();
		double elapsedMs = (now - lastReportTime).count() / 1000000.0;

		double alpha = (reportMsAvg > 0.1) ? (elapsedMs / reportMsAvg) : 0.0;

		if (alpha < 0.0) alpha = 0.0;
		if (alpha > 1.0) alpha = 1.0;

		double nextX = currX + (currX - prevX);
		double nextY = currY + (currY - prevY);
		double nextP = currP + (currP - prevP);
		if (nextP < 0) nextP = 0;

		*outX = currX + (nextX - currX) * alpha;
		*outY = currY + (nextY - currY) * alpha;
		*outP = currP + (nextP - currP) * alpha;
		if (*outP < 0) *outP = 0;
	}

	bool HasSample() const {
		return initialized;
	}

	void Reset(double x, double y, double pressure) {
		prevX = currX = x;
		prevY = currY = y;
		prevP = currP = pressure;
		lastReportTime = chrono::high_resolution_clock::now();
		initialized = false;
		reportMsAvg = 5.0;
	}
} overclockInterp;

static atomic<long long> g_packetReadTimeNs{0};
static atomic<bool>      g_packetTimingArmed{false};

static constexpr int     kLatencyBufSize = 4096;
static atomic<uint32_t>  g_latencyHead{0};
static uint32_t          g_latencyBuf[kLatencyBufSize] = {0};
static chrono::steady_clock::time_point g_lastLatencyReport = chrono::steady_clock::now();

static inline long long NowNs() {
	return chrono::duration_cast<chrono::nanoseconds>(
		chrono::steady_clock::now().time_since_epoch()).count();
}

static void RecordLatencyUs(uint32_t us) {
	uint32_t idx = g_latencyHead.fetch_add(1, memory_order_relaxed) & (kLatencyBufSize - 1);
	g_latencyBuf[idx] = us;
}

static void MaybePublishLatency() {
	auto now = chrono::steady_clock::now();
	double dtMs = chrono::duration<double, milli>(now - g_lastLatencyReport).count();
	if (dtMs < 500.0) return;
	g_lastLatencyReport = now;

	uint32_t head = g_latencyHead.load(memory_order_relaxed);
	uint32_t available = head < kLatencyBufSize ? head : kLatencyBufSize;
	uint32_t window = available < (kLatencyBufSize / 2) ? available : (kLatencyBufSize / 2);
	if (window < 16) return;

	static uint32_t scratch[kLatencyBufSize / 2];
	double sumUs = 0.0;
	uint32_t maxUs = 0;
	for (uint32_t i = 0; i < window; i++) {
		uint32_t idx = (head - 1 - i) & (kLatencyBufSize - 1);
		uint32_t v = g_latencyBuf[idx];
		scratch[i] = v;
		sumUs += v;
		if (v > maxUs) maxUs = v;
	}
	std::sort(scratch, scratch + window);
	uint32_t p99 = scratch[(uint32_t)((double)window * 0.99)];

	double avgMs = (sumUs / (double)window) / 1000.0;
	double p99Ms = (double)p99 / 1000.0;
	double maxMs = (double)maxUs / 1000.0;

	char buf[160];
	int n = snprintf(buf, sizeof(buf),
		"[STATUS] LATENCY %0.3f %0.3f %0.3f %u\n",
		avgMs, p99Ms, maxMs, window);
	// Mirror to shmem so the GUI sees fresh latency without having to wait
	// for the text line through the pipe.
	g_statusShmem.PushLatency((float)avgMs, (float)p99Ms, (float)maxMs, (int)window);
	if (n > 0) {
		_write(_fileno(stdout), buf, n);
	}
}

#if defined(_WIN32)
static VOID CALLBACK FilterTimerCallback(UINT wTimerID, UINT msg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2);
#else
static void FilterTimerCallback(unsigned int wTimerID, unsigned int msg,
                                unsigned long dwUser, unsigned long dw1, unsigned long dw2);
#endif

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

static void OverclockTimerLoop() {
	platform::ThreadBoostHandle boost = platform::BoostCurrentThread(platform::ThreadBoostTier::Timer);

	int64_t intervalNs = (int64_t)(1000000000.0 / overclockTargetHz);
	if (intervalNs < 1) intervalNs = 1;

	int64_t spinThresholdNs = intervalNs / 4;
	if (spinThresholdNs > 100000) spinThresholdNs = 100000;
	if (spinThresholdNs < 5000)   spinThresholdNs = 5000;

	int64_t nextTickNs = platform::MonotonicNs();
	while (overclockTimerRunning.load()) {
		FilterTimerCallback(0, 0, 0, 0, 0);
		nextTickNs += intervalNs;

		int64_t now = platform::MonotonicNs();
		if (now > nextTickNs + intervalNs * 4) {
			nextTickNs = now + intervalNs;
		}

		platform::SleepUntilNs(nextTickNs, spinThresholdNs);
	}

	platform::RestoreCurrentThread(boost);
}

void StopOverclockTimer() {
	overclockTimerRunning.store(false);
	if (overclockTimerThread != NULL) {
		if (overclockTimerThread->joinable()) {
			if (overclockTimerThread->get_id() == this_thread::get_id()) {
				overclockTimerThread->detach();
			}
			else {
				overclockTimerThread->join();
			}
		}
		delete overclockTimerThread;
		overclockTimerThread = NULL;
	}
}

void StartOverclockTimer(double targetHz) {
	if (targetHz < 30.0) targetHz = 30.0;
	if (targetHz > 2000.0) targetHz = 2000.0;

	StopOverclockTimer();
	overclockTargetHz = targetHz;
	overclockTimerRunning.store(true);
	overclockTimerThread = new thread(OverclockTimerLoop);
}

static void PenRateTimerLoop() {
	platform::ThreadBoostHandle boost = platform::BoostCurrentThread(platform::ThreadBoostTier::Timer);

	double targetHz = penRateLimitHz;
	if (targetHz < 30.0)   targetHz = 30.0;
	if (targetHz > 1000.0) targetHz = 1000.0;

	int64_t intervalNs = (int64_t)(1000000000.0 / targetHz);
	if (intervalNs < 1) intervalNs = 1;

	int64_t spinThresholdNs = intervalNs / 4;
	if (spinThresholdNs > 100000) spinThresholdNs = 100000;
	if (spinThresholdNs < 5000)   spinThresholdNs = 5000;

	int64_t nextTickNs = platform::MonotonicNs();
	while (penRateTimerRunning.load()) {
		FilterTimerCallback(0, 0, 0, 0, 0);
		nextTickNs += intervalNs;

		int64_t now = platform::MonotonicNs();
		if (now > nextTickNs + intervalNs * 4) {
			nextTickNs = now + intervalNs;
		}

		platform::SleepUntilNs(nextTickNs, spinThresholdNs);
	}

	platform::RestoreCurrentThread(boost);
}

static void StopPenRateTimer() {
	penRateTimerRunning.store(false);
	if (penRateTimerThread != NULL) {
		if (penRateTimerThread->joinable()) {
			if (penRateTimerThread->get_id() == this_thread::get_id()) {
				penRateTimerThread->detach();
			}
			else {
				penRateTimerThread->join();
			}
		}
		delete penRateTimerThread;
		penRateTimerThread = NULL;
	}
}

static void StartPenRateTimer(double targetHz) {
	if (targetHz < 30.0) targetHz = 30.0;
	if (targetHz > 1000.0) targetHz = 1000.0;

	StopPenRateTimer();
	penRateLimitHz = targetHz;
	penRateTimerRunning.store(true);
	penRateTimerThread = new thread(PenRateTimerLoop);
}

void ResetPenRateLimiter() {
	lock_guard<mutex> lock(penRateLimitMutex);
	double hz = penRateLimitHz;
	if (hz < 30.0) hz = 30.0;
	if (hz > 1000.0) hz = 1000.0;

	auto interval = chrono::duration_cast<chrono::steady_clock::duration>(
		chrono::duration<double, milli>(1000.0 / hz));
	lastPenOutputTime = chrono::steady_clock::now() - interval;
	outputHzWindowStart = chrono::steady_clock::now();
	outputHzPacketCount = 0;
	measuredOutputRate = 0;
}

int WritePenReport(bool force) {
	if (vmulti == NULL)
		return 0;

	auto now = chrono::steady_clock::now();
	if (penRateLimitActive && !force && vmulti->mode != VMulti::ModeRelativeMouse) {
		double hz = penRateLimitHz;
		if (hz < 30.0) hz = 30.0;
		if (hz > 1000.0) hz = 1000.0;

		auto interval = chrono::duration_cast<chrono::steady_clock::duration>(
			chrono::duration<double, milli>(1000.0 / hz));
		lock_guard<mutex> lock(penRateLimitMutex);
		if (now - lastPenOutputTime < interval)
			return 0;
		lastPenOutputTime = now;
	}
	else {
		lock_guard<mutex> lock(penRateLimitMutex);
		lastPenOutputTime = now;
	}

	int result = vmulti->WriteReport();
	if (result > 0) {

		if (g_packetTimingArmed.load(memory_order_relaxed)) {
			long long readNs = g_packetReadTimeNs.load(memory_order_relaxed);
			long long deltaNs = NowNs() - readNs;
			if (deltaNs >= 0 && deltaNs < 500000000LL) {
				RecordLatencyUs((uint32_t)(deltaNs / 1000));
			}
			g_packetTimingArmed.store(false, memory_order_relaxed);
		}

		lock_guard<mutex> lock(penRateLimitMutex);
		outputHzPacketCount++;
		double windowMs = chrono::duration<double, milli>(now - outputHzWindowStart).count();
		if (windowMs >= 500.0) {
			measuredOutputRate = (double)outputHzPacketCount / (windowMs / 1000.0);
			outputHzPacketCount = 0;
			outputHzWindowStart = now;
		}
	}
	return result;
}

bool IsTimedOutputEnabled() {
	if (tablet == NULL) return false;
	if (overclockActive) return true;
	if (penRateLimitActive) return true;
	for (int filterIndex = 0; filterIndex < tablet->filterTimedCount; filterIndex++) {
		TabletFilter *filter = tablet->filterTimed[filterIndex];
		if (filter == NULL) continue;
		if (filter->isEnabled) return true;
		if (filter == &tablet->smoothing && tablet->smoothing.AntichatterEnabled) return true;
	}
	return false;
}

void InvalidateTimedOutputCache() {
	timedOutputEnabledCache.store(IsTimedOutputEnabled(), memory_order_relaxed);
}

void RefreshTimedOutputTimer() {
	InvalidateTimedOutputCache();

	if (tablet == NULL || tablet->filterTimedCount <= 0 || tablet->filterTimed[0] == NULL)
		return;
	if (tablet->filterTimed[0]->callback == NULL)
		return;

	if (overclockActive) {
		StopPenRateTimer();
		tablet->filterTimed[0]->StopTimer();
		return;
	}

	StopOverclockTimer();

	if (penRateLimitActive) {
		double hz = penRateLimitHz;
		if (hz < 30.0) hz = 30.0;
		if (hz > 1000.0) hz = 1000.0;
		double interval = 1000.0 / hz;
		for (int fi = 0; fi < tablet->filterTimedCount; fi++) {
			tablet->filterTimed[fi]->timerInterval = interval;
		}
		tablet->smoothing.SetLatency(tablet->smoothing.latency);
		tablet->filterTimed[0]->StopTimer();
		StartPenRateTimer(hz);
		return;
	}

	StopPenRateTimer();

	if (IsTimedOutputEnabled()) {
		tablet->filterTimed[0]->StartTimer();
	}
	else {
		tablet->filterTimed[0]->StopTimer();
	}
}

void InitConsole() {
#if defined(_WIN32)

	HANDLE inputHandle;
	DWORD consoleMode = 0;
	inputHandle = GetStdHandle(STD_INPUT_HANDLE);
	GetConsoleMode(inputHandle, &consoleMode);
	consoleMode = (consoleMode & ~ENABLE_QUICK_EDIT_MODE);
	consoleMode = (consoleMode & ~ENABLE_MOUSE_INPUT);
	consoleMode = (consoleMode & ~ENABLE_WINDOW_INPUT);
	SetConsoleMode(inputHandle, consoleMode);
#else

#endif
}

void RunTabletThread() {
	int status;
	bool isFirstReport = true;
	bool isResent = false;
	double x, y;
	TabletFilter *filter;
	bool filterTimedEnabled;

	platform::ThreadBoostHandle readerBoost =
		platform::BoostCurrentThread(platform::ThreadBoostTier::Producer);

	chrono::high_resolution_clock::time_point timeNow = chrono::high_resolution_clock::now();

	while (true) {

		status = tablet->ReadPosition();

		if (status == Tablet::PacketValid) {
			g_packetReadTimeNs.store(NowNs(), memory_order_relaxed);
			g_packetTimingArmed.store(true, memory_order_relaxed);
		}

		if (status == Tablet::PacketValid) {
			isResent = false;

			hzPacketCount++;

		}
		else if (status == Tablet::PacketInvalid) {
			continue;

		}
		else if (status == Tablet::PacketPositionInvalid) {
			if (!isResent && tablet->state.isValid) {
				isResent = true;
				tablet->state.isValid = false;
			}
			else {
				continue;
			}

		}
		else {
			LOG_ERROR("Tablet Read Error!\n");
			CleanupAndExit(1);
		}

		if (isFirstReport) {
			isFirstReport = false;
			continue;
		}

		if (tablet->debugEnabled) {
			timeNow = chrono::high_resolution_clock::now();
			double delta = (timeNow - timeBegin).count() / 1000000.0;

			LOG_DEBUG("RAW:%0.3f,%0.3f,%0.3f\n",
				delta,
				tablet->state.position.x,
				tablet->state.position.y
			);
		}

		{
			lock_guard<mutex> lock(tabletStateMutex);

			if (status == Tablet::PacketPositionInvalid) {
				tablet->state.buttons = 0;
			}

			if (tablet->filterPacketCount > 0) {
				for (int filterIndex = 0; filterIndex < tablet->filterPacketCount; filterIndex++) {
					filter = tablet->filterPacket[filterIndex];
					if (filter != NULL && filter->isEnabled) {
						filter->SetReportState(tablet->state.buttons, tablet->state.pressure, tablet->state.z);
						filter->SetTarget(tablet->state.position, tablet->state.z);
						filter->Update();
						filter->GetPosition(&tablet->state.position);
					}
				}
			}

			if (status == Tablet::PacketValid) {
				tabletReportSequence.fetch_add(1, memory_order_relaxed);
			}
		}

		PublishTabletState(
			tablet->state.position.x,
			tablet->state.position.y,
			tablet->state.pressure,
			tablet->state.z,
			tablet->state.buttons,
			tablet->state.isValid,
			tabletReportSequence.load(memory_order_relaxed));

		filterTimedEnabled = timedOutputEnabledCache.load(memory_order_relaxed);

		static Vector2D last;

		if (
			(tablet->buttonMap[1] == 6 and tablet->state.buttons == 33)
			or
			(tablet->buttonMap[2] == 6 and tablet->state.buttons == 5)
			) {
			tablet->state.buttons &= ~(1 << 0);

			double dy = last.y - tablet->state.position.y;
			int delta = (int)(-dy * tablet->settings.mouseWheelSpeed);
			if (delta != 0) {
				vmulti->EmulateWheel(delta);
			}
		}
		last = tablet->state.position;

		if (livePosEnabled) {
			auto posNow = chrono::high_resolution_clock::now();
			double posDelta = (posNow - lastPosReport).count() / 1000000.0;

			double hzWindowMs = (posNow - hzWindowStart).count() / 1000000.0;
			if (hzWindowMs >= 500.0) {
				measuredReportRate = (double)hzPacketCount / (hzWindowMs / 1000.0);
				hzPacketCount = 0;
				hzWindowStart = posNow;
			}

			MaybePublishLatency();

			// Push every packet into the shared-memory ring (zero syscalls).
			// The text [STATUS] POS line below is still emitted but rate-limited
			// to ~60 Hz, kept for backwards compatibility with older GUIs that
			// don't know about shmem.
			{
				double shmemRate = penRateLimitActive ? measuredOutputRate : measuredReportRate;
				uint64_t tsNs = (uint64_t)(posNow - timeBegin).count();
				g_statusShmem.PushPos(
					tsNs,
					(float)tablet->state.position.x,
					(float)tablet->state.position.y,
					(float)tablet->state.pressure,
					(float)shmemRate,
					(tablet->state.buttons & 1) != 0);
			}

			if (posDelta >= 16.0) {
				double statusRate = penRateLimitActive ? measuredOutputRate : measuredReportRate;

				char statusBuf[128];
				int statusLen = snprintf(statusBuf, sizeof(statusBuf),
					"[STATUS] POS %0.4f %0.4f %0.4f %0.1f\n",
					tablet->state.position.x,
					tablet->state.position.y,
					tablet->state.pressure,
					statusRate);
				if (statusLen > 0) {
					_write(_fileno(stdout), statusBuf, statusLen);
				}
				lastPosReport = posNow;
			}
		}

		if (tablet->filterTimedCount == 0 || !filterTimedEnabled) {

			if (vmulti->mode == VMulti::ModeRelativeMouse) {

				x = tablet->state.position.x;
				y = tablet->state.position.y;

				mapper->GetRotatedTabletPosition(&x, &y);

				if (!tablet->state.isValid) {
					vmulti->InvalidateRelativeData();
				}

				vmulti->CreateReport(tablet->state.buttons, x, y, tablet->state.pressure);

				WritePenReport(false);

			}
			else {

				x = tablet->state.position.x;
				y = tablet->state.position.y;

				if (mapper->GetScreenPosition(&x, &y)) {

					vmulti->CreateReport(tablet->state.buttons, x, y, tablet->state.pressure);

					WritePenReport(vmulti->buttonsChanged || !tablet->state.isValid);
				}

			}
		}
	}

}

#if defined(_WIN32)
static VOID CALLBACK FilterTimerCallback(UINT wTimerID, UINT msg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
#else
static void FilterTimerCallback(unsigned int wTimerID, unsigned int msg,
                                unsigned long dwUser, unsigned long dw1, unsigned long dw2)
#endif
{
	(void)wTimerID; (void)msg; (void)dwUser; (void)dw1; (void)dw2;
	if (!timedOutputEnabledCache.load(memory_order_relaxed))
		return;

	Vector2D position, position_prev;
	double z;
	TabletFilter *filter;
	BYTE buttons;
	bool stateValid;
	bool buttonsChangedNow;
	double pressure;
	unsigned long long reportSequence;

	chrono::high_resolution_clock::time_point timeNow = chrono::high_resolution_clock::now();

	{
		TabletStateSnapshot snap = ReadTabletState();
		position.Set(snap.posX, snap.posY);
		z = snap.z;
		buttons = snap.buttons;
		stateValid = snap.isValid;
		pressure = snap.pressure;
		reportSequence = snap.sequence;
	}
	buttonsChangedNow = (buttons != vmulti->pendingButtons);

	tablet->filterTimed[0]->GetPosition(&position_prev);

	double noMovement = 0.0;
	double dxM = position.x - prevPos.x;
	double dyM = position.y - prevPos.y;
	if (dxM * dxM + dyM * dyM > 0.000000001)
	{
		lastMovement = timeNow;
		prevPos = position;
	}
	else
	{
		noMovement = (timeNow - lastMovement).count() / 1000000.0;
	}

	for (int filterIndex = 0; filterIndex < tablet->filterTimedCount; filterIndex++) {

		filter = tablet->filterTimed[filterIndex];

		bool filterActive = filter->isEnabled;
		if (filter == &tablet->smoothing && tablet->smoothing.AntichatterEnabled) {
			filterActive = true;
		}
		if (!filterActive) continue;

		if (noMovement > 35)
		{
			filter->Reset(position);

			if (!buttonsChangedNow && !vmulti->buttonsChanged) continue;
		}

		filter->SetReportState(tablet->state.buttons, tablet->state.pressure, z);
		filter->SetTarget(position, z);

		filter->Update();

		filter->GetPosition(&position);

	}

	if (noMovement > 35 && !buttonsChangedNow && !vmulti->buttonsChanged) return;

	if (tablet->debugEnabled) {
		timeNow = chrono::high_resolution_clock::now();
		double delta = (timeNow - timeBegin).count() / 1000000.0;

		if (round(position.x*100) != round(position_prev.x * 100) or round(position.y * 100) != round(position_prev.y * 100)) {
			LOG_DEBUG("FIL:%0.3f,%0.3f,%0.3f\n",
				delta,
				position.x,
				position.y
			);
		}

	}

	static unsigned long long lastOverclockSequence = 0;
	static bool hasOverclockSequence = false;
	static bool wasOverclockActive = false;

	if (!overclockActive) {
		wasOverclockActive = false;
		hasOverclockSequence = false;
	}

	if (overclockActive) {
		if (!wasOverclockActive || !stateValid) {
			overclockInterp.Reset(position.x, position.y, pressure);
			hasOverclockSequence = false;
			wasOverclockActive = true;
		}

		if (stateValid && (!hasOverclockSequence || reportSequence != lastOverclockSequence)) {
			overclockInterp.OnNewReport(position.x, position.y, pressure);
			lastOverclockSequence = reportSequence;
			hasOverclockSequence = true;
		}
	}

	if (vmulti->mode == VMulti::ModeRelativeMouse) {

		mapper->GetRotatedTabletPosition(&position.x, &position.y);

		if (!stateValid) {
			vmulti->InvalidateRelativeData();
		}

		vmulti->CreateReport(
			buttons,
			position.x,
			position.y,
			pressure
		);

		if (!stateValid) {
			if (vmulti->buttonsChanged) {
				WritePenReport(true);
			}
		} else if (overclockActive) {
			if (vmulti->HasReportChanged() || vmulti->buttonsChanged) {
				WritePenReport(vmulti->buttonsChanged || buttonsChangedNow);
			}
		} else if (vmulti->HasReportChanged()
			|| vmulti->buttonsChanged
			|| vmulti->reportRelativeMouse.x != 0
			|| vmulti->reportRelativeMouse.y != 0
			) {
			WritePenReport(vmulti->buttonsChanged || buttonsChangedNow);
		}

	}

	else {

		if (overclockActive && stateValid && overclockInterp.HasSample()) {
			double interpX, interpY, interpPressure;
			overclockInterp.Evaluate(&interpX, &interpY, &interpPressure);

			if (!mapper->GetScreenPosition(&interpX, &interpY)) {
				return;
			}

			vmulti->CreateReport(buttons, interpX, interpY, interpPressure);

			if (stateValid) {
				WritePenReport(vmulti->buttonsChanged || buttonsChangedNow);
			}
		}

		else {

			if (!mapper->GetScreenPosition(&position.x, &position.y)) {

				return;
			}

			vmulti->CreateReport(
				buttons,
				position.x,
				position.y,
				pressure
			);

			if ((vmulti->HasReportChanged() || vmulti->buttonsChanged) && stateValid) {
				WritePenReport(vmulti->buttonsChanged || buttonsChangedNow);
			}
		}
	}
}

static bool StartServiceRuntime(bool *running) {
	if (running != NULL && *running) {
		LOG_INFO("Driver is already started!\n");
		return true;
	}

	if (tablet == NULL) {
		LOG_ERROR("Tablet not found!\n");
		CleanupAndExit(1);
		return false;
	}

	EnsureAetherPluginDirectory();
	{
		lock_guard<mutex> lock(tabletStateMutex);
		tablet->ReloadPluginFilters(GetAetherPluginDirectory());
	}

	if (!tablet->Init()) {
		LOG_ERROR("Tablet init failed after retries!\n");
		LOG_ERROR("Possible fixes:\n");
		LOG_ERROR("1) Uninstall other tablet drivers (Wacom, XP-Pen, Huion).\n");
		LOG_ERROR("2) Stop tablet services: net stop TabletInputService\n");
		LOG_ERROR("3) Kill blocking processes: taskkill /F /IM WTabletServicePro.exe\n");
		LOG_ERROR("4) Disable Windows Ink in Windows Settings.\n");
		LOG_ERROR("5) Reconnect the tablet USB cable and try again.\n");
		LOG_ERROR("Driver will continue without init (may work with some tablets).\n");
		LOG_WARNING("Proceeding without init — some features may not work.\n");
	}

	mapper->tablet = tablet;

	if (running != NULL)
		*running = true;

	if (tablet->filterTimedCount > 0) {
		tablet->filterTimed[0]->callback = FilterTimerCallback;
		if (overclockActive) {
			tablet->filterTimed[0]->StopTimer();
			StartOverclockTimer(overclockTargetHz);
		}
		else {
			RefreshTimedOutputTimer();
		}
	}

	tabletThread = new thread(RunTabletThread);
#if defined(_WIN32)
	if (GetPriorityClass(GetCurrentProcess()) == HIGH_PRIORITY_CLASS) {
		SetThreadPriority(tabletThread->native_handle(), THREAD_PRIORITY_HIGHEST);
	}
#endif

	LOG_INFO("AetherGUI service started!\n");
	LogStatus();

	if (mapper != NULL) {
		LOG_STATUS("AREA_TABLET %0.4f %0.4f %0.4f %0.4f\n",
			mapper->areaTablet.width, mapper->areaTablet.height,
			mapper->areaTablet.x, mapper->areaTablet.y);
		LOG_STATUS("AREA_SCREEN %0.0f %0.0f %0.0f %0.0f\n",
			mapper->areaScreen.width, mapper->areaScreen.height,
			mapper->areaScreen.x, mapper->areaScreen.y);
	}
	if (vmulti != NULL) {
		const char* mn = "unknown";
		switch (vmulti->mode) {
			case VMulti::ModeAbsoluteMouse:  mn = "Absolute"; break;
			case VMulti::ModeRelativeMouse:  mn = "Relative"; break;
			case VMulti::ModeDigitizer:      mn = "Digitizer"; break;
			case VMulti::ModeSendInput:      mn = "SendInput"; break;
			case VMulti::ModeAbsoluteVMulti: mn = "RawAbsolute"; break;
		}
		LOG_STATUS("OUTPUT_MODE %s\n", mn);
	}
	return true;
}

int main(int argc, char**argv) {
	string line;
	string filename;
	CommandLine *cmd;
	bool running = false;

	vmulti = NULL;
	tablet = NULL;
	tabletThread = NULL;

	InitConsole();

	mapper = new ScreenMapper(tablet);
	mapper->SetRotation(0);

	platform::GlobalInit();

	// Bring up the shared-memory transport. Best-effort: on failure (no
	// permissions, exotic sandbox) we just don't have the fast path and the
	// GUI falls back to text [STATUS] POS lines.
	if (!g_statusShmem.Initialize()) {
		LOG_INFO("shmem: CreateFileMapping failed, GUI will use text status only\n");
	}

#if defined(_WIN32)
	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

	// Match the GUI's DPI awareness. Without this the service reads desktop
	// metrics in logical pixels (e.g. 1152 instead of 1440 on a 125% monitor)
	// and SendInput/SetCursorPos work in logical coordinates while the GUI
	// sends area rectangles in physical pixels. The two sides disagreed by
	// exactly the DPI factor, which is what "cursor stops at 80%" looked like.
	{
		HMODULE user32 = GetModuleHandleW(L"user32.dll");
		typedef BOOL (WINAPI *PFN_SPDAC)(DPI_AWARENESS_CONTEXT);
		PFN_SPDAC pSetCtx = user32 ? reinterpret_cast<PFN_SPDAC>(
			GetProcAddress(user32, "SetProcessDpiAwarenessContext")) : nullptr;
		bool ok = pSetCtx && pSetCtx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
		if (!ok) {
			SetProcessDPIAware();
		}
	}

	{
		struct { ULONG Version; ULONG ControlMask; ULONG StateMask; } throttling = {};
		throttling.Version = 1;
		throttling.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
		throttling.StateMask = 0;
		SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &throttling, sizeof(throttling));
	}
#endif

	LOGGER_START();

	vmulti = new VMulti();
	if (vmulti->hidDevice == NULL) {
		LOG_WARNING("VMulti HID device not found.\n");
		LOG_WARNING("Windows Ink (Digitizer) mode will not work.\n");
		LOG_WARNING("Absolute, Relative and SendInput modes work without VMulti.\n");
		LOG_INFO("To use Windows Ink, install the VMulti driver.\n");
	} else {
		LOG_INFO("VMulti device opened.\n");
	}

	filename = "init.cfg";

	if (argc > 1) {
		filename = argv[1];
	}
	if (!ReadCommandFile(filename)) {
		LOG_WARNING("Can't open '%s' — using embedded tablet database\n", filename.c_str());

		auto embeddedCmds = EmbeddedConfig::GetAllCommands();
		LOG_INFO("\\ Loading embedded config (%d commands)\n", (int)embeddedCmds.size());
		for (const auto& cmdLine : embeddedCmds) {
			CommandLine* ecmd = new CommandLine(cmdLine);

			if (ecmd->is("Tablet") && tablet != NULL && tablet->IsConfigured()) {
				LOG_INFO(">> %s\n", ecmd->line.c_str());
				LOG_INFO("Tablet is already defined!\n");
				delete ecmd;
				break;
			}
			ProcessCommand(ecmd);
			delete ecmd;
		}
		LOG_INFO("/ End of embedded config\n");
	}

	if (argc <= 1) {
		LOG_INFO("No GUI/stdin config supplied; starting service automatically.\n");
		StartServiceRuntime(&running);
	}

	while (true) {

		if (!cin) break;

		try {
			getline(cin, line);
		}
		catch (exception) {
			break;
		}

		if (line.length() > 0) {
			cmd = new CommandLine(line);

			if (cmd->is("start")) {
				LOG_INFO(">> %s\n", cmd->line.c_str());
				StartServiceRuntime(&running);
			}
			else if (cmd->is("echo")) {
				if (cmd->valueCount > 0) {
					LOG_INFO("%s\n", cmd->line.c_str() + 5);
				}
				else {
					LOG_INFO("\n");
				}

			}
			else {
				ProcessCommand(cmd);
			}
			delete cmd;
		}

	}

	CleanupAndExit(0);
	return 0;
}

void CleanupAndExit(int code) {
	StopOverclockTimer();

	if (tablet != NULL) {
		if (tablet->filterTimedCount != 0) {
			tablet->filterTimed[0]->StopTimer();
		}
	}

	if (vmulti != NULL) {
		vmulti->ResetReport();
	}
	LOGGER_STOP();
	g_statusShmem.Shutdown();
	platform::GlobalShutdown();
	platform::SleepMs(500);

	exit(code);
}

