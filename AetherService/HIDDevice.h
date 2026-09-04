#pragma once

#if defined(_WIN32)
	#include <windows.h>
	#include <stdio.h>
	#include <tchar.h>
	#include <strsafe.h>
	#include <math.h>
	#include <SetupAPI.h>
	#include <hidsdi.h>
	#include <hidpi.h>
	#include <psapi.h>
#else
	#include <cstdint>
	#include <cstdio>
	using HANDLE = void*;
	using BYTE   = uint8_t;
	using UCHAR  = uint8_t;
	using WORD   = uint16_t;
	using USHORT = uint16_t;
	using UINT   = uint32_t;
	using UINT16 = uint16_t;
	using UINT32 = uint32_t;
	using DWORD  = uint32_t;
	using LONG   = int32_t;
	using ULONG  = uint32_t;
	using WCHAR  = wchar_t;
#endif

#include <iostream>
#include <string>

using namespace std;

class HIDDevice {
private:
	HANDLE _deviceHandle;
#if defined(_WIN32)

	PHIDP_PREPARSED_DATA _preparsedData = NULL;
#endif
public:
	bool isOpen;
	bool debugEnabled;
	USHORT vendorId;
	USHORT productId;
	USHORT usagePage;
	USHORT usage;
	int inputReportLength;
	int outputReportLength;
	int featureReportLength;
	int stringId;
	string stringMatch;
	int stringId2;
	string stringMatch2;

	struct DigitizerReport {
		int x;
		int y;
		int pressure;
		bool tipSwitch;
		bool barrelSwitch;
		bool inRange;
		bool valid;
	};

	HIDDevice(USHORT VendorId, USHORT ProductId, USHORT UsagePage, USHORT Usage, int InputReportLength = 0, int StringId = 0, string StringMatch = "", int StringId2 = 0, string StringMatch2 = "");
	HIDDevice();
	~HIDDevice();
	bool OpenDevice(HANDLE *handle, USHORT vendorId, USHORT productId, USHORT usagePage, USHORT usage, int inputReportLength = 0, int stringId = 0, string stringMatch = "", int stringId2 = 0, string stringMatch2 = "");

	bool OpenFeatureCapable(USHORT vendorId, USHORT productId);

	bool IsDigitizer();

	bool GetLogicalMax(USHORT usagePage, USHORT usage, long *outMax);

	bool ParseDigitizer(void *buffer, int length, DigitizerReport *out);

	int Read(void *buffer, int length);
	int Write(void *buffer, int length);
	bool SetFeature(void *buffer, int length);
	bool GetFeature(void *buffer, int length);
	bool GetIndexedString(int stringId, string *result = NULL);
	void CloseDevice();
	bool Reopen();
};
