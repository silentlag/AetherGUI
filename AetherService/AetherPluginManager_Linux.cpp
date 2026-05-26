

#if !defined(_WIN32)

#include "AetherPluginManager.h"
#include "AetherPluginApi.h"
#include "Platform.h"

#define LOG_MODULE "Plugin"
#include "Logger.h"

#include <cstring>
#include <fstream>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>

namespace {

std::wstring DecodeUtf8(const std::string& s) {
	std::wstring out;
	out.reserve(s.size());
	size_t i = 0;
	while (i < s.size()) {
		unsigned char c = (unsigned char)s[i];
		unsigned int cp = 0;
		int extra = 0;
		if (c < 0x80) { cp = c; extra = 0; }
		else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
		else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
		else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
		else { i++; continue; }
		i++;
		while (extra-- > 0 && i < s.size()) {
			unsigned char cc = (unsigned char)s[i++];
			if ((cc & 0xC0) != 0x80) break;
			cp = (cp << 6) | (cc & 0x3F);
		}
		out.push_back((wchar_t)cp);
	}
	return out;
}

std::string EncodeUtf8(const std::wstring& s) {
	std::string out;
	out.reserve(s.size());
	for (wchar_t wc : s) {
		unsigned int cp = (unsigned int)wc;
		if (cp < 0x80) {
			out.push_back((char)cp);
		}
		else if (cp < 0x800) {
			out.push_back((char)(0xC0 | (cp >> 6)));
			out.push_back((char)(0x80 | (cp & 0x3F)));
		}
		else if (cp < 0x10000) {
			out.push_back((char)(0xE0 | (cp >> 12)));
			out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
			out.push_back((char)(0x80 | (cp & 0x3F)));
		}
		else {
			out.push_back((char)(0xF0 | (cp >> 18)));
			out.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
			out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
			out.push_back((char)(0x80 | (cp & 0x3F)));
		}
	}
	return out;
}

std::wstring AppendSlash(const std::wstring& dir) {
	if (dir.empty()) return dir + L"/";
	wchar_t last = dir.back();
	if (last == L'/' || last == L'\\') return dir;
	return dir + L"/";
}

std::wstring GetExeDirectory() {

	char buf[PATH_MAX] = {0};
	ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
	if (n <= 0) {
		return L"./";
	}
	buf[n] = 0;
	std::string path(buf);
	size_t slash = path.find_last_of('/');
	std::string dir = (slash == std::string::npos) ? path : path.substr(0, slash + 1);
	return DecodeUtf8(dir);
}

bool CopyFileNarrow(const std::string& src, const std::string& dst) {
	std::ifstream in(src, std::ios::binary);
	if (!in) return false;
	std::ofstream out(dst, std::ios::binary | std::ios::trunc);
	if (!out) return false;
	out << in.rdbuf();
	return out.good();
}

}

std::wstring Utf8ToWideService(const std::string& text) { return DecodeUtf8(text); }
std::string  WideToUtf8Service(const std::wstring& text) { return EncodeUtf8(text); }

std::wstring GetAetherServiceDirectory() { return GetExeDirectory(); }

std::wstring GetAetherPluginDirectory() {
	return GetAetherServiceDirectory() + L"plugins/";
}

bool EnsureAetherPluginDirectory() {
	std::wstring dir = GetAetherPluginDirectory();
	if (platform::MakeDirectory(dir.c_str())) return true;
	LOG_ERROR("Failed to create plugin directory: %s\\n", EncodeUtf8(dir).c_str());
	return false;
}

bool InstallAetherPluginDll(const std::wstring& sourcePath, std::wstring* installedPath) {

	if (!EnsureAetherPluginDirectory())
		return false;

	if (!platform::PathExists(sourcePath.c_str())) {
		LOG_ERROR("Plugin not found: %s\\n", EncodeUtf8(sourcePath).c_str());
		return false;
	}

	std::string srcUtf8 = EncodeUtf8(sourcePath);

	if (platform::PathIsDirectory(sourcePath.c_str())) {
		std::vector<std::wstring> entries;
		platform::ListDirectory(sourcePath.c_str(), &entries);

		std::wstring trimmed = sourcePath;
		if (!trimmed.empty() && (trimmed.back() == L'/' || trimmed.back() == L'\\'))
			trimmed.pop_back();
		size_t slash = trimmed.find_last_of(L"/\\");
		std::wstring leaf = (slash == std::wstring::npos) ? trimmed : trimmed.substr(slash + 1);

		std::wstring pluginDir = GetAetherPluginDirectory() + leaf + L"/";
		platform::MakeDirectory(pluginDir.c_str());

		bool copied = false;
		std::wstring firstInstalled;
		for (const auto& name : entries) {
			if (name.size() < 4) continue;
			std::wstring ext = name.substr(name.size() - 3);
			if (ext != L".so") continue;
			std::wstring src = AppendSlash(sourcePath) + name;
			std::wstring dst = pluginDir + name;
			if (!CopyFileNarrow(EncodeUtf8(src), EncodeUtf8(dst))) {
				LOG_ERROR("Failed to copy plugin .so to: %s\\n", EncodeUtf8(dst).c_str());
				return false;
			}
			if (firstInstalled.empty()) firstInstalled = dst;
			copied = true;
		}
		if (!copied) {
			LOG_ERROR("Plugin folder contains no .so files: %s\\n", EncodeUtf8(sourcePath).c_str());
			return false;
		}
		if (installedPath != nullptr) *installedPath = firstInstalled;
		LOG_INFO("Installed plugin folder: %s\\n", EncodeUtf8(pluginDir).c_str());
		return true;
	}

	std::wstring fname = sourcePath;
	size_t slash = fname.find_last_of(L"/\\");
	if (slash != std::wstring::npos) fname = fname.substr(slash + 1);
	std::wstring stem = fname;
	size_t dot = stem.find_last_of(L'.');
	if (dot != std::wstring::npos) stem = stem.substr(0, dot);

	std::wstring pluginDir = GetAetherPluginDirectory() + stem + L"/";
	platform::MakeDirectory(pluginDir.c_str());
	std::wstring dst = pluginDir + fname;

	if (!CopyFileNarrow(EncodeUtf8(sourcePath), EncodeUtf8(dst))) {
		LOG_ERROR("Failed to copy plugin .so to: %s\\n", EncodeUtf8(dst).c_str());
		return false;
	}
	if (installedPath != nullptr) *installedPath = dst;
	LOG_INFO("Installed plugin: %s\\n", EncodeUtf8(dst).c_str());
	return true;
}

#endif
