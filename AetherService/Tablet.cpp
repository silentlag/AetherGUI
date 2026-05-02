#include "stdafx.h"
#include "Tablet.h"

#define LOG_MODULE "Tablet"
#include "Logger.h"

static inline UINT16 ReadLe16(const UCHAR *data, int offset) {
	return (UINT16)(data[offset] | (data[offset + 1] << 8));
}

static inline UINT16 ReadBe16(const UCHAR *data, int offset) {
	return (UINT16)((data[offset] << 8) | data[offset + 1]);
}





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




Tablet::Tablet(USHORT vendorId, USHORT productId, USHORT usagePage, USHORT usage, int inputReportLength, int stringId, string stringMatch, int stringId2, string stringMatch2) : Tablet() {
	hidDevice = new HIDDevice(vendorId, productId, usagePage, usage, inputReportLength, stringId, stringMatch, stringId2, stringMatch2);
	if (hidDevice->isOpen) {
		this->isOpen = true;
	}
	else {
		delete hidDevice;
		hidDevice = NULL;
	}
}




Tablet::Tablet() {

	name = "Unknown";
	usbDevice = NULL;
	hidDevice = NULL;
	hidDevice2 = NULL;

	usbPipeId = 0;

	
	initFeature = NULL;
	initFeatureLength = 0;
	initReport = NULL;
	initReportLength = 0;
	memset(initStringIds, 0, sizeof(initStringIds));
	initStringCount = 0;

	
	memset(&state, 0, sizeof(state));

	
	filterTimed[0] = &smoothing;
	filterTimedCount = 1;
	ResetPacketFilters();

	peak.isEnabled = true;

	
	memset(&buttonMap, 0, sizeof(buttonMap));
	buttonMap[0] = 1;
	buttonMap[1] = 2;
	buttonMap[2] = 3;

	
	isOpen = false;

	
	debugEnabled = false;

	
	skipPackets = 5;

	
	tipDownCounter = 0;

}




Tablet::~Tablet() {
	ClearPluginFilters();
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


void Tablet::ResetPacketFilters() {
	filterPacketCount = 0;
	filterPacket[filterPacketCount++] = &noise;
	filterPacket[filterPacketCount++] = &reconstructor;
	filterPacket[filterPacketCount++] = &adaptive;
	filterPacket[filterPacketCount++] = &aetherSmooth;
}


void Tablet::ClearPluginFilters() {
	for (size_t i = 0; i < pluginFilters.size(); i++) {
		delete pluginFilters[i];
	}
	pluginFilters.clear();
	ResetPacketFilters();
}


void Tablet::ReloadPluginFilters(const std::wstring& pluginDirectory) {
	ClearPluginFilters();

	DWORD attrs = GetFileAttributesW(pluginDirectory.c_str());
	if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
		LOG_INFO("Plugin directory is empty or missing: %ls\n", pluginDirectory.c_str());
		return;
	}

	int loaded = 0;
	WIN32_FIND_DATAW folderData = {};
	std::wstring folderPattern = pluginDirectory + L"*";
	HANDLE folderFind = FindFirstFileW(folderPattern.c_str(), &folderData);
	if (folderFind != INVALID_HANDLE_VALUE) {
		do {
			if (!(folderData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				continue;
			if (wcscmp(folderData.cFileName, L".") == 0 || wcscmp(folderData.cFileName, L"..") == 0)
				continue;

			std::wstring dllPattern = pluginDirectory + folderData.cFileName + L"\\*.dll";
			WIN32_FIND_DATAW dllData = {};
			HANDLE dllFind = FindFirstFileW(dllPattern.c_str(), &dllData);
			if (dllFind == INVALID_HANDLE_VALUE)
				continue;

			do {
				if (dllData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
					continue;
				if (filterPacketCount >= (int)(sizeof(filterPacket) / sizeof(filterPacket[0]))) {
					LOG_WARNING("Plugin filter limit reached, skipping remaining plugins.\n");
					FindClose(dllFind);
					FindClose(folderFind);
					LOG_INFO("Loaded %d Aether plugin(s).\n", loaded);
					return;
				}

				std::wstring dllPath = pluginDirectory + folderData.cFileName + L"\\" + dllData.cFileName;
				TabletFilterPlugin* plugin = new TabletFilterPlugin();
				if (plugin->Load(dllPath)) {
					pluginFilters.push_back(plugin);
					filterPacket[filterPacketCount++] = plugin;
					loaded++;
				}
				else {
					delete plugin;
				}
			} while (FindNextFileW(dllFind, &dllData));
			FindClose(dllFind);
		} while (FindNextFileW(folderFind, &folderData));
		FindClose(folderFind);
	}

	LOG_INFO("Loaded %d Aether plugin(s).\n", loaded);
}


int Tablet::LoadPluginFilters(const std::wstring& pluginDirectory) {
	ReloadPluginFilters(pluginDirectory);
	return (int)pluginFilters.size();
}





bool Tablet::Init() {
	const int MAX_RETRIES = 5;
	const int RETRY_DELAY_MS = 500;

	if (hidDevice != NULL && initStringCount > 0) {
		for (int i = 0; i < initStringCount; i++) {
			string indexedString = "";
			if (hidDevice->GetIndexedString(initStringIds[i], &indexedString)) {
				LOG_INFO("HID init string %d read: %s\n", initStringIds[i], indexedString.c_str());
			}
			else {
				LOG_WARNING("HID init string %d read failed.\n", initStringIds[i]);
			}
		}
	}

	
	if (!initFeatureReports.empty() && hidDevice != NULL) {
		for (size_t reportIndex = 0; reportIndex < initFeatureReports.size(); reportIndex++) {
			vector<BYTE> &report = initFeatureReports[reportIndex];
			bool sent = false;
			for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
				if (hidDevice->SetFeature(report.data(), (int)report.size())) {
					LOG_INFO("Tablet init feature report %d/%d sent successfully.\n",
						(int)reportIndex + 1, (int)initFeatureReports.size());
					sent = true;
					break;
				}
				DWORD err = GetLastError();
				LOG_WARNING("Init feature report %d/%d attempt %d/%d failed (error 0x%08X).\n",
					(int)reportIndex + 1, (int)initFeatureReports.size(), attempt + 1, MAX_RETRIES, err);
				if (err == ERROR_DEVICE_NOT_CONNECTED) {
					LOG_ERROR("Device disconnected during init.\n");
					return false;
				}
				if (attempt < MAX_RETRIES - 1) {
					LOG_INFO("Retrying in %d ms...\n", RETRY_DELAY_MS);
					Sleep(RETRY_DELAY_MS);
				}
			}
			if (!sent) {
				LOG_ERROR("All init feature attempts failed. Another driver may be blocking the device.\n");
				LOG_ERROR("Try: taskkill /F /IM WTabletServicePro.exe  or  net stop TabletInputService\n");
				return false;
			}
		}
		return true;
	}

	
	if (!initOutputReports.empty() && hidDevice != NULL) {
		for (size_t reportIndex = 0; reportIndex < initOutputReports.size(); reportIndex++) {
			vector<BYTE> &report = initOutputReports[reportIndex];
			bool sent = false;
			for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
				if (hidDevice->Write(report.data(), (int)report.size())) {
					LOG_INFO("Tablet init report %d/%d sent successfully.\n",
						(int)reportIndex + 1, (int)initOutputReports.size());
					sent = true;
					break;
				}
				DWORD err = GetLastError();
				LOG_WARNING("Init report %d/%d attempt %d/%d failed (error 0x%08X).\n",
					(int)reportIndex + 1, (int)initOutputReports.size(), attempt + 1, MAX_RETRIES, err);
				if (err == ERROR_DEVICE_NOT_CONNECTED) {
					LOG_ERROR("Device disconnected during init.\n");
					return false;
				}
				if (attempt < MAX_RETRIES - 1) {
					LOG_INFO("Retrying in %d ms...\n", RETRY_DELAY_MS);
					Sleep(RETRY_DELAY_MS);
				}
			}
			if (!sent) {
				LOG_ERROR("All init report attempts failed. Another driver may be blocking the device.\n");
				LOG_ERROR("Try: taskkill /F /IM WTabletServicePro.exe  or  net stop TabletInputService\n");
				return false;
			}
		}
		return true;
	}

	
	if (usbDevice != NULL) {
		BYTE buffer[64];
		int status;

		
		status = usbDevice->ControlTransfer(0x80, 0x06, (0x03 << 8) | 200, 0x0409, buffer, 64);

		
		status += usbDevice->ControlTransfer(0x80, 0x06, (0x03 << 8) | 100, 0x0409, buffer, 64);

		if (status > 0) {
			return true;
		}
		LOG_ERROR("USB init failed (Huion-style control transfers returned 0).\n");
		return false;
	}

	
	return true;
}





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




int Tablet::ReadPosition() {
	UCHAR buffer[1024];
	UCHAR *data;
	int buttonIndex;


	
	memset(buffer, 0, sizeof(buffer));
	if (!this->Read(buffer, settings.reportLength)) {
		return -1;
	}

	
	if (skipPackets > 0) {
		skipPackets--;
		return Tablet::PacketInvalid;
	}


	
	memset(&reportData, 0, sizeof(reportData));
	int reportOffset = settings.reportOffset;
	if (settings.type == TabletSettings::TypeWacomDrivers && reportOffset == 0) {
		reportOffset = 1;
	}
	data = buffer + reportOffset;

	
	
	
	if (settings.type == TabletSettings::TypeWacomIntuos) {
		if (settings.reportLength == 11 && settings.reportOffset == 0) {
			data = buffer + 1;
		}
		reportData.reportId = data[0];
		reportData.buttons = data[1] & ~0x01;
		reportData.x = ((data[2] * 0x100 + data[3]) << 1) | ((data[9] >> 1) & 1);
		reportData.y = ((data[4] * 0x100 + data[5]) << 1) | (data[9] & 1);
		reportData.pressure = (data[6] << 3) | ((data[7] & 0xC0) >> 5) | (data[1] & 1);
		

	
	
	
	}
	else if (settings.type == TabletSettings::TypeWacom4100) {

		
		if (settings.reportLength == 193) {
			data = buffer + 1;
		}

		reportData.reportId = data[0];
		reportData.buttons = data[1] & ~0x01;
		reportData.x = (data[2] | (data[3] << 8) | (data[4] << 16));
		reportData.y = (data[5] | (data[6] << 8) | (data[7] << 16));
		reportData.pressure = (data[8] | (data[9] << 8));

		
		
		
	}
	else if (settings.type == TabletSettings::TypeWacomIntuosV2) {
		if (data[0] == 0x10) {
			reportData.reportId = data[0];
			reportData.buttons = data[1] & ~0x01;
			reportData.x = data[2] | (data[3] << 8) | (data[4] << 16);
			reportData.y = data[5] | (data[6] << 8) | (data[7] << 16);
			reportData.pressure = data[8] | (data[9] << 8);
			reportData.z = data[16];
		}
		else if (data[0] == 0x1E) {
			reportData.reportId = data[0];
			reportData.buttons = (data[2] & ~0x01) | (data[1] & 0x20);
			reportData.x = data[3] | (data[4] << 8) | (data[5] << 16);
			reportData.y = data[6] | (data[7] << 8) | (data[8] << 16);
			reportData.pressure = data[9] | (data[10] << 8);
			reportData.z = data[11];
		}
		else {
			return Tablet::PacketInvalid;
		}
	}
	else if (settings.type == TabletSettings::TypeWacomIntuosV3) {
		if (data[0] == 0x1F && data[1] == 0x01) {
			reportData.reportId = data[0];
			reportData.buttons = data[2] & ~0x01;
			reportData.x = data[3] | (data[4] << 8);
			reportData.y = data[5] | (data[6] << 8);
			reportData.pressure = data[7] | (data[8] << 8);
			reportData.z = data[13];
		}
		else if (data[0] == 0x1E) {
			reportData.reportId = data[0];
			reportData.buttons = data[2] & ~0x01;
			reportData.x = data[3] | (data[4] << 8) | (data[5] << 16);
			reportData.y = data[6] | (data[7] << 8) | (data[8] << 16);
			reportData.pressure = data[9] | (data[10] << 8);
			reportData.z = data[19];
		}
		else {
			return Tablet::PacketInvalid;
		}
	}
	else if (settings.type == TabletSettings::TypeUCLogic) {
		if (data[1] == 0xC0 || (data[1] & 0x40)) {
			return Tablet::PacketPositionInvalid;
		}
		reportData.reportId = data[0];
		reportData.buttons = data[1] & ~0x01;
		reportData.x = data[2] | (data[3] << 8);
		reportData.y = data[4] | (data[5] << 8);
		reportData.pressure = data[6] | (data[7] << 8);
		reportData.z = 0;
	}
	else if (settings.type == TabletSettings::TypeUCLogicV1) {
		if (data[1] == 0xE0 || (data[1] & 0x40) == 0) {
			return Tablet::PacketPositionInvalid;
		}
		reportData.reportId = data[0];
		reportData.buttons = data[1] & ~0x41;
		reportData.x = data[2] | (data[3] << 8);
		reportData.y = data[4] | (data[5] << 8);
		reportData.pressure = data[6] | (data[7] << 8);
		reportData.z = 0;
	}
	else if (settings.type == TabletSettings::TypeUCLogicV2) {
		if (data[1] == 0xE0 || data[1] == 0xF0) {
			return Tablet::PacketPositionInvalid;
		}
		reportData.reportId = data[0];
		reportData.buttons = data[1] & ~0x01;
		reportData.x = data[2] | (data[3] << 8);
		reportData.y = data[4] | (data[5] << 8);
		reportData.pressure = data[6] | (data[7] << 8);
		reportData.z = 0;
	}
	else if (settings.type == TabletSettings::TypeInspiroy) {
		if (data[1] == 0x00 || data[1] == 0xE0 || data[1] == 0xE3 || data[1] == 0xF1 || (data[1] & 0x80) == 0) {
			return Tablet::PacketPositionInvalid;
		}
		reportData.reportId = data[0];
		reportData.buttons = data[1] & ~0x81;
		reportData.x = data[2] | (data[3] << 8);
		reportData.y = data[4] | (data[5] << 8);
		reportData.pressure = data[6] | (data[7] << 8);
		reportData.z = 0;
	}
	else if (settings.type == TabletSettings::TypeXPPen) {
		if (data[1] == 0xC0 || (data[1] & 0x10)) {
			return Tablet::PacketPositionInvalid;
		}
		reportData.reportId = data[0];
		reportData.buttons = data[1] & ~0x01;
		reportData.x = data[2] | (data[3] << 8);
		reportData.y = data[4] | (data[5] << 8);
		reportData.pressure = data[6] | (data[7] << 8);
		if (settings.reportLength >= 12) {
			reportData.x |= data[10] << 16;
			reportData.y |= data[11] << 16;
		}
		reportData.z = 0;
	}
	else if (settings.type == TabletSettings::TypeXPPenOffsetPressure) {
		if (data[1] == 0xC0 || (data[1] & 0x10)) {
			return Tablet::PacketPositionInvalid;
		}
		reportData.reportId = data[0];
		reportData.buttons = data[1] & ~0x01;
		reportData.x = data[2] | (data[3] << 8);
		reportData.y = data[4] | (data[5] << 8);
		reportData.pressure = data[6] | (data[7] << 8);
		if (settings.reportLength >= 10) {
			reportData.pressure &= 0x1FFF;
		}
		if (settings.reportLength >= 12) {
			reportData.x |= data[10] << 16;
			reportData.y |= data[11] << 16;
		}
		reportData.z = 0;
	}
	else if (settings.type == TabletSettings::TypeXPPenGen2) {
		if (data[1] == 0xC0 || data[1] == 0xF0 || (data[1] & 0xF0) != 0xA0) {
			return Tablet::PacketPositionInvalid;
		}
		reportData.reportId = data[0];
		reportData.buttons = data[1] & ~0xA1;
		reportData.x = data[2] | (data[3] << 8) | (data[10] << 16);
		reportData.y = data[4] | (data[5] << 8) | (data[11] << 16);
		reportData.pressure = ((data[6] | (data[7] << 8)) & 0xBFFF) | ((data[13] & 0x01) << 13);
		reportData.z = 0;
	}
	else if (settings.type == TabletSettings::TypeXPPenOffsetAux) {
		if (data[1] == 0xC0 || (data[1] & 0x20)) {
			return Tablet::PacketPositionInvalid;
		}
		reportData.reportId = data[0];
		reportData.buttons = data[1] & ~0x01;
		reportData.x = data[2] | (data[3] << 8);
		reportData.y = data[4] | (data[5] << 8);
		reportData.pressure = data[6] | (data[7] << 8);
		reportData.z = 0;
	}
	else if (settings.type == TabletSettings::TypeGiano) {
		if (data[1] == 0xF1 || ((data[1] & 0x20) && (data[1] & 0x40))) {
			return Tablet::PacketPositionInvalid;
		}
		reportData.reportId = data[0];
		reportData.buttons = data[1] & ~0x01;
		reportData.x = data[2] | (data[3] << 8) | ((data[8] & 0x01) << 16);
		reportData.y = data[4] | (data[5] << 8) | ((data[9] & 0x01) << 16);
		reportData.pressure = data[6] | (data[7] << 8);
		reportData.z = 0;
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
		if (settings.reportLength == 11 && settings.reportOffset == 0) {
			data = buffer + 1;
		}

		reportData.reportId = data[0];
		reportData.buttons = data[1];
		reportData.x = ((data[3] | (data[2] << 8)) << 1) | ((data[9] >> 1) & 1);
		reportData.y = ((data[5] | (data[4] << 8)) << 1) | (data[9] & 1);
		reportData.pressure = (data[6] << 3) | ((data[7] & 0xC0) >> 5) | (data[1] & 1);
		reportData.z = data[9] >> 2;
	}
	else if (settings.type == TabletSettings::TypeAcepen) {
		if (data[1] != 0x41 || (data[2] & 0xF0) != 0xA0) {
			return Tablet::PacketPositionInvalid;
		}
		reportData.reportId = data[1];
		reportData.buttons = data[2] & 0x06;
		reportData.x = ReadLe16(data, 3);
		reportData.y = ReadLe16(data, 5);
		reportData.pressure = ReadLe16(data, 7);
		reportData.z = 0;
	}
	else if (settings.type == TabletSettings::TypeBosto) {
		if (data[1] == 0x00) {
			return Tablet::PacketPositionInvalid;
		}
		reportData.reportId = data[0];
		reportData.buttons = ((data[1] & 0x20) ? 0x02 : 0x00) | ((data[1] & 0x02) ? 0x04 : 0x00);
		reportData.x = ReadLe16(data, 2);
		reportData.y = ReadLe16(data, 4);
		reportData.pressure = ReadLe16(data, 6);
		reportData.z = 0;
	}
	else if (settings.type == TabletSettings::TypeFlooGoo) {
		if (data[0] != 0x01 || (data[1] & 0x20) == 0) {
			return Tablet::PacketPositionInvalid;
		}
		reportData.reportId = data[0];
		reportData.buttons = data[1] & 0x0E;
		reportData.x = ReadLe16(data, 2);
		reportData.y = ReadLe16(data, 4);
		reportData.pressure = ReadLe16(data, 6);
		reportData.z = 0;
	}
	else if (settings.type == TabletSettings::TypeGenius) {
		if (data[0] != 0x10) {
			return Tablet::PacketPositionInvalid;
		}
		reportData.reportId = data[0];
		reportData.buttons = data[1] & 0x0F;
		reportData.x = ReadLe16(data, 2);
		reportData.y = ReadLe16(data, 4);
		reportData.pressure = ReadLe16(data, 6);
		reportData.z = 0;
	}
	else if (settings.type == TabletSettings::TypeGeniusV2) {
		if (data[0] != 0x02) {
			return Tablet::PacketPositionInvalid;
		}
		reportData.reportId = data[0];
		reportData.buttons = ((data[5] & 0x08) ? 0x02 : 0x00) | ((data[5] & 0x10) ? 0x04 : 0x00);
		reportData.x = ReadLe16(data, 1);
		reportData.y = ReadLe16(data, 3);
		reportData.pressure = (data[5] & 0x04) ? ReadLe16(data, 6) : 0;
		reportData.z = 0;
	}
	else if (settings.type == TabletSettings::TypeLifetec) {
		if (data[0] != 0x02) {
			return Tablet::PacketPositionInvalid;
		}
		reportData.reportId = data[0];
		reportData.buttons = ((data[5] & 0x08) ? 0x02 : 0x00) | ((data[5] & 0x10) ? 0x04 : 0x00);
		reportData.x = ReadLe16(data, 1);
		reportData.y = ReadLe16(data, 3);
		reportData.pressure = ReadLe16(data, 6);
		reportData.z = 0;
	}
	else if (settings.type == TabletSettings::TypeRobotPen) {
		if (data[1] != 0x42) {
			return Tablet::PacketPositionInvalid;
		}
		reportData.reportId = data[1];
		reportData.buttons = (data[11] & 0x02) ? 0x02 : 0x00;
		reportData.x = ReadLe16(data, 6);
		reportData.y = ReadLe16(data, 8);
		reportData.pressure = ReadLe16(data, 10);
		reportData.z = 0;
	}
	else if (settings.type == TabletSettings::TypeVeikk) {
		if (data[1] == 0x43 || (data[2] & 0x20) == 0) {
			return Tablet::PacketPositionInvalid;
		}
		reportData.reportId = data[1];
		reportData.buttons = data[2] & 0x06;
		reportData.x = ReadLe16(data, 3) | (data[5] << 16);
		reportData.y = ReadLe16(data, 6) | (data[8] << 16);
		reportData.pressure = ReadLe16(data, 9);
		reportData.z = 0;
	}
	else if (settings.type == TabletSettings::TypeVeikkA15) {
		if (data[1] == 0x43 || (data[2] & 0x20) == 0) {
			return Tablet::PacketPositionInvalid;
		}
		reportData.reportId = data[1];
		reportData.buttons = data[2] & 0x06;
		reportData.x = ReadLe16(data, 3);
		reportData.y = ReadLe16(data, 5);
		reportData.pressure = ReadLe16(data, 7);
		reportData.z = 0;
	}
	else if (settings.type == TabletSettings::TypeVeikkV1) {
		if (data[0] == 0x03 || data[1] != 0x41 || (data[2] & 0xF0) != 0xA0) {
			return Tablet::PacketPositionInvalid;
		}
		reportData.reportId = data[1];
		reportData.buttons = data[2] & 0x06;
		reportData.x = ReadLe16(data, 3);
		reportData.y = ReadLe16(data, 5);
		reportData.pressure = ReadLe16(data, 7);
		reportData.z = 0;
	}
	else if (settings.type == TabletSettings::TypeVeikkTilt) {
		if (data[1] != 0x41 || data[2] == 0xC0) {
			return Tablet::PacketPositionInvalid;
		}
		reportData.reportId = data[1];
		reportData.buttons = data[2] & 0x06;
		reportData.x = ReadLe16(data, 3) | (data[5] << 16);
		reportData.y = ReadLe16(data, 6) | (data[8] << 16);
		reportData.pressure = ReadLe16(data, 9);
		reportData.z = 0;
	}
	else if (settings.type == TabletSettings::TypeWoodPad) {
		if ((data[9] & 0x03) != 0x03) {
			return Tablet::PacketPositionInvalid;
		}
		reportData.reportId = data[0];
		reportData.buttons = ((data[9] & 0x08) ? 0x02 : 0x00) | ((data[9] & 0x10) ? 0x04 : 0x00);
		reportData.x = ReadLe16(data, 1);
		reportData.y = ReadLe16(data, 5);
		reportData.pressure = (data[9] & 0x04) ? ReadLe16(data, 10) : 0;
		reportData.z = 0;
	}
	else if (settings.type == TabletSettings::TypeXenceLabs) {
		if ((data[1] & 0xF0) == 0xF0 || (data[1] & 0x20) == 0) {
			return Tablet::PacketPositionInvalid;
		}
		reportData.reportId = data[0];
		reportData.buttons = data[1] & 0x0E;
		reportData.x = ReadLe16(data, 2);
		reportData.y = ReadLe16(data, 4);
		reportData.pressure = ReadLe16(data, 6);
		reportData.z = 0;
	}
	else if (settings.type == TabletSettings::TypeXENX) {
		if (data[0] != 0x01 || data[1] == 0) {
			return Tablet::PacketPositionInvalid;
		}
		reportData.reportId = data[0];
		reportData.buttons = data[1] & 0x46;
		reportData.x = ReadLe16(data, 2);
		reportData.y = ReadLe16(data, 4);
		reportData.pressure = ReadLe16(data, 6);
		reportData.z = 0;
	}
	else if (settings.type == TabletSettings::TypeWacomGraphire) {
		if (data[0] != 0x02) {
			return Tablet::PacketPositionInvalid;
		}
		reportData.reportId = data[0];
		reportData.buttons = data[1] & 0x07;
		reportData.x = ReadLe16(data, 2);
		reportData.y = ReadLe16(data, 4);
		reportData.pressure = (data[1] & 0x01) ? (data[6] | ((data[7] & 0x03) << 8)) : 0;
		reportData.z = 0;
		if ((data[1] & 0x80) == 0 && reportData.x == 0 && reportData.y == 0 && reportData.pressure == 0) {
			return Tablet::PacketPositionInvalid;
		}
	}
	else if (settings.type == TabletSettings::TypeWacomBambooPad) {
		if (data[0] != 0x10 || data[1] != 0x01) {
			return Tablet::PacketPositionInvalid;
		}
		reportData.reportId = data[0];
		reportData.buttons = (data[2] & 0x02) ? 0x02 : 0x00;
		reportData.x = ReadLe16(data, 3);
		reportData.y = ReadLe16(data, 5);
		reportData.pressure = ReadLe16(data, 7);
		reportData.z = 0;
	}
	else if (settings.type == TabletSettings::TypeWacomCintiqV1) {
		if ((data[0] != 0x02 && data[0] != 0x10) || (data[0] == 0x10 && data[1] == 0x20) || data[1] == 0x80) {
			return Tablet::PacketPositionInvalid;
		}
		if ((data[1] & 0x20) == 0 && (data[1] & 0x0A) != 0x0A) {
			return Tablet::PacketPositionInvalid;
		}
		reportData.reportId = data[0];
		reportData.buttons = data[1] & ~0x01;
		reportData.x = (ReadBe16(data, 2) << 1) | ((data[9] >> 1) & 1);
		reportData.y = (ReadBe16(data, 4) << 1) | (data[9] & 1);
		reportData.pressure = (data[6] << 3) | ((data[7] & 0xC0) >> 5) | (data[1] & 1);
		reportData.z = data[9];
	}
	else if (settings.type == TabletSettings::TypeWacomPL) {
		if ((data[1] & 0x40) == 0) {
			return Tablet::PacketPositionInvalid;
		}
		reportData.reportId = data[0];
		reportData.buttons = ((data[4] & 0x10) ? 0x02 : 0x00) | ((data[4] & 0x20) ? 0x04 : 0x00);
		reportData.x = ((data[1] & 0x03) << 14) | (data[2] << 7) | data[3];
		reportData.y = ((data[4] & 0x03) << 14) | (data[5] << 7) | data[6];
		reportData.pressure = ((data[7] ^ 0x40) << 2) | ((data[4] & 0x40) >> 5) | ((data[4] & 0x04) >> 2);
		reportData.z = 0;
	}
	else if (settings.type == TabletSettings::TypeWacomPTU) {
		reportData.reportId = data[0];
		reportData.buttons = ((data[1] & 0x02) ? 0x02 : 0x00) | ((data[1] & 0x10) ? 0x04 : 0x00) | ((data[1] & 0x04) ? 0x08 : 0x00);
		reportData.x = ReadLe16(data, 2);
		reportData.y = ReadLe16(data, 4);
		reportData.pressure = ReadLe16(data, 6);
		reportData.z = 0;
		if ((data[1] & 0x20) == 0 && reportData.x == 0 && reportData.y == 0 && reportData.pressure == 0) {
			return Tablet::PacketPositionInvalid;
		}
	}
	else {
		reportData.reportId = data[0];
		reportData.buttons = data[1];
		reportData.x = data[2] | (data[3] << 8);
		reportData.y = data[4] | (data[5] << 8);
		reportData.pressure = data[6] | (data[7] << 8);
		reportData.z = data[8] | (data[9] << 8);
	}


	
	if (settings.reportId > 0 && reportData.reportId != settings.reportId) {
		return Tablet::PacketInvalid;
	}



	
	if (settings.detectMask > 0 && (reportData.buttons & settings.detectMask) != settings.detectMask) {
		return Tablet::PacketPositionInvalid;
	}

	
	if (settings.ignoreMask > 0 && (reportData.buttons & settings.ignoreMask) == settings.ignoreMask) {
		return Tablet::PacketPositionInvalid;
	}

	
	
	
	if (settings.clickPressure > 0) {
		reportData.buttons &= ~1;
		if ((int)reportData.pressure > settings.clickPressure) {
			reportData.buttons |= 1;
		}

		
	}
	else if (reportData.pressure > 1) {
		
		reportData.buttons |= 1;
	}

	
	if (settings.keepTipDown > 0) {
		if (reportData.buttons & 0x01) {
			tipDownCounter = settings.keepTipDown;
		}
		if (tipDownCounter-- >= 0) {
			reportData.buttons |= 1;
		}
	}


	
	state.isValid = true;

	
	reportData.buttons = reportData.buttons & 0x0F;
	state.buttons = 0;
	for (buttonIndex = 0; buttonIndex < sizeof(buttonMap); buttonIndex++) {

		
		if (buttonMap[buttonIndex] > 0) {

			
			if ((reportData.buttons & (1 << buttonIndex)) > 0) {
				state.buttons |= (1 << (buttonMap[buttonIndex] - 1));
			}
		}
	}

	
	state.position.x = (double)reportData.x * settings.invMaxX * settings.width;
	state.position.y = (double)reportData.y * settings.invMaxY * settings.height;
	state.z = (double)reportData.z;
	if (settings.skew != 0)
		state.position.x += state.position.y * settings.skew;
	state.pressure = ((double)reportData.pressure / (double)settings.maxPressure);

	
	benchmark.Update(state.position);

	
	return Tablet::PacketValid;
}





bool Tablet::Read(void *buffer, int length) {
	if (!isOpen) return false;
	bool status = false;
	if (usbDevice != NULL) {
		status = usbDevice->Read(usbPipeId, buffer, length) > 0;
	}
	else if (hidDevice != NULL) {
		int readLength = length;
		if (hidDevice->inputReportLength > 0) {
			readLength = hidDevice->inputReportLength;
		}
		status = hidDevice->Read(buffer, readLength);
	}
	if (debugEnabled && status) {
		LOG_DEBUGBUFFER(buffer, length, "Read: ");
	}
	return status;
}




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
