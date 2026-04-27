#include "stdafx.h"
#include "Tablet.h"

#define LOG_MODULE "Tablet"
#include "Logger.h"


//
// USB Device Constructor
//
Tablet::Tablet(string usbGUID, int stringId, string stringMatch) : Tablet() {
	usbDevice = new USBDevice(usbGUID, stringId, stringMatch);
	if (usbDevice->isOpen) {
		this->isOpen = true;
		usbPipeId = 0x81;
	}
	else {
		delete usbDevice;
		usbDevice = NULL;
	}
}

//
// HID Device Constructor
//
Tablet::Tablet(USHORT vendorId, USHORT productId, USHORT usagePage, USHORT usage, int inputReportLength) : Tablet() {
	hidDevice = new HIDDevice(vendorId, productId, usagePage, usage, inputReportLength);
	if (hidDevice->isOpen) {
		this->isOpen = true;
	}
	else {
		delete hidDevice;
		hidDevice = NULL;
	}
}

//
// Common constructor
//
Tablet::Tablet() {

	name = "Unknown";
	usbDevice = NULL;
	hidDevice = NULL;
	hidDevice2 = NULL;

	usbPipeId = 0;

	// Init reports
	initFeature = NULL;
	initFeatureLength = 0;
	initReport = NULL;
	initReportLength = 0;

	// Reset state
	memset(&state, 0, sizeof(state));

	// Filters
	filterTimed[0] = &smoothing;
	filterTimedCount = 1;
	filterPacket[0] = &noise;
	filterPacket[1] = &reconstructor;
	filterPacket[2] = &adaptive;
	filterPacket[3] = &aetherSmooth;
	filterPacketCount = 4;

	peak.isEnabled = true;

	// Button map
	memset(&buttonMap, 0, sizeof(buttonMap));
	buttonMap[0] = 1;
	buttonMap[1] = 2;
	buttonMap[2] = 3;

	// Tablet connection open
	isOpen = false;

	// Debug output
	debugEnabled = false;

	// Skip first packets, some of those might be invalid.
	skipPackets = 5;

	// Keep tip down packet counter
	tipDownCounter = 0;

}

//
// Destructor
//
Tablet::~Tablet() {
	CloseDevice();
	if (usbDevice != NULL)
		delete usbDevice;
	if (hidDevice != NULL)
		delete hidDevice;
	if (hidDevice2 != NULL)
		delete hidDevice2;
	if (initReport != NULL)
		delete initReport;
	if (initFeature != NULL)
		delete initFeature;
}


//
// Init — with retry logic for when other drivers/services hold the device
//
bool Tablet::Init() {
	const int MAX_RETRIES = 5;
	const int RETRY_DELAY_MS = 500;

	// Feature report
	if (initFeature != NULL && hidDevice != NULL) {
		for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
			if (hidDevice->SetFeature(initFeature, initFeatureLength)) {
				LOG_INFO("Tablet init feature report sent successfully.\n");
				return true;
			}
			DWORD err = GetLastError();
			LOG_WARNING("Init feature attempt %d/%d failed (error 0x%08X).\n", attempt + 1, MAX_RETRIES, err);
			if (err == ERROR_DEVICE_NOT_CONNECTED) {
				LOG_ERROR("Device disconnected during init.\n");
				return false;
			}
			if (attempt < MAX_RETRIES - 1) {
				LOG_INFO("Retrying in %d ms...\n", RETRY_DELAY_MS);
				Sleep(RETRY_DELAY_MS);
			}
		}
		LOG_ERROR("All init feature attempts failed. Another driver may be blocking the device.\n");
		LOG_ERROR("Try: taskkill /F /IM WTabletServicePro.exe  or  net stop TabletInputService\n");
		return false;
	}

	// Output report
	if (initReport != NULL && hidDevice != NULL) {
		for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
			if (hidDevice->Write(initReport, initReportLength)) {
				LOG_INFO("Tablet init report sent successfully.\n");
				return true;
			}
			DWORD err = GetLastError();
			LOG_WARNING("Init report attempt %d/%d failed (error 0x%08X).\n", attempt + 1, MAX_RETRIES, err);
			if (err == ERROR_DEVICE_NOT_CONNECTED) {
				LOG_ERROR("Device disconnected during init.\n");
				return false;
			}
			if (attempt < MAX_RETRIES - 1) {
				LOG_INFO("Retrying in %d ms...\n", RETRY_DELAY_MS);
				Sleep(RETRY_DELAY_MS);
			}
		}
		LOG_ERROR("All init report attempts failed. Another driver may be blocking the device.\n");
		LOG_ERROR("Try: taskkill /F /IM WTabletServicePro.exe  or  net stop TabletInputService\n");
		return false;
	}

	// USB (Huion)
	if (usbDevice != NULL) {
		BYTE buffer[64];
		int status;

		// String Id 200
		status = usbDevice->ControlTransfer(0x80, 0x06, (0x03 << 8) | 200, 0x0409, buffer, 64);

		// String Id 100
		status += usbDevice->ControlTransfer(0x80, 0x06, (0x03 << 8) | 100, 0x0409, buffer, 64);

		if (status > 0) {
			return true;
		}
		LOG_ERROR("USB init failed (Huion-style control transfers returned 0).\n");
		return false;
	}

	// No init required — this is fine
	return true;
}


//
// Check if the tablet has enough configuration parameters set
//
bool Tablet::IsConfigured() {
	if (
		settings.maxX > 1 &&
		settings.maxY > 1 &&
		settings.maxPressure > 1 &&
		settings.width > 1 &&
		settings.height > 1
		) return true;
	return false;
}

//
// Read Position
//
int Tablet::ReadPosition() {
	UCHAR buffer[1024];
	UCHAR *data;
	int buttonIndex;


	// Read report
	if (!this->Read(buffer, settings.reportLength)) {
		return -1;
	}

	// Skip packets
	if (skipPackets > 0) {
		skipPackets--;
		return Tablet::PacketInvalid;
	}


	// Set data pointer
	if (settings.type == TabletSettings::TypeWacomDrivers) {
		data = buffer + 1;
	}
	else {
		data = buffer;
	}

	//
	// Wacom Intuos data format
	//
	if (settings.type == TabletSettings::TypeWacomIntuos) {
		reportData.reportId = data[0];
		reportData.buttons = data[1] & ~0x01;
		reportData.x = ((data[2] * 0x100 + data[3]) << 1) | ((data[9] >> 1) & 1);
		reportData.y = ((data[4] * 0x100 + data[5]) << 1) | (data[9] & 1);
		reportData.pressure = (data[6] << 3) | ((data[7] & 0xC0) >> 5) | (data[1] & 1);
		//distance = buffer[9] >> 2;

	//
	// Wacom 4100 data format
	//
	}
	else if (settings.type == TabletSettings::TypeWacom4100) {

		// Wacom driver device
		if (settings.reportLength == 193) {
			data = buffer + 1;
		}

		reportData.reportId = data[0];
		reportData.buttons = data[1] & ~0x01;
		reportData.x = (data[2] | (data[3] << 8) | (data[4] << 16));
		reportData.y = (data[5] | (data[6] << 8) | (data[7] << 16));
		reportData.pressure = (data[8] | (data[9] << 8));

		//
		// Copy buffer to struct
		//
	}
	else if (settings.type == TabletSettings::TypeWacomBamboo) {
		reportData.reportId = data[0];
		reportData.buttons = 0;
		if (data[1] & 0x01) reportData.buttons |= 0x01;
		if (data[1] & 0x02) reportData.buttons |= 0x02;
		if (data[1] & 0x04) reportData.buttons |= 0x04;

		reportData.x = data[2] | (data[3] << 8);
		reportData.y = data[4] | (data[5] << 8);
		reportData.pressure = (data[1] & 0x01) ? (data[6] | ((data[7] & 0x03) << 8)) : 0;
		reportData.z = 0;

		bool hasPosition =
			(data[1] & 0x80) ||
			reportData.x != 0 ||
			reportData.y != 0 ||
			reportData.pressure != 0;
		if (!hasPosition) {
			return Tablet::PacketPositionInvalid;
		}
	}
	else if (settings.type == TabletSettings::TypeWacomIntuos4) {
		if (settings.reportLength == 11) {
			data = buffer + 1;
		}

		reportData.reportId = data[0];
		reportData.buttons = data[1];
		reportData.x = ((data[3] | (data[2] << 8)) << 1) | ((data[9] >> 1) & 1);
		reportData.y = ((data[5] | (data[4] << 8)) << 1) | (data[9] & 1);
		reportData.pressure = (data[6] << 3) | ((data[7] & 0xC0) >> 5) | (data[1] & 1);
		reportData.z = data[9] >> 2;
	}
	else {
		memcpy(&reportData, data, sizeof(reportData));
	}


	// Validate packet id
	if (settings.reportId > 0 && reportData.reportId != settings.reportId) {
		return Tablet::PacketInvalid;
	}



	// Detect mask
	if (settings.detectMask > 0 && (reportData.buttons & settings.detectMask) != settings.detectMask) {
		return Tablet::PacketPositionInvalid;
	}

	// Ignore mask
	if (settings.ignoreMask > 0 && (reportData.buttons & settings.ignoreMask) == settings.ignoreMask) {
		return Tablet::PacketPositionInvalid;
	}

	//
	// Use pen pressure to detect the pen tip click
	//
	if (settings.clickPressure > 0) {
		reportData.buttons &= ~1;
		if (reportData.pressure > settings.clickPressure) {
			reportData.buttons |= 1;
		}

		// Force tip button down if pressure is detected
	}
	else if (reportData.pressure > 1) {
		// Threshold=1 to catch even the lightest tap immediately
		reportData.buttons |= 1;
	}

	// Keep pen tip button down for a few packets
	if (settings.keepTipDown > 0) {
		if (reportData.buttons & 0x01) {
			tipDownCounter = settings.keepTipDown;
		}
		if (tipDownCounter-- >= 0) {
			reportData.buttons |= 1;
		}
	}


	// Set valid
	state.isValid = true;

	// Button map
	reportData.buttons = reportData.buttons & 0x0F;
	state.buttons = 0;
	for (buttonIndex = 0; buttonIndex < sizeof(buttonMap); buttonIndex++) {

		// Button is set
		if (buttonMap[buttonIndex] > 0) {

			// Button is pressed
			if ((reportData.buttons & (1 << buttonIndex)) > 0) {
				state.buttons |= (1 << (buttonMap[buttonIndex] - 1));
			}
		}
	}

	// Convert report data to state (uses pre-computed reciprocals)
	state.position.x = (double)reportData.x * settings.invMaxX * settings.width;
	state.position.y = (double)reportData.y * settings.invMaxY * settings.height;
	state.z = (double)data[8];
	if (settings.skew != 0)
		state.position.x += state.position.y * settings.skew;
	state.pressure = ((double)reportData.pressure / (double)settings.maxPressure);

	// Tablet benchmark update
	benchmark.Update(state.position);

	// Packet and position is valid
	return Tablet::PacketValid;
}


//
// Read report from tablet
//
bool Tablet::Read(void *buffer, int length) {
	if (!isOpen) return false;
	bool status = false;
	if (usbDevice != NULL) {
		status = usbDevice->Read(usbPipeId, buffer, length) > 0;
	}
	else if (hidDevice != NULL) {
		int readLength = length;
		if (hidDevice->inputReportLength > readLength) {
			readLength = hidDevice->inputReportLength;
		}
		status = hidDevice->Read(buffer, readLength);
	}
	if (debugEnabled && status) {
		LOG_DEBUGBUFFER(buffer, length, "Read: ");
	}
	return status;
}

//
// Write report to the tablet
//
bool Tablet::Write(void *buffer, int length) {
	if (!isOpen) return false;
	if (usbDevice != NULL) {
		return usbDevice->Write(usbPipeId, buffer, length) > 0;
	}
	else if (hidDevice != NULL) {
		return hidDevice->Write(buffer, length);
	}
	return false;
}

//
// Close tablet
//
void Tablet::CloseDevice() {
	if (isOpen) {
		if (usbDevice != NULL) {
			usbDevice->CloseDevice();
		}
		else if (hidDevice != NULL) {
			hidDevice->CloseDevice();
		}
	}
	isOpen = false;
}
