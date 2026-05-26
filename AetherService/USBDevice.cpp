#include "stdafx.h"
#if defined(_WIN32)

#include "USBDevice.h"

#define LOG_MODULE "USBDevice"
#include "Logger.h"

USBDevice::USBDevice(string Guid, int StringId, string StringMatch) {
	this->guid = Guid;
	this->stringId = StringId;
	this->stringMatch = StringMatch;
	isOpen = false;
	if (this->OpenDevice(guid, stringId, stringMatch)) {
		isOpen = true;
	}
}
USBDevice::~USBDevice() {
	this->CloseDevice();
}

bool USBDevice::OpenDevice(string usbDeviceGUIDString, int stringId, string stringSearch) {
	GUID usbDeviceGUID;

	HDEVINFO                         deviceInfo;
	SP_DEVICE_INTERFACE_DATA         deviceInterfaceData;
	PSP_DEVICE_INTERFACE_DETAIL_DATA deviceInterfaceDetailData;
	SP_DEVINFO_DATA                  deviceInfoData;
	DWORD dwSize;
	DWORD dwMemberIdx;

	HANDLE deviceHandle = 0;
	WINUSB_INTERFACE_HANDLE usbHandle = 0;

	USB_INTERFACE_DESCRIPTOR usbInterfaceDescriptor;

	ULONG readBytes = 0;

	std::wstring stemp = std::wstring(usbDeviceGUIDString.begin(), usbDeviceGUIDString.end());
	LPCWSTR wstringGUID = stemp.c_str();

	_deviceHandle = NULL;
	_usbHandle = NULL;

	HRESULT hr = CLSIDFromString(wstringGUID, (LPCLSID)&usbDeviceGUID);
	if (hr != S_OK) {
		LOG_ERROR("Can't create the USB Device GUID!\n");
		return false;
	}

	deviceInfo = SetupDiGetClassDevs(&usbDeviceGUID, NULL, 0, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
	if (deviceInfo == INVALID_HANDLE_VALUE) {
		LOG_ERROR("Device info invalid!\n");
		SetupDiDestroyDeviceInfoList(deviceInfo);
		return false;
	}

	deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	dwMemberIdx = 0;
	SetupDiEnumDeviceInterfaces(deviceInfo, NULL, &usbDeviceGUID, dwMemberIdx, &deviceInterfaceData);
	while (GetLastError() != ERROR_NO_MORE_ITEMS) {

		deviceInfoData.cbSize = sizeof(deviceInfoData);
		SetupDiGetDeviceInterfaceDetail(deviceInfo, &deviceInterfaceData, NULL, 0, &dwSize, NULL);

		deviceInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(dwSize);
		deviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

		if (SetupDiGetDeviceInterfaceDetail(deviceInfo, &deviceInterfaceData, deviceInterfaceDetailData, dwSize, &dwSize, &deviceInfoData)) {

			deviceHandle = CreateFile(deviceInterfaceDetailData->DevicePath,
				GENERIC_READ | GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				NULL,
				OPEN_EXISTING,
				FILE_FLAG_OVERLAPPED,
				NULL);

			if (deviceHandle != INVALID_HANDLE_VALUE) {

				WinUsb_Initialize(deviceHandle, &usbHandle);
				if (!usbHandle) {
					LOG_ERROR("ERROR! Unable to start WinUSB for the device!\n");
					if (deviceHandle != INVALID_HANDLE_VALUE)
						CloseHandle(deviceHandle);
					return false;
				}

				ZeroMemory(&usbInterfaceDescriptor, sizeof(USB_INTERFACE_DESCRIPTOR));
				if (WinUsb_QueryInterfaceSettings(usbHandle, 0, &usbInterfaceDescriptor)) {

					WINUSB_SETUP_PACKET setupPacket;
					ULONG bytesRead;
					BYTE buffer[64];
					string str = "";

					setupPacket.RequestType = 0x80;
					setupPacket.Request = 0x06;
					setupPacket.Value = (0x03 << 8) | stringId;
					setupPacket.Index = 0x0409;
					setupPacket.Length = 64;

					if (WinUsb_ControlTransfer(usbHandle, setupPacket, buffer, 64, &bytesRead, NULL)) {

						for (int i = 2; i < (int)bytesRead; i += 2) {
							str.push_back(buffer[i]);
						}
						LOG_DEBUG("USB String (%d): %s\n", stringId, str.c_str());

						if (bytesRead >= stringSearch.length() * 2) {

							if (str.compare(0, stringSearch.size(), stringSearch) == 0) {
								_deviceHandle = deviceHandle;
								_usbHandle = usbHandle;
							}
						}
					}
				}
				else {
					LOG_ERROR("ERROR! Can't query interface settings!\n");
				}

				if (_usbHandle == NULL && usbHandle && usbHandle != INVALID_HANDLE_VALUE)
					WinUsb_Free(usbHandle);

				if (_deviceHandle == NULL && deviceHandle && deviceHandle != INVALID_HANDLE_VALUE)
					CloseHandle(deviceHandle);
			}
		}

		std::free(deviceInterfaceDetailData);

		SetupDiEnumDeviceInterfaces(deviceInfo, NULL, &usbDeviceGUID, ++dwMemberIdx, &deviceInterfaceData);
	}

	SetupDiDestroyDeviceInfoList(deviceInfo);

	if (_deviceHandle && _deviceHandle != INVALID_HANDLE_VALUE
		&&
		_usbHandle && _usbHandle != INVALID_HANDLE_VALUE
		)
		return true;

	return false;
}

int USBDevice::Read(UCHAR pipeId, void *buffer, int length) {
	ULONG bytesRead;
	try {
		if (WinUsb_ReadPipe(_usbHandle, pipeId, (UCHAR *)buffer, length, &bytesRead, 0)) {
			return (int)bytesRead;
		}
	}
	catch (exception &e) {
		LOG_ERROR("Exception USB: %s\n", e.what());
	}
	return 0;
}

int USBDevice::Write(UCHAR pipeId, void *buffer, int length) {
	ULONG bytesWritten;
	if (WinUsb_WritePipe(_usbHandle, pipeId, (UCHAR *)buffer, length, &bytesWritten, 0)) {
		return (int)bytesWritten;
	}
	return 0;
}

int USBDevice::ControlTransfer(UCHAR requestType, UCHAR request, USHORT value, USHORT index, void *buffer, USHORT length) {
	WINUSB_SETUP_PACKET usbSetupPacket;
	ULONG bytesRead;
	usbSetupPacket.RequestType = requestType;
	usbSetupPacket.Request = request;
	usbSetupPacket.Length = length;
	usbSetupPacket.Value = value;
	usbSetupPacket.Index = index;
	if (WinUsb_ControlTransfer(_usbHandle, usbSetupPacket, (UCHAR*)buffer, length, &bytesRead, NULL)) {
		return bytesRead;
	}
	return 0;
}

void USBDevice::CloseDevice() {
	if (_usbHandle != NULL && _usbHandle != INVALID_HANDLE_VALUE) {
		try {
			printf("ASD\n");
			WinUsb_Free(_usbHandle);
		}
		catch (exception &e) {
			printf("WinUsb ERROR! %s\n", e.what());
		}
	}
	if (_deviceHandle != NULL && _deviceHandle != INVALID_HANDLE_VALUE) {
		try {
			printf("DAS\n");
			CloseHandle(_deviceHandle);
		}
		catch (exception &e) {
			printf("HID ERROR! %s\n", e.what());
		}
	}
	isOpen = false;
}

#else

#include "USBDevice.h"

#define LOG_MODULE "USBDevice"
#include "Logger.h"

#if defined(AETHER_HAVE_LIBUSB)
  #include <libusb-1.0/libusb.h>
  #include <cstring>
  #include <algorithm>
#endif

#if defined(AETHER_HAVE_LIBUSB)

namespace {

libusb_context* g_ctx = nullptr;
int             g_ctxRefcount = 0;

void AcquireCtx() {
	if (g_ctxRefcount++ == 0) {
		if (libusb_init(&g_ctx) != 0) {
			g_ctx = nullptr;
			LOG_ERROR("libusb_init failed; USB-only tablets won't be detected.\n");
		}
	}
}

void ReleaseCtx() {
	if (--g_ctxRefcount <= 0 && g_ctx != nullptr) {
		libusb_exit(g_ctx);
		g_ctx = nullptr;
		g_ctxRefcount = 0;
	}
}

bool StringMatches(libusb_device_handle* h, int stringId, const std::string& wanted) {
	if (stringId <= 0) return wanted.empty();
	unsigned char buf[256] = {0};
	int n = libusb_get_string_descriptor_ascii(h, (uint8_t)stringId, buf, sizeof(buf) - 1);
	if (n <= 0) return false;
	std::string s((char*)buf, (size_t)n);
	if (wanted.empty()) return true;
	return s.find(wanted) != std::string::npos;
}

}

struct UsbBackingHandle {
	libusb_device_handle* handle;
	int                   interfaceNumber;
	bool                  kernelDriverDetached;
};

USBDevice::USBDevice(string Guid, int StringId, string StringMatch)
	: _deviceHandle(nullptr), _usbHandle(nullptr),
	  guid(Guid), stringId(StringId), stringMatch(StringMatch), isOpen(false) {
	if (OpenDevice(guid, stringId, stringMatch)) {
		isOpen = true;
	}
}

USBDevice::~USBDevice() { CloseDevice(); }

bool USBDevice::OpenDevice(string , int sid, string smatch) {
	AcquireCtx();
	if (g_ctx == nullptr) return false;

	libusb_device** list = nullptr;
	ssize_t count = libusb_get_device_list(g_ctx, &list);
	if (count < 0) {
		LOG_ERROR("libusb_get_device_list failed: %ld\n", (long)count);
		return false;
	}

	libusb_device_handle* picked = nullptr;
	int pickedIface = -1;
	bool pickedDetach = false;

	for (ssize_t i = 0; i < count; i++) {
		libusb_device* dev = list[i];
		libusb_device_descriptor desc{};
		if (libusb_get_device_descriptor(dev, &desc) != 0) continue;

		libusb_device_handle* h = nullptr;
		if (libusb_open(dev, &h) != 0) continue;

		if (!StringMatches(h, sid, smatch)) {
			libusb_close(h);
			continue;
		}

		int iface = 0;
		bool detached = false;
		if (libusb_kernel_driver_active(h, iface) == 1) {
			if (libusb_detach_kernel_driver(h, iface) == 0) {
				detached = true;
			} else {
				LOG_WARNING("Cannot detach kernel driver from interface %d "
					"(another driver owns it).\n", iface);
				libusb_close(h);
				continue;
			}
		}
		if (libusb_claim_interface(h, iface) != 0) {
			if (detached) libusb_attach_kernel_driver(h, iface);
			libusb_close(h);
			continue;
		}

		picked = h;
		pickedIface = iface;
		pickedDetach = detached;
		break;
	}

	libusb_free_device_list(list, 1);

	if (picked == nullptr) {
		return false;
	}

	auto* back = new UsbBackingHandle{ picked, pickedIface, pickedDetach };
	_usbHandle    = back;
	_deviceHandle = back->handle;

	isOpen = true;
	return true;
}

void USBDevice::CloseDevice() {
	auto* back = static_cast<UsbBackingHandle*>(_usbHandle);
	if (back != nullptr) {
		if (back->handle != nullptr) {
			libusb_release_interface(back->handle, back->interfaceNumber);
			if (back->kernelDriverDetached) {
				libusb_attach_kernel_driver(back->handle, back->interfaceNumber);
			}
			libusb_close(back->handle);
		}
		delete back;
	}
	_usbHandle = nullptr;
	_deviceHandle = nullptr;
	if (isOpen) ReleaseCtx();
	isOpen = false;
}

int USBDevice::Read(UCHAR pipeId, void* buffer, int length) {
	auto* back = static_cast<UsbBackingHandle*>(_usbHandle);
	if (back == nullptr || back->handle == nullptr) return 0;
	int transferred = 0;
	int r = libusb_bulk_transfer(back->handle, pipeId,
	                             (unsigned char*)buffer, length,
	                             &transferred,  5000);
	if (r != 0 && r != LIBUSB_ERROR_TIMEOUT) {
		LOG_DEBUG("libusb_bulk_transfer IN ep 0x%02X failed: %s\n",
			pipeId, libusb_error_name(r));
		return 0;
	}
	return transferred;
}

int USBDevice::Write(UCHAR pipeId, void* buffer, int length) {
	auto* back = static_cast<UsbBackingHandle*>(_usbHandle);
	if (back == nullptr || back->handle == nullptr) return 0;
	int transferred = 0;
	int r = libusb_bulk_transfer(back->handle, pipeId,
	                             (unsigned char*)buffer, length,
	                             &transferred,  5000);
	if (r != 0) {
		LOG_DEBUG("libusb_bulk_transfer OUT ep 0x%02X failed: %s\n",
			pipeId, libusb_error_name(r));
		return 0;
	}
	return transferred;
}

int USBDevice::ControlTransfer(UCHAR requestType, UCHAR request,
                               USHORT value, USHORT index,
                               void* buffer, USHORT length) {
	auto* back = static_cast<UsbBackingHandle*>(_usbHandle);
	if (back == nullptr || back->handle == nullptr) return 0;
	int r = libusb_control_transfer(back->handle,
	                                requestType, request, value, index,
	                                (unsigned char*)buffer, length,
	                                 5000);
	if (r < 0) {
		LOG_DEBUG("libusb_control_transfer 0x%02X 0x%02X failed: %s\n",
			requestType, request, libusb_error_name(r));
		return 0;
	}
	return r;
}

#else

USBDevice::USBDevice(string Guid, int StringId, string StringMatch)
	: _deviceHandle(nullptr), _usbHandle(nullptr),
	  guid(Guid), stringId(StringId), stringMatch(StringMatch), isOpen(false) {}

USBDevice::~USBDevice() {}

bool USBDevice::OpenDevice(string, int, string) { return false; }
int  USBDevice::Read(UCHAR, void*, int)         { return 0; }
int  USBDevice::Write(UCHAR, void*, int)        { return 0; }
int  USBDevice::ControlTransfer(UCHAR, UCHAR, USHORT, USHORT, void*, USHORT) { return 0; }
void USBDevice::CloseDevice()                   { isOpen = false; }

#endif

#endif
