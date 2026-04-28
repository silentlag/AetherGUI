#pragma once

#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>
#include <math.h>
#include <SetupAPI.h>
#include <hidsdi.h>
#include <psapi.h>
#include <iostream>


using namespace std;

class HIDDevice {
private:
	HANDLE _deviceHandle;
public:
	bool isOpen;
	bool debugEnabled;
	USHORT vendorId;
	USHORT productId;
	USHORT usagePage;
	USHORT usage;
	int inputReportLength;
	int stringId;
	string stringMatch;
	int stringId2;
	string stringMatch2;

	HIDDevice(USHORT VendorId, USHORT ProductId, USHORT UsagePage, USHORT Usage, int InputReportLength = 0, int StringId = 0, string StringMatch = "", int StringId2 = 0, string StringMatch2 = "");
	HIDDevice();
	~HIDDevice();
	bool OpenDevice(HANDLE *handle, USHORT vendorId, USHORT productId, USHORT usagePage, USHORT usage, int inputReportLength = 0, int stringId = 0, string stringMatch = "", int stringId2 = 0, string stringMatch2 = "");
	int Read(void *buffer, int length);
	int Write(void *buffer, int length);
	bool SetFeature(void *buffer, int length);
	bool GetFeature(void *buffer, int length);
	bool GetIndexedString(int stringId, string *result = NULL);
	void CloseDevice();
};
