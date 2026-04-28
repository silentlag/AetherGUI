#include "stdafx.h"
#include "HIDDevice.h"
#include <regex>

#define LOG_MODULE "HIDDevice"
#include "Logger.h"

static string WideStringToUtf8(const WCHAR *value) {
	if (value == NULL || value[0] == 0) {
		return "";
	}
	int length = WideCharToMultiByte(CP_UTF8, 0, value, -1, NULL, 0, NULL, NULL);
	if (length <= 1) {
		return "";
	}
	string result(length - 1, '\0');
	WideCharToMultiByte(CP_UTF8, 0, value, -1, &result[0], length, NULL, NULL);
	return result;
}

static bool ReadHidIndexedString(HANDLE handle, int stringId, string *result) {
	if (stringId <= 0 || handle == NULL || handle == INVALID_HANDLE_VALUE) {
		return false;
	}
	WCHAR stringBuffer[512];
	memset(stringBuffer, 0, sizeof(stringBuffer));
	if (!HidD_GetIndexedString(handle, stringId, stringBuffer, sizeof(stringBuffer))) {
		return false;
	}
	if (result != NULL) {
		*result = WideStringToUtf8(stringBuffer);
	}
	return true;
}

static bool DeviceStringMatches(const string &value, const string &pattern) {
	if (pattern.empty()) {
		return true;
	}
	try {
		return regex_search(value, regex(pattern, regex_constants::icase));
	}
	catch (const regex_error &) {
		return value.find(pattern) != string::npos;
	}
}

HIDDevice::HIDDevice(USHORT VendorId, USHORT ProductId, USHORT UsagePage, USHORT Usage, int InputReportLength, int StringId, string StringMatch, int StringId2, string StringMatch2) : HIDDevice() {
	this->vendorId = VendorId;
	this->productId = ProductId;
	this->usagePage = UsagePage;
	this->usage = Usage;
	this->inputReportLength = InputReportLength;
	this->stringId = StringId;
	this->stringMatch = StringMatch;
	this->stringId2 = StringId2;
	this->stringMatch2 = StringMatch2;
	if (this->OpenDevice(&this->_deviceHandle, this->vendorId, this->productId, this->usagePage, this->usage, this->inputReportLength, this->stringId, this->stringMatch, this->stringId2, this->stringMatch2)) {
		isOpen = true;
	}
}

HIDDevice::HIDDevice() {
	isOpen = false;
	_deviceHandle = NULL;
	inputReportLength = 0;
	stringId = 0;
	stringMatch = "";
	stringId2 = 0;
	stringMatch2 = "";
}

HIDDevice::~HIDDevice() {
	CloseDevice();
}

bool HIDDevice::OpenDevice(HANDLE *handle, USHORT vendorId, USHORT productId, USHORT usagePage, USHORT usage, int inputReportLength, int stringId, string stringMatch, int stringId2, string stringMatch2) {
	HDEVINFO                         deviceInfo;
	SP_DEVICE_INTERFACE_DATA         deviceInterfaceData;
	PSP_DEVICE_INTERFACE_DETAIL_DATA deviceInterfaceDetailData;
	SP_DEVINFO_DATA                  deviceInfoData;
	DWORD dwSize, dwMemberIdx;
	GUID hidGuid;
	BYTE stringBytes[1024];

	PHIDP_PREPARSED_DATA hidPreparsedData;
	HIDD_ATTRIBUTES hidAttributes;
	HIDP_CAPS hidCapabilities;

	HANDLE deviceHandle;

	HANDLE resultHandle = 0;
	int resultScore = -1;
	USHORT resultUsagePage = usagePage;
	USHORT resultUsage = usage;
	int resultInputReportLength = inputReportLength;
	int targetPathCount = 0;
	int targetMetadataOpenFailures = 0;
	int targetMatchedCandidates = 0;
	int targetReadOpenFailures = 0;
	int vendorPathCount = 0;
	int vendorMetadataCount = 0;
	bool verboseTarget = (vendorId == 0x056A && productId == 0x0018);

	HidD_GetHidGuid(&hidGuid);

	
	deviceInfo = SetupDiGetClassDevs(&hidGuid, NULL, 0, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
	if (deviceInfo == INVALID_HANDLE_VALUE) {
		LOG_ERROR("Invalid device info!\n");
		return false;
	}

	
	dwMemberIdx = 0;
	while (true) {
		deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
		if (!SetupDiEnumDeviceInterfaces(deviceInfo, NULL, &hidGuid, dwMemberIdx, &deviceInterfaceData)) {
			if (GetLastError() == ERROR_NO_MORE_ITEMS) {
				break;
			}
			dwMemberIdx++;
			continue;
		}

		deviceInfoData.cbSize = sizeof(deviceInfoData);

		
		SetupDiGetDeviceInterfaceDetail(deviceInfo, &deviceInterfaceData, NULL, 0, &dwSize, NULL);

		
		deviceInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(dwSize);
		if (deviceInterfaceDetailData == NULL) {
			dwMemberIdx++;
			continue;
		}
		deviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		
		if (SetupDiGetDeviceInterfaceDetail(deviceInfo, &deviceInterfaceData, deviceInterfaceDetailData, dwSize, &dwSize, &deviceInfoData)) {

			TCHAR targetVidPidLower[64];
			TCHAR targetVidPidUpper[64];
			_stprintf_s(targetVidPidLower, _T("vid_%04x&pid_%04x"), vendorId, productId);
			_stprintf_s(targetVidPidUpper, _T("VID_%04X&PID_%04X"), vendorId, productId);
			bool pathLooksTarget =
			_tcsstr(deviceInterfaceDetailData->DevicePath, targetVidPidLower) != NULL ||
			_tcsstr(deviceInterfaceDetailData->DevicePath, targetVidPidUpper) != NULL;
			if (pathLooksTarget) {
				targetPathCount++;
			}
			TCHAR vendorLower[32];
			TCHAR vendorUpper[32];
			_stprintf_s(vendorLower, _T("vid_%04x"), vendorId);
			_stprintf_s(vendorUpper, _T("VID_%04X"), vendorId);
			bool pathLooksVendor =
				_tcsstr(deviceInterfaceDetailData->DevicePath, vendorLower) != NULL ||
				_tcsstr(deviceInterfaceDetailData->DevicePath, vendorUpper) != NULL;
			if (pathLooksVendor) {
				vendorPathCount++;
			}

			
			
			HANDLE inspectHandle = CreateFile(
				deviceInterfaceDetailData->DevicePath,
				0,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				NULL,
				OPEN_EXISTING,
				0,
				NULL);

			
			if (inspectHandle != INVALID_HANDLE_VALUE) {

				
				memset(&hidAttributes, 0, sizeof(hidAttributes));
				hidAttributes.Size = sizeof(HIDD_ATTRIBUTES);
				bool hasAttributes = HidD_GetAttributes(inspectHandle, &hidAttributes) != FALSE;

				
				hidPreparsedData = NULL;
				bool hasPreparsedData = HidD_GetPreparsedData(inspectHandle, &hidPreparsedData) != FALSE;

				
				memset(&hidCapabilities, 0, sizeof(hidCapabilities));
				bool hasCapabilities = hasPreparsedData &&
					HidP_GetCaps(hidPreparsedData, &hidCapabilities) == HIDP_STATUS_SUCCESS;

				
				if (this->debugEnabled && hasAttributes && hasCapabilities) {

					string manufacturerName = "";
					string productName = "";

					
					if (HidD_GetManufacturerString(inspectHandle, &stringBytes, sizeof(stringBytes))) {
						for (int i = 0; i < (int)sizeof(stringBytes); i += 2) {
							if (stringBytes[i]) {
								manufacturerName.push_back(stringBytes[i]);
							}
							else {
								break;
							}
						}
					}

					
					if (HidD_GetProductString(inspectHandle, &stringBytes, sizeof(stringBytes))) {
						for (int i = 0; i < (int)sizeof(stringBytes); i += 2) {
							if (stringBytes[i]) {
								productName.push_back(stringBytes[i]);
							}
							else {
								break;
							}
						}
					}

					LOG_DEBUG("HID Device: Vendor: '%s' Product: '%s'\n", manufacturerName.c_str(), productName.c_str());
					LOG_DEBUG("  Vendor Id: 0x%04X, Product Id: 0x%04X\n",
						hidAttributes.VendorID,
						hidAttributes.ProductID
					);
					LOG_DEBUG("  Usage Page: 0x%04X, Usage: 0x%04X\n",
						hidCapabilities.UsagePage,
						hidCapabilities.Usage
					);
					LOG_DEBUG("  FeatureLen: %d, InputLen: %d, OutputLen: %d\n",
						hidCapabilities.FeatureReportByteLength,
						hidCapabilities.InputReportByteLength,
						hidCapabilities.OutputReportByteLength
					);
					LOG_DEBUG("\n");
				}

				bool usageWildcard = (usagePage == 0 && usage == 0);
				bool usageMatches = usageWildcard ||
					(hidCapabilities.UsagePage == usagePage && hidCapabilities.Usage == usage);
				int inputLengthDelta = inputReportLength > 0
					? abs((int)hidCapabilities.InputReportByteLength - inputReportLength)
					: 0;
				bool inputLengthMatches = (inputReportLength <= 0) ||
					((int)hidCapabilities.InputReportByteLength == inputReportLength);

				int score = -1;
				if (hasAttributes && hasCapabilities &&
					hidAttributes.VendorID == vendorId) {

					vendorMetadataCount++;
					if (verboseTarget && hidAttributes.ProductID != productId) {
						LOG_WARNING("Wacom HID present: 0x%04X 0x%04X usage 0x%04X 0x%04X inputLen %d\n",
							hidAttributes.VendorID,
							hidAttributes.ProductID,
							hidCapabilities.UsagePage,
							hidCapabilities.Usage,
							hidCapabilities.InputReportByteLength);
					}
				}

				if (hasAttributes && hasCapabilities &&
					hidAttributes.VendorID == vendorId &&
					hidAttributes.ProductID == productId) {

					bool stringMatches = true;
					if (stringId > 0 && !stringMatch.empty()) {
						string indexedString = "";
						stringMatches =
							ReadHidIndexedString(inspectHandle, stringId, &indexedString) &&
							DeviceStringMatches(indexedString, stringMatch);
					}
					if (stringMatches && stringId2 > 0 && !stringMatch2.empty()) {
						string indexedString = "";
						stringMatches =
							ReadHidIndexedString(inspectHandle, stringId2, &indexedString) &&
							DeviceStringMatches(indexedString, stringMatch2);
					}

					if (usageMatches && inputLengthMatches && stringMatches) {
						score = usageWildcard ? 50 : 100;
						if (inputReportLength > 0) {
							score += 20 - inputLengthDelta;
						}
						if (stringId > 0 && !stringMatch.empty()) {
							score += 30;
						}
						if (stringId2 > 0 && !stringMatch2.empty()) {
							score += 30;
						}
					}
					else if (usageWildcard && inputReportLength <= 0 && hidCapabilities.InputReportByteLength > 0) {
						
						
						score = 10;
					}
				}

				
				if (score >= 0) {
					targetMatchedCandidates++;
					DWORD readWriteError = ERROR_SUCCESS;
					DWORD readOnlyError = ERROR_SUCCESS;

					
					deviceHandle = CreateFile(
						deviceInterfaceDetailData->DevicePath,
						GENERIC_READ | GENERIC_WRITE,
						FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
						NULL,
						OPEN_EXISTING,
						0,
						NULL);

					if (deviceHandle == INVALID_HANDLE_VALUE) {
						readWriteError = GetLastError();
						deviceHandle = CreateFile(
							deviceInterfaceDetailData->DevicePath,
							GENERIC_READ,
							FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
							NULL,
							OPEN_EXISTING,
							0,
							NULL);
					}

					if (deviceHandle == INVALID_HANDLE_VALUE) {
						readOnlyError = GetLastError();
						targetReadOpenFailures++;
						LOG_WARNING("HID candidate matched 0x%04X 0x%04X usage 0x%04X 0x%04X inputLen %d, but read open failed (rw=0x%08X read=0x%08X).\n",
							hidAttributes.VendorID,
							hidAttributes.ProductID,
							hidCapabilities.UsagePage,
							hidCapabilities.Usage,
							hidCapabilities.InputReportByteLength,
							readWriteError,
							readOnlyError);
					}
					else if (score > resultScore) {
						if (resultHandle && resultHandle != INVALID_HANDLE_VALUE) {
							CloseHandle(resultHandle);
						}
						resultHandle = deviceHandle;
						resultScore = score;
						resultUsagePage = hidCapabilities.UsagePage;
						resultUsage = hidCapabilities.Usage;
						resultInputReportLength = hidCapabilities.InputReportByteLength;
					}
					else {
						CloseHandle(deviceHandle);
					}
				}

				
				if (hidPreparsedData != NULL) {
					HidD_FreePreparsedData(hidPreparsedData);
				}
				CloseHandle(inspectHandle);
			}
			else if (pathLooksTarget) {
				targetMetadataOpenFailures++;
				LOG_WARNING("HID path exists for 0x%04X 0x%04X, but metadata open failed (error 0x%08X).\n",
					vendorId,
					productId,
					GetLastError());
			}
		}

		
		free(deviceInterfaceDetailData);

		
		dwMemberIdx++;
	}

	
	SetupDiDestroyDeviceInfoList(deviceInfo);

	if ((!resultHandle || resultHandle == INVALID_HANDLE_VALUE) && (verboseTarget || targetPathCount > 0)) {
		LOG_WARNING("HID diagnostics 0x%04X 0x%04X request usage 0x%04X 0x%04X inputLen %d: paths=%d metadataFail=%d matched=%d readFail=%d vendorPaths=%d vendorOpen=%d\n",
			vendorId,
			productId,
			usagePage,
			usage,
			inputReportLength,
			targetPathCount,
			targetMetadataOpenFailures,
			targetMatchedCandidates,
			targetReadOpenFailures,
			vendorPathCount,
			vendorMetadataCount);
	}

	
	if (resultHandle && resultHandle != INVALID_HANDLE_VALUE) {
		this->usagePage = resultUsagePage;
		this->usage = resultUsage;
		this->inputReportLength = resultInputReportLength;
		memcpy(handle, &resultHandle, sizeof(HANDLE));
		return true;
	}

	return false;
}


int HIDDevice::Read(void *buffer, int length) {
	
	DWORD bytesRead;
	if (ReadFile(_deviceHandle, buffer, length, &bytesRead, 0)) {
		return bytesRead;
	}
	return 0;
}


int HIDDevice::Write(void *buffer, int length) {
	DWORD bytesWritten;
	if (WriteFile(_deviceHandle, buffer, length, &bytesWritten, 0)) {
		return bytesWritten;
	}
	return 0;
}


bool HIDDevice::SetFeature(void *buffer, int length) {
	return HidD_SetFeature(_deviceHandle, buffer, length);
}


bool HIDDevice::GetFeature(void *buffer, int length) {
	return HidD_GetFeature(_deviceHandle, buffer, length);
}

bool HIDDevice::GetIndexedString(int stringId, string *result) {
	return ReadHidIndexedString(_deviceHandle, stringId, result);
}


void HIDDevice::CloseDevice() {
	if (isOpen && _deviceHandle != NULL && _deviceHandle != INVALID_HANDLE_VALUE) {
		try {
			CloseHandle(_deviceHandle);
		}
		catch (exception) {}
	}
	isOpen = false;
}
