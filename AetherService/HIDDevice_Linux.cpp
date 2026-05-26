

#if !defined(_WIN32)

#include "HIDDevice.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/hidraw.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <vector>

#include <libudev.h>

#if defined(AETHER_HAVE_LIBUSB)
  #include <libusb-1.0/libusb.h>
#endif

namespace {

int ScoreNode(const hidraw_devinfo& info,
			  USHORT wantVid, USHORT wantPid,
			  USHORT wantUsagePage, USHORT wantUsage,
			  int wantInputLen, int actualInputLen,
			  USHORT actualUsagePage, USHORT actualUsage) {
	if ((USHORT)info.vendor != wantVid)  return 0;
	if ((USHORT)info.product != wantPid) return 0;

	// When the config specifies a usage page or usage, treat them as
	// hard filters rather than score-only hints. Otherwise the very
	// first matching VID/PID with any vendor collection beats stricter
	// later entries that match the actual top-level collection.
	if (wantUsagePage > 0 && actualUsagePage != wantUsagePage) return 0;
	if (wantUsage > 0     && actualUsage     != wantUsage)     return 0;

	int score = 1;

	if (wantUsagePage > 0) score += 8;
	if (wantUsage > 0)     score += 4;
	if (wantInputLen > 0) {
		if (actualInputLen == wantInputLen) score += 4;
	}
	return score;
}

int QueryReportDescriptorSize(int fd) {
	int size = 0;
	if (ioctl(fd, HIDIOCGRDESCSIZE, &size) < 0) return 0;
	return size;
}

struct TopCollection {
	USHORT usagePage;
	USHORT usage;
	int inputLenBytes;
};

struct ParsedReportInfo {
	int inputLenBytes;
	USHORT usagePage;
	USHORT usage;
	std::vector<TopCollection> collections;
};

ParsedReportInfo ParseInputReportDescriptor(int fd) {
	ParsedReportInfo r{ 0, 0, 0, {} };
	hidraw_report_descriptor desc{};
	desc.size = QueryReportDescriptorSize(fd);
	if (desc.size == 0 || desc.size > (int)sizeof(desc.value)) return r;
	if (ioctl(fd, HIDIOCGRDESC, &desc) < 0) return r;

	int reportSize = 0;
	int reportCount = 0;
	int bestInputBits = 0;

	USHORT curUsagePage = 0;
	USHORT curUsage = 0;
	int collectionDepth = 0;

	// We track every Application top-level collection (depth 0 when its
	// Collection item is parsed). For each, record the usage page/usage in
	// scope and the largest Input bit-count we see while inside it.
	int curCollectionIndex = -1;
	int curCollectionMaxBits = 0;

	auto finalizeCurrent = [&]() {
		if (curCollectionIndex >= 0) {
			TopCollection& tc = r.collections[curCollectionIndex];
			int bytes = (curCollectionMaxBits + 7) / 8;
			tc.inputLenBytes = bytes > 0 ? bytes + 1 : 0;
		}
		curCollectionIndex = -1;
		curCollectionMaxBits = 0;
	};

	const uint8_t* p   = desc.value;
	const uint8_t* end = desc.value + desc.size;

	while (p < end) {
		uint8_t prefix = *p++;
		uint8_t tag    = (prefix >> 4) & 0xF;
		uint8_t type   = (prefix >> 2) & 0x3;
		int dataSize   = prefix & 0x3;
		if (dataSize == 3) dataSize = 4;
		if (p + dataSize > end) break;

		uint32_t data = 0;
		for (int i = 0; i < dataSize; i++) data |= ((uint32_t)p[i]) << (8 * i);
		p += dataSize;

		if (type == 1) {
			if (tag == 0)      curUsagePage = (USHORT)data;
			else if (tag == 7) reportSize = (int)data;
			else if (tag == 9) reportCount = (int)data;
		}
		else if (type == 2) {
			if (tag == 0) curUsage = (USHORT)data;
		}
		else if (type == 0 && tag == 0xA) {

			if (collectionDepth == 0) {
				finalizeCurrent();
				TopCollection tc{ curUsagePage, curUsage, 0 };
				r.collections.push_back(tc);
				curCollectionIndex = (int)r.collections.size() - 1;
				curCollectionMaxBits = 0;
			}
			collectionDepth++;
		}
		else if (type == 0 && tag == 0xC) {
			if (collectionDepth > 0) collectionDepth--;
			if (collectionDepth == 0) finalizeCurrent();
		}
		else if (type == 0 && tag == 8) {

			int bits = reportSize * reportCount;
			if (bits > bestInputBits) bestInputBits = bits;
			if (bits > curCollectionMaxBits) curCollectionMaxBits = bits;
		}
	}
	finalizeCurrent();

	if (!r.collections.empty()) {
		r.usagePage = r.collections[0].usagePage;
		r.usage     = r.collections[0].usage;
	}

	int bytes = (bestInputBits + 7) / 8;
	r.inputLenBytes = bytes > 0 ? bytes + 1 : 0;
	return r;
}

}

HIDDevice::HIDDevice(USHORT VendorId, USHORT ProductId, USHORT UsagePage, USHORT Usage,
					 int InputReportLength, int StringId, string StringMatch,
					 int StringId2, string StringMatch2)
	: _deviceHandle(nullptr) {
	isOpen = false;
	debugEnabled = false;
	vendorId = VendorId; productId = ProductId;
	usagePage = UsagePage; usage = Usage;
	inputReportLength = InputReportLength;
	outputReportLength = 0;
	featureReportLength = 0;
	stringId = StringId; stringMatch = StringMatch;
	stringId2 = StringId2; stringMatch2 = StringMatch2;

	// Match the Windows constructor behavior: actually try to open a
	// matching hidraw node now so `isOpen` reflects whether the tablet
	// was found. Without this, every `Tablet 0xVID 0xPID ...` line in
	// the embedded config reports "Can't open" even when the device is
	// connected, because OpenDevice never ran.
	HANDLE handle = nullptr;
	this->OpenDevice(&handle, VendorId, ProductId, UsagePage, Usage,
					 InputReportLength, StringId, StringMatch,
					 StringId2, StringMatch2);
}

HIDDevice::HIDDevice() : _deviceHandle(nullptr) {
	isOpen = false;
	debugEnabled = false;
	vendorId = productId = usagePage = usage = 0;
	inputReportLength = outputReportLength = featureReportLength = 0;
	stringId = stringId2 = 0;
}

HIDDevice::~HIDDevice() {
	CloseDevice();
}

bool HIDDevice::OpenDevice(HANDLE* handle, USHORT vid, USHORT pid,
						   USHORT upage, USHORT u, int inputLen,
						   int sid, string smatch, int sid2, string smatch2) {
	this->vendorId = vid; this->productId = pid;
	this->usagePage = upage; this->usage = u;
	this->inputReportLength = inputLen;
	this->stringId = sid; this->stringMatch = smatch;
	this->stringId2 = sid2; this->stringMatch2 = smatch2;

	udev* ud = udev_new();
	if (ud == nullptr) return false;

	udev_enumerate* en = udev_enumerate_new(ud);
	udev_enumerate_add_match_subsystem(en, "hidraw");
	udev_enumerate_scan_devices(en);

	int  bestFd    = -1;
	int  bestScore = 0;
	int  bestInputLen = 0;
	int  bestOutputLen = 0;
	int  bestFeatureLen = 0;

	udev_list_entry* devices = udev_enumerate_get_list_entry(en);
	udev_list_entry* entry;
	udev_list_entry_foreach(entry, devices) {
		const char* path = udev_list_entry_get_name(entry);
		udev_device* dev = udev_device_new_from_syspath(ud, path);
		if (dev == nullptr) continue;

		const char* devnode = udev_device_get_devnode(dev);
		if (devnode == nullptr) { udev_device_unref(dev); continue; }

		int fd = ::open(devnode, O_RDWR | O_NONBLOCK);
		if (fd < 0) { udev_device_unref(dev); continue; }

		hidraw_devinfo info{};
		if (ioctl(fd, HIDIOCGRAWINFO, &info) < 0) {
			::close(fd);
			udev_device_unref(dev);
			continue;
		}

		ParsedReportInfo parsed = ParseInputReportDescriptor(fd);
		int actualInputLen   = parsed.inputLenBytes;
		USHORT actualUsagePage  = parsed.usagePage;
		USHORT actualUsage      = parsed.usage;

		// Some devices (e.g. Wacom CTL-472) expose multiple top-level
		// Application collections in one report descriptor. The "tablet"
		// usage we want is usually not the first one, so score against
		// every top-level collection and keep the best.
		int score = ScoreNode(info, vid, pid, upage, u, inputLen, actualInputLen,
							  actualUsagePage, actualUsage);
		for (const TopCollection& tc : parsed.collections) {
			int trialLen = tc.inputLenBytes > 0 ? tc.inputLenBytes : actualInputLen;
			int trial = ScoreNode(info, vid, pid, upage, u, inputLen, trialLen,
								  tc.usagePage, tc.usage);
			if (trial > score) {
				score = trial;
				actualUsagePage = tc.usagePage;
				actualUsage = tc.usage;
				if (tc.inputLenBytes > 0) actualInputLen = tc.inputLenBytes;
			}
		}

		if (score > bestScore) {
			if (bestFd >= 0) ::close(bestFd);
			bestFd        = fd;
			bestScore     = score;
			bestInputLen  = actualInputLen > 0 ? actualInputLen : inputLen;

			bestOutputLen = bestInputLen;
			bestFeatureLen = bestInputLen;
		}
		else {
			::close(fd);
		}
		udev_device_unref(dev);
	}

	udev_enumerate_unref(en);
	udev_unref(ud);

	if (bestFd < 0) return false;

	int flags = fcntl(bestFd, F_GETFL, 0);
	if (flags >= 0) fcntl(bestFd, F_SETFL, flags & ~O_NONBLOCK);

	_deviceHandle = reinterpret_cast<void*>((intptr_t)bestFd);
	this->inputReportLength   = bestInputLen;
	this->outputReportLength  = bestOutputLen;
	this->featureReportLength = bestFeatureLen;
	if (handle != nullptr) *handle = _deviceHandle;
	isOpen = true;
	return true;
}

int HIDDevice::Read(void* buffer, int length) {
	if (!isOpen) return 0;
	int fd = (int)(intptr_t)_deviceHandle;
	ssize_t r;
	do {
		r = ::read(fd, buffer, (size_t)length);
	} while (r < 0 && errno == EINTR);
	return r > 0 ? (int)r : 0;
}

int HIDDevice::Write(void* buffer, int length) {
	if (!isOpen) return 0;
	int fd = (int)(intptr_t)_deviceHandle;
	ssize_t w;
	do {
		w = ::write(fd, buffer, (size_t)length);
	} while (w < 0 && errno == EINTR);
	return w > 0 ? (int)w : 0;
}

bool HIDDevice::SetFeature(void* buffer, int length) {
	if (!isOpen) return false;
	int fd = (int)(intptr_t)_deviceHandle;
	return ioctl(fd, HIDIOCSFEATURE(length), buffer) >= 0;
}

bool HIDDevice::GetFeature(void* buffer, int length) {
	if (!isOpen) return false;
	int fd = (int)(intptr_t)_deviceHandle;
	return ioctl(fd, HIDIOCGFEATURE(length), buffer) >= 0;
}

bool HIDDevice::GetIndexedString(int sid, string* result) {
	if (result != nullptr) result->clear();
	if (!isOpen || sid <= 0) return false;
#if defined(AETHER_HAVE_LIBUSB)

	libusb_context* ctx = nullptr;
	if (libusb_init(&ctx) != 0) return false;

	libusb_device** list = nullptr;
	ssize_t count = libusb_get_device_list(ctx, &list);
	if (count < 0) { libusb_exit(ctx); return false; }

	bool found = false;
	for (ssize_t i = 0; i < count; i++) {
		libusb_device_descriptor d{};
		if (libusb_get_device_descriptor(list[i], &d) != 0) continue;
		if (d.idVendor != vendorId || d.idProduct != productId) continue;

		libusb_device_handle* h = nullptr;
		if (libusb_open(list[i], &h) != 0) continue;
		unsigned char buf[256] = {0};
		int n = libusb_get_string_descriptor_ascii(h, (uint8_t)sid, buf, sizeof(buf) - 1);
		libusb_close(h);
		if (n > 0) {
			if (result != nullptr) result->assign((char*)buf, (size_t)n);
			found = true;
			break;
		}
	}

	libusb_free_device_list(list, 1);
	libusb_exit(ctx);
	return found;
#else
	(void)sid; (void)result;
	return false;
#endif
}

void HIDDevice::CloseDevice() {
	if (!isOpen) return;
	int fd = (int)(intptr_t)_deviceHandle;
	if (fd >= 0) ::close(fd);
	_deviceHandle = nullptr;
	isOpen = false;
}

#endif
