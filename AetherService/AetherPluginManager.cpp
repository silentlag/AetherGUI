#include "stdafx.h"
#include "AetherPluginManager.h"
#include "AetherPluginApi.h"

#define LOG_MODULE "Plugin"
#include "Logger.h"

std::wstring Utf8ToWideService(const std::string& text) {
	if (text.empty())
		return std::wstring();

	int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0);
	if (size <= 0)
		size = MultiByteToWideChar(CP_ACP, 0, text.c_str(), (int)text.size(), nullptr, 0);
	if (size <= 0)
		return std::wstring(text.begin(), text.end());

	std::wstring result(size, L'\0');
	UINT codePage = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), &result[0], size) > 0 ? CP_UTF8 : CP_ACP;
	if (codePage == CP_ACP)
		MultiByteToWideChar(CP_ACP, 0, text.c_str(), (int)text.size(), &result[0], size);
	return result;
}

std::string WideToUtf8Service(const std::wstring& text) {
	if (text.empty())
		return std::string();

	int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0, nullptr, nullptr);
	if (size <= 0)
		return std::string();

	std::string result(size, '\0');
	WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), &result[0], size, nullptr, nullptr);
	return result;
}

std::wstring GetAetherServiceDirectory() {
	wchar_t exePath[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, exePath, MAX_PATH);
	std::wstring dir(exePath);
	size_t slash = dir.find_last_of(L"\\/");
	if (slash != std::wstring::npos)
		dir = dir.substr(0, slash + 1);
	return dir;
}

std::wstring GetAetherPluginDirectory() {
	return GetAetherServiceDirectory() + L"plugins\\";
}

bool EnsureAetherPluginDirectory() {
	std::wstring dir = GetAetherPluginDirectory();
	if (CreateDirectoryW(dir.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS)
		return true;

	LOG_ERROR("Failed to create plugin directory: %ls\n", dir.c_str());
	return false;
}

static std::wstring GetFileNameWithoutExtension(const std::wstring& path) {
	size_t slash = path.find_last_of(L"\\/");
	std::wstring name = (slash == std::wstring::npos) ? path : path.substr(slash + 1);
	size_t dot = name.find_last_of(L'.');
	if (dot != std::wstring::npos)
		name = name.substr(0, dot);
	return name;
}

static std::wstring GetFileName(const std::wstring& path) {
	size_t slash = path.find_last_of(L"\\/");
	return (slash == std::wstring::npos) ? path : path.substr(slash + 1);
}

static std::wstring TrimTrailingSlash(const std::wstring& path) {
	if (path.size() <= 3)
		return path;
	if (path.back() == L'\\' || path.back() == L'/')
		return path.substr(0, path.size() - 1);
	return path;
}

static std::wstring GetDirectoryName(const std::wstring& path) {
	std::wstring trimmed = TrimTrailingSlash(path);
	size_t slash = trimmed.find_last_of(L"\\/");
	return (slash == std::wstring::npos) ? trimmed : trimmed.substr(slash + 1);
}

static std::wstring NormalizeFullPath(const std::wstring& path) {
	wchar_t fullPath[MAX_PATH] = {};
	DWORD length = GetFullPathNameW(path.c_str(), MAX_PATH, fullPath, nullptr);
	if (length > 0 && length < MAX_PATH)
		return TrimTrailingSlash(fullPath);
	return TrimTrailingSlash(path);
}

static bool SamePath(const std::wstring& a, const std::wstring& b) {
	return _wcsicmp(NormalizeFullPath(a).c_str(), NormalizeFullPath(b).c_str()) == 0;
}

static bool ValidateAetherPluginDll(const std::wstring& path) {
	DWORD oldMode = SetErrorMode(SEM_FAILCRITICALERRORS);
	HMODULE module = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR);
	if (!module)
		module = LoadLibraryW(path.c_str());
	DWORD loadError = module ? 0 : GetLastError();
	SetErrorMode(oldMode);

	if (!module) {
		LOG_ERROR("Invalid plugin DLL or architecture/dependency mismatch: %ls (error %lu)\n", path.c_str(), loadError);
		if (loadError == ERROR_BAD_EXE_FORMAT)
			LOG_ERROR("Plugin DLL has the wrong architecture or is not a native DLL. Build it as Release x64.\n");
		else if (loadError == ERROR_MOD_NOT_FOUND)
			LOG_ERROR("Plugin DLL dependency is missing. Rebuild with the correct runtime/toolset or ship dependencies.\n");
		return false;
	}

	auto getInfo = (AetherPluginGetInfoFn)GetProcAddress(module, "AetherPluginGetInfo");
	auto process = (AetherPluginProcessFn)GetProcAddress(module, "AetherPluginProcess");
	if (!getInfo || !process) {
		LOG_ERROR("Plugin DLL missing required Aether exports: %ls\n", path.c_str());
		FreeLibrary(module);
		return false;
	}

	AetherPluginInfo info = {};
	bool ok = getInfo(&info) != 0 && info.apiVersion == AETHER_PLUGIN_API_VERSION;
	if (!ok)
		LOG_ERROR("Plugin DLL API version mismatch: %ls\n", path.c_str());
	FreeLibrary(module);
	return ok;
}

static bool FindFirstDll(const std::wstring& directory, std::wstring* dllPath) {
	std::wstring dir = TrimTrailingSlash(directory);
	std::wstring pattern = dir + L"\\*.dll";
	WIN32_FIND_DATAW data = {};
	HANDLE find = FindFirstFileW(pattern.c_str(), &data);
	if (find == INVALID_HANDLE_VALUE)
		return false;

	do {
		if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			if (dllPath != NULL)
				*dllPath = dir + L"\\" + data.cFileName;
			FindClose(find);
			return true;
		}
	} while (FindNextFileW(find, &data));

	FindClose(find);
	return false;
}

static bool CopyDllsFromDirectory(const std::wstring& sourceDir, const std::wstring& destinationDir, std::wstring* firstInstalledPath) {
	std::wstring source = TrimTrailingSlash(sourceDir);
	std::wstring pattern = source + L"\\*.dll";
	WIN32_FIND_DATAW data = {};
	HANDLE find = FindFirstFileW(pattern.c_str(), &data);
	if (find == INVALID_HANDLE_VALUE)
		return false;

	bool copied = false;
	do {
		if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			continue;

		std::wstring src = source + L"\\" + data.cFileName;
		if (!ValidateAetherPluginDll(src))
			continue;
		std::wstring dst = destinationDir + data.cFileName;
		if (SamePath(src, dst)) {
			if (firstInstalledPath != NULL && firstInstalledPath->empty())
				*firstInstalledPath = dst;
			copied = true;
			continue;
		}

		if (!CopyFileW(src.c_str(), dst.c_str(), FALSE)) {
			LOG_ERROR("Failed to copy plugin DLL to: %ls\n", dst.c_str());
			FindClose(find);
			return false;
		}

		if (firstInstalledPath != NULL && firstInstalledPath->empty())
			*firstInstalledPath = dst;
		copied = true;
	} while (FindNextFileW(find, &data));

	FindClose(find);
	return copied;
}

bool InstallAetherPluginDll(const std::wstring& sourcePath, std::wstring* installedPath) {
	if (!EnsureAetherPluginDirectory())
		return false;

	DWORD attrs = GetFileAttributesW(sourcePath.c_str());
	if (attrs == INVALID_FILE_ATTRIBUTES) {
		LOG_ERROR("Plugin DLL not found: %ls\n", sourcePath.c_str());
		return false;
	}

	if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
		std::wstring firstDll;
		if (!FindFirstDll(sourcePath, &firstDll)) {
			LOG_ERROR("Plugin folder contains no DLL: %ls\n", sourcePath.c_str());
			return false;
		}

		std::wstring pluginName = GetDirectoryName(sourcePath);
		std::wstring pluginDir = GetAetherPluginDirectory() + pluginName + L"\\";
		if (!CreateDirectoryW(pluginDir.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
			LOG_ERROR("Failed to create plugin folder: %ls\n", pluginDir.c_str());
			return false;
		}

		if (SamePath(sourcePath, pluginDir)) {
			if (installedPath != NULL)
				*installedPath = firstDll;
			LOG_INFO("Plugin folder is already installed: %ls\n", pluginDir.c_str());
			return true;
		}

		std::wstring installed;
		if (!CopyDllsFromDirectory(sourcePath, pluginDir, &installed)) {
			LOG_ERROR("Failed to install plugin folder: %ls\n", sourcePath.c_str());
			return false;
		}

		if (installedPath != NULL)
			*installedPath = installed;
		LOG_INFO("Installed plugin folder: %ls\n", pluginDir.c_str());
		return true;
	}

	if (!ValidateAetherPluginDll(sourcePath))
		return false;

	std::wstring pluginName = GetFileNameWithoutExtension(sourcePath);
	std::wstring pluginDir = GetAetherPluginDirectory() + pluginName + L"\\";
	if (!CreateDirectoryW(pluginDir.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
		LOG_ERROR("Failed to create plugin folder: %ls\n", pluginDir.c_str());
		return false;
	}

	std::wstring destination = pluginDir + GetFileName(sourcePath);
	if (SamePath(sourcePath, destination)) {
		if (installedPath != NULL)
			*installedPath = destination;
		LOG_INFO("Plugin DLL is already installed: %ls\n", destination.c_str());
		return true;
	}

	if (!CopyFileW(sourcePath.c_str(), destination.c_str(), FALSE)) {
		DWORD err = GetLastError();
		if (err == ERROR_SHARING_VIOLATION || err == ERROR_ACCESS_DENIED) {
			std::wstring oldPath = pluginDir + GetFileNameWithoutExtension(sourcePath) + L".old.dll";
			DeleteFileW(oldPath.c_str());
			MoveFileExW(destination.c_str(), oldPath.c_str(), MOVEFILE_REPLACE_EXISTING);
			if (!CopyFileW(sourcePath.c_str(), destination.c_str(), FALSE)) {
				LOG_ERROR("Failed to replace locked plugin DLL: %ls (error %lu)\n", destination.c_str(), GetLastError());
				return false;
			}
		}
		else {
			LOG_ERROR("Failed to copy plugin DLL to: %ls (error %lu)\n", destination.c_str(), err);
			return false;
		}
	}

	if (installedPath != NULL)
		*installedPath = destination;

	LOG_INFO("Installed plugin DLL: %ls\n", destination.c_str());
	return true;
}
