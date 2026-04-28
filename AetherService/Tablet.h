#pragma once

#include <string>
#include <vector>

#include "USBDevice.h"
#include "HIDDevice.h"
#include "TabletSettings.h"
#include "TabletFilterSmoothing.h"
#include "TabletFilterNoiseReduction.h"
#include "TabletFilterPeak.h"
#include "TabletFilterReconstructor.h"
#include "TabletFilterAdaptive.h"
#include "TabletFilterAetherSmooth.h"
#include "TabletBenchmark.h"
#include "Vector2D.h"

using namespace std;

class Tablet {
public:

	USBDevice * usbDevice;
	HIDDevice * hidDevice;
	HIDDevice * hidDevice2;
	int usbPipeId;

	
	
	
	enum TabletButtons {
		Button1, Button2, Button3, Button4,
		Button5, Button6, Button7, Button8
	};

	
	enum TabletPacketState {
		PacketPositionInvalid = 0,
		PacketValid = 1,
		PacketInvalid = 2
	};

	
	
	
#pragma pack(1)
	struct {
		BYTE reportId;
		BYTE buttons;
		UINT x;
		UINT y;
		UINT pressure;
		UINT z;
	} reportData;

	
	
	
	struct {
		bool isValid;
		BYTE buttons;
		Vector2D position;
		double pressure;
		double z;
	} state;

	
	TabletSettings settings;

	
	TabletFilterSmoothing smoothing;

	
	TabletFilterNoiseReduction noise;

	
	TabletFilterPeak peak;

	
	TabletFilterReconstructor reconstructor;

	
	TabletFilterAdaptive adaptive;

	
	TabletFilterAetherSmooth aetherSmooth;

	
	TabletFilter *filterTimed[10];
	int filterTimedCount;

	
	TabletFilter *filterPacket[10];
	int filterPacketCount;

	
	TabletBenchmark benchmark;

	
	BYTE buttonMap[16];

	
	string name = "Unknown";
	bool isOpen;
	bool debugEnabled;
	int skipPackets;

	
	int tipDownCounter;

	
	vector<vector<BYTE>> initFeatureReports;
	vector<vector<BYTE>> initOutputReports;
	BYTE *initFeature;
	int initFeatureLength;
	BYTE *initReport;
	int initReportLength;
	int initStringIds[8];
	int initStringCount;

	Tablet(string usbGUID, int stringId, string stringMatch);
	Tablet(USHORT vendorId, USHORT productId, USHORT usagePage, USHORT usage, int inputReportLength = 0, int stringId = 0, string stringMatch = "", int stringId2 = 0, string stringMatch2 = "");
	Tablet();
	~Tablet();

	bool Init();
	bool IsConfigured();

	int ReadPosition();
	bool Write(void *buffer, int length);
	bool Read(void *buffer, int length);
	void CloseDevice();

};
