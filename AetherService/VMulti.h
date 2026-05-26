#pragma once

#include "HIDDevice.h"
#include "Vector2D.h"

#if !defined(_WIN32)
	#include <cstdint>
	using BYTE = uint8_t;

#endif

class VMulti {
public:
	HIDDevice * hidDevice;
	BYTE reportBuffer[65];
	BYTE lastReportBuffer[65];
public:

	enum VMultiMode {
		ModeAbsoluteMouse,
		ModeRelativeMouse,
		ModeDigitizer,
		ModeSendInput,
		ModeAbsoluteVMulti,
		// "Artist" mode: emits both absolute (digitizer) events for
		// desktop apps and relative-motion deltas on the virtual
		// mouse so games that grab the cursor or use raw mouse input
		// keep receiving input. Mirrors OpenTabletDriver's Artist
		// Mode.
		ModeArtist
	};

	struct {
		BYTE vmultiId;
		BYTE reportLength;
		BYTE reportId;
		BYTE buttons;
		USHORT x;
		USHORT y;
		BYTE wheel;
	} reportAbsoluteMouse;

	struct {
		BYTE vmultiId;
		BYTE reportLength;
		BYTE reportId;
		BYTE buttons;
		BYTE x;
		BYTE y;
		BYTE wheel;
	} reportRelativeMouse;

	struct {
		BYTE vmultiId;
		BYTE reportLength;
		BYTE reportId;
		BYTE buttons;
		USHORT x;
		USHORT y;
		USHORT pressure;
	} reportDigitizer;

	typedef struct {
		int x;
		int y;
	} PositionInt;

	struct {
		PositionInt currentPosition;
		Vector2D lastPosition;
		Vector2D targetPosition;
		double sensitivity;
		double resetDistance;
		double accumX;
		double accumY;
		bool firstReport;
	} relativeData;

	struct {
		double primaryWidth  = 0;
		double primaryHeight = 0;
		double virtualWidth  = 0;
		double virtualHeight = 0;
		double virtualX      = 0;
		double virtualY      = 0;
	} monitorInfo;

	VMultiMode mode;
	bool isOpen;
	bool debugEnabled;
	bool outputEnabled;
	int lastButtons;
	int pendingButtons;
	bool buttonsChanged;
	bool monitorInfoLocked;

	VMulti();
	~VMulti();
	bool HasReportChanged();
	void ResetRelativeData(double x, double y);
	void InvalidateRelativeData();
	void UpdateMonitorInfo();
	void SetMonitorInfo(double primaryWidth, double primaryHeight, double virtualWidth, double virtualHeight, double virtualX, double virtualY);
	void CreateReport(BYTE buttons, double x, double y, double pressure);
	int ResetReport();
	int WriteReport();

	void EmulateWheel(int delta);
};
