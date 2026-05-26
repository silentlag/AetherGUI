

#if !defined(_WIN32)

#include "VMulti.h"
#include "Platform.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>

#define LOG_MODULE "VMulti"
#include "Logger.h"

namespace {

struct LinuxOutput {
	int mouseFd;
	int digitizerFd;
	int absMaxX;
	int absMaxY;
	int absMaxPressure;
	bool initialized;
	// Last reported absolute screen position, used by ModeArtist to
	// derive relative motion deltas for the virtual mouse.
	int lastArtistX;
	int lastArtistY;
	double artistAccumX;
	double artistAccumY;
	bool artistHavePrev;
};

LinuxOutput g_out{ -1, -1, 0, 0, 0, false, 0, 0, 0.0, 0.0, false };

bool WriteEvent(int fd, uint16_t type, uint16_t code, int32_t value) {
	if (fd < 0) return false;
	input_event ev{};
	gettimeofday(&ev.time, nullptr);
	ev.type = type;
	ev.code = code;
	ev.value = value;
	ssize_t r = ::write(fd, &ev, sizeof(ev));
	return r == (ssize_t)sizeof(ev);
}

void Sync(int fd) {
	WriteEvent(fd, EV_SYN, SYN_REPORT, 0);
}

int CreateUinputAbsoluteMouse(int absMaxX, int absMaxY) {
	int fd = ::open("/dev/uinput", O_WRONLY | O_NONBLOCK);
	if (fd < 0) {
		LOG_ERROR("Cannot open /dev/uinput (errno %d). Is the uinput module loaded "
			"and does the user have permission?\n", errno);
		return -1;
	}

	ioctl(fd, UI_SET_EVBIT, EV_KEY);
	ioctl(fd, UI_SET_EVBIT, EV_ABS);
	ioctl(fd, UI_SET_EVBIT, EV_REL);
	ioctl(fd, UI_SET_EVBIT, EV_SYN);

	ioctl(fd, UI_SET_KEYBIT, BTN_LEFT);
	ioctl(fd, UI_SET_KEYBIT, BTN_RIGHT);
	ioctl(fd, UI_SET_KEYBIT, BTN_MIDDLE);
	ioctl(fd, UI_SET_KEYBIT, BTN_SIDE);
	ioctl(fd, UI_SET_KEYBIT, BTN_EXTRA);

	ioctl(fd, UI_SET_RELBIT, REL_X);
	ioctl(fd, UI_SET_RELBIT, REL_Y);
	ioctl(fd, UI_SET_RELBIT, REL_WHEEL);

	ioctl(fd, UI_SET_ABSBIT, ABS_X);
	ioctl(fd, UI_SET_ABSBIT, ABS_Y);

	// libinput / Wayland compositors classify uinput devices via the
	// INPUT_PROP_* flags. Without INPUT_PROP_POINTER, a device with
	// both REL and ABS axes is treated as a relative-only mouse and
	// the ABS_X/ABS_Y events the service writes in Absolute mode are
	// dropped — which is why the cursor doesn't move at all on Wayland.
	ioctl(fd, UI_SET_PROPBIT, INPUT_PROP_POINTER);

	uinput_user_dev dev{};
	std::strncpy(dev.name, "AetherService Virtual Mouse", UINPUT_MAX_NAME_SIZE - 1);
	dev.id.bustype = BUS_USB;
	dev.id.vendor  = 0x1234;
	dev.id.product = 0x5678;
	dev.id.version = 1;

	dev.absmin[ABS_X] = 0;
	dev.absmax[ABS_X] = absMaxX;
	dev.absmin[ABS_Y] = 0;
	dev.absmax[ABS_Y] = absMaxY;

	if (::write(fd, &dev, sizeof(dev)) != (ssize_t)sizeof(dev)) {
		LOG_ERROR("uinput write failed for virtual mouse (errno %d).\n", errno);
		::close(fd);
		return -1;
	}
	if (ioctl(fd, UI_DEV_CREATE) < 0) {
		LOG_ERROR("UI_DEV_CREATE failed for virtual mouse (errno %d).\n", errno);
		::close(fd);
		return -1;
	}
	return fd;
}

int CreateUinputDigitizer(int absMaxX, int absMaxY, int absMaxPressure) {
	int fd = ::open("/dev/uinput", O_WRONLY | O_NONBLOCK);
	if (fd < 0) {
		LOG_ERROR("Cannot open /dev/uinput for digitizer (errno %d).\n", errno);
		return -1;
	}

	ioctl(fd, UI_SET_EVBIT, EV_KEY);
	ioctl(fd, UI_SET_EVBIT, EV_ABS);
	ioctl(fd, UI_SET_EVBIT, EV_SYN);

	ioctl(fd, UI_SET_KEYBIT, BTN_TOOL_PEN);
	ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH);
	ioctl(fd, UI_SET_KEYBIT, BTN_STYLUS);
	ioctl(fd, UI_SET_KEYBIT, BTN_STYLUS2);

	ioctl(fd, UI_SET_ABSBIT, ABS_X);
	ioctl(fd, UI_SET_ABSBIT, ABS_Y);
	ioctl(fd, UI_SET_ABSBIT, ABS_PRESSURE);

	// Mark as a direct-input device so libinput recognizes it as a
	// graphics tablet stylus rather than ignoring it.
	ioctl(fd, UI_SET_PROPBIT, INPUT_PROP_DIRECT);

	uinput_user_dev dev{};
	std::strncpy(dev.name, "AetherService Virtual Pen", UINPUT_MAX_NAME_SIZE - 1);
	dev.id.bustype = BUS_USB;
	dev.id.vendor  = 0x1234;
	dev.id.product = 0x5679;
	dev.id.version = 1;

	dev.absmin[ABS_X] = 0;
	dev.absmax[ABS_X] = absMaxX;
	dev.absmin[ABS_Y] = 0;
	dev.absmax[ABS_Y] = absMaxY;
	dev.absmin[ABS_PRESSURE] = 0;
	dev.absmax[ABS_PRESSURE] = absMaxPressure;

	if (::write(fd, &dev, sizeof(dev)) != (ssize_t)sizeof(dev)) {
		LOG_ERROR("uinput write failed for virtual pen (errno %d).\n", errno);
		::close(fd);
		return -1;
	}
	if (ioctl(fd, UI_DEV_CREATE) < 0) {
		LOG_ERROR("UI_DEV_CREATE failed for virtual pen (errno %d).\n", errno);
		::close(fd);
		return -1;
	}
	return fd;
}

void EnsureInitialized(VMulti* self) {
	if (g_out.initialized) return;

	int absMaxX = (int)self->monitorInfo.primaryWidth;
	int absMaxY = (int)self->monitorInfo.primaryHeight;
	if (absMaxX < 1) absMaxX = 1920;
	if (absMaxY < 1) absMaxY = 1080;

	g_out.absMaxX = absMaxX;
	g_out.absMaxY = absMaxY;
	g_out.absMaxPressure = 8191;

	g_out.mouseFd     = CreateUinputAbsoluteMouse(absMaxX, absMaxY);
	g_out.digitizerFd = CreateUinputDigitizer(absMaxX, absMaxY, g_out.absMaxPressure);
	g_out.initialized = true;

	if (g_out.mouseFd < 0 && g_out.digitizerFd < 0) {
		LOG_ERROR("All uinput devices failed to open. The driver will run but \n"
			"cursor / pen events won't reach the system. Check that the \n"
			"'uinput' kernel module is loaded and that the user has \n"
			"rw access to /dev/uinput (see install/udev/60-aether-uinput.rules).\n");
	}
}

}

VMulti::VMulti() {
	isOpen = false;
	debugEnabled = false;
	lastButtons = 0;
	pendingButtons = 0;
	buttonsChanged = false;
	monitorInfoLocked = false;
	// On Linux/Wayland, libinput only honors absolute coordinates on
	// devices it recognizes as tablet/touchscreen. A plain "mouse" with
	// ABS axes silently drops ABS events, so the cursor doesn't move.
	// Default to Digitizer mode so events go to the virtual pen, which
	// libinput's tablet code handles correctly. The GUI/config can still
	// override this.
	mode = ModeDigitizer;

	reportAbsoluteMouse = {};
	reportRelativeMouse = {};
	reportDigitizer = {};
	std::memset(&relativeData, 0, sizeof(relativeData));
	relativeData.sensitivity = 1;
	relativeData.resetDistance = 400;
	relativeData.firstReport = true;

	UpdateMonitorInfo();

	std::memset(reportBuffer, 0, sizeof(reportBuffer));
	std::memset(lastReportBuffer, 0, sizeof(lastReportBuffer));

	hidDevice = nullptr;
	isOpen = true;
	outputEnabled = true;

	EnsureInitialized(this);
}

VMulti::~VMulti() {
	if (g_out.initialized) {
		if (g_out.mouseFd >= 0) {
			ioctl(g_out.mouseFd, UI_DEV_DESTROY);
			::close(g_out.mouseFd);
			g_out.mouseFd = -1;
		}
		if (g_out.digitizerFd >= 0) {
			ioctl(g_out.digitizerFd, UI_DEV_DESTROY);
			::close(g_out.digitizerFd);
			g_out.digitizerFd = -1;
		}
		g_out.initialized = false;
	}
}

bool VMulti::HasReportChanged() {

	return true;
}

void VMulti::ResetRelativeData(double x, double y) {
	relativeData.lastPosition.Set(x, y);
	relativeData.targetPosition.Set(x, y);
	relativeData.accumX = 0;
	relativeData.accumY = 0;
	relativeData.firstReport = false;
}

void VMulti::InvalidateRelativeData() {
	relativeData.firstReport = true;
}

void VMulti::UpdateMonitorInfo() {
	if (monitorInfoLocked) return;
	platform::MonitorInfo m = platform::QueryMonitorInfo();
	monitorInfo.primaryWidth  = m.primaryWidth;
	monitorInfo.primaryHeight = m.primaryHeight;
	monitorInfo.virtualWidth  = m.virtualWidth;
	monitorInfo.virtualHeight = m.virtualHeight;
	monitorInfo.virtualX      = m.virtualX;
	monitorInfo.virtualY      = m.virtualY;
}

void VMulti::SetMonitorInfo(double primaryWidth, double primaryHeight,
							double virtualWidth, double virtualHeight,
							double virtualX, double virtualY) {
	monitorInfo.primaryWidth  = primaryWidth;
	monitorInfo.primaryHeight = primaryHeight;
	monitorInfo.virtualWidth  = virtualWidth;
	monitorInfo.virtualHeight = virtualHeight;
	monitorInfo.virtualX      = virtualX;
	monitorInfo.virtualY      = virtualY;
	monitorInfoLocked = true;
}

void VMulti::CreateReport(BYTE buttons, double x, double y, double pressure) {
	pendingButtons = buttons;
	buttonsChanged = (buttons != lastButtons);

	if (mode == ModeAbsoluteMouse || mode == ModeAbsoluteVMulti || mode == ModeSendInput) {
		double normX = x / monitorInfo.primaryWidth;
		double normY = y / monitorInfo.primaryHeight;
		if (normX < 0) normX = 0; if (normX > 1) normX = 1;
		if (normY < 0) normY = 0; if (normY > 1) normY = 1;
		reportAbsoluteMouse.x = (USHORT)(normX * 32767.0 + 0.5);
		reportAbsoluteMouse.y = (USHORT)(normY * 32767.0 + 0.5);
		reportAbsoluteMouse.buttons = (BYTE)(buttons & 0x07);
	}
	else if (mode == ModeArtist) {
		// Store both the absolute position (for the digitizer event)
		// and the raw screen-space coordinates so WriteReport can
		// derive a relative delta for the virtual mouse.
		double normX = x / monitorInfo.primaryWidth;
		double normY = y / monitorInfo.primaryHeight;
		if (normX < 0) normX = 0; if (normX > 1) normX = 1;
		if (normY < 0) normY = 0; if (normY > 1) normY = 1;
		reportDigitizer.x = (USHORT)(normX * 32767.0 + 0.5);
		reportDigitizer.y = (USHORT)(normY * 32767.0 + 0.5);
		reportDigitizer.pressure = (USHORT)(pressure * 2047.0);
		if (reportDigitizer.pressure > 2047) reportDigitizer.pressure = 2047;
		reportDigitizer.buttons = buttons | 0x20;
		// Reuse reportAbsoluteMouse.x/y as raw pixel screen coords for
		// the delta calculation done in WriteReport().
		int rawX = (int)x;
		int rawY = (int)y;
		if (rawX < 0) rawX = 0;
		if (rawY < 0) rawY = 0;
		if (rawX > (int)monitorInfo.primaryWidth)  rawX = (int)monitorInfo.primaryWidth;
		if (rawY > (int)monitorInfo.primaryHeight) rawY = (int)monitorInfo.primaryHeight;
		reportAbsoluteMouse.x = (USHORT)rawX;
		reportAbsoluteMouse.y = (USHORT)rawY;
		reportAbsoluteMouse.buttons = (BYTE)(buttons & 0x07);
	}
	else if (mode == ModeRelativeMouse) {

		reportRelativeMouse.x = (BYTE)((int)x);
		reportRelativeMouse.y = (BYTE)((int)y);
		reportRelativeMouse.buttons = (BYTE)(buttons & 0x07);
	}
	else if (mode == ModeDigitizer) {
		double normX = x / monitorInfo.primaryWidth;
		double normY = y / monitorInfo.primaryHeight;
		if (normX < 0) normX = 0; if (normX > 1) normX = 1;
		if (normY < 0) normY = 0; if (normY > 1) normY = 1;
		reportDigitizer.x = (USHORT)(normX * 32767.0 + 0.5);
		reportDigitizer.y = (USHORT)(normY * 32767.0 + 0.5);
		reportDigitizer.pressure = (USHORT)(pressure * 2047.0);
		if (reportDigitizer.pressure > 2047) reportDigitizer.pressure = 2047;
		reportDigitizer.buttons = buttons | 0x20;
	}

	lastButtons = buttons;
}

int VMulti::ResetReport() {
	pendingButtons = 0;
	lastButtons = 0;
	buttonsChanged = false;
	g_out.artistHavePrev = false;
	g_out.artistAccumX = 0.0;
	g_out.artistAccumY = 0.0;
	if (g_out.mouseFd >= 0) {
		WriteEvent(g_out.mouseFd, EV_KEY, BTN_LEFT,   0);
		WriteEvent(g_out.mouseFd, EV_KEY, BTN_RIGHT,  0);
		WriteEvent(g_out.mouseFd, EV_KEY, BTN_MIDDLE, 0);
		Sync(g_out.mouseFd);
	}
	if (g_out.digitizerFd >= 0) {
		WriteEvent(g_out.digitizerFd, EV_KEY, BTN_TOUCH,    0);
		WriteEvent(g_out.digitizerFd, EV_KEY, BTN_TOOL_PEN, 0);
		Sync(g_out.digitizerFd);
	}
	return 0;
}

int VMulti::WriteReport() {
	if (!outputEnabled) return 1;
	EnsureInitialized(this);

	if (mode == ModeAbsoluteMouse || mode == ModeAbsoluteVMulti || mode == ModeSendInput) {
		if (g_out.mouseFd < 0) return 0;

		int posX = (int)((double)reportAbsoluteMouse.x / 32767.0 * (double)g_out.absMaxX + 0.5);
		int posY = (int)((double)reportAbsoluteMouse.y / 32767.0 * (double)g_out.absMaxY + 0.5);

		WriteEvent(g_out.mouseFd, EV_ABS, ABS_X, posX);
		WriteEvent(g_out.mouseFd, EV_ABS, ABS_Y, posY);
		WriteEvent(g_out.mouseFd, EV_KEY, BTN_LEFT,   (reportAbsoluteMouse.buttons & 0x01) ? 1 : 0);
		WriteEvent(g_out.mouseFd, EV_KEY, BTN_RIGHT,  (reportAbsoluteMouse.buttons & 0x02) ? 1 : 0);
		WriteEvent(g_out.mouseFd, EV_KEY, BTN_MIDDLE, (reportAbsoluteMouse.buttons & 0x04) ? 1 : 0);
		Sync(g_out.mouseFd);
		return 1;
	}
	else if (mode == ModeRelativeMouse) {
		if (g_out.mouseFd < 0) return 0;
		WriteEvent(g_out.mouseFd, EV_REL, REL_X, (int8_t)reportRelativeMouse.x);
		WriteEvent(g_out.mouseFd, EV_REL, REL_Y, (int8_t)reportRelativeMouse.y);
		WriteEvent(g_out.mouseFd, EV_KEY, BTN_LEFT,   (reportRelativeMouse.buttons & 0x01) ? 1 : 0);
		WriteEvent(g_out.mouseFd, EV_KEY, BTN_RIGHT,  (reportRelativeMouse.buttons & 0x02) ? 1 : 0);
		WriteEvent(g_out.mouseFd, EV_KEY, BTN_MIDDLE, (reportRelativeMouse.buttons & 0x04) ? 1 : 0);
		Sync(g_out.mouseFd);
		return 1;
	}
	else if (mode == ModeDigitizer) {
		if (g_out.digitizerFd < 0) return 0;
		int posX = (int)((double)reportDigitizer.x / 32767.0 * (double)g_out.absMaxX + 0.5);
		int posY = (int)((double)reportDigitizer.y / 32767.0 * (double)g_out.absMaxY + 0.5);
		int pressure = (int)((double)reportDigitizer.pressure / 2047.0 * (double)g_out.absMaxPressure + 0.5);

		WriteEvent(g_out.digitizerFd, EV_ABS, ABS_X, posX);
		WriteEvent(g_out.digitizerFd, EV_ABS, ABS_Y, posY);
		WriteEvent(g_out.digitizerFd, EV_ABS, ABS_PRESSURE, pressure);
		WriteEvent(g_out.digitizerFd, EV_KEY, BTN_TOOL_PEN, 1);
		WriteEvent(g_out.digitizerFd, EV_KEY, BTN_TOUCH,    pressure > 0 ? 1 : 0);
		Sync(g_out.digitizerFd);
		return 1;
	}
	else if (mode == ModeArtist) {
		// Drive the digitizer (absolute) and the mouse (relative
		// motion) simultaneously. Desktop/Wayland compositors get
		// absolute positioning from the tablet device; full-screen
		// games that grab the pointer or read raw mouse input get
		// usable relative motion from the mouse device.
		int curX = reportAbsoluteMouse.x;
		int curY = reportAbsoluteMouse.y;

		if (g_out.digitizerFd >= 0) {
			int posX = (int)((double)reportDigitizer.x / 32767.0 * (double)g_out.absMaxX + 0.5);
			int posY = (int)((double)reportDigitizer.y / 32767.0 * (double)g_out.absMaxY + 0.5);
			int pressure = (int)((double)reportDigitizer.pressure / 2047.0 * (double)g_out.absMaxPressure + 0.5);
			WriteEvent(g_out.digitizerFd, EV_ABS, ABS_X, posX);
			WriteEvent(g_out.digitizerFd, EV_ABS, ABS_Y, posY);
			WriteEvent(g_out.digitizerFd, EV_ABS, ABS_PRESSURE, pressure);
			WriteEvent(g_out.digitizerFd, EV_KEY, BTN_TOOL_PEN, 1);
			WriteEvent(g_out.digitizerFd, EV_KEY, BTN_TOUCH,    pressure > 0 ? 1 : 0);
			Sync(g_out.digitizerFd);
		}

		if (g_out.mouseFd >= 0) {
			if (!g_out.artistHavePrev) {
				g_out.lastArtistX = curX;
				g_out.lastArtistY = curY;
				g_out.artistAccumX = 0.0;
				g_out.artistAccumY = 0.0;
				g_out.artistHavePrev = true;
			}
			double dx = (double)(curX - g_out.lastArtistX) + g_out.artistAccumX;
			double dy = (double)(curY - g_out.lastArtistY) + g_out.artistAccumY;
			int idx = (int)dx;
			int idy = (int)dy;
			g_out.artistAccumX = dx - (double)idx;
			g_out.artistAccumY = dy - (double)idy;
			g_out.lastArtistX = curX;
			g_out.lastArtistY = curY;

			if (idx != 0) WriteEvent(g_out.mouseFd, EV_REL, REL_X, idx);
			if (idy != 0) WriteEvent(g_out.mouseFd, EV_REL, REL_Y, idy);
			WriteEvent(g_out.mouseFd, EV_KEY, BTN_LEFT,   (reportAbsoluteMouse.buttons & 0x01) ? 1 : 0);
			WriteEvent(g_out.mouseFd, EV_KEY, BTN_RIGHT,  (reportAbsoluteMouse.buttons & 0x02) ? 1 : 0);
			WriteEvent(g_out.mouseFd, EV_KEY, BTN_MIDDLE, (reportAbsoluteMouse.buttons & 0x04) ? 1 : 0);
			Sync(g_out.mouseFd);
		}
		return 1;
	}
	return 0;
}

void VMulti::EmulateWheel(int delta) {

	EnsureInitialized(this);
	if (g_out.mouseFd < 0) return;

	int notches = delta / 120;
	if (notches == 0 && delta != 0) {

		notches = (delta > 0) ? 1 : -1;
	}
	WriteEvent(g_out.mouseFd, EV_REL, REL_WHEEL, notches);
	Sync(g_out.mouseFd);
}

#endif
