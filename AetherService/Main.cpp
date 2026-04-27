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

#include <atomic>

#define LOG_MODULE ""
#include "Logger.h"

// Power throttling — use anonymous struct to avoid redefinition conflicts
#ifndef PROCESS_POWER_THROTTLING_EXECUTION_SPEED
#define PROCESS_POWER_THROTTLING_EXECUTION_SPEED 0x1
#endif

#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "winusb.lib")
#pragma comment(lib, "winmm.lib")


// Global variables...
Tablet *tablet;
VMulti *vmulti;
ScreenMapper *mapper;
thread *tabletThread;
chrono::high_resolution_clock::time_point timeBegin = chrono::high_resolution_clock::now();
chrono::high_resolution_clock::time_point lastMovement = chrono::high_resolution_clock::now();
Vector2D prevPos;

// Live position reporting for GUI (~60Hz throttled)
chrono::high_resolution_clock::time_point lastPosReport = chrono::high_resolution_clock::now();
bool livePosEnabled = true;

// Mutex for tablet state accessed from both tablet thread and timer callback
mutex tabletStateMutex;

// Overclock state — when true, timer callback sends reports every tick
bool overclockActive = false;
double overclockTargetHz = 1000.0;
static atomic<bool> overclockTimerRunning(false);
static thread *overclockTimerThread = NULL;

// Incremented by the tablet thread whenever a new valid USB packet has been
// filtered into tablet->state. The timer thread uses this to interpolate
// between real device reports instead of between its own timer ticks.
unsigned long long tabletReportSequence = 0;

// Real Hz measurement in service
double hzAccumulator = 0;
int hzPacketCount = 0;
chrono::high_resolution_clock::time_point hzWindowStart = chrono::high_resolution_clock::now();
double measuredReportRate = 0;

// === Overclock interpolation state ===
// Inspired by high-rate interpolation approaches used by tablet-driver plugins.
// The smoothing filter already runs at high frequency, but between USB
// reports it converges toward the same target — so consecutive ticks
// produce nearly-identical positions. By tracking the last two
// *filter-output* snapshots and interpolating between them with a
// time-based alpha we generate genuinely new intermediate positions
// on every timer tick, giving real sub-packet smoothness at the
// overclock rate.
struct OverclockInterp {
	double prevX, prevY, prevP;     // filter output at previous USB report
	double currX, currY, currP;     // filter output at latest USB report
	chrono::high_resolution_clock::time_point lastReportTime;
	double reportMsAvg;             // EMA of interval between USB reports
	bool initialized;

	OverclockInterp() : prevX(0), prevY(0), prevP(0),
		currX(0), currY(0), currP(0),
		reportMsAvg(5.0), initialized(false) {}

	// Called once per new USB packet, after all filters have run,
	// with the final smoothed position that the filter pipeline produced.
	void OnNewReport(double x, double y, double pressure) {
		auto now = chrono::high_resolution_clock::now();
		if (initialized) {
			double deltaMs = (now - lastReportTime).count() / 1000000.0;
			if (deltaMs > 0.5 && deltaMs < 150.0)
				reportMsAvg += (deltaMs - reportMsAvg) * 0.1;
		}
		lastReportTime = now;

		// Shift: current becomes previous
		prevX = currX;  prevY = currY;  prevP = currP;
		currX = x;      currY = y;      currP = pressure;

		if (!initialized) {
			prevX = x; prevY = y; prevP = pressure;
			initialized = true;
		}
	}

	// Called on every timer tick to get an interpolated position.
	// alpha is derived from how much time has passed since the last
	// USB report relative to the average report interval.
	void Evaluate(double *outX, double *outY, double *outP) {
		auto now = chrono::high_resolution_clock::now();
		double elapsedMs = (now - lastReportTime).count() / 1000000.0;

		// alpha 0..1  — 0 = at currX, 1 = one full interval ahead (extrapolate)
		double alpha = (reportMsAvg > 0.1) ? (elapsedMs / reportMsAvg) : 0.0;
		// Clamp: allow slight extrapolation (up to 1.0) but no further
		if (alpha < 0.0) alpha = 0.0;
		if (alpha > 1.0) alpha = 1.0;

		// Interpolate from curr toward the next predicted point.
		// The predicted next point = curr + (curr - prev), i.e. linear
		// extrapolation.  At alpha=0 we output curr, at alpha=1 we
		// output the predicted next point.
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

static VOID CALLBACK FilterTimerCallback(UINT wTimerID, UINT msg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2);

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

static void OverclockTimerLoop() {
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

	typedef chrono::steady_clock Clock;
	Clock::duration interval = chrono::duration_cast<Clock::duration>(
		chrono::duration<double, milli>(1000.0 / overclockTargetHz));
	if (interval.count() < 1) {
		interval = Clock::duration(1);
	}

	HANDLE waitTimer = CreateWaitableTimerExW(
		NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
	if (waitTimer == NULL) {
		waitTimer = CreateWaitableTimerW(NULL, FALSE, NULL);
	}

	Clock::time_point nextTick = Clock::now();
	while (overclockTimerRunning.load()) {
		FilterTimerCallback(0, 0, 0, 0, 0);
		nextTick += interval;

		Clock::time_point now = Clock::now();
		if (now > nextTick + interval * 4) {
			nextTick = now + interval;
		}

		now = Clock::now();
		if (now < nextTick) {
			long long remaining100ns = chrono::duration_cast<chrono::nanoseconds>(nextTick - now).count() / 100;
			if (remaining100ns < 1) remaining100ns = 1;

			if (waitTimer != NULL) {
				LARGE_INTEGER dueTime;
				dueTime.QuadPart = -remaining100ns;
				if (SetWaitableTimer(waitTimer, &dueTime, 0, NULL, NULL, FALSE)) {
					WaitForSingleObject(waitTimer, INFINITE);
				}
				else {
					Sleep(0);
				}
			}
			else {
				Sleep(0);
			}
		}
	}

	if (waitTimer != NULL) {
		CloseHandle(waitTimer);
	}
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
	if (targetHz < 125.0) targetHz = 125.0;
	if (targetHz > 2000.0) targetHz = 2000.0;

	StopOverclockTimer();
	overclockTargetHz = targetHz;
	overclockTimerRunning.store(true);
	overclockTimerThread = new thread(OverclockTimerLoop);
}

//
// Init console parameters
//
void InitConsole() {
	HANDLE inputHandle;
	DWORD consoleMode = 0;
	inputHandle = GetStdHandle(STD_INPUT_HANDLE);
	GetConsoleMode(inputHandle, &consoleMode);
	consoleMode = (consoleMode & ~ENABLE_QUICK_EDIT_MODE);
	consoleMode = (consoleMode & ~ENABLE_MOUSE_INPUT);
	consoleMode = (consoleMode & ~ENABLE_WINDOW_INPUT);
	SetConsoleMode(inputHandle, consoleMode);
}

//
// Tablet process
//
void RunTabletThread() {
	int status;
	bool isFirstReport = true;
	bool isResent = false;
	double x, y;
	TabletFilter *filter;
	bool filterTimedEnabled;

	//chrono::high_resolution_clock::time_point timeBegin = chrono::high_resolution_clock::now();
	chrono::high_resolution_clock::time_point timeNow = chrono::high_resolution_clock::now();

	//
	// Main Loop
	//

	while (true) {

		//
		// Read tablet position
		//
		status = tablet->ReadPosition();

		// Position OK
		if (status == Tablet::PacketValid) {
			isResent = false;

			// Count packets for Hz measurement
			hzPacketCount++;

			// Invalid packet id
		}
		else if (status == Tablet::PacketInvalid) {
			continue;

			// Valid packet but position is not in-range or invalid
		}
		else if (status == Tablet::PacketPositionInvalid) {
			if (!isResent && tablet->state.isValid) {
				isResent = true;
				tablet->state.isValid = false;
			}
			else {
				continue;
			}
			// Reading failed
		}
		else {
			LOG_ERROR("Tablet Read Error!\n");
			CleanupAndExit(1);
		}

		//
		// Don't send the first report
		//
		if (isFirstReport) {
			isFirstReport = false;
			continue;
		}

		// Debug messages
		if (tablet->debugEnabled) {
			timeNow = chrono::high_resolution_clock::now();
			double delta = (timeNow - timeBegin).count() / 1000000.0;
			/*LOG_DEBUG("STATE: %0.3f, %d, %0.3f, %0.3f, %0.3f, %0.3f\n",
				delta,
				tablet->state.buttons,
				tablet->state.position.x,
				tablet->state.position.y,
				tablet->state.pressure,
				tablet->state.z
			);*/
			LOG_DEBUG("RAW:%0.3f,%0.3f,%0.3f\n",
				delta,
				tablet->state.position.x,
				tablet->state.position.y
			);
		}


		// Set output values
		// Lock state during modification — timer callback reads under same lock
		{
			lock_guard<mutex> lock(tabletStateMutex);

			if (status == Tablet::PacketPositionInvalid) {
				tablet->state.buttons = 0;
			}

			// Packet filters
			if (tablet->filterPacketCount > 0) {
				for (int filterIndex = 0; filterIndex < tablet->filterPacketCount; filterIndex++) {
					filter = tablet->filterPacket[filterIndex];
					if (filter != NULL && filter->isEnabled) {
						filter->SetTarget(tablet->state.position, tablet->state.z);
						filter->Update();
						filter->GetPosition(&tablet->state.position);
					}
				}
			}

			if (status == Tablet::PacketValid) {
				tabletReportSequence++;
			}
		} // unlock


		// Timed filter enabled?
		filterTimedEnabled = false;
		for (int filterIndex = 0; filterIndex < tablet->filterTimedCount; filterIndex++) {
			if (tablet->filterTimed[filterIndex]->isEnabled)
				filterTimedEnabled = true;
		}

		static Vector2D last;
		if ( 
			// Button binded to wheel + Binded button pressed + tip pressed
			(tablet->buttonMap[1] == 6 and tablet->state.buttons == 33)
			or
			(tablet->buttonMap[2] == 6 and tablet->state.buttons == 5)
			) {
			tablet->state.buttons &= ~(1 << 0);
			mouse_event(MOUSEEVENTF_WHEEL, 0, 0, (DWORD)(-(last.y - tablet->state.position.y) * tablet->settings.mouseWheelSpeed), 0);
		}
		last = tablet->state.position;

		// Send live position + measured Hz to GUI (~60Hz throttled)
		// Uses direct printf to avoid Logger overhead in hot path
		if (livePosEnabled) {
			auto posNow = chrono::high_resolution_clock::now();
			double posDelta = (posNow - lastPosReport).count() / 1000000.0;

			// Calculate real report rate every 500ms window
			double hzWindowMs = (posNow - hzWindowStart).count() / 1000000.0;
			if (hzWindowMs >= 500.0) {
				measuredReportRate = (double)hzPacketCount / (hzWindowMs / 1000.0);
				hzPacketCount = 0;
				hzWindowStart = posNow;
			}

			if (posDelta >= 16.0) { // ~60Hz to GUI
				printf("[STATUS] POS %0.4f %0.4f %0.4f %0.1f\n",
					tablet->state.position.x,
					tablet->state.position.y,
					tablet->state.pressure,
					measuredReportRate);
				fflush(stdout);
				lastPosReport = posNow;
			}
		}

		// Do not write report when timed filter is enabled
		if (tablet->filterTimedCount == 0 || !filterTimedEnabled) {

			// Relative mode
			if (vmulti->mode == VMulti::ModeRelativeMouse) {

				x = tablet->state.position.x;
				y = tablet->state.position.y;

				// Map position to virtual screen (values between 0 and 1)
				mapper->GetRotatedTabletPosition(&x, &y);

				if (!tablet->state.isValid) {
					vmulti->InvalidateRelativeData();
				}

				// Create VMulti report
				vmulti->CreateReport(tablet->state.buttons, x, y, tablet->state.pressure);

				// Write report to VMulti device
				vmulti->WriteReport();



				// Absolute / Digitizer mode
			}
			else {
				// Get x & y from the tablet state
				x = tablet->state.position.x;
				y = tablet->state.position.y;

				// Map position to virtual screen (values betweeb 0->1)
				if (mapper->GetScreenPosition(&x, &y)) {
					// Create VMulti report
					vmulti->CreateReport(tablet->state.buttons, x, y, tablet->state.pressure);

					// Write report to VMulti device
					vmulti->WriteReport();
				}
				// else: AreaLimiting blocked this input
			}
		}
	}

}

//
// Tablet filter timer callback
//

static VOID CALLBACK FilterTimerCallback(UINT wTimerID, UINT msg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
	Vector2D position, position_prev;
	double z;
	TabletFilter *filter;
	BYTE buttons;
	bool stateValid;
	bool buttonsChangedNow;
	double pressure;
	unsigned long long reportSequence;

	chrono::high_resolution_clock::time_point timeNow = chrono::high_resolution_clock::now();

	// Read tablet state under lock to prevent data race with tablet thread
	{
		lock_guard<mutex> lock(tabletStateMutex);
		position.Set(tablet->state.position);
		z = tablet->state.z;
		buttons = tablet->state.buttons;
		stateValid = tablet->state.isValid;
		pressure = tablet->state.pressure;
		reportSequence = tabletReportSequence;
	}
	buttonsChangedNow = (buttons != vmulti->pendingButtons);
	// For debug
	tablet->filterTimed[0]->GetPosition(&position_prev);

	// Detect absence of movement (squared distance to avoid sqrt)
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

	// Loop through filters
	for (int filterIndex = 0; filterIndex < tablet->filterTimedCount; filterIndex++) {

		// Filter
		filter = tablet->filterTimed[filterIndex];

		// Filter enabled?
		if (!filter->isEnabled) continue; // Skip this filter, don't abort the pipeline


		if (noMovement > 35)
		{
			filter->Reset(position);
			// Don't skip if buttons changed — must send click events!
			if (!buttonsChangedNow && !vmulti->buttonsChanged) continue;
		}

		// Set filter targets
		filter->SetTarget(position, z);

		// Update filter position
		filter->Update();

		// Set output vector
		filter->GetPosition(&position);

	}

	// If truly idle and no button changes, skip sending
	if (noMovement > 35 && !buttonsChangedNow && !vmulti->buttonsChanged) return;


	// Debug messages
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

	// Feed the overclock interpolator with the latest filtered position.
	// On each tick the smoothing filter converges a little further toward
	// the raw target — we snapshot the output so that between USB reports
	// we can interpolate/extrapolate from the previous snapshot to the
	// current one, producing genuinely new positions at the timer rate.
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

		// Consume only real USB reports. The timer can run faster than the
		// tablet, so feeding every tick would collapse alpha back to zero.
		if (stateValid && (!hasOverclockSequence || reportSequence != lastOverclockSequence)) {
			overclockInterp.OnNewReport(position.x, position.y, pressure);
			lastOverclockSequence = reportSequence;
			hasOverclockSequence = true;
		}
	}


	//
	// Relative mode
	//
	if (vmulti->mode == VMulti::ModeRelativeMouse) {

		// Map position to virtual screen (values between 0 and 1)
		mapper->GetRotatedTabletPosition(&position.x, &position.y);

		if (!stateValid) {
			vmulti->InvalidateRelativeData();
		}

		// Create VMulti report
		vmulti->CreateReport(
			buttons,
			position.x,
			position.y,
			pressure
		);

		// Write report to VMulti device if report has changed OR buttons changed
		// When overclock active, always send to maximize poll rate
		if (!stateValid) {
			if (vmulti->buttonsChanged) {
				vmulti->WriteReport();
			}
		} else if (overclockActive) {
			if (vmulti->HasReportChanged() || vmulti->buttonsChanged) {
				vmulti->WriteReport();
			}
		} else if (vmulti->HasReportChanged()
			|| vmulti->buttonsChanged
			|| vmulti->reportRelativeMouse.x != 0
			|| vmulti->reportRelativeMouse.y != 0
			) {
			vmulti->WriteReport();
		}


	}

	//
	// Absolute / Digitizer mode
	//
	else {

		// === Overclock interpolation ===
		// Instead of re-sending the same smoothed position on every tick,
		// linearly interpolate/extrapolate from the previous filter output
		// toward the current one based on elapsed time since the last USB
		// report.  This produces genuinely new intermediate positions at
		// the timer rate (e.g. 1000 Hz), giving real sub-packet smoothness
		// while the existing smoothing filter does all the heavy lifting.
		if (overclockActive && stateValid && overclockInterp.HasSample()) {
			double interpX, interpY, interpPressure;
			overclockInterp.Evaluate(&interpX, &interpY, &interpPressure);

			// Map the interpolated tablet-space position to screen
			if (!mapper->GetScreenPosition(&interpX, &interpY)) {
				return; // AreaLimiting blocked
			}

			vmulti->CreateReport(buttons, interpX, interpY, interpPressure);

			if (stateValid) {
				vmulti->WriteReport();
			}
		}
		// === Normal path (no overclock) ===
		else {
			// Map position to virtual screen (values between 0->1)
			if (!mapper->GetScreenPosition(&position.x, &position.y)) {
				// AreaLimiting blocked this position
				return;
			}

			// Create VMulti report
			vmulti->CreateReport(
				buttons,
				position.x,
				position.y,
				pressure
			);

			if ((vmulti->HasReportChanged() || vmulti->buttonsChanged) && stateValid) {
				vmulti->WriteReport();
			}
		}
	}
}



//
// Main
//
int main(int argc, char**argv) {
	string line;
	string filename;
	CommandLine *cmd;
	bool running = false;

	// Init global variables
	vmulti = NULL;
	tablet = NULL;
	tabletThread = NULL;

	// Init console
	InitConsole();

	// Screen mapper
	mapper = new ScreenMapper(tablet);
	mapper->SetRotation(0);

	// Set high timer resolution globally (sub-ms scheduling)
	timeBeginPeriod(1);

	// Set high priority to prevent Windows from throttling us in background
	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

	// Disable Windows 11 power throttling for this process
	// This prevents timer resolution degradation when window loses focus
	{
		struct { ULONG Version; ULONG ControlMask; ULONG StateMask; } throttling = {};
		throttling.Version = 1;
		throttling.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
		throttling.StateMask = 0; // 0 = disable throttling
		SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &throttling, sizeof(throttling));
	}

	// Logger
	//LOGGER_DIRECT = true;
	LOGGER_START();

	// VMulti Device
	vmulti = new VMulti();
	if (vmulti->hidDevice == NULL) {
		LOG_WARNING("VMulti HID device not found.\n");
		LOG_WARNING("Windows Ink (Digitizer) mode will not work.\n");
		LOG_WARNING("Absolute, Relative and SendInput modes work without VMulti.\n");
		LOG_INFO("To use Windows Ink, install the VMulti driver.\n");
	} else {
		LOG_INFO("VMulti device opened.\n");
	}

	// Read init file — fall back to embedded config if files are missing
	filename = "init.cfg";

	if (argc > 1) {
		filename = argv[1];
	}
	if (!ReadCommandFile(filename)) {
		LOG_WARNING("Can't open '%s' — using embedded tablet database\n", filename.c_str());

		// Process all embedded config commands as fallback
		auto embeddedCmds = EmbeddedConfig::GetAllCommands();
		LOG_INFO("\\ Loading embedded config (%d commands)\n", (int)embeddedCmds.size());
		for (const auto& cmdLine : embeddedCmds) {
			CommandLine* ecmd = new CommandLine(cmdLine);
			// Stop processing if tablet is already found
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


	//
	// Main loop that reads input from the console.
	//
	while (true) {

		// Broken pipe
		if (!cin) break;

		// Read line from the console
		try {
			getline(cin, line);
		}
		catch (exception) {
			break;
		}

		// Process valid lines
		if (line.length() > 0) {
			cmd = new CommandLine(line);


			//
			// Start command
			//
			if (cmd->is("start")) {
				LOG_INFO(">> %s\n", cmd->line.c_str());

				if (running) {
					LOG_INFO("Driver is already started!\n");
					delete cmd;
					continue;
				}

				// Unknown tablet
				if (tablet == NULL) {
					LOG_ERROR("Tablet not found!\n");
					CleanupAndExit(1);
				}

				// Tablet init (with retry)
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

				// Set screen mapper tablet
				mapper->tablet = tablet;

				// Set running state
				running = true;

				// Timed filter timer
				if (tablet->filterTimedCount > 0) {
					tablet->filterTimed[0]->callback = FilterTimerCallback;
					if (overclockActive) {
						tablet->filterTimed[0]->StopTimer();
						StartOverclockTimer(overclockTargetHz);
					}
					else {
						tablet->filterTimed[0]->StartTimer();
					}
				}

				// Start the tablet thread
				tabletThread = new thread(RunTabletThread);
				if (GetPriorityClass(GetCurrentProcess()) == HIGH_PRIORITY_CLASS) {
					SetThreadPriority(tabletThread->native_handle(), THREAD_PRIORITY_HIGHEST);
				}

				LOG_INFO("AetherGUI service started!\n");
				LogStatus();


				//
				// Echo
				//
			}
			else if (cmd->is("echo")) {
				if (cmd->valueCount > 0) {
					LOG_INFO("%s\n", cmd->line.c_str() + 5);
				}
				else {
					LOG_INFO("\n");
				}


				//
				// Process all other commands
				//
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



//
// Cleanup and exit
//
void CleanupAndExit(int code) {
	StopOverclockTimer();
	/*
	if(tablet != NULL)
		delete tablet;
	if(vmulti != NULL)
		delete vmulti;
		*/

		// Delete filter timer
	if (tablet != NULL) {
		if (tablet->filterTimedCount != 0) {
			tablet->filterTimed[0]->StopTimer();
		}
	}

	if (vmulti != NULL) {
		vmulti->ResetReport();
	}
	LOGGER_STOP();
	Sleep(500);

	//printf("Press enter to exit...");
	//getchar();
	exit(code);
}

