

#if !defined(_WIN32)

#include "Platform.h"

#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstring>

#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

#include <dirent.h>
#include <dlfcn.h>

#include <libudev.h>

#if defined(AETHER_HAVE_XRANDR) && defined(AETHER_HAVE_X11)
  #include <X11/Xlib.h>
  #include <X11/extensions/Xrandr.h>
#endif

namespace platform {

namespace {

struct LinuxBoost {
	int oldPolicy;
	sched_param oldParam;
	bool restored;
};

thread_local int tlsTimerFd = -1;

int GetOrCreateThreadTimerFd() {
	if (tlsTimerFd < 0) {
		tlsTimerFd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
	}
	return tlsTimerFd;
}

int RtPriorityForTier(ThreadBoostTier tier) {

	switch (tier) {
		case ThreadBoostTier::Producer: return 50;
		case ThreadBoostTier::Timer:    return 30;
	}
	return 30;
}

}

void GlobalInit() {

	rlimit rl{};
	if (getrlimit(RLIMIT_RTPRIO, &rl) == 0) {
		if (rl.rlim_max > rl.rlim_cur) {
			rl.rlim_cur = rl.rlim_max;
			(void)setrlimit(RLIMIT_RTPRIO, &rl);
		}
	}
}

void GlobalShutdown() {

}

MonitorInfo QueryMonitorInfo() {
	MonitorInfo m{};

	m.primaryWidth  = 1920.0;
	m.primaryHeight = 1080.0;
	m.virtualWidth  = 1920.0;
	m.virtualHeight = 1080.0;
	m.virtualX = 0.0;
	m.virtualY = 0.0;

#if defined(AETHER_HAVE_XRANDR) && defined(AETHER_HAVE_X11)

	Display* dpy = XOpenDisplay(nullptr);
	if (dpy != nullptr) {
		Window root = DefaultRootWindow(dpy);
		XRRScreenResources* res = XRRGetScreenResourcesCurrent(dpy, root);
		RROutput primary = XRRGetOutputPrimary(dpy, root);

		if (res != nullptr) {
			double minX = 0, minY = 0, maxX = 0, maxY = 0;
			bool haveBounds = false;
			double primaryW = 0, primaryH = 0;

			for (int i = 0; i < res->noutput; i++) {
				XRROutputInfo* oi = XRRGetOutputInfo(dpy, res, res->outputs[i]);
				if (oi == nullptr) continue;
				if (oi->connection != RR_Connected || oi->crtc == 0) {
					XRRFreeOutputInfo(oi);
					continue;
				}
				XRRCrtcInfo* ci = XRRGetCrtcInfo(dpy, res, oi->crtc);
				if (ci == nullptr) {
					XRRFreeOutputInfo(oi);
					continue;
				}

				double x0 = (double)ci->x;
				double y0 = (double)ci->y;
				double x1 = x0 + (double)ci->width;
				double y1 = y0 + (double)ci->height;
				if (!haveBounds) {
					minX = x0; minY = y0; maxX = x1; maxY = y1;
					haveBounds = true;
				} else {
					if (x0 < minX) minX = x0;
					if (y0 < minY) minY = y0;
					if (x1 > maxX) maxX = x1;
					if (y1 > maxY) maxY = y1;
				}

				if (res->outputs[i] == primary) {
					primaryW = (double)ci->width;
					primaryH = (double)ci->height;
				}

				XRRFreeCrtcInfo(ci);
				XRRFreeOutputInfo(oi);
			}

			if (haveBounds) {
				m.virtualX      = minX;
				m.virtualY      = minY;
				m.virtualWidth  = maxX - minX;
				m.virtualHeight = maxY - minY;
			}
			if (primaryW > 0 && primaryH > 0) {
				m.primaryWidth  = primaryW;
				m.primaryHeight = primaryH;
			} else if (haveBounds) {

				m.primaryWidth  = m.virtualWidth;
				m.primaryHeight = m.virtualHeight;
			}

			XRRFreeScreenResources(res);
		}
		XCloseDisplay(dpy);
		return m;
	}

#endif

	FILE* f = popen("xdpyinfo 2>/dev/null | awk '/dimensions:/ {print $2}'", "r");
	if (f != nullptr) {
		char buf[64] = {0};
		if (fgets(buf, sizeof(buf), f) != nullptr) {
			int w = 0, h = 0;
			if (sscanf(buf, "%dx%d", &w, &h) == 2 && w > 0 && h > 0) {
				m.primaryWidth = m.virtualWidth = (double)w;
				m.primaryHeight = m.virtualHeight = (double)h;
			}
		}
		pclose(f);
	}
	return m;
}

void SleepMs(unsigned ms) {
	timespec ts{};
	ts.tv_sec  = ms / 1000;
	ts.tv_nsec = (long)(ms % 1000) * 1000000L;
	while (nanosleep(&ts, &ts) < 0 && errno == EINTR) {}
}

uint32_t LastErrorCode() {
	return (uint32_t)errno;
}

uint32_t ErrorDeviceNotConnected() { return (uint32_t)ENODEV; }
uint32_t ErrorAccessDenied()       { return (uint32_t)EACCES; }

ThreadBoostHandle BoostCurrentThread(ThreadBoostTier tier) {
	LinuxBoost* boost = new LinuxBoost{};
	boost->restored = false;

	pthread_t self = pthread_self();
	if (pthread_getschedparam(self, &boost->oldPolicy, &boost->oldParam) != 0) {

		boost->oldPolicy = SCHED_OTHER;
		boost->oldParam = sched_param{};
	}

	sched_param param{};
	param.sched_priority = RtPriorityForTier(tier);

	if (pthread_setschedparam(self, SCHED_FIFO, &param) != 0) {

	}
	return ThreadBoostHandle{ boost };
}

void RestoreCurrentThread(ThreadBoostHandle handle) {
	LinuxBoost* boost = static_cast<LinuxBoost*>(handle.impl);
	if (boost == nullptr || boost->restored) {
		delete boost;
		return;
	}
	pthread_setschedparam(pthread_self(), boost->oldPolicy, &boost->oldParam);
	boost->restored = true;
	delete boost;
}

int64_t MonotonicNs() {
	using namespace std::chrono;
	return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

int64_t SleepUntilNs(int64_t deadlineNs, int64_t spinThresholdNs) {
	int64_t now = MonotonicNs();
	while (now < deadlineNs) {
		int64_t remainingNs = deadlineNs - now;
		if (remainingNs > spinThresholdNs) {
			int fd = GetOrCreateThreadTimerFd();
			if (fd >= 0) {
				int64_t wakeNs = deadlineNs - spinThresholdNs;
				timespec ts{};
				clock_gettime(CLOCK_MONOTONIC, &ts);
				int64_t baseNs = (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
				int64_t deltaNs = wakeNs - baseNs;
				if (deltaNs < 1) deltaNs = 1;

				itimerspec it{};
				it.it_value.tv_sec  = deltaNs / 1000000000LL;
				it.it_value.tv_nsec = deltaNs % 1000000000LL;

				if (timerfd_settime(fd, 0, &it, nullptr) == 0) {
					uint64_t expirations = 0;
					ssize_t r;
					do {
						r = read(fd, &expirations, sizeof(expirations));
					} while (r < 0 && errno == EINTR);
					(void)r;
				}
				else {
					sched_yield();
				}
			}
			else {
				sched_yield();
			}
		}
		else {
			CpuPause();
		}
		now = MonotonicNs();
	}
	return now;
}

}

#include <codecvt>
#include <locale>
#include <mutex>

namespace platform {
namespace {

std::string WideToUtf8(const wchar_t* w) {
	if (w == nullptr) return std::string();
	std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> conv;
	try { return conv.to_bytes(w); } catch (...) { return std::string(); }
}

std::mutex& DlMutex() { static std::mutex m; return m; }
thread_local std::string tlsLastDlError;

}

DynLib DynLibLoad(const wchar_t* path) {
	std::lock_guard<std::mutex> lk(DlMutex());
	std::string narrow = WideToUtf8(path);
	dlerror();
	void* h = dlopen(narrow.c_str(), RTLD_NOW | RTLD_LOCAL);
	if (h == nullptr) {
		const char* e = dlerror();
		tlsLastDlError = e ? e : "dlopen failed";
	} else {
		tlsLastDlError.clear();
	}
	return DynLib{ h };
}

bool DynLibValid(const DynLib& lib) { return lib.handle != nullptr; }

void* DynLibSymbol(const DynLib& lib, const char* name) {
	if (lib.handle == nullptr || name == nullptr) return nullptr;
	dlerror();
	void* sym = dlsym(lib.handle, name);
	if (sym == nullptr) {
		const char* e = dlerror();
		if (e) tlsLastDlError = e;
	}
	return sym;
}

void DynLibUnload(DynLib& lib) {
	if (lib.handle != nullptr) {
		dlclose(lib.handle);
		lib.handle = nullptr;
	}
}

std::string DynLibLastError() { return tlsLastDlError; }

bool PathExists(const wchar_t* path) {
	struct stat st{};
	return ::stat(WideToUtf8(path).c_str(), &st) == 0;
}

bool PathIsDirectory(const wchar_t* path) {
	struct stat st{};
	if (::stat(WideToUtf8(path).c_str(), &st) != 0) return false;
	return S_ISDIR(st.st_mode);
}

bool MakeDirectory(const wchar_t* path) {
	std::string narrow = WideToUtf8(path);
	if (narrow.empty()) return false;
	if (::mkdir(narrow.c_str(), 0755) == 0) return true;

	if (errno == EEXIST) {
		struct stat st{};
		if (::stat(narrow.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) return true;
	}
	return false;
}

bool ListDirectory(const wchar_t* path, std::vector<std::wstring>* out) {
	if (out == nullptr) return false;
	out->clear();
	DIR* d = ::opendir(WideToUtf8(path).c_str());
	if (d == nullptr) return false;
	std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> conv;
	for (struct dirent* e = ::readdir(d); e != nullptr; e = ::readdir(d)) {
		if (e->d_name[0] == '.' &&
		    (e->d_name[1] == '\0' || (e->d_name[1] == '.' && e->d_name[2] == '\0'))) {
			continue;
		}
		try { out->push_back(conv.from_bytes(e->d_name)); }
		catch (...) {  }
	}
	::closedir(d);
	return true;
}

}

#endif
