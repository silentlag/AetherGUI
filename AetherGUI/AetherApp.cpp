#include "AetherApp.h"
#include "Version.h"

#include <SetupAPI.h>
#include <hidsdi.h>
#include <winhttp.h>
#include <shobjidl.h>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "winhttp.lib")

static bool FileExists(const std::string& path) {
	DWORD attrs = GetFileAttributesA(path.c_str());
	return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

static std::string WideToUtf8(const std::wstring& text) {
	if (text.empty())
		return std::string();

	int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (size <= 1)
		return std::string();

	std::string result(size, '\0');
	WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, &result[0], size, nullptr, nullptr);
	if (!result.empty() && result.back() == '\0')
		result.pop_back();
	return result;
}

static std::wstring Ellipsize(const std::wstring& text, size_t maxChars) {
	if (text.length() <= maxChars)
		return text;
	if (maxChars <= 3)
		return text.substr(0, maxChars);
	return text.substr(0, maxChars - 3) + L"...";
}

static std::wstring Utf8ToWide(const std::string& text) {
	if (text.empty())
		return std::wstring();

	int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0);
	if (size <= 0)
		return std::wstring();

	std::wstring result(size, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), &result[0], size);
	return result;
}

static std::wstring LowerWide(std::wstring text) {
	if (!text.empty())
		CharLowerBuffW(&text[0], (DWORD)text.size());
	return text;
}

static bool WideContainsNoCase(const std::wstring& text, const wchar_t* needle) {
	if (needle == nullptr || needle[0] == 0)
		return false;
	return LowerWide(text).find(LowerWide(std::wstring(needle))) != std::wstring::npos;
}

static bool IsDefaultPluginRepoOwner(const std::wstring& owner) {
	return owner.empty() || LowerWide(owner) == L"opentabletdriver";
}

static bool StringEndsWithNoCase(const std::string& text, const char* suffix) {
	size_t suffixLen = strlen(suffix);
	if (text.size() < suffixLen)
		return false;
	std::string tail = text.substr(text.size() - suffixLen);
	std::transform(tail.begin(), tail.end(), tail.begin(), [](unsigned char ch) { return (char)std::tolower(ch); });
	std::string lowSuffix = suffix;
	std::transform(lowSuffix.begin(), lowSuffix.end(), lowSuffix.begin(), [](unsigned char ch) { return (char)std::tolower(ch); });
	return tail == lowSuffix;
}

static std::wstring JoinPathForDisplay(const std::wstring& left, const std::wstring& right) {
	return left.empty() ? right : (left + L"\\" + right);
}

static bool FileExistsWide(const std::wstring& path) {
	DWORD attrs = GetFileAttributesW(path.c_str());
	return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

static bool DirectoryExistsWide(const std::wstring& path) {
	DWORD attrs = GetFileAttributesW(path.c_str());
	return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

static uint64_t Fnv1aAppend(uint64_t hash, const void* data, size_t size) {
	const unsigned char* bytes = static_cast<const unsigned char*>(data);
	for (size_t i = 0; i < size; ++i) {
		hash ^= bytes[i];
		hash *= 1099511628211ull;
	}
	return hash;
}

static std::wstring HashFileWide(const std::wstring& path) {
	std::ifstream in(path, std::ios::binary);
	if (!in.good())
		return L"";

	uint64_t hash = 14695981039346656037ull;
	char buffer[8192];
	while (in.good()) {
		in.read(buffer, sizeof(buffer));
		std::streamsize read = in.gcount();
		if (read > 0)
			hash = Fnv1aAppend(hash, buffer, (size_t)read);
	}

	wchar_t text[32];
	swprintf_s(text, L"%016llX", (unsigned long long)hash);
	return text;
}

static std::wstring QuoteArg(const std::wstring& arg) {
	return L"\"" + arg + L"\"";
}

static std::wstring SanitizePathSegment(std::wstring text) {
	if (text.empty())
		return L"Plugin";
	for (wchar_t& ch : text) {
		if (ch == L'<' || ch == L'>' || ch == L':' || ch == L'"' || ch == L'/' || ch == L'\\' || ch == L'|' || ch == L'?' || ch == L'*')
			ch = L'_';
	}
	return text;
}

static bool IsAsciiAlphaNum(wchar_t ch) {
	return (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') || (ch >= L'0' && ch <= L'9');
}

static wchar_t LowerAscii(wchar_t ch) {
	return (ch >= L'A' && ch <= L'Z') ? (wchar_t)(ch - L'A' + L'a') : ch;
}

static std::wstring CatalogToken(const std::wstring& text) {
	std::wstring result;
	for (wchar_t ch : text) {
		if (IsAsciiAlphaNum(ch))
			result.push_back(LowerAscii(ch));
	}
	return result;
}

static std::wstring NormalizePluginIdentity(const std::wstring& text) {
	std::wstring token = CatalogToken(text);
	const std::wstring suffixes[] = { L"aether", L"native", L"plugin", L"filter", L"filters", L"port", L"otd" };
	bool changed = true;
	while (changed && !token.empty()) {
		changed = false;
		for (const std::wstring& suffix : suffixes) {
			if (token.length() > suffix.length() + 3 && token.rfind(suffix) == token.length() - suffix.length()) {
				token.resize(token.length() - suffix.length());
				changed = true;
			}
		}
	}
	return token;
}

static std::vector<std::wstring> CatalogWords(const std::wstring& text) {
	std::vector<std::wstring> words;
	std::wstring word;
	wchar_t prev = 0;
	for (wchar_t raw : text) {
		if (!IsAsciiAlphaNum(raw)) {
			if (!word.empty()) {
				words.push_back(word);
				word.clear();
			}
			prev = 0;
			continue;
		}

		bool newCamelWord = prev >= L'a' && prev <= L'z' && raw >= L'A' && raw <= L'Z';
		if (newCamelWord && !word.empty()) {
			words.push_back(word);
			word.clear();
		}
		word.push_back(LowerAscii(raw));
		prev = raw;
	}
	if (!word.empty())
		words.push_back(word);
	return words;
}

static bool IsCatalogStopWord(const std::wstring& word) {
	return word == L"otd" || word == L"aether" || word == L"plugin" ||
		word == L"plugins" || word == L"filter" || word == L"filters" ||
		word == L"native" || word == L"driver" || word == L"smoothing";
}

static bool CatalogNamesMatch(const std::wstring& left, const std::wstring& right) {
	std::wstring leftToken = CatalogToken(left);
	std::wstring rightToken = CatalogToken(right);
	if (leftToken.empty() || rightToken.empty())
		return false;
	if (leftToken == rightToken || leftToken.find(rightToken) != std::wstring::npos || rightToken.find(leftToken) != std::wstring::npos)
		return true;

	std::wstring leftIdentity = NormalizePluginIdentity(left);
	std::wstring rightIdentity = NormalizePluginIdentity(right);
	if (!leftIdentity.empty() && !rightIdentity.empty() &&
		(leftIdentity == rightIdentity || leftIdentity.find(rightIdentity) != std::wstring::npos || rightIdentity.find(leftIdentity) != std::wstring::npos))
		return true;

	std::vector<std::wstring> leftWords = CatalogWords(left);
	std::vector<std::wstring> rightWords = CatalogWords(right);
	for (const std::wstring& lw : leftWords) {
		if (lw.length() < 5 || IsCatalogStopWord(lw))
			continue;
		for (const std::wstring& rw : rightWords) {
			if (rw.length() < 5 || IsCatalogStopWord(rw))
				continue;
			if (lw == rw)
				return true;
		}
	}
	return false;
}

static std::string QuoteCommandArg(const std::string& arg) {
	std::string quoted = "\"";
	for (char ch : arg) {
		if (ch == '"')
			quoted += "\\\"";
		else
			quoted += ch;
	}
	quoted += "\"";
	return quoted;
}

static bool RunHiddenProcess(std::wstring commandLine, const std::wstring& workingDir, DWORD& exitCode) {
	exitCode = 1;
	std::vector<wchar_t> cmd(commandLine.begin(), commandLine.end());
	cmd.push_back(0);

	STARTUPINFOW si = {};
	PROCESS_INFORMATION pi = {};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;

	BOOL started = CreateProcessW(
		nullptr,
		cmd.data(),
		nullptr,
		nullptr,
		FALSE,
		CREATE_NO_WINDOW,
		nullptr,
		workingDir.empty() ? nullptr : workingDir.c_str(),
		&si,
		&pi);

	if (!started)
		return false;

	WaitForSingleObject(pi.hProcess, INFINITE);
	GetExitCodeProcess(pi.hProcess, &exitCode);
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	return true;
}

static std::wstring FindMSBuildPath() {
	const wchar_t* paths[] = {
		L"C:\\Program Files\\Microsoft Visual Studio\\18\\Community\\MSBuild\\Current\\Bin\\MSBuild.exe",
		L"C:\\Program Files\\Microsoft Visual Studio\\18\\BuildTools\\MSBuild\\Current\\Bin\\MSBuild.exe",
		L"C:\\Program Files\\Microsoft Visual Studio\\18\\Professional\\MSBuild\\Current\\Bin\\MSBuild.exe",
		L"C:\\Program Files\\Microsoft Visual Studio\\18\\Enterprise\\MSBuild\\Current\\Bin\\MSBuild.exe",
		L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\MSBuild\\Current\\Bin\\MSBuild.exe",
		L"C:\\Program Files\\Microsoft Visual Studio\\2022\\BuildTools\\MSBuild\\Current\\Bin\\MSBuild.exe",
		L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\MSBuild\\Current\\Bin\\MSBuild.exe",
		L"C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\MSBuild\\Current\\Bin\\MSBuild.exe"
	};
	for (const wchar_t* path : paths) {
		if (FileExistsWide(path))
			return path;
	}
	return L"MSBuild.exe";
}

static std::wstring FindFirstSourceFile(const std::wstring& root, const std::vector<std::wstring>& extensions) {
	namespace fs = std::filesystem;
	std::error_code ec;
	fs::path rootPath(root);
	if (!fs::exists(rootPath, ec))
		return L"";

	for (const auto& ext : extensions) {
		for (const auto& entry : fs::directory_iterator(rootPath, fs::directory_options::skip_permission_denied, ec)) {
			if (ec)
				break;
			if (entry.is_regular_file(ec) && LowerWide(entry.path().extension().wstring()) == ext)
				return entry.path().wstring();
		}
	}

	for (const auto& ext : extensions) {
		for (const auto& entry : fs::recursive_directory_iterator(rootPath, fs::directory_options::skip_permission_denied, ec)) {
			if (ec)
				break;
			if (!entry.is_regular_file(ec))
				continue;
			if (LowerWide(entry.path().extension().wstring()) == ext)
				return entry.path().wstring();
		}
	}

	return L"";
}

static std::wstring FindBuildScript(const std::wstring& root) {
	namespace fs = std::filesystem;
	std::error_code ec;
	const wchar_t* names[] = { L"build_aether_plugin.ps1", L"build_tabletdriverfilters_plugins.ps1", L"build.ps1", L"Build.ps1" };
	for (const wchar_t* name : names) {
		fs::path candidate = fs::path(root) / name;
		if (fs::exists(candidate, ec) && fs::is_regular_file(candidate, ec))
			return candidate.wstring();
	}
	for (const auto& entry : fs::recursive_directory_iterator(fs::path(root), fs::directory_options::skip_permission_denied, ec)) {
		if (ec)
			break;
		if (!entry.is_regular_file(ec))
			continue;
		std::wstring fileName = LowerWide(entry.path().filename().wstring());
		if (fileName == L"build_aether_plugin.ps1" || fileName == L"build_tabletdriverfilters_plugins.ps1")
			return entry.path().wstring();
	}
	return L"";
}

static bool DllHasAetherExports(const std::wstring& path) {
	HMODULE module = LoadLibraryExW(path.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES);
	if (!module)
		return false;
	bool hasExports = GetProcAddress(module, "AetherPluginGetInfo") != nullptr &&
		GetProcAddress(module, "AetherPluginProcess") != nullptr;
	FreeLibrary(module);
	return hasExports;
}

struct AetherGuiPluginInfo {
	int apiVersion;
	const char* name;
	const char* description;
};

struct AetherGuiPluginOptionInfo {
	int apiVersion;
	const char* key;
	const char* label;
	int type;
	double minValue;
	double maxValue;
	double defaultValue;
	const char* format;
	const char* description;
};

struct PluginOptionMetadata {
	int type = 0;
	std::string key;
	std::wstring label;
	float minValue = 0.0f;
	float maxValue = 1.0f;
	float defaultValue = 0.0f;
	std::wstring format = L"%.2f";
};

typedef int(__cdecl* AetherGuiPluginGetInfoFn)(AetherGuiPluginInfo* info);
typedef int(__cdecl* AetherGuiPluginGetOptionCountFn)();
typedef int(__cdecl* AetherGuiPluginGetOptionInfoFn)(int index, AetherGuiPluginOptionInfo* info);

static bool ReadAetherPluginMetadata(const std::wstring& path, std::wstring& name, std::wstring& description, std::vector<PluginOptionMetadata>& options) {
	name.clear();
	description.clear();
	options.clear();

	HMODULE module = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR);
	if (!module)
		module = LoadLibraryW(path.c_str());
	if (!module)
		return false;

	if (auto getInfo = (AetherGuiPluginGetInfoFn)GetProcAddress(module, "AetherPluginGetInfo")) {
		AetherGuiPluginInfo info = {};
		if (getInfo(&info)) {
			if (info.name && info.name[0])
				name = Utf8ToWide(info.name);
			if (info.description && info.description[0])
				description = Utf8ToWide(info.description);
		}
	}

	auto getOptionCount = (AetherGuiPluginGetOptionCountFn)GetProcAddress(module, "AetherPluginGetOptionCount");
	auto getOptionInfo = (AetherGuiPluginGetOptionInfoFn)GetProcAddress(module, "AetherPluginGetOptionInfo");
	if (getOptionCount && getOptionInfo) {
		int count = getOptionCount();
		if (count < 0)
			count = 0;
		if (count > 64)
			count = 64;

		for (int i = 0; i < count; ++i) {
			AetherGuiPluginOptionInfo info = {};
			if (!getOptionInfo(i, &info) || !info.key || !info.key[0])
				continue;

			PluginOptionMetadata option;
			option.type = info.type;
			option.key = info.key;
			option.label = Utf8ToWide(info.label && info.label[0] ? info.label : info.key);
			option.minValue = (float)info.minValue;
			option.maxValue = (float)info.maxValue;
			option.defaultValue = (float)info.defaultValue;
			if (option.maxValue < option.minValue)
				std::swap(option.minValue, option.maxValue);
			if (info.format && info.format[0])
				option.format = Utf8ToWide(info.format);
			options.push_back(option);
		}
	}

	FreeLibrary(module);
	return !name.empty() || !description.empty() || !options.empty();
}

static std::wstring FindBuiltAetherDll(const std::wstring& root) {
	namespace fs = std::filesystem;
	std::error_code ec;
	std::wstring newestFallback;
	fs::file_time_type newestTime = fs::file_time_type::min();

	for (const auto& entry : fs::recursive_directory_iterator(fs::path(root), fs::directory_options::skip_permission_denied, ec)) {
		if (ec)
			break;
		if (!entry.is_regular_file(ec))
			continue;
		if (LowerWide(entry.path().extension().wstring()) != L".dll")
			continue;

		std::wstring path = entry.path().wstring();
		if (DllHasAetherExports(path))
			return path;

		fs::file_time_type writeTime = entry.last_write_time(ec);
		if (!ec && writeTime > newestTime) {
			newestTime = writeTime;
			newestFallback = path;
		}
	}

	return newestFallback;
}

static bool BuildAetherPluginSourceFolder(const std::wstring& folder, std::wstring& dllPath, std::wstring& status) {
	std::wstring previousDllPath;
	std::wstring previousDllHash;
	if (DirectoryExistsWide(folder)) {
		previousDllPath = FindBuiltAetherDll(folder);
		if (!previousDllPath.empty())
			previousDllHash = HashFileWide(previousDllPath);
	}

	if (!DirectoryExistsWide(folder)) {
		status = L"Source folder does not exist";
		return false;
	}

	std::wstring script = FindBuildScript(folder);
	std::wstring commandLine;
	std::wstring buildKind;
	if (!script.empty()) {
		commandLine = L"powershell.exe -ExecutionPolicy Bypass -File " + QuoteArg(script);
		buildKind = L"PowerShell build script";
	}
	else {
		std::wstring solution = FindFirstSourceFile(folder, { L".sln" });
		if (!solution.empty()) {
			commandLine = QuoteArg(FindMSBuildPath()) + L" " + QuoteArg(solution) + L" /p:Configuration=Release /p:Platform=x64";
			buildKind = L"Visual Studio solution";
		}
		else {
			std::wstring vcxproj = FindFirstSourceFile(folder, { L".vcxproj" });
			if (!vcxproj.empty()) {
				commandLine = QuoteArg(FindMSBuildPath()) + L" " + QuoteArg(vcxproj) + L" /p:Configuration=Release /p:Platform=x64";
				buildKind = L"C++ project";
			}
			else {
				std::wstring csproj = FindFirstSourceFile(folder, { L".csproj" });
				if (!csproj.empty()) {
					commandLine = L"dotnet build " + QuoteArg(csproj) + L" -c Release";
					buildKind = L".NET project";
				}
			}
		}
	}

	if (commandLine.empty()) {
		status = L"No build script, solution, vcxproj or csproj found";
		return false;
	}

	DWORD exitCode = 1;
	if (!RunHiddenProcess(commandLine, folder, exitCode)) {
		status = L"Failed to start " + buildKind;
		return false;
	}
	if (exitCode != 0) {
		wchar_t buffer[96];
		swprintf_s(buffer, L"Build failed with exit code %lu", exitCode);
		status = buffer;
		return false;
	}

	dllPath = FindBuiltAetherDll(folder);
	if (dllPath.empty()) {
		status = L"Build completed, but no DLL was produced";
		return false;
	}
	if (dllPath == previousDllPath && !previousDllHash.empty() && HashFileWide(dllPath) == previousDllHash) {
		status = L"Build finished, but output DLL did not change. Check if the source folder is included in the project/build script.";
		return false;
	}
	if (!DllHasAetherExports(dllPath)) {
		status = L"Built DLL is not an Aether native plugin";
		return false;
	}

	status = L"Built and installed " + Ellipsize(std::filesystem::path(dllPath).filename().wstring(), 42);
	return true;
}

static std::map<std::wstring, std::string> g_httpUtf8Cache;
static std::mutex g_httpUtf8CacheMutex;

static bool HttpGetUtf8(const wchar_t* host, const std::wstring& path, std::string& body) {
	body.clear();
	std::wstring cacheKey = std::wstring(host ? host : L"") + L"|" + path;
	{
		std::lock_guard<std::mutex> lock(g_httpUtf8CacheMutex);
		auto cached = g_httpUtf8Cache.find(cacheKey);
		if (cached != g_httpUtf8Cache.end()) {
			body = cached->second;
			return !body.empty();
		}
	}
	HINTERNET session = WinHttpOpen(
		L"AetherGUI/" AETHERGUI_VERSION_W,
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS,
		0);
	if (!session)
		return false;

	bool ok = false;
	HINTERNET connect = WinHttpConnect(session, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
	if (connect) {
		HINTERNET request = WinHttpOpenRequest(
			connect,
			L"GET",
			path.c_str(),
			nullptr,
			WINHTTP_NO_REFERER,
			WINHTTP_DEFAULT_ACCEPT_TYPES,
			WINHTTP_FLAG_SECURE);

		if (request) {
			LPCWSTR headers =
				L"User-Agent: AetherGUI/" AETHERGUI_VERSION_W L"\r\n"
				L"Accept: application/vnd.github+json\r\n"
				L"X-GitHub-Api-Version: 2022-11-28\r\n";

			if (WinHttpSendRequest(request, headers, (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
				WinHttpReceiveResponse(request, nullptr)) {

				DWORD status = 0;
				DWORD statusSize = sizeof(status);
				WinHttpQueryHeaders(
					request,
					WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
					WINHTTP_HEADER_NAME_BY_INDEX,
					&status,
					&statusSize,
					WINHTTP_NO_HEADER_INDEX);

				if (status == 200) {
					DWORD available = 0;
					while (WinHttpQueryDataAvailable(request, &available) && available > 0) {
						std::string chunk(available, '\0');
						DWORD read = 0;
						if (!WinHttpReadData(request, &chunk[0], available, &read) || read == 0)
							break;
						chunk.resize(read);
						body += chunk;
					}
					ok = !body.empty();
				}
			}
			WinHttpCloseHandle(request);
		}
		WinHttpCloseHandle(connect);
	}

	WinHttpCloseHandle(session);
	if (ok) {
		std::lock_guard<std::mutex> lock(g_httpUtf8CacheMutex);
		if (g_httpUtf8Cache.size() > 256)
			g_httpUtf8Cache.clear();
		g_httpUtf8Cache[cacheKey] = body;
	}
	return ok;
}

static std::vector<std::string> JsonTreePaths(const std::string& json, bool jsonOnly = true) {
	std::vector<std::string> paths;
	std::string marker = "\"path\"";
	size_t pos = 0;
	while ((pos = json.find(marker, pos)) != std::string::npos) {
		pos = json.find(':', pos + marker.size());
		if (pos == std::string::npos) break;
		pos = json.find('"', pos + 1);
		if (pos == std::string::npos) break;
		std::string path;
		bool escaping = false;
		for (size_t i = pos + 1; i < json.size(); i++) {
			char ch = json[i];
			if (escaping) {
				path.push_back(ch);
				escaping = false;
			}
			else if (ch == '\\') {
				escaping = true;
			}
			else if (ch == '"') {
				pos = i + 1;
				break;
			}
			else {
				path.push_back(ch);
			}
		}

		if (!jsonOnly || StringEndsWithNoCase(path, ".json"))
			paths.push_back(path);
	}
	return paths;
}

static std::string JsonStringValue(const std::string& json, const char* key) {
	std::string pattern = "\"";
	pattern += key;
	pattern += "\"";

	size_t pos = json.find(pattern);
	if (pos == std::string::npos)
		return std::string();

	pos = json.find(':', pos + pattern.size());
	if (pos == std::string::npos)
		return std::string();

	pos = json.find('"', pos + 1);
	if (pos == std::string::npos)
		return std::string();

	std::string result;
	bool escaping = false;
	for (size_t i = pos + 1; i < json.size(); i++) {
		char ch = json[i];
		if (escaping) {
			switch (ch) {
			case '"': result.push_back('"'); break;
			case '\\': result.push_back('\\'); break;
			case '/': result.push_back('/'); break;
			case 'b': result.push_back('\b'); break;
			case 'f': result.push_back('\f'); break;
			case 'n': result.push_back('\n'); break;
			case 'r': result.push_back('\r'); break;
			case 't': result.push_back('\t'); break;
			default: result.push_back(ch); break;
			}
			escaping = false;
			continue;
		}
		if (ch == '\\') {
			escaping = true;
			continue;
		}
		if (ch == '"')
			break;
		result.push_back(ch);
	}
	return result;
}

static std::vector<int> ParseVersionNumbers(const std::string& version) {
	std::vector<int> numbers;
	int current = -1;
	for (char ch : version) {
		if (ch >= '0' && ch <= '9') {
			if (current < 0)
				current = 0;
			current = current * 10 + (ch - '0');
		}
		else if (current >= 0) {
			numbers.push_back(current);
			current = -1;
		}
	}
	if (current >= 0)
		numbers.push_back(current);
	return numbers;
}

static std::string NormalizeVersionString(std::string version) {
	std::transform(version.begin(), version.end(), version.begin(), [](unsigned char ch) { return (char)std::tolower(ch); });

	// GitHub tags are often "v1.0.1" or "AetherGUI-v1.0.1" while the local
	// version is usually "1.0.1". Compare only the numeric semantic part first.
	size_t firstDigit = std::string::npos;
	for (size_t i = 0; i < version.size(); ++i) {
		if (version[i] >= '0' && version[i] <= '9') {
			firstDigit = i;
			break;
		}
	}
	if (firstDigit == std::string::npos)
		return version;

	std::string result;
	for (size_t i = firstDigit; i < version.size(); ++i) {
		char ch = version[i];
		if ((ch >= '0' && ch <= '9') || ch == '.')
			result.push_back(ch);
		else
			break;
	}
	while (!result.empty() && result.back() == '.')
		result.pop_back();
	return result;
}

static bool IsVersionNewer(const std::string& latest, const std::string& current) {
	std::string latestNormalized = NormalizeVersionString(latest);
	std::string currentNormalized = NormalizeVersionString(current);
	if (!latestNormalized.empty() && latestNormalized == currentNormalized)
		return false;

	std::vector<int> latestNumbers = ParseVersionNumbers(latestNormalized.empty() ? latest : latestNormalized);
	std::vector<int> currentNumbers = ParseVersionNumbers(currentNormalized.empty() ? current : currentNormalized);
	if (latestNumbers.empty() || currentNumbers.empty())
		return false;

	size_t count = std::max(latestNumbers.size(), currentNumbers.size());
	for (size_t i = 0; i < count; i++) {
		int a = i < latestNumbers.size() ? latestNumbers[i] : 0;
		int b = i < currentNumbers.size() ? currentNumbers[i] : 0;
		if (a > b) return true;
		if (a < b) return false;
	}
	return false;
}

struct ReleaseInfo {
	std::string tag;
	std::string htmlUrl;
};

static bool FetchLatestRelease(ReleaseInfo& info) {
	bool ok = false;
	HINTERNET session = WinHttpOpen(
		L"AetherGUI/" AETHERGUI_VERSION_W,
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS,
		0);
	if (!session)
		return false;

	HINTERNET connect = WinHttpConnect(session, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
	if (connect) {
		HINTERNET request = WinHttpOpenRequest(
			connect,
			L"GET",
			AETHERGUI_GITHUB_LATEST_RELEASE_API_PATH,
			nullptr,
			WINHTTP_NO_REFERER,
			WINHTTP_DEFAULT_ACCEPT_TYPES,
			WINHTTP_FLAG_SECURE);

		if (request) {
			LPCWSTR headers =
				L"User-Agent: AetherGUI/" AETHERGUI_VERSION_W L"\r\n"
				L"Accept: application/vnd.github+json\r\n"
				L"X-GitHub-Api-Version: 2022-11-28\r\n";

			if (WinHttpSendRequest(request, headers, (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
				WinHttpReceiveResponse(request, nullptr)) {

				DWORD status = 0;
				DWORD statusSize = sizeof(status);
				WinHttpQueryHeaders(
					request,
					WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
					WINHTTP_HEADER_NAME_BY_INDEX,
					&status,
					&statusSize,
					WINHTTP_NO_HEADER_INDEX);

				if (status == 200) {
					std::string body;
					DWORD available = 0;
					while (WinHttpQueryDataAvailable(request, &available) && available > 0) {
						std::string chunk(available, '\0');
						DWORD read = 0;
						if (!WinHttpReadData(request, &chunk[0], available, &read) || read == 0)
							break;
						chunk.resize(read);
						body += chunk;
					}

					info.tag = JsonStringValue(body, "tag_name");
					info.htmlUrl = JsonStringValue(body, "html_url");
					ok = !info.tag.empty();
				}
			}
			WinHttpCloseHandle(request);
		}
		WinHttpCloseHandle(connect);
	}

	WinHttpCloseHandle(session);
	return ok;
}

static void StartUpdateCheck(HWND hwnd) {
	std::thread([hwnd]() {
		Sleep(1500);

		ReleaseInfo latest;
		if (!FetchLatestRelease(latest))
			return;
		if (!IsVersionNewer(latest.tag, AETHERGUI_VERSION))
			return;
		if (!IsWindow(hwnd))
			return;

		PendingUpdateInfo* info = new PendingUpdateInfo();
		info->latestTag = Utf8ToWide(latest.tag);
		info->currentVersion = AETHERGUI_VERSION_W;
		info->releaseUrl = latest.htmlUrl.empty()
			? std::wstring(AETHERGUI_GITHUB_RELEASES_URL)
			: Utf8ToWide(latest.htmlUrl);

		if (!PostMessageW(hwnd, WM_AETHER_UPDATE_AVAILABLE, 0, reinterpret_cast<LPARAM>(info))) {
			delete info;
		}
	}).detach();
}

struct MonitorEnumContext {
	std::vector<AetherApp::DisplayTarget>* targets = nullptr;
	int virtualLeft = 0;
	int virtualTop = 0;
	int index = 1;
};

static BOOL CALLBACK EnumDisplayTargetProc(HMONITOR monitor, HDC, LPRECT, LPARAM data) {
	MonitorEnumContext* ctx = reinterpret_cast<MonitorEnumContext*>(data);
	MONITORINFOEXW mi = {};
	mi.cbSize = sizeof(mi);
	if (!GetMonitorInfoW(monitor, &mi))
		return TRUE;

	AetherApp::DisplayTarget target;
	target.x = (float)(mi.rcMonitor.left - ctx->virtualLeft);
	target.y = (float)(mi.rcMonitor.top - ctx->virtualTop);
	target.width = (float)(mi.rcMonitor.right - mi.rcMonitor.left);
	target.height = (float)(mi.rcMonitor.bottom - mi.rcMonitor.top);

	wchar_t label[96];
	swprintf_s(label, L"Monitor %d  %.0fx%.0f  X%+.0f Y%+.0f",
		ctx->index, target.width, target.height, target.x, target.y);
	target.label = label;

	ctx->targets->push_back(target);
	ctx->index++;
	return TRUE;
}

bool AetherApp::Initialize(HWND hwnd) {
	hWnd = hwnd;
	if (!renderer.Initialize(hwnd)) return false;

	RECT rc;
	GetClientRect(hwnd, &rc);
	OnResize((UINT)(rc.right - rc.left), (UINT)(rc.bottom - rc.top));

	lastFrameTime = std::chrono::high_resolution_clock::now();

	sidebar.x = 0;
	sidebar.y = Theme::Size::HeaderHeight;
	sidebar.AddTab(L"Area", L"\xE774");
	sidebar.AddTab(L"Filters", L"\xE71C");
	sidebar.AddTab(L"Settings", L"\xE713");
	sidebar.AddTab(L"Console", L"\xE756");
	sidebar.AddTab(L"About", L"\xE946");

	char exePath[MAX_PATH];
	GetModuleFileNameA(NULL, exePath, MAX_PATH);
	std::string exeDir(exePath);
	size_t lastSlash = exeDir.find_last_of('\\');
	if (lastSlash != std::string::npos) {
		exeDir = exeDir.substr(0, lastSlash + 1);
	}
	servicePath = exeDir + "AetherService.exe";
	std::string serviceCandidates[] = {
		servicePath,
		exeDir + "..\\..\\x64\\Release\\AetherService.exe",
		exeDir + "..\\..\\AetherService\\x64\\Release\\AetherService.exe",
		exeDir + "..\\..\\..\\x64\\Release\\AetherService.exe",
		exeDir + "..\\..\\..\\AetherService\\x64\\Release\\AetherService.exe",
		exeDir + "..\\x64\\Release\\AetherService.exe"
	};
	for (const auto& candidate : serviceCandidates) {
		if (FileExists(candidate)) {
			servicePath = candidate;
			break;
		}
	}

	startStopBtn.Layout(0, 0, 100, 30, L"Start", true);

	srand((unsigned)GetTickCount());
	for (int i = 0; i < MAX_STARS; i++) stars[i].active = false;
	twinkleStarsInitialized = false;
	starSpawnTimer = 1.0f + (rand() % 300) / 100.0f;

	RefreshDetectedScreen();

	InitControls();
	EnsureConfigDirectory();
	RefreshConfigFiles();
	AutoLoadConfig();
	ApplyDpiScale();
	InitializeSettingsUndo();
	StartUpdateCheck(hWnd);

	
	vmultiCheckDone = true;
	{
		GUID hidGuid;
		HidD_GetHidGuid(&hidGuid);
		HDEVINFO devInfo = SetupDiGetClassDevs(&hidGuid, NULL, 0, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
		vmultiInstalled = false;
		if (devInfo != INVALID_HANDLE_VALUE) {
			SP_DEVICE_INTERFACE_DATA ifData;
			ifData.cbSize = sizeof(ifData);
			for (DWORD idx = 0; SetupDiEnumDeviceInterfaces(devInfo, NULL, &hidGuid, idx, &ifData); idx++) {
				DWORD size = 0;
				SetupDiGetDeviceInterfaceDetailW(devInfo, &ifData, NULL, 0, &size, NULL);
				if (size > 0) {
					auto detail = (PSP_DEVICE_INTERFACE_DETAIL_DATA_W)malloc(size);
					detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
					if (SetupDiGetDeviceInterfaceDetailW(devInfo, &ifData, detail, size, NULL, NULL)) {
						HANDLE h = CreateFileW(detail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
						if (h != INVALID_HANDLE_VALUE) {
							HIDD_ATTRIBUTES attr = {};
							attr.Size = sizeof(attr);
							if (HidD_GetAttributes(h, &attr) && attr.VendorID == 0x00FF && attr.ProductID == 0xBACC) {
								vmultiInstalled = true;
							}
							CloseHandle(h);
						}
					}
					free(detail);
				}
				if (vmultiInstalled) break;
			}
			SetupDiDestroyDeviceInfoList(devInfo);
		}
	}

	autoStartAttempted = true;
	if (vmultiInstalled) {
		StartDriverService();
	}
	return true;
}

void AetherApp::Shutdown() {
	driver.Stop();
	renderer.Shutdown();
}

void AetherApp::InitControls() {
	float cx = Theme::Size::SidebarWidth + Theme::Size::Padding;
	float cw = Theme::Runtime::WindowWidth - Theme::Size::SidebarWidth - Theme::Size::Padding * 2;
	float hw = (cw - Theme::Size::Padding) * 0.5f;

	area.tabletWidth.Layout(cx, 0, hw, L"Tablet Width (mm)", 1, 300, 76.2f, L"Width of the active tablet area in mm");
	area.tabletHeight.Layout(cx, 0, hw, L"Tablet Height (mm)", 1, 200, 47.625f, L"Height of the active tablet area in mm");
	area.tabletX.Layout(cx, 0, hw, L"Center X (mm)", 0.01f, 200, 76.0f, L"Horizontal center position of the area");
	area.tabletY.Layout(cx, 0, hw, L"Center Y (mm)", 0.01f, 200, 47.5f, L"Vertical center position of the area");
	area.screenWidth.Layout(cx + hw + Theme::Size::Padding, 0, hw, L"Screen Width (px)", 100, 7680, detectedScreenW, L"Width of the screen mapping area in pixels");
	area.screenHeight.Layout(cx + hw + Theme::Size::Padding, 0, hw, L"Screen Height (px)", 100, 4320, detectedScreenH, L"Height of the screen mapping area in pixels");
	area.screenX.Layout(cx + hw + Theme::Size::Padding, 0, hw, L"Screen X", 0, 7680, 0, L"Horizontal offset for multi-monitor setups");
	area.screenY.Layout(cx + hw + Theme::Size::Padding, 0, hw, L"Screen Y", 0, 4320, 0, L"Vertical offset for multi-monitor setups");
	area.rotation.Layout(cx, 0, cw, L"Rotation", -180, 180, 0, L"Rotate the tablet area in degrees");
	area.aspectRatio.Layout(cx, 0, hw, L"Aspect Ratio", 0.50f, 3.00f, Clamp(detectedScreenW / detectedScreenH, 0.50f, 3.00f), L"Target tablet area proportion when Aspect Ratio is enabled");
	area.aspectRatio.format = L"%.3f";
	area.customValues.Layout(cx, 0, L"Custom Values", L"Show advanced position controls");
	area.customValues.value = false;
	area.autoCenter.Layout(cx, 0, L"Auto Center", L"Keep the screen area centered when resolution values change");
	area.autoCenter.value = true;
	area.lockAspect.Layout(cx, 0, L"Aspect Ratio", L"Keep tablet area at the selected proportion");

	filters.smoothingEnabled.Layout(cx, 0, L"Smoothing", L"Low-pass filter to reduce cursor jitter");
	filters.smoothingLatency.Layout(cx, 0, hw, L"Latency (ms)", 0, 100, 2, L"Time to reach target position. Higher = smoother but more lag");
	filters.smoothingInterval.Layout(cx + hw + 16, 0, hw, L"Interval (ms)", 1, 16, 2, L"Timer tick interval. Lower = smoother updates");

	filters.antichatterEnabled.Layout(cx, 0, L"Antichatter", L"Adaptive smoothing that increases near stationary positions");
	filters.antichatterStrength.Layout(cx, 0, hw, L"Strength", 1, 10, 3, L"Power curve exponent. Higher = more smoothing at rest");
	filters.antichatterMultiplier.Layout(cx + hw + 16, 0, hw, L"Multiplier", 1, 1000, 1, L"Scale factor for the distance weight");
	filters.antichatterOffsetX.Layout(cx, 0, hw, L"Offset X", -2, 5, 0, L"Distance offset for the weight formula");
	filters.antichatterOffsetY.Layout(cx + hw + 16, 0, hw, L"Offset Y", -1, 10, 0, L"Weight offset floor. Negative = allow zero weight");

	filters.noiseEnabled.Layout(cx, 0, L"Noise Reduction", L"Average recent positions to smooth out sensor noise");
	filters.noiseBuffer.Layout(cx, 0, hw, L"Buffer (packets)", 1, 50, 10, L"Number of recent positions to average. Higher = smoother but more lag");
	filters.noiseBuffer.format = L"%.0f";
	filters.noiseThreshold.Layout(cx + hw + 16, 0, hw, L"Threshold (mm)", 0, 20, 0.5f, L"Ignore movements smaller than this distance");
	filters.noiseIterations.Layout(cx, 0, hw, L"Iterations", 1, 50, 10, L"Number of averaging passes. More = heavier smoothing");
	filters.noiseIterations.format = L"%.0f";

	filters.velCurveEnabled.Layout(cx, 0, L"Prediction", L"Extrapolate cursor position based on velocity to reduce perceived lag");
	filters.velCurveMinSpeed.Layout(cx, 0, hw, L"Offset X", 0, 40, 3, L"Distance offset for the prediction curve");
	filters.velCurveMaxSpeed.Layout(cx + hw + 16, 0, hw, L"Offset Y", 0, 5, 0.3f, L"Base prediction amount added at all speeds");
	filters.velCurveSmoothing.Layout(cx, 0, hw, L"Strength", 0, 10, 1.1f, L"How far ahead to predict. Higher = more compensation");
	filters.velCurveSharpness.Layout(cx + hw + 16, 0, hw, L"Sharpness", 0.1f, 5, 1.0f, L"How quickly prediction ramps up with speed");

	filters.snapEnabled.Layout(cx, 0, L"Position Snap", L"Eliminate micro-jitter when the cursor is nearly stationary");
	filters.snapRadius.Layout(cx, 0, hw, L"Snap Radius (px)", 0.1f, 10, 0.5f, L"Movement below this threshold is suppressed");
	filters.snapSmooth.Layout(cx + hw + 16, 0, hw, L"Smoothness", 0, 1, 0.3f, L"How gradually snapping transitions. 0 = hard snap, 1 = very soft");

	filters.reconstructorEnabled.Layout(cx, 0, L"Reconstructor", L"Compensate for tablet hardware processing delay using velocity prediction. High values at your own risk!");
	filters.reconStrength.Layout(cx, 0, hw, L"Strength", 0, 2, 0.5f, L"How aggressively to compensate latency. 0 = off, 1 = full");
	filters.reconVelSmooth.Layout(cx + hw + 16, 0, hw, L"Vel. Smoothing", 0, 0.99f, 0.6f, L"Smoothing on velocity estimation. Higher = more stable but slower");
	filters.reconAccelCap.Layout(cx, 0, hw, L"Accel Cap", 1, 200, 50, L"Maximum acceleration magnitude to prevent overshoot on direction changes");
	filters.reconPredTime.Layout(cx + hw + 16, 0, hw, L"Pred. Time (ms)", 0.1f, 50, 5, L"How far ahead to extrapolate position in milliseconds");

	filters.adaptiveEnabled.Layout(cx, 0, L"Adaptive Filter", L"Statistical filter that balances prediction and measurement for optimal smoothing");
	filters.adaptiveProcessNoise.Layout(cx, 0, hw, L"Process Noise (Q)", 0.001f, 10, 0.02f, L"Expected movement variance. Higher = trusts measurements more (less smooth)");
	filters.adaptiveMeasNoise.Layout(cx + hw + 16, 0, hw, L"Meas. Noise (R)", 0.001f, 50, 0.5f, L"Expected sensor noise. Higher = trusts prediction more (smoother)");
	filters.adaptiveVelWeight.Layout(cx, 0, hw, L"Velocity Weight", 0, 5, 1, L"How much velocity influences the prediction model");

	outputMode.AddOption(L"Absolute");
	outputMode.AddOption(L"Relative");
	outputMode.AddOption(L"Windows Ink");
	outputMode.selected = 0;

	dpiScale.Layout(cx, 0, cw, L"DPI Scale (%)", 75.0f, 200.0f, Clamp(GetSystemDpiScale() * 100.0f, 75.0f, 200.0f), L"Scale the interface for high-DPI and custom resolution setups");
	dpiScale.format = L"%.0f";

	const wchar_t* btnOpts[] = { L"Disable", L"Mouse 1", L"Mouse 2", L"Mouse 3", L"Mouse 4", L"Mouse 5", L"Mouse Wheel" };
	buttonTip.Layout(cx, 0, hw, L"Pen Tip", L"Action when pen tip touches the tablet");
	buttonBottom.Layout(cx, 0, hw, L"Bottom Button", L"Action for the lower pen barrel button");
	buttonTop.Layout(cx, 0, hw, L"Top Button", L"Action for the upper pen barrel button");
	for (int i = 0; i < 7; i++) {
		buttonTip.AddOption(btnOpts[i]);
		buttonBottom.AddOption(btnOpts[i]);
		buttonTop.AddOption(btnOpts[i]);
	}
	buttonTip.selected = 1;
	buttonBottom.selected = 2;
	buttonTop.selected = 3;

	forceFullArea.Layout(cx, 0, L"Force Full Area", L"Use entire tablet surface");
	areaClipping.Layout(cx, 0, L"Area Clipping", L"Clip cursor to screen area bounds");
	areaClipping.value = true;
	areaLimiting.Layout(cx, 0, L"Area Limiting", L"Block input outside tablet area entirely");
	areaLimiting.value = false;

	tipThreshold.Layout(cx, 0, hw, L"Tip Threshold (%)", 0, 100, 1, L"Pressure needed to register a click. 0 = any touch, higher = harder press");
	tipThreshold.format = L"%.0f";

	overclockEnabled.Layout(cx, 0, L"Overclock", L"Boost driver timer rate for smoother cursor movement");
	overclockHz.Layout(cx, 0, hw, L"Target Rate (Hz)", 125, 2000, 1000, L"Target timer frequency. Higher = smoother. Actual rate depends on USB polling");
	overclockHz.format = L"%.0f";
	penRateLimitEnabled.Layout(cx, 0, L"Pen Rate Limit", L"Cap outgoing pen reports for tablets that feel too smooth at high Hz");
	penRateLimitHz.Layout(cx, 0, hw, L"Limit Rate (Hz)", 30, 1000, 133, L"Output report cap. Example: 133 Hz gives Gaomon a Wacom-like cadence");
	penRateLimitHz.format = L"%.0f";

	saveConfigBtn.Layout(0, 0, 100, 28, L"Save Config", false, L"Save all current settings to disk");
	loadConfigBtn.Layout(0, 0, 100, 28, L"Load Config", false, L"Reload settings from the saved config file");
	installPluginBtn.Layout(0, 0, 100, 28, L"Install DLL", false, L"Install a native Aether plugin DLL");
	installSourcePluginBtn.Layout(0, 0, 120, 28, L"Build Source", false, L"Build an Aether plugin source folder and install the produced DLL");
	reloadPluginBtn.Layout(0, 0, 100, 28, L"Reload", false, L"Reload native Aether plugins");
	listPluginBtn.Layout(0, 0, 100, 28, L"List", false, L"Open plugin manager");
	pluginManagerInstallBtn.Layout(0, 0, 120, 28, L"Install DLL", true, L"Install a native Aether plugin DLL");
	pluginManagerInstallSourceBtn.Layout(0, 0, 120, 28, L"Build Source", false, L"Build an Aether plugin source folder and install the produced DLL");
	pluginManagerRefreshBtn.Layout(0, 0, 120, 28, L"Refresh", false, L"Reload installed plugin filters");
	pluginManagerDeleteBtn.Layout(0, 0, 120, 28, L"Uninstall", false, L"Remove the selected plugin");
	pluginManagerCloseBtn.Layout(0, 0, 120, 28, L"Close", false, L"Close plugin manager");
	pluginManagerSourceBtn.Layout(0, 0, 120, 28, L"Source", false, L"Switch repository source");
	pluginManagerAetherTabBtn.Layout(0, 0, 120, 28, L"Aether Filters", true, L"Installed native Aether filters");
	pluginManagerOtdTabBtn.Layout(0, 0, 120, 28, L"OTD Ports", false, L"GitHub source ports in OTDPlugins");
	pluginManagerSourceCodeBtn.Layout(0, 0, 120, 28, L"Source Code", false, L"Open selected plugin source code");
	pluginManagerWikiBtn.Layout(0, 0, 120, 28, L"Wiki", false, L"Open selected plugin wiki");
	pluginManagerApplySourceBtn.Layout(0, 0, 120, 28, L"Apply", true, L"Apply repository source");
	pluginManagerCancelSourceBtn.Layout(0, 0, 120, 28, L"Cancel", false, L"Cancel source changes");
	pluginManagerInstallRepoBtn.Layout(0, 0, 120, 28, L"Install", true, L"Install selected repository plugin when native Aether port is available");
	updateOpenBtn.Layout(0, 0, 140, 30, L"Open Release", true, L"Open the latest GitHub release page");
	updateLaterBtn.Layout(0, 0, 100, 30, L"Later", false, L"Close this update notification");
	pluginRepoOwnerInput.Layout(0, 0, 240, L"Owner");
	pluginRepoNameInput.Layout(0, 0, 240, L"Name");
	pluginRepoRefInput.Layout(0, 0, 240, L"Ref");
	wcscpy_s(pluginRepoOwnerInput.buffer, pluginRepoOwner.c_str());
	pluginRepoOwnerInput.cursor = (int)pluginRepoOwner.length();
	wcscpy_s(pluginRepoNameInput.buffer, pluginRepoName.c_str());
	pluginRepoNameInput.cursor = (int)pluginRepoName.length();
	wcscpy_s(pluginRepoRefInput.buffer, pluginRepoRef.c_str());
	pluginRepoRefInput.cursor = (int)pluginRepoRef.length();

	consoleInput.Layout(cx, 0, cw, L"Type command...");

	accentPicker.Layout(cx, 0, cw * 0.5f);
	accentPicker.SetRGB(Theme::Custom::AccentR, Theme::Custom::AccentG, Theme::Custom::AccentB);

	
	aether.enabled.Layout(cx, 0, L"Aether Smooth", L"Adaptive multi-stage filter pipeline");
	aether.lagRemovalEnabled.Layout(cx, 0, L"Lag Removal", L"Counteract internal tablet processing delay");
	aether.lagRemovalStrength.Layout(cx, 0, hw, L"Strength", 0.1f, 2.0f, 0.6f, L"Lower = more aggressive, Higher = smoother");
	aether.stabilizerEnabled.Layout(cx, 0, L"Stabilizer", L"Velocity-adaptive smoothing (Adaptive Flow)");
	aether.stabilizerStability.Layout(cx, 0, hw, L"Stability", 0.01f, 10.0f, 1.0f, L"Smoothing at low speeds");
	aether.stabilizerSensitivity.Layout(cx + hw + 16, 0, hw, L"Sensitivity", 0.001f, 0.1f, 0.015f, L"Response to fast movement");
	aether.stabilizerSensitivity.format = L"%.4f";
	aether.snappingEnabled.Layout(cx, 0, L"Dynamic Snapping", L"Zero lag during fast aim snaps");
	aether.snappingInner.Layout(cx, 0, hw, L"Snap Radius (Min)", 0.0f, 5.0f, 0.5f, L"Area where smoothing is strongest");
	aether.snappingOuter.Layout(cx + hw + 16, 0, hw, L"Snap Radius (Max)", 0.5f, 20.0f, 3.0f, L"Beyond this smoothing is disabled");
	aether.rhythmFlowEnabled.Layout(cx, 0, L"Rhythm Flow", L"osu-inspired motion filter: calm on micro-jitter, quick on sharp turns");
	aether.rhythmFlowStrength.Layout(cx, 0, hw, L"Flow Strength", 0.0f, 1.0f, 0.35f, L"Base smoothing on calm motion");
	aether.rhythmFlowRelease.Layout(cx + hw + 16, 0, hw, L"Turn Release", 0.0f, 1.0f, 0.75f, L"How quickly smoothing opens on direction changes");
	aether.rhythmFlowJitter.Layout(cx, 0, hw, L"Jitter Window (mm)", 0.0f, 1.5f, 0.18f, L"Small raw movement range treated as hand tremor");
	aether.rhythmFlowJitter.format = L"%.2f";
	aether.suppressionEnabled.Layout(cx, 0, L"Suppression", L"Lock cursor when below threshold");
	aether.suppressionTime.Layout(cx, 0, hw, L"Time (ms)", 0.0f, 50.0f, 5.0f, L"Jitter suppression window");

	
	uiThemeCount = 12;
	uiThemeDefaults[0]  = &Theme::Themes::Midnight;
	uiThemeDefaults[1]  = &Theme::Themes::Abyss;
	uiThemeDefaults[2]  = &Theme::Themes::Nord;
	uiThemeDefaults[3]  = &Theme::Themes::Void;
	uiThemeDefaults[4]  = &Theme::Themes::Rose;
	uiThemeDefaults[5]  = &Theme::Themes::Ember;
	uiThemeDefaults[6]  = &Theme::Themes::Matcha;
	uiThemeDefaults[7]  = &Theme::Themes::Lavender;
	uiThemeDefaults[8]  = &Theme::Themes::Snow;
	uiThemeDefaults[9]  = &Theme::Themes::Linen;
	uiThemeDefaults[10] = &Theme::Themes::Frost;
	uiThemeDefaults[11] = &Theme::Themes::Blossom;
	for (int i = 0; i < uiThemeCount; i++) {
		uiThemes[i] = *uiThemeDefaults[i]; 
	}
	currentTheme = 0;
	editingTheme = -1;
	editingSlot = -1;
	slotPicker.Layout(0, 0, 180);
	slotHexInput.Layout(0, 0, 100, L"#000000");

	
	hexColorInput.Layout(cx, 0, 100, L"#7F9BD4");
	{
		wchar_t hexBuf[16];
		swprintf_s(hexBuf, L"#%02X%02X%02X",
			(int)(Theme::Custom::AccentR * 255),
			(int)(Theme::Custom::AccentG * 255),
			(int)(Theme::Custom::AccentB * 255));
		wcscpy_s(hexColorInput.buffer, hexBuf);
		hexColorInput.cursor = (int)wcslen(hexBuf);
	}

	
	visualizerToggle.Layout(cx, 0, L"Input Visualizer", L"Show pen trail overlay on tablet area");

}

void AetherApp::CaptureSettingsSnapshot(SettingsSnapshot& snapshot) const {
	snapshot.sliders.clear();
	snapshot.ints.clear();
	snapshot.toggles.clear();

	auto slider = [&](const Slider& control) { snapshot.sliders.push_back(control.value); };
	auto toggle = [&](const Toggle& control) { snapshot.toggles.push_back(control.value); };

	slider(area.tabletWidth); slider(area.tabletHeight); slider(area.tabletX); slider(area.tabletY);
	slider(area.screenWidth); slider(area.screenHeight); slider(area.screenX); slider(area.screenY);
	slider(area.rotation); slider(area.aspectRatio);
	slider(tipThreshold); slider(overclockHz); slider(penRateLimitHz); slider(dpiScale);
	slider(filters.smoothingLatency); slider(filters.smoothingInterval);
	slider(filters.antichatterStrength); slider(filters.antichatterMultiplier);
	slider(filters.antichatterOffsetX); slider(filters.antichatterOffsetY);
	slider(filters.noiseBuffer); slider(filters.noiseThreshold); slider(filters.noiseIterations);
	slider(filters.velCurveMinSpeed); slider(filters.velCurveMaxSpeed);
	slider(filters.velCurveSmoothing); slider(filters.velCurveSharpness);
	slider(filters.snapRadius); slider(filters.snapSmooth);
	slider(filters.reconStrength); slider(filters.reconVelSmooth);
	slider(filters.reconAccelCap); slider(filters.reconPredTime);
	slider(filters.adaptiveProcessNoise); slider(filters.adaptiveMeasNoise); slider(filters.adaptiveVelWeight);
	slider(aether.lagRemovalStrength); slider(aether.stabilizerStability); slider(aether.stabilizerSensitivity);
	slider(aether.snappingInner); slider(aether.snappingOuter); slider(aether.suppressionTime);
	slider(aether.rhythmFlowStrength); slider(aether.rhythmFlowRelease); slider(aether.rhythmFlowJitter);
	snapshot.sliders.push_back(Theme::Custom::AccentR);
	snapshot.sliders.push_back(Theme::Custom::AccentG);
	snapshot.sliders.push_back(Theme::Custom::AccentB);

	toggle(area.customValues); toggle(area.autoCenter); toggle(area.lockAspect);
	toggle(forceFullArea); toggle(areaClipping); toggle(areaLimiting);
	toggle(overclockEnabled); toggle(penRateLimitEnabled);
	toggle(filters.smoothingEnabled); toggle(filters.antichatterEnabled); toggle(filters.noiseEnabled);
	toggle(filters.velCurveEnabled); toggle(filters.snapEnabled); toggle(filters.reconstructorEnabled);
	toggle(filters.adaptiveEnabled);
	toggle(aether.enabled); toggle(aether.lagRemovalEnabled); toggle(aether.stabilizerEnabled);
	toggle(aether.snappingEnabled); toggle(aether.suppressionEnabled);
	toggle(aether.rhythmFlowEnabled);
	toggle(visualizerToggle);

	snapshot.ints.push_back(outputMode.selected);
	snapshot.ints.push_back(buttonTip.selected);
	snapshot.ints.push_back(buttonBottom.selected);
	snapshot.ints.push_back(buttonTop.selected);
	snapshot.ints.push_back(currentTheme);
	snapshot.ints.push_back(particleStyle);
	snapshot.ints.push_back(selectedDisplayTarget);
}

bool AetherApp::SettingsSnapshotsEqual(const SettingsSnapshot& a, const SettingsSnapshot& b) const {
	if (a.sliders.size() != b.sliders.size() || a.ints != b.ints || a.toggles != b.toggles)
		return false;
	for (size_t i = 0; i < a.sliders.size(); i++) {
		if (fabsf(a.sliders[i] - b.sliders[i]) > 0.0005f)
			return false;
	}
	return true;
}

void AetherApp::ApplySettingsSnapshot(const SettingsSnapshot& snapshot) {
	size_t s = 0, t = 0, n = 0;
	float snapshotAccentR = Theme::Custom::AccentR;
	float snapshotAccentG = Theme::Custom::AccentG;
	float snapshotAccentB = Theme::Custom::AccentB;
	auto setSlider = [&](Slider& control) {
		if (s >= snapshot.sliders.size()) return;
		control.value = Clamp(snapshot.sliders[s++], control.minVal, control.maxVal);
		control.animValue = control.value;
		control.editMode = false;
	};
	auto setToggle = [&](Toggle& control) {
		if (t >= snapshot.toggles.size()) return;
		control.value = snapshot.toggles[t++];
		control.animT = control.value ? 1.0f : 0.0f;
	};
	auto nextInt = [&]() -> int {
		if (n >= snapshot.ints.size()) return 0;
		return snapshot.ints[n++];
	};

	setSlider(area.tabletWidth); setSlider(area.tabletHeight); setSlider(area.tabletX); setSlider(area.tabletY);
	setSlider(area.screenWidth); setSlider(area.screenHeight); setSlider(area.screenX); setSlider(area.screenY);
	setSlider(area.rotation); setSlider(area.aspectRatio);
	setSlider(tipThreshold); setSlider(overclockHz); setSlider(penRateLimitHz); setSlider(dpiScale);
	setSlider(filters.smoothingLatency); setSlider(filters.smoothingInterval);
	setSlider(filters.antichatterStrength); setSlider(filters.antichatterMultiplier);
	setSlider(filters.antichatterOffsetX); setSlider(filters.antichatterOffsetY);
	setSlider(filters.noiseBuffer); setSlider(filters.noiseThreshold); setSlider(filters.noiseIterations);
	setSlider(filters.velCurveMinSpeed); setSlider(filters.velCurveMaxSpeed);
	setSlider(filters.velCurveSmoothing); setSlider(filters.velCurveSharpness);
	setSlider(filters.snapRadius); setSlider(filters.snapSmooth);
	setSlider(filters.reconStrength); setSlider(filters.reconVelSmooth);
	setSlider(filters.reconAccelCap); setSlider(filters.reconPredTime);
	setSlider(filters.adaptiveProcessNoise); setSlider(filters.adaptiveMeasNoise); setSlider(filters.adaptiveVelWeight);
	setSlider(aether.lagRemovalStrength); setSlider(aether.stabilizerStability); setSlider(aether.stabilizerSensitivity);
	setSlider(aether.snappingInner); setSlider(aether.snappingOuter); setSlider(aether.suppressionTime);
	setSlider(aether.rhythmFlowStrength); setSlider(aether.rhythmFlowRelease); setSlider(aether.rhythmFlowJitter);
	if (s + 2 < snapshot.sliders.size()) {
		snapshotAccentR = Clamp(snapshot.sliders[s++], 0.0f, 1.0f);
		snapshotAccentG = Clamp(snapshot.sliders[s++], 0.0f, 1.0f);
		snapshotAccentB = Clamp(snapshot.sliders[s++], 0.0f, 1.0f);
	}

	setToggle(area.customValues); setToggle(area.autoCenter); setToggle(area.lockAspect);
	setToggle(forceFullArea); setToggle(areaClipping); setToggle(areaLimiting);
	setToggle(overclockEnabled); setToggle(penRateLimitEnabled);
	setToggle(filters.smoothingEnabled); setToggle(filters.antichatterEnabled); setToggle(filters.noiseEnabled);
	setToggle(filters.velCurveEnabled); setToggle(filters.snapEnabled); setToggle(filters.reconstructorEnabled);
	setToggle(filters.adaptiveEnabled);
	setToggle(aether.enabled); setToggle(aether.lagRemovalEnabled); setToggle(aether.stabilizerEnabled);
	setToggle(aether.snappingEnabled); setToggle(aether.suppressionEnabled);
	setToggle(aether.rhythmFlowEnabled);
	setToggle(visualizerToggle);

	outputMode.selected = (int)Clamp((float)nextInt(), 0.0f, (float)std::max(0, outputMode.optionCount - 1));
	buttonTip.selected = (int)Clamp((float)nextInt(), 0.0f, (float)std::max(0, buttonTip.optionCount - 1));
	buttonBottom.selected = (int)Clamp((float)nextInt(), 0.0f, (float)std::max(0, buttonBottom.optionCount - 1));
	buttonTop.selected = (int)Clamp((float)nextInt(), 0.0f, (float)std::max(0, buttonTop.optionCount - 1));
	currentTheme = (int)Clamp((float)nextInt(), 0.0f, (float)std::max(0, uiThemeCount - 1));
	particleStyle = (int)Clamp((float)nextInt(), 0.0f, 3.0f);
	if (!displayTargets.empty())
		selectedDisplayTarget = (int)Clamp((float)nextInt(), 0.0f, (float)((int)displayTargets.size() - 1));

	if (currentTheme >= 0 && currentTheme < uiThemeCount) {
		Theme::ApplyTheme(uiThemes[currentTheme]);
		if (hWnd) { extern void ApplyAetherWindowTheme(HWND); ApplyAetherWindowTheme(hWnd); }
	}
	Theme::Custom::SetAccent(snapshotAccentR, snapshotAccentG, snapshotAccentB);
	accentPicker.SetRGB(Theme::Custom::AccentR, Theme::Custom::AccentG, Theme::Custom::AccentB);
	ApplyDpiScale();
	ClampScreenArea();
	ApplyAspectLock(false);
}

void AetherApp::TrackSettingsUndo() {
	if (applyingUndo)
		return;

	SettingsSnapshot current;
	CaptureSettingsSnapshot(current);
	if (!hasSettingsSnapshot) {
		lastSettingsSnapshot = current;
		hasSettingsSnapshot = true;
		return;
	}
	if (SettingsSnapshotsEqual(lastSettingsSnapshot, current))
		return;

	undoStack.push_back(lastSettingsSnapshot);
	if (undoStack.size() > 80)
		undoStack.erase(undoStack.begin());
	lastSettingsSnapshot = current;
}

void AetherApp::PushUndoCheckpoint() {
	SettingsSnapshot current;
	CaptureSettingsSnapshot(current);
	if (!hasSettingsSnapshot) {
		lastSettingsSnapshot = current;
		hasSettingsSnapshot = true;
		return;
	}

	if (undoStack.empty() || !SettingsSnapshotsEqual(undoStack.back(), current)) {
		undoStack.push_back(current);
		if (undoStack.size() > 80)
			undoStack.erase(undoStack.begin());
	}
	lastSettingsSnapshot = current;
}

void AetherApp::InitializeSettingsUndo() {
	undoStack.clear();
	CaptureSettingsSnapshot(lastSettingsSnapshot);
	hasSettingsSnapshot = true;
}

void AetherApp::UndoLastSettingsChange() {
	if (undoStack.empty())
		return;

	SettingsSnapshot snapshot = undoStack.back();
	undoStack.pop_back();
	applyingUndo = true;
	ApplySettingsSnapshot(snapshot);
	ApplyAllSettings();
	AutoSaveConfig();
	applyingUndo = false;
	lastSettingsSnapshot = snapshot;
	hasSettingsSnapshot = true;
}

bool AetherApp::IsTextEditingActive() const {
	if (slotHexInput.focused || hexColorInput.focused || consoleInput.focused)
		return true;

	const Slider* sliders[] = {
		&area.tabletWidth, &area.tabletHeight, &area.tabletX, &area.tabletY,
		&area.screenWidth, &area.screenHeight, &area.screenX, &area.screenY,
		&area.rotation, &area.aspectRatio, &dpiScale, &tipThreshold, &overclockHz, &penRateLimitHz,
		&filters.smoothingLatency, &filters.smoothingInterval,
		&filters.antichatterStrength, &filters.antichatterMultiplier,
		&filters.antichatterOffsetX, &filters.antichatterOffsetY,
		&filters.noiseBuffer, &filters.noiseThreshold, &filters.noiseIterations,
		&filters.velCurveMinSpeed, &filters.velCurveMaxSpeed,
		&filters.velCurveSmoothing, &filters.velCurveSharpness,
		&filters.snapRadius, &filters.snapSmooth,
		&filters.reconStrength, &filters.reconVelSmooth,
		&filters.reconAccelCap, &filters.reconPredTime,
		&filters.adaptiveProcessNoise, &filters.adaptiveMeasNoise, &filters.adaptiveVelWeight,
		&aether.lagRemovalStrength, &aether.stabilizerStability, &aether.stabilizerSensitivity,
		&aether.snappingInner, &aether.snappingOuter, &aether.suppressionTime,
		&aether.rhythmFlowStrength, &aether.rhythmFlowRelease, &aether.rhythmFlowJitter
	};
	for (const Slider* slider : sliders) {
		if (slider->editMode)
			return true;
	}
	return false;
}

bool AetherApp::FocusNextEditableRow(bool reverse) {
	std::vector<Slider*> rows;
	auto add = [&](Slider& slider) { rows.push_back(&slider); };

	switch (sidebar.activeIndex) {
	case 0:
		add(area.screenWidth);
		add(area.screenHeight);
		if (area.customValues.value) {
			add(area.screenX);
			add(area.screenY);
		}
		add(area.tabletWidth);
		add(area.tabletHeight);
		add(area.tabletX);
		add(area.tabletY);
		add(area.rotation);
		if (area.lockAspect.value)
			add(area.aspectRatio);
		break;
	case 1:
		if (filters.smoothingEnabled.value) {
			add(filters.smoothingLatency);
			add(filters.smoothingInterval);
		}
		if (filters.antichatterEnabled.value) {
			add(filters.antichatterStrength);
			add(filters.antichatterMultiplier);
			add(filters.antichatterOffsetX);
			add(filters.antichatterOffsetY);
		}
		if (filters.noiseEnabled.value) {
			add(filters.noiseBuffer);
			add(filters.noiseThreshold);
			add(filters.noiseIterations);
		}
		if (filters.velCurveEnabled.value) {
			add(filters.velCurveMinSpeed);
			add(filters.velCurveMaxSpeed);
			add(filters.velCurveSmoothing);
			add(filters.velCurveSharpness);
		}
		if (filters.reconstructorEnabled.value) {
			add(filters.reconStrength);
			add(filters.reconVelSmooth);
			add(filters.reconAccelCap);
			add(filters.reconPredTime);
		}
		if (filters.adaptiveEnabled.value) {
			add(filters.adaptiveProcessNoise);
			add(filters.adaptiveMeasNoise);
			add(filters.adaptiveVelWeight);
		}
		if (aether.enabled.value) {
			if (aether.lagRemovalEnabled.value)
				add(aether.lagRemovalStrength);
			if (aether.stabilizerEnabled.value) {
				add(aether.stabilizerStability);
				add(aether.stabilizerSensitivity);
			}
			if (aether.snappingEnabled.value) {
				add(aether.snappingInner);
				add(aether.snappingOuter);
			}
			if (aether.rhythmFlowEnabled.value) {
				add(aether.rhythmFlowStrength);
				add(aether.rhythmFlowRelease);
				add(aether.rhythmFlowJitter);
			}
			if (aether.suppressionEnabled.value)
				add(aether.suppressionTime);
		}
		if (overclockEnabled.value)
			add(overclockHz);
		if (penRateLimitEnabled.value)
			add(penRateLimitHz);
		break;
	case 2:
		add(tipThreshold);
		add(dpiScale);
		break;
	default:
		break;
	}

	if (rows.empty())
		return false;

	int active = -1;
	for (int i = 0; i < (int)rows.size(); i++) {
		if (rows[i]->editMode) {
			active = i;
			break;
		}
	}
	if (active < 0)
		return false;

	bool changed = rows[active]->CommitEdit();
	int next = active + (reverse ? -1 : 1);
	if (next < 0) next = (int)rows.size() - 1;
	if (next >= (int)rows.size()) next = 0;
	rows[next]->BeginEdit();

	if (changed) {
		if (rows[active] == &dpiScale) {
			ApplyDpiScale();
			AutoSaveConfig();
		}
		else {
			if (sidebar.activeIndex == 0)
				ApplyAspectLock(rows[active] == &area.tabletHeight);
			ApplyAllSettings();
		}
	}
	return true;
}

void AetherApp::SwitchTabByKeyboard(int direction) {
	if (sidebar.tabs.empty())
		return;

	int oldTab = sidebar.activeIndex;
	int count = (int)sidebar.tabs.size();
	int next = (oldTab + direction) % count;
	if (next < 0) next += count;
	if (next == oldTab)
		return;

	prevTab = oldTab;
	sidebar.activeIndex = next;
	tabTransitionT = 0.0f;
	tabSlideOffset = (next > oldTab) ? 40.0f : -40.0f;
}

float AetherApp::GetSystemDpiScale() const {
	HWND target = hWnd ? hWnd : nullptr;
	HDC hdc = GetDC(target);
	float scale = 1.0f;
	if (hdc) {
		scale = GetDeviceCaps(hdc, LOGPIXELSX) / 96.0f;
		ReleaseDC(target, hdc);
	}
	return Clamp(scale, 0.75f, 2.50f);
}

float AetherApp::GetSelectedDpiScale() const {
	return Clamp(dpiScale.value / 100.0f, 0.75f, 2.00f);
}

void AetherApp::ApplyDpiScale() {
	if (!renderer.pDWriteFactory)
		return;
	renderer.SetDpiScale(GetSelectedDpiScale());
}

void AetherApp::OnMouseMove(float x, float y) {
	if (isPluginCatalogDragScrolling) {
		pluginCatalogScrollY = pluginCatalogDragStartOffset + (pluginCatalogDragStartY - y);
		ClampPluginCatalogScroll();
	}
	
	if (isDragScrolling) {
		float dy = dragScrollStartY - y; 
		float* scrollTarget = nullptr;
		switch (sidebar.activeIndex) {
		case 0: scrollTarget = &areaScrollY; break;
		case 1: scrollTarget = &filterScrollY; break;
		case 2: scrollTarget = &settingsScrollY; break;
		}
		if (scrollTarget) {
			*scrollTarget = dragScrollStartOffset + dy;
			if (*scrollTarget < 0) *scrollTarget = 0;
			ClampScrollOffsets();
		}
	}
	mouseX = x; mouseY = y;
}
void AetherApp::OnMouseDown() {
	if (hWnd)
		SetFocus(hWnd);
	mouseDown = true;
	mouseClicked = true;
}
void AetherApp::OnMouseUp() {
	mouseDown = false;
	isPluginCatalogDragScrolling = false;
	if (isDraggingArea) {
		isDraggingArea = false;
		dragTarget = 0;
		ApplyAllSettings();
	}
}

void AetherApp::OnMiddleMouseDown() {
	middleMouseDown = true;
	
	if (mouseX > Theme::Size::SidebarWidth && mouseY > GetContentAreaTop() && mouseY < GetContentAreaBottom()) {
		isDragScrolling = true;
		dragScrollStartY = mouseY;
		switch (sidebar.activeIndex) {
		case 0: dragScrollStartOffset = areaScrollY; break;
		case 1: dragScrollStartOffset = filterScrollY; break;
		case 2: dragScrollStartOffset = settingsScrollY; break;
		default: dragScrollStartOffset = 0; break;
		}
	}
}

void AetherApp::OnMiddleMouseUp() {
	middleMouseDown = false;
	isDragScrolling = false;
}

void AetherApp::OnRightMouseDown() {
	rightMouseDown = true;
	
	if (mouseX > Theme::Size::SidebarWidth && mouseY > GetContentAreaTop() && mouseY < GetContentAreaBottom()) {
		isDragScrolling = true;
		dragScrollStartY = mouseY;
		switch (sidebar.activeIndex) {
		case 0: dragScrollStartOffset = areaScrollY; break;
		case 1: dragScrollStartOffset = filterScrollY; break;
		case 2: dragScrollStartOffset = settingsScrollY; break;
		default: dragScrollStartOffset = 0; break;
		}
	}
}

void AetherApp::OnRightMouseUp() {
	rightMouseDown = false;
	isDragScrolling = false;
}
void AetherApp::RefreshDetectedScreen() {
	int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
	int top = GetSystemMetrics(SM_YVIRTUALSCREEN);
	int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
	int primaryWidth = GetSystemMetrics(SM_CXSCREEN);
	int primaryHeight = GetSystemMetrics(SM_CYSCREEN);

	virtualScreenX = (float)left;
	virtualScreenY = (float)top;
	primaryScreenW = (float)((primaryWidth > 0) ? primaryWidth : 1920);
	primaryScreenH = (float)((primaryHeight > 0) ? primaryHeight : 1080);
	detectedScreenW = (float)((width > 0) ? width : primaryScreenW);
	detectedScreenH = (float)((height > 0) ? height : primaryScreenH);

	displayTargets.clear();
	DisplayTarget desktop;
	desktop.x = 0;
	desktop.y = 0;
	desktop.width = detectedScreenW;
	desktop.height = detectedScreenH;
	desktop.label = L"Full desktop";
	displayTargets.push_back(desktop);

	MonitorEnumContext ctx;
	ctx.targets = &displayTargets;
	ctx.virtualLeft = left;
	ctx.virtualTop = top;
	EnumDisplayMonitors(nullptr, nullptr, EnumDisplayTargetProc, reinterpret_cast<LPARAM>(&ctx));

	if (selectedDisplayTarget >= (int)displayTargets.size())
		selectedDisplayTarget = 0;
}

void AetherApp::SendDisplaySettingsToDriver() {
	if (!driver.isConnected)
		return;

	char cmd[256];
	sprintf_s(cmd, "DesktopSize %.0f %.0f", detectedScreenW, detectedScreenH);
	driver.SendCommand(cmd);
	SendStaticMonitorInfoToDriver();

	float actualScreenX = area.screenX.value + virtualScreenX;
	float actualScreenY = area.screenY.value + virtualScreenY;
	sprintf_s(cmd, "ScreenArea %.0f %.0f %.0f %.0f",
		area.screenWidth.value, area.screenHeight.value,
		actualScreenX, actualScreenY);
	driver.SendCommand(cmd);
	driver.SendCommand("UpdateMonitorInfo");
}

void AetherApp::SendStaticMonitorInfoToDriver() {
	if (!driver.isConnected)
		return;

	char cmd[256];
	sprintf_s(cmd, "StaticMonitorInfo %.0f %.0f %.0f %.0f %.0f %.0f",
		primaryScreenW,
		primaryScreenH,
		detectedScreenW,
		detectedScreenH,
		virtualScreenX,
		virtualScreenY);
	driver.SendCommand(cmd);
}

void AetherApp::SendStartupSettingsToDriver() {
	if (!driver.isConnected)
		return;

	char cmd[256];
	sprintf_s(cmd, "DesktopSize %.0f %.0f", detectedScreenW, detectedScreenH);
	driver.SendCommand(cmd);
	SendStaticMonitorInfoToDriver();

	float actualScreenX = area.screenX.value + virtualScreenX;
	float actualScreenY = area.screenY.value + virtualScreenY;
	sprintf_s(cmd, "ScreenArea %.0f %.0f %.0f %.0f",
		area.screenWidth.value, area.screenHeight.value,
		actualScreenX, actualScreenY);
	driver.SendCommand(cmd);
	sprintf_s(cmd, "TabletArea %.2f %.2f %.2f %.2f",
		area.tabletWidth.value, area.tabletHeight.value,
		area.tabletX.value, area.tabletY.value);
	driver.SendCommand(cmd);

	sprintf_s(cmd, "Rotate %.1f", area.rotation.value);
	driver.SendCommand(cmd);

	const char* modes[] = { "Mode Absolute", "Mode Relative", "Mode Digitizer" };
	driver.SendCommand(modes[outputMode.selected]);

	sprintf_s(cmd, "Sensitivity %.4f", area.screenWidth.value / area.tabletWidth.value);
	driver.SendCommand(cmd);

	driver.SendCommand("UpdateMonitorInfo");

	sprintf_s(cmd, "ButtonMap %d %d %d",
		buttonTip.selected, buttonBottom.selected, buttonTop.selected);
	driver.SendCommand(cmd);

	driver.SendCommand(areaClipping.value ? "AreaClipping on" : "AreaClipping off");
	driver.SendCommand(areaLimiting.value ? "AreaLimiting on" : "AreaLimiting off");
	sprintf_s(cmd, "TipThreshold %.0f", tipThreshold.value);
	driver.SendCommand(cmd);
	sprintf_s(cmd, "Overclock %s %.0f", overclockEnabled.value ? "on" : "off", overclockHz.value);
	driver.SendCommand(cmd);
	sprintf_s(cmd, "PenRateLimit %s %.0f", penRateLimitEnabled.value ? "on" : "off", penRateLimitHz.value);
	driver.SendCommand(cmd);

	SendFilterSettings();
	driver.SendCommand("PluginReload");
	Sleep(80);
	SendPluginSettings();

	driver.SendCommand("start");
}

bool AetherApp::StartDriverService() {
	if (driver.isConnected)
		return true;

	ClampScreenArea();
	ApplyAspectLock(false);
	if (!driver.Start(servicePath))
		return false;

	Sleep(800);
	SendStartupSettingsToDriver();
	autoStartRetryCount = 0;
	autoStartRetryTimer = 0.0f;
	return true;
}

void AetherApp::ApplyDisplayTarget(int index) {
	if (displayTargets.empty())
		return;

	if (index < 0)
		index = (int)displayTargets.size() - 1;
	if (index >= (int)displayTargets.size())
		index = 0;

	selectedDisplayTarget = index;
	const DisplayTarget& target = displayTargets[selectedDisplayTarget];
	area.screenWidth.value = target.width;
	area.screenHeight.value = target.height;
	area.screenX.value = target.x;
	area.screenY.value = target.y;
	if (!area.lockAspect.value && target.height > 1.0f) {
		area.aspectRatio.value = Clamp(target.width / target.height, area.aspectRatio.minVal, area.aspectRatio.maxVal);
		area.aspectRatio.animValue = area.aspectRatio.value;
	}

	ClampScreenArea();
	ApplyAspectLock(false);
	SendStartupSettingsToDriver();
}

float AetherApp::GetScreenAspectRatio() const {
	if (area.lockAspect.value)
		return Clamp(area.aspectRatio.value, 0.50f, 3.00f);

	float screenW = area.screenWidth.value;
	float screenH = area.screenHeight.value;

	if (screenW <= 1.0f) screenW = (detectedScreenW > 1.0f) ? detectedScreenW : 1920.0f;
	if (screenH <= 1.0f) screenH = (detectedScreenH > 1.0f) ? detectedScreenH : 1080.0f;

	return Clamp(screenW / screenH, 0.1f, 10.0f);
}

void AetherApp::ClampScreenArea() {
	float desktopW = std::max(100.0f, detectedScreenW);
	float desktopH = std::max(100.0f, detectedScreenH);
	float targetX = 0.0f;
	float targetY = 0.0f;
	float targetW = desktopW;
	float targetH = desktopH;

	if (selectedDisplayTarget >= 0 && selectedDisplayTarget < (int)displayTargets.size()) {
		const DisplayTarget& target = displayTargets[selectedDisplayTarget];
		targetX = target.x;
		targetY = target.y;
		targetW = std::max(100.0f, target.width);
		targetH = std::max(100.0f, target.height);
	}

	area.screenWidth.maxVal = targetW;
	area.screenHeight.maxVal = targetH;

	area.screenWidth.value = Clamp(area.screenWidth.value, area.screenWidth.minVal, targetW);
	area.screenHeight.value = Clamp(area.screenHeight.value, area.screenHeight.minVal, targetH);

	// The X/Y sliders must use the *real* allowed range, not the full desktop size.
	// Otherwise the user can keep dragging the thumb past the point where the
	// preview/driver area is already clamped to the screen edge.
	float maxX = targetX + targetW - area.screenWidth.value;
	float maxY = targetY + targetH - area.screenHeight.value;
	if (maxX < targetX) maxX = targetX;
	if (maxY < targetY) maxY = targetY;

	area.screenX.minVal = targetX;
	area.screenY.minVal = targetY;
	area.screenX.maxVal = maxX;
	area.screenY.maxVal = maxY;

	// Always keep the mapped rectangle fully inside the selected monitor/desktop.
	// If the user types coordinates that would place the area outside the current
	// resolution (for example X=1700 with W=400 on a 1920px screen), move the
	// rectangle back inside instead of letting the driver map to an unreachable zone.
	area.screenX.value = Clamp(area.screenX.value, area.screenX.minVal, area.screenX.maxVal);
	area.screenY.value = Clamp(area.screenY.value, area.screenY.minVal, area.screenY.maxVal);
}

void AetherApp::CenterScreenArea() {
	float targetX = 0.0f;
	float targetY = 0.0f;
	float targetW = detectedScreenW;
	float targetH = detectedScreenH;
	if (selectedDisplayTarget >= 0 && selectedDisplayTarget < (int)displayTargets.size()) {
		const DisplayTarget& target = displayTargets[selectedDisplayTarget];
		targetX = target.x;
		targetY = target.y;
		targetW = target.width;
		targetH = target.height;
	}

	float offsetX = std::max(0.0f, targetW - area.screenWidth.value) * 0.5f;
	float offsetY = std::max(0.0f, targetH - area.screenHeight.value) * 0.5f;
	area.screenX.value = targetX + offsetX;
	area.screenY.value = targetY + offsetY;
	ClampScreenArea();
}

void AetherApp::ClampTabletAreaToFull(float fullTabletW, float fullTabletH) {
	area.tabletWidth.value = Clamp(area.tabletWidth.value, area.tabletWidth.minVal, fullTabletW);
	area.tabletHeight.value = Clamp(area.tabletHeight.value, area.tabletHeight.minVal, fullTabletH);

	float halfW = area.tabletWidth.value * 0.5f;
	float halfH = area.tabletHeight.value * 0.5f;
	area.tabletX.value = Clamp(area.tabletX.value, halfW, fullTabletW - halfW);
	area.tabletY.value = Clamp(area.tabletY.value, halfH, fullTabletH - halfH);
}

void AetherApp::ApplyAspectLock(bool preserveHeight) {
	float fullTabletW = (driver.tabletWidth > 1.0f) ? driver.tabletWidth : 152.0f;
	float fullTabletH = (driver.tabletHeight > 1.0f) ? driver.tabletHeight : 95.0f;

	if (area.lockAspect.value) {
		float aspect = GetScreenAspectRatio();

		if (preserveHeight) {
			area.tabletWidth.value = area.tabletHeight.value * aspect;
			if (area.tabletWidth.value > fullTabletW) {
				area.tabletWidth.value = fullTabletW;
				area.tabletHeight.value = area.tabletWidth.value / aspect;
			}
		}
		else {
			area.tabletHeight.value = area.tabletWidth.value / aspect;
			if (area.tabletHeight.value > fullTabletH) {
				area.tabletHeight.value = fullTabletH;
				area.tabletWidth.value = area.tabletHeight.value * aspect;
			}
		}
	}

	ClampTabletAreaToFull(fullTabletW, fullTabletH);
}

void AetherApp::OnResize(UINT width, UINT height) {
	if (width == 0 || height == 0)
		return;

	clientWidth = (float)width;
	clientHeight = (float)height;
	Theme::Runtime::SetWindowSize(clientWidth, clientHeight);
	ApplyDpiScale();
	renderer.Resize(width, height);
	ClampScrollOffsets();
}

void AetherApp::OnDisplayChange() {
	RefreshDetectedScreen();
	ClampScreenArea();
	if (!area.lockAspect.value && area.screenHeight.value > 1.0f) {
		area.aspectRatio.value = Clamp(area.screenWidth.value / area.screenHeight.value, area.aspectRatio.minVal, area.aspectRatio.maxVal);
		area.aspectRatio.animValue = area.aspectRatio.value;
	}
	ApplyAspectLock(false);
	SendDisplaySettingsToDriver();
	AutoSaveConfig();
}

void AetherApp::OnMouseWheel(float delta) {
	scrollDelta += delta;
	float scrollSpeed = 40.0f;
	if (pluginManagerOpen && !pluginSourceEditorOpen) {
		float overlayW = Theme::Runtime::WindowWidth;
		float overlayH = Theme::Runtime::WindowHeight;
		float w = std::min(960.0f, overlayW - 36.0f);
		float h = std::min(650.0f, overlayH - 54.0f);
		float x = (overlayW - w) * 0.5f;
		float y = (overlayH - h) * 0.5f;
		if (PointInRect(mouseX, mouseY, x + 14.0f, y + 88.0f, w - 28.0f, h - 148.0f)) {
			pluginCatalogScrollY -= delta * scrollSpeed;
			ClampPluginCatalogScroll();
			return;
		}
	}
	switch (sidebar.activeIndex) {
	case 0: areaScrollY -= delta * scrollSpeed; break;
	case 1: filterScrollY -= delta * scrollSpeed; break;
	case 2: settingsScrollY -= delta * scrollSpeed; break;
	}
	if (areaScrollY < 0) areaScrollY = 0;
	if (filterScrollY < 0) filterScrollY = 0;
	ClampScrollOffsets();
}

void AetherApp::OnChar(wchar_t ch) {
	if (pluginSourceEditorOpen) {
		pluginRepoOwnerInput.OnChar(ch);
		pluginRepoNameInput.OnChar(ch);
		pluginRepoRefInput.OnChar(ch);
		return;
	}

	bool tabletWidthCommitted = area.tabletWidth.OnChar(ch);
	bool tabletHeightCommitted = area.tabletHeight.OnChar(ch);
	bool tabletXCommitted = area.tabletX.OnChar(ch);
	bool tabletYCommitted = area.tabletY.OnChar(ch);
	bool screenWidthCommitted = area.screenWidth.OnChar(ch);
	bool screenHeightCommitted = area.screenHeight.OnChar(ch);
	bool screenXCommitted = area.screenX.OnChar(ch);
	bool screenYCommitted = area.screenY.OnChar(ch);
	bool rotationCommitted = area.rotation.OnChar(ch);
	bool aspectCommitted = area.aspectRatio.OnChar(ch);
	bool dpiCommitted = dpiScale.OnChar(ch);
	bool settingsCommitted = tipThreshold.OnChar(ch);
	bool filterCommitted = overclockHz.OnChar(ch);
	filterCommitted |= penRateLimitHz.OnChar(ch);

	filterCommitted |= filters.smoothingLatency.OnChar(ch); filterCommitted |= filters.smoothingInterval.OnChar(ch);
	filterCommitted |= filters.antichatterStrength.OnChar(ch); filterCommitted |= filters.antichatterMultiplier.OnChar(ch);
	filterCommitted |= filters.antichatterOffsetX.OnChar(ch); filterCommitted |= filters.antichatterOffsetY.OnChar(ch);
	filterCommitted |= filters.noiseBuffer.OnChar(ch); filterCommitted |= filters.noiseThreshold.OnChar(ch); filterCommitted |= filters.noiseIterations.OnChar(ch);
	filterCommitted |= filters.velCurveMinSpeed.OnChar(ch); filterCommitted |= filters.velCurveMaxSpeed.OnChar(ch);
	filterCommitted |= filters.velCurveSmoothing.OnChar(ch); filterCommitted |= filters.velCurveSharpness.OnChar(ch);
	filterCommitted |= filters.snapRadius.OnChar(ch); filterCommitted |= filters.snapSmooth.OnChar(ch);
	filterCommitted |= filters.reconStrength.OnChar(ch); filterCommitted |= filters.reconVelSmooth.OnChar(ch);
	filterCommitted |= filters.reconAccelCap.OnChar(ch); filterCommitted |= filters.reconPredTime.OnChar(ch);
	filterCommitted |= filters.adaptiveProcessNoise.OnChar(ch); filterCommitted |= filters.adaptiveMeasNoise.OnChar(ch); filterCommitted |= filters.adaptiveVelWeight.OnChar(ch);
	filterCommitted |= aether.lagRemovalStrength.OnChar(ch); filterCommitted |= aether.stabilizerStability.OnChar(ch); filterCommitted |= aether.stabilizerSensitivity.OnChar(ch);
	filterCommitted |= aether.snappingInner.OnChar(ch); filterCommitted |= aether.snappingOuter.OnChar(ch); filterCommitted |= aether.suppressionTime.OnChar(ch);
	filterCommitted |= aether.rhythmFlowStrength.OnChar(ch); filterCommitted |= aether.rhythmFlowRelease.OnChar(ch); filterCommitted |= aether.rhythmFlowJitter.OnChar(ch);
	for (size_t pluginIndex = 0; pluginIndex < pluginEntries.size(); ++pluginIndex) {
		for (auto& option : pluginEntries[pluginIndex].options) {
			if (option.kind == PluginEntry::PluginOption::SliderOption && option.slider.OnChar(ch)) {
				SendPluginOption(pluginIndex, option);
				filterCommitted = true;
			}
		}
	}

	bool areaCommitted =
		tabletWidthCommitted || tabletHeightCommitted || tabletXCommitted || tabletYCommitted ||
		screenWidthCommitted || screenHeightCommitted || screenXCommitted || screenYCommitted ||
		rotationCommitted || aspectCommitted;

	if (screenWidthCommitted || screenHeightCommitted) {
		if (area.autoCenter.value) {
			CenterScreenArea();
		}
		else if (area.customValues.value) {
			ClampScreenArea();
		}
		else {
			CenterScreenArea();
		}
		if (!area.lockAspect.value && area.screenHeight.value > 1.0f) {
			area.aspectRatio.value = Clamp(area.screenWidth.value / area.screenHeight.value, area.aspectRatio.minVal, area.aspectRatio.maxVal);
			area.aspectRatio.animValue = area.aspectRatio.value;
		}
	}

	if (areaCommitted) {
		if (tabletWidthCommitted || tabletHeightCommitted || aspectCommitted || area.lockAspect.value)
			ApplyAspectLock(tabletHeightCommitted && !tabletWidthCommitted);
		ApplyAllSettings();
	}
	else if (dpiCommitted) {
		ApplyDpiScale();
		AutoSaveConfig();
	}
	else if (settingsCommitted || filterCommitted) {
		ApplyAllSettings();
	}

	
	if (slotHexInput.OnChar(ch)) {
		if (editingTheme >= 0 && editingTheme < uiThemeCount && editingSlot >= 0) {
			std::wstring hex = slotHexInput.GetText();
			const wchar_t* p = hex.c_str();
			if (p[0] == L'#') p++;
			if (wcslen(p) >= 6) {
				int ri = 0, gi = 0, bi = 0;
				swscanf_s(p, L"%02x%02x%02x", &ri, &gi, &bi);
				float r = ri / 255.0f, g = gi / 255.0f, b = bi / 255.0f;
				SetThemeSlotColor(uiThemes[editingTheme], editingSlot, r, g, b);
				slotPicker.SetRGB(r, g, b);
				if (editingTheme == currentTheme) {
					Theme::ApplyTheme(uiThemes[editingTheme]);
					if (hWnd) { extern void ApplyAetherWindowTheme(HWND); ApplyAetherWindowTheme(hWnd); }
				}
				AutoSaveConfig();
			}
		}
	}

	
	if (hexColorInput.OnChar(ch)) {
		std::wstring hex = hexColorInput.GetText();
		const wchar_t* p = hex.c_str();
		if (p[0] == L'#') p++;
		if (wcslen(p) >= 6) {
			int ri = 0, gi = 0, bi = 0;
			swscanf_s(p, L"%02x%02x%02x", &ri, &gi, &bi);
			Theme::Custom::SetAccent(ri / 255.0f, gi / 255.0f, bi / 255.0f);
			accentPicker.SetRGB(ri / 255.0f, gi / 255.0f, bi / 255.0f);
			AutoSaveConfig();
		}
	}

	
	if (consoleInput.OnChar(ch)) {
		std::wstring wcmd = consoleInput.GetText();
		if (wcmd.length() > 0) {
			std::string cmd = WideToUtf8(wcmd);
			driver.SendCommand(cmd);
			commandHistory.push_back(cmd);
			commandHistoryIdx = (int)commandHistory.size();
			consoleInput.Clear();
		}
	}
}

void AetherApp::OnKeyDown(int vk) {
	bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
	bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
	if (pluginSourceEditorOpen) {
		if (vk == VK_ESCAPE) {
			pluginSourceEditorOpen = false;
			return;
		}
		if (pluginRepoOwnerInput.OnKeyDown(vk)) return;
		if (pluginRepoNameInput.OnKeyDown(vk)) return;
		if (pluginRepoRefInput.OnKeyDown(vk)) return;
	}
	if (updateModalOpen && vk == VK_ESCAPE) {
		updateModalOpen = false;
		return;
	}
	if (pluginManagerOpen && vk == VK_ESCAPE) {
		pluginManagerOpen = false;
		return;
	}
	if (ctrl && vk == 'Z' && !slotHexInput.focused && !hexColorInput.focused && !consoleInput.focused) {
		UndoLastSettingsChange();
		return;
	}
	if (vk == VK_TAB) {
		if (FocusNextEditableRow(shift))
			return;
		if (!IsTextEditingActive())
			SwitchTabByKeyboard(shift ? -1 : 1);
		return;
	}

	Slider* editableSliders[] = {
		&area.tabletWidth, &area.tabletHeight, &area.tabletX, &area.tabletY,
		&area.screenWidth, &area.screenHeight, &area.screenX, &area.screenY,
		&area.rotation, &area.aspectRatio, &dpiScale, &tipThreshold, &overclockHz, &penRateLimitHz,
		&filters.smoothingLatency, &filters.smoothingInterval,
		&filters.antichatterStrength, &filters.antichatterMultiplier,
		&filters.antichatterOffsetX, &filters.antichatterOffsetY,
		&filters.noiseBuffer, &filters.noiseThreshold, &filters.noiseIterations,
		&filters.velCurveMinSpeed, &filters.velCurveMaxSpeed,
		&filters.velCurveSmoothing, &filters.velCurveSharpness,
		&filters.snapRadius, &filters.snapSmooth,
		&filters.reconStrength, &filters.reconVelSmooth,
		&filters.reconAccelCap, &filters.reconPredTime,
		&filters.adaptiveProcessNoise, &filters.adaptiveMeasNoise, &filters.adaptiveVelWeight,
		&aether.lagRemovalStrength, &aether.stabilizerStability, &aether.stabilizerSensitivity,
		&aether.snappingInner, &aether.snappingOuter, &aether.suppressionTime,
		&aether.rhythmFlowStrength, &aether.rhythmFlowRelease, &aether.rhythmFlowJitter
	};
	for (Slider* slider : editableSliders) {
		if (slider->OnKeyDown(vk, ctrl, shift)) return;
	}
	for (auto& plugin : pluginEntries) {
		for (auto& option : plugin.options) {
			if (option.kind == PluginEntry::PluginOption::SliderOption && option.slider.OnKeyDown(vk, ctrl, shift)) return;
		}
	}

	
	if (slotHexInput.OnKeyDown(vk)) return;
	if (hexColorInput.OnKeyDown(vk)) return;
	if (consoleInput.OnKeyDown(vk)) return;

	if (consoleInput.focused && !commandHistory.empty()) {
		if (vk == VK_UP) {
			commandHistoryIdx--;
			if (commandHistoryIdx < 0) commandHistoryIdx = 0;
			std::wstring wcmd(commandHistory[commandHistoryIdx].begin(), commandHistory[commandHistoryIdx].end());
			wcscpy_s(consoleInput.buffer, wcmd.c_str());
			consoleInput.cursor = (int)wcmd.length();
		}
		if (vk == VK_DOWN) {
			commandHistoryIdx++;
			if (commandHistoryIdx >= (int)commandHistory.size()) {
				commandHistoryIdx = (int)commandHistory.size();
				consoleInput.Clear();
			} else {
				std::wstring wcmd(commandHistory[commandHistoryIdx].begin(), commandHistory[commandHistoryIdx].end());
				wcscpy_s(consoleInput.buffer, wcmd.c_str());
				consoleInput.cursor = (int)wcmd.length();
			}
		}
	}
}

float AetherApp::GetContentAreaTop() {
	return Theme::Size::HeaderHeight;
}

float AetherApp::GetContentAreaBottom() {
	return Theme::Runtime::WindowHeight - 28;
}

void AetherApp::BeginClipContent() {
	if (!renderer.pRT) return;
	D2D1_RECT_F clip = D2D1::RectF(
		Theme::Size::SidebarWidth, GetContentAreaTop(),
		Theme::Runtime::WindowWidth, GetContentAreaBottom()
	);
	renderer.pRT->PushAxisAlignedClip(clip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
}

void AetherApp::EndClipContent() {
	if (!renderer.pRT) return;
	renderer.pRT->PopAxisAlignedClip();
}

void AetherApp::Tick() {
	auto now = std::chrono::high_resolution_clock::now();
	deltaTime = std::chrono::duration<float>(now - lastFrameTime).count();
	if (deltaTime > 0.1f) deltaTime = 0.016f;
	lastFrameTime = now;

	Tooltip::Reset();

	if (vmultiInstalled && autoStartEnabled && !driver.isConnected) {
		autoStartRetryTimer -= deltaTime;
		if (autoStartRetryTimer <= 0.0f) {
			autoStartRetryCount++;
			autoStartRetryTimer = autoStartRetryCount < 8 ? 1.5f : 5.0f;
			StartDriverService();
		}
	}

	
	{
		std::lock_guard<std::mutex> lock(driver.trailMutex);
		for (auto& p : driver.trail) p.age += deltaTime;
		
		while (!driver.trail.empty() && driver.trail.front().age > 3.0f)
			driver.trail.erase(driver.trail.begin());
	}

	renderer.BeginFrame();
	DrawBackground();
	DrawHeader();

	int oldTab = sidebar.activeIndex;
	bool modalOpen = pluginManagerOpen || pluginSourceEditorOpen || updateModalOpen;
	bool frameClick = mouseClicked;
	if (modalOpen)
		mouseClicked = false;
	sidebar.Update(mouseX, mouseY, mouseClicked, deltaTime);
	sidebar.Draw(renderer);

	if (sidebar.activeIndex != oldTab && oldTab >= 0) {
		prevTab = oldTab;
		tabTransitionT = 0.0f;
		tabSlideOffset = (sidebar.activeIndex > oldTab) ? 40.0f : -40.0f;
	}
	if (tabTransitionT < 1.0f) {
		tabTransitionT += deltaTime * Theme::Anim::SpeedFast * 0.7f;
		if (tabTransitionT > 1.0f) tabTransitionT = 1.0f;
	}
	float easeT = 1.0f - (1.0f - tabTransitionT) * (1.0f - tabTransitionT);
	tabFadeAlpha = easeT;
	float currentSlide = tabSlideOffset * (1.0f - easeT);

	BeginClipContent();
	D2D1_MATRIX_3X2_F oldTransform;
	renderer.pRT->GetTransform(&oldTransform);
	renderer.pRT->SetTransform(
		D2D1::Matrix3x2F::Translation(0, currentSlide) * oldTransform);

	switch (sidebar.activeIndex) {
	case 0: DrawAreaPanel(); break;
	case 1: DrawFilterPanel(); break;
	case 2: DrawSettingsPanel(); break;
	case 3: DrawConsolePanel(); break;
	case 4: DrawAboutPanel(); break;
	}

	renderer.pRT->SetTransform(oldTransform);
	EndClipContent();
	if (modalOpen)
		mouseClicked = frameClick;

	
	{
		float contentH = 0, scrollY = 0;
		float* scrollPtr = nullptr;
		switch (sidebar.activeIndex) {
		case 0: contentH = areaContentH; scrollY = areaScrollY; scrollPtr = &areaScrollY; break;
		case 1: contentH = filterContentH; scrollY = filterScrollY; scrollPtr = &filterScrollY; break;
		case 2: contentH = settingsContentH; scrollY = settingsScrollY; scrollPtr = &settingsScrollY; break;
		}
		float visibleH = GetContentAreaBottom() - GetContentAreaTop();
		if (contentH > visibleH + 1.0f && visibleH > 10.0f) {
			float trackX = Theme::Runtime::WindowWidth - 6.0f;
			float trackY = GetContentAreaTop() + 2.0f;
			float trackH = visibleH - 4.0f;
			float thumbRatio = visibleH / contentH;
			float thumbH = trackH * thumbRatio;
			if (thumbH < 20.0f) thumbH = 20.0f;
			float maxScroll = contentH - visibleH;
			float scrollRatio = (maxScroll > 0) ? (scrollY / maxScroll) : 0;
			float thumbY = trackY + (trackH - thumbH) * scrollRatio;
			
			D2D1_COLOR_F trackCol = Theme::BorderSubtle();
			trackCol.a = 0.15f;
			renderer.FillRoundedRect(trackX, trackY, 4.0f, trackH, 2.0f, trackCol);
			
			if (scrollPtr && mouseClicked && PointInRect(mouseX, mouseY, trackX - 4.0f, trackY, 12.0f, trackH)) {
				float clickRatio = (mouseY - trackY) / trackH;
				*scrollPtr = maxScroll * clickRatio;
				ClampScrollOffsets();
			}
			
			bool thumbHovered = PointInRect(mouseX, mouseY, trackX - 4.0f, thumbY, 12.0f, thumbH);
			D2D1_COLOR_F thumbCol = (isDragScrolling || thumbHovered) ? Theme::AccentPrimary() : Theme::TextMuted();
			thumbCol.a = (isDragScrolling || thumbHovered) ? 0.6f : 0.3f;
			renderer.FillRoundedRect(trackX, thumbY, 4.0f, thumbH, 2.0f, thumbCol);
		}
	}

	DrawStatusBar();
	if (pluginManagerOpen)
		DrawPluginManagerModal();
	if (pluginSourceEditorOpen)
		DrawPluginSourceModal();
	if (updateModalOpen)
		DrawUpdateModal();
	Tooltip::Draw(renderer);
	renderer.EndFrame();

	mouseClicked = false;
	scrollDelta = 0;
}

void AetherApp::UpdateControls() {}

void AetherApp::SendFilterSettings() {
	if (!driver.isConnected)
		return;

	char cmd[256];

	if (filters.smoothingEnabled.value) {
		sprintf_s(cmd, "SmoothingInterval %d", (int)filters.smoothingInterval.value);
		driver.SendCommand(cmd);
		sprintf_s(cmd, "Smoothing %.2f", filters.smoothingLatency.value);
		driver.SendCommand(cmd);
	} else {
		driver.SendCommand("Smoothing off");
	}

	sprintf_s(cmd, "AntichatterEnabled %d", filters.antichatterEnabled.value ? 1 : 0);
	driver.SendCommand(cmd);
	if (filters.antichatterEnabled.value) {
		sprintf_s(cmd, "AntichatterStrength %.2f", filters.antichatterStrength.value);
		driver.SendCommand(cmd);
		sprintf_s(cmd, "AntichatterMultiplier %.2f", filters.antichatterMultiplier.value);
		driver.SendCommand(cmd);
		sprintf_s(cmd, "AntichatterOffsetX %.4f", filters.antichatterOffsetX.value);
		driver.SendCommand(cmd);
		sprintf_s(cmd, "AntichatterOffsetY %.4f", filters.antichatterOffsetY.value);
		driver.SendCommand(cmd);
	}

	if (filters.noiseEnabled.value) {
		sprintf_s(cmd, "Noise %.0f %.4f %.0f",
			filters.noiseBuffer.value, filters.noiseThreshold.value,
			filters.noiseIterations.value);
		driver.SendCommand(cmd);
	} else {
		driver.SendCommand("Noise off");
	}

	if (filters.velCurveEnabled.value) {
		sprintf_s(cmd, "PredictionEnabled 1");
		driver.SendCommand(cmd);
		sprintf_s(cmd, "PredictionStrength %.4f", filters.velCurveSmoothing.value);
		driver.SendCommand(cmd);
		sprintf_s(cmd, "PredictionSharpness %.4f", filters.velCurveSharpness.value);
		driver.SendCommand(cmd);
		sprintf_s(cmd, "PredictionOffsetX %.4f", filters.velCurveMinSpeed.value);
		driver.SendCommand(cmd);
		sprintf_s(cmd, "PredictionOffsetY %.4f", filters.velCurveMaxSpeed.value);
		driver.SendCommand(cmd);
	} else {
		driver.SendCommand("PredictionEnabled 0");
	}

	if (filters.reconstructorEnabled.value) {
		sprintf_s(cmd, "Reconstructor %.4f %.4f %.2f %.2f",
			filters.reconStrength.value, filters.reconVelSmooth.value,
			filters.reconAccelCap.value, filters.reconPredTime.value);
		driver.SendCommand(cmd);
	} else {
		driver.SendCommand("Reconstructor off");
	}

	if (filters.adaptiveEnabled.value) {
		sprintf_s(cmd, "Adaptive %.6f %.6f %.4f",
			filters.adaptiveProcessNoise.value, filters.adaptiveMeasNoise.value,
			filters.adaptiveVelWeight.value);
		driver.SendCommand(cmd);
	} else {
		driver.SendCommand("Adaptive off");
	}

	
	if (aether.enabled.value) {
		driver.SendCommand("AetherSmooth on");
		sprintf_s(cmd, "AS_LagRemoval %d %.4f",
			aether.lagRemovalEnabled.value ? 1 : 0, aether.lagRemovalStrength.value);
		driver.SendCommand(cmd);
		sprintf_s(cmd, "AS_Stabilizer %d %.4f %.6f",
			aether.stabilizerEnabled.value ? 1 : 0,
			aether.stabilizerStability.value, aether.stabilizerSensitivity.value);
		driver.SendCommand(cmd);
		sprintf_s(cmd, "AS_Snapping %d %.4f %.4f",
			aether.snappingEnabled.value ? 1 : 0,
			aether.snappingInner.value, aether.snappingOuter.value);
		driver.SendCommand(cmd);
		sprintf_s(cmd, "AS_RhythmFlow %d %.4f %.4f %.4f",
			aether.rhythmFlowEnabled.value ? 1 : 0,
			aether.rhythmFlowStrength.value,
			aether.rhythmFlowRelease.value,
			aether.rhythmFlowJitter.value);
		driver.SendCommand(cmd);
		sprintf_s(cmd, "AS_Suppress %d %.4f",
			aether.suppressionEnabled.value ? 1 : 0, aether.suppressionTime.value);
		driver.SendCommand(cmd);
	} else {
		driver.SendCommand("AetherSmooth off");
	}
}

void AetherApp::ApplyAllSettings() {
	if (!driver.isConnected) {
		AutoSaveConfig();
		return;
	}

	char cmd[256];
	ClampScreenArea();

	sprintf_s(cmd, "DesktopSize %.0f %.0f", detectedScreenW, detectedScreenH);
	driver.SendCommand(cmd);
	SendStaticMonitorInfoToDriver();

	{
		float actualScreenX = area.screenX.value + virtualScreenX;
		float actualScreenY = area.screenY.value + virtualScreenY;
		sprintf_s(cmd, "ScreenArea %.0f %.0f %.0f %.0f",
			area.screenWidth.value, area.screenHeight.value,
			actualScreenX, actualScreenY);
		driver.SendCommand(cmd);
	}

	sprintf_s(cmd, "TabletArea %.2f %.2f %.2f %.2f",
		area.tabletWidth.value, area.tabletHeight.value,
		area.tabletX.value, area.tabletY.value);
	driver.SendCommand(cmd);

	sprintf_s(cmd, "Rotate %.1f", area.rotation.value);
	driver.SendCommand(cmd);

	const char* modes[] = { "Mode Absolute", "Mode Relative", "Mode Digitizer" };
	driver.SendCommand(modes[outputMode.selected]);

	sprintf_s(cmd, "Sensitivity %.4f", area.screenWidth.value / area.tabletWidth.value);
	driver.SendCommand(cmd);

	driver.SendCommand("UpdateMonitorInfo");

	sprintf_s(cmd, "ButtonMap %d %d %d",
		buttonTip.selected, buttonBottom.selected, buttonTop.selected);
	driver.SendCommand(cmd);

	driver.SendCommand(areaClipping.value ? "AreaClipping on" : "AreaClipping off");
	driver.SendCommand(areaLimiting.value ? "AreaLimiting on" : "AreaLimiting off");

	sprintf_s(cmd, "TipThreshold %.0f", tipThreshold.value);
	driver.SendCommand(cmd);

	sprintf_s(cmd, "Overclock %s %.0f", overclockEnabled.value ? "on" : "off", overclockHz.value);
	driver.SendCommand(cmd);
	sprintf_s(cmd, "PenRateLimit %s %.0f", penRateLimitEnabled.value ? "on" : "off", penRateLimitHz.value);
	driver.SendCommand(cmd);

	SendFilterSettings();
	SendPluginSettings();

	AutoSaveConfig();
}

std::wstring AetherApp::GetConfigPath() {
	std::wstring dir = GetConfigDirectory();
	return dir + L"_autosave.cfg";
}

std::wstring AetherApp::GetConfigDirectory() {
	wchar_t exePath[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, exePath, MAX_PATH);
	std::wstring dir(exePath);
	size_t slash = dir.find_last_of(L"\\/");
	if (slash != std::wstring::npos)
		dir = dir.substr(0, slash + 1);
	return dir + L"config\\";
}

void AetherApp::EnsureConfigDirectory() {
	std::wstring dir = GetConfigDirectory();
	CreateDirectoryW(dir.c_str(), nullptr);
}

void AetherApp::RefreshConfigFiles() {
	EnsureConfigDirectory();
	std::wstring active = activeConfigPath.empty() ? GetConfigPath() : activeConfigPath;
	std::wstring selectedPath;
	if (selectedConfigIndex >= 0 && selectedConfigIndex < (int)configEntries.size()) {
		selectedPath = configEntries[selectedConfigIndex].path;
	}
	configEntries.clear();

	std::wstring pattern = GetConfigDirectory() + L"*.cfg";
	WIN32_FIND_DATAW data = {};
	HANDLE find = FindFirstFileW(pattern.c_str(), &data);
	if (find != INVALID_HANDLE_VALUE) {
		do {
			if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
				if (_wcsicmp(data.cFileName, L"_autosave.cfg") == 0)
					continue;
				ConfigEntry entry;
				entry.name = data.cFileName;
				entry.path = GetConfigDirectory() + data.cFileName;
				configEntries.push_back(entry);
			}
		} while (FindNextFileW(find, &data));
		FindClose(find);
	}

	std::sort(configEntries.begin(), configEntries.end(), [](const ConfigEntry& a, const ConfigEntry& b) {
		return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
	});

	selectedConfigIndex = -1;
	if (!selectedPath.empty()) {
		for (int i = 0; i < (int)configEntries.size(); i++) {
			if (_wcsicmp(configEntries[i].path.c_str(), selectedPath.c_str()) == 0) {
				selectedConfigIndex = i;
				break;
			}
		}
	}
	for (int i = 0; i < (int)configEntries.size(); i++) {
		if (selectedConfigIndex < 0 && _wcsicmp(configEntries[i].path.c_str(), active.c_str()) == 0) {
			selectedConfigIndex = i;
			break;
		}
	}
	if (selectedConfigIndex < 0 && !configEntries.empty()) {
		selectedConfigIndex = 0;
	}
}

void AetherApp::AutoSaveConfig() {
	TrackSettingsUndo();
	EnsureConfigDirectory();
	SaveConfig(GetConfigPath());
	RefreshConfigFiles();
}

void AetherApp::AutoLoadConfig() {
	EnsureConfigDirectory();
	std::wstring path = GetConfigPath();
	DWORD attrs = GetFileAttributesW(path.c_str());
	if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
		activeConfigPath.clear();
		LoadConfig(path);
	}
	RefreshConfigFiles();
}

void AetherApp::PrepareModalDialog() {
	if (hWnd && GetCapture() == hWnd)
		ReleaseCapture();
	mouseDown = false;
	mouseClicked = false;
	isDragScrolling = false;
	isPluginCatalogDragScrolling = false;
	isDraggingArea = false;
	middleMouseDown = false;
	rightMouseDown = false;
}

void AetherApp::SyncLoadedControlVisuals() {
	auto syncSlider = [](Slider& slider) {
		slider.value = Clamp(slider.value, slider.minVal, slider.maxVal);
		slider.animValue = slider.value;
		slider.editMode = false;
		slider.isDragging = false;
		slider.isMouseDraggingText = false;
		slider.ClearSelection();
	};
	auto syncToggle = [](Toggle& toggle) {
		toggle.animT = toggle.value ? 1.0f : 0.0f;
	};

	Slider* sliders[] = {
		&area.tabletWidth, &area.tabletHeight, &area.tabletX, &area.tabletY,
		&area.screenWidth, &area.screenHeight, &area.screenX, &area.screenY,
		&area.rotation, &area.aspectRatio,
		&dpiScale, &tipThreshold, &overclockHz, &penRateLimitHz,
		&filters.smoothingLatency, &filters.smoothingInterval,
		&filters.antichatterStrength, &filters.antichatterMultiplier,
		&filters.antichatterOffsetX, &filters.antichatterOffsetY,
		&filters.noiseBuffer, &filters.noiseThreshold, &filters.noiseIterations,
		&filters.velCurveMinSpeed, &filters.velCurveMaxSpeed,
		&filters.velCurveSmoothing, &filters.velCurveSharpness,
		&filters.snapRadius, &filters.snapSmooth,
		&filters.reconStrength, &filters.reconVelSmooth,
		&filters.reconAccelCap, &filters.reconPredTime,
		&filters.adaptiveProcessNoise, &filters.adaptiveMeasNoise, &filters.adaptiveVelWeight,
		&aether.lagRemovalStrength, &aether.stabilizerStability, &aether.stabilizerSensitivity,
		&aether.snappingInner, &aether.snappingOuter,
		&aether.rhythmFlowStrength, &aether.rhythmFlowRelease, &aether.rhythmFlowJitter,
		&aether.suppressionTime
	};
	for (Slider* slider : sliders)
		syncSlider(*slider);

	Toggle* toggles[] = {
		&area.customValues, &area.autoCenter, &area.lockAspect,
		&forceFullArea, &areaClipping, &areaLimiting,
		&overclockEnabled, &penRateLimitEnabled,
		&filters.smoothingEnabled, &filters.antichatterEnabled, &filters.noiseEnabled,
		&filters.velCurveEnabled, &filters.snapEnabled, &filters.reconstructorEnabled,
		&filters.adaptiveEnabled,
		&aether.enabled, &aether.lagRemovalEnabled, &aether.stabilizerEnabled,
		&aether.snappingEnabled, &aether.rhythmFlowEnabled, &aether.suppressionEnabled,
		&visualizerToggle
	};
	for (Toggle* toggle : toggles)
		syncToggle(*toggle);

	for (PluginEntry& plugin : pluginEntries) {
		syncToggle(plugin.enabled);
		for (auto& option : plugin.options) {
			if (option.kind == PluginEntry::PluginOption::ToggleOption)
				syncToggle(option.toggle);
			else
				syncSlider(option.slider);
		}
	}
}

bool AetherApp::SaveConfigWithDialog() {
	PrepareModalDialog();
	EnsureConfigDirectory();
	wchar_t filePath[MAX_PATH] = {};
	wcscpy_s(filePath, L"default.cfg");
	if (!activeConfigPath.empty()) {
		size_t slash = activeConfigPath.find_last_of(L"\\/");
		std::wstring name = (slash == std::wstring::npos) ? activeConfigPath : activeConfigPath.substr(slash + 1);
		wcsncpy_s(filePath, name.c_str(), _TRUNCATE);
	}

	std::wstring dir = GetConfigDirectory();
	OPENFILENAMEW ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFilter = L"Aether Config (*.cfg)\0*.cfg\0All Files (*.*)\0*.*\0";
	ofn.lpstrFile = filePath;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrInitialDir = dir.c_str();
	ofn.lpstrDefExt = L"cfg";
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

	if (!GetSaveFileNameW(&ofn))
		return false;

	activeConfigPath = filePath;
	SaveConfig(activeConfigPath);
	RefreshConfigFiles();
	return true;
}

bool AetherApp::LoadConfigWithDialog() {
	PrepareModalDialog();
	EnsureConfigDirectory();
	wchar_t filePath[MAX_PATH] = {};
	std::wstring dir = GetConfigDirectory();
	OPENFILENAMEW ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFilter = L"Aether Config (*.cfg)\0*.cfg\0All Files (*.*)\0*.*\0";
	ofn.lpstrFile = filePath;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrInitialDir = dir.c_str();
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

	if (!GetOpenFileNameW(&ofn))
		return false;

	activeConfigPath = filePath;
	LoadConfig(activeConfigPath);
	ApplyAllSettings();
	RefreshConfigFiles();
	return true;
}

std::wstring AetherApp::GetPluginDirectory() const {
	wchar_t exePath[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, exePath, MAX_PATH);

	std::wstring dir(exePath);
	size_t slash = dir.find_last_of(L"\\/");
	if (slash != std::wstring::npos)
		dir = dir.substr(0, slash + 1);

	return dir + L"plugins\\";
}

static bool DeleteDirectoryTree(const std::wstring& dir) {
	WIN32_FIND_DATAW data = {};
	HANDLE find = FindFirstFileW((dir + L"\\*").c_str(), &data);
	if (find != INVALID_HANDLE_VALUE) {
		do {
			if (lstrcmpW(data.cFileName, L".") == 0 || lstrcmpW(data.cFileName, L"..") == 0)
				continue;

			std::wstring path = dir + L"\\" + data.cFileName;
			if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				DeleteDirectoryTree(path);
			} else {
				SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
				DeleteFileW(path.c_str());
			}
		} while (FindNextFileW(find, &data));
		FindClose(find);
	}

	SetFileAttributesW(dir.c_str(), FILE_ATTRIBUTE_NORMAL);
	return RemoveDirectoryW(dir.c_str()) != 0 || GetLastError() == ERROR_FILE_NOT_FOUND;
}

void AetherApp::ConfigurePluginDefaults(PluginEntry& entry) {
	std::vector<PluginEntry::PluginOption> previousOptions = entry.options;
	entry.options.clear();

	auto restoreSlider = [&](PluginEntry::PluginOption& option) {
		for (const auto& previous : previousOptions) {
			if (previous.key == option.key) {
				option.slider.value = Clamp(previous.slider.value, option.slider.minVal, option.slider.maxVal);
				option.slider.animValue = option.slider.value;
				return;
			}
		}
	};
	auto restoreToggle = [&](PluginEntry::PluginOption& option) {
		for (const auto& previous : previousOptions) {
			if (previous.key == option.key) {
				option.toggle.value = previous.toggle.value;
				option.toggle.animT = previous.toggle.value ? 1.0f : 0.0f;
				return;
			}
		}
	};
	auto addSlider = [&](const char* key, const wchar_t* label, float minValue, float maxValue, float value, const wchar_t* format = L"%.2f") {
		PluginEntry::PluginOption option;
		option.kind = PluginEntry::PluginOption::SliderOption;
		option.key = key;
		option.label = label;
		option.slider.minVal = minValue;
		option.slider.maxVal = maxValue;
		option.slider.value = value;
		option.slider.animValue = value;
		option.slider.format = format;
		restoreSlider(option);
		entry.options.push_back(option);
	};
	auto addToggle = [&](const char* key, const wchar_t* label, bool value) {
		PluginEntry::PluginOption option;
		option.kind = PluginEntry::PluginOption::ToggleOption;
		option.key = key;
		option.label = label;
		option.toggle.value = value;
		option.toggle.animT = value ? 1.0f : 0.0f;
		restoreToggle(option);
		entry.options.push_back(option);
	};

	{
		std::wstring metadataName;
		std::wstring metadataDescription;
		std::vector<PluginOptionMetadata> metadataOptions;
		if (ReadAetherPluginMetadata(GetPluginDirectory() + entry.key, metadataName, metadataDescription, metadataOptions)) {
			if (!metadataName.empty())
				entry.name = metadataName;
			if (!metadataDescription.empty())
				entry.description = metadataDescription;
			for (const PluginOptionMetadata& metadata : metadataOptions) {
				if (metadata.type == 1) {
					addToggle(metadata.key.c_str(), metadata.label.c_str(), metadata.defaultValue > 0.5f);
				}
				else {
					const wchar_t* format = metadata.format.empty() ? L"%.2f" : metadata.format.c_str();
					addSlider(
						metadata.key.c_str(),
						metadata.label.c_str(),
						metadata.minValue,
						metadata.maxValue,
						Clamp(metadata.defaultValue, metadata.minValue, metadata.maxValue),
						format);
				}
			}
			if (!entry.options.empty())
				return;
		}
	}

	std::wstring identity = entry.key + L" " + entry.name + L" " + entry.dllName;
	if (WideContainsNoCase(identity, L"pleasant")) {
		entry.description = L"PleasantAim smoothing stack with lag removal, stabilizer, radial follow, debounce, and optional resampling.";
		addToggle("enableAntismoothing", L"Lag Removal", true);
		addSlider("antismoothing", L"Lag Strength", 0.1f, 2.0f, 0.6f);
		addToggle("enableSmoothing", L"Stabilizer", true);
		addSlider("stability", L"Stability", 0.01f, 20.0f, 1.0f);
		addSlider("speedSensitivity", L"Sensitivity", 0.0f, 1.0f, 0.015f, L"%.4f");
		addToggle("enableRadialFollow", L"Radial Follow", true);
		addSlider("radialInner", L"Inner Radius", 0.0f, 5.0f, 0.5f);
		addSlider("radialOuter", L"Outer Radius", 0.001f, 20.0f, 3.0f);
		addToggle("enableDebounce", L"Debounce", true);
		addSlider("debounceMs", L"Debounce (ms)", 0.0f, 100.0f, 5.0f);
		addToggle("enableResampling", L"Resampling", false);
		addSlider("outputFrequency", L"Output Hz", 60.0f, 2000.0f, 1000.0f, L"%.0f");
	}
	else if (WideContainsNoCase(identity, L"hawkusmoothing")) {
		entry.description = L"Hawku-style latency smoothing for steadier cursor movement.";
		addSlider("latency", L"Latency (ms)", 0.0f, 30.0f, 2.0f);
	}
	else if (WideContainsNoCase(identity, L"hawkunoise")) {
		entry.description = L"Hawku geometric-median noise reduction for tablet jitter.";
		addSlider("samples", L"Samples", 0.0f, 64.0f, 10.0f, L"%.0f");
		addSlider("threshold", L"Distance Threshold", 0.0f, 10.0f, 0.5f);
	}
	else if (WideContainsNoCase(identity, L"devocub")) {
		entry.description = L"Devocub antichatter with optional prediction controls.";
		addSlider("latency", L"Latency (ms)", 0.0f, 30.0f, 2.0f);
		addSlider("strength", L"Strength", 0.0f, 10.0f, 3.0f);
		addSlider("multiplier", L"Multiplier", 0.0f, 5.0f, 1.0f);
		addSlider("offsetX", L"Offset X", -5.0f, 5.0f, 0.0f);
		addSlider("offsetY", L"Offset Y", -5.0f, 5.0f, 1.0f);
		addToggle("prediction", L"Prediction", false);
		addSlider("predictionStrength", L"Prediction Strength", 0.0f, 5.0f, 1.1f);
		addSlider("predictionSharpness", L"Prediction Sharpness", 0.0f, 5.0f, 1.0f);
		addSlider("predictionOffsetX", L"Prediction Offset X", 0.0f, 10.0f, 3.0f);
		addSlider("predictionOffsetY", L"Prediction Offset Y", -2.0f, 2.0f, 0.3f);
	}
	else {
		entry.description = L"Native Aether DLL plugin. This plugin did not expose option metadata, so only enable state is available.";
	}
}

void AetherApp::RefreshPluginList() {
	std::vector<PluginEntry> previousEntries = pluginEntries;
	pluginEntries.clear();

	std::wstring root = GetPluginDirectory();
	CreateDirectoryW(root.c_str(), nullptr);

	WIN32_FIND_DATAW folderData = {};
	HANDLE folderFind = FindFirstFileW((root + L"*").c_str(), &folderData);
	if (folderFind != INVALID_HANDLE_VALUE) {
		do {
			if (!(folderData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				continue;
			if (lstrcmpW(folderData.cFileName, L".") == 0 || lstrcmpW(folderData.cFileName, L"..") == 0)
				continue;

			std::wstring folder = root + folderData.cFileName + L"\\";
			WIN32_FIND_DATAW dllData = {};
			HANDLE dllFind = FindFirstFileW((folder + L"*.dll").c_str(), &dllData);
			if (dllFind == INVALID_HANDLE_VALUE)
				continue;

			do {
				if (dllData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
					continue;

				PluginEntry entry;
				entry.key = std::wstring(folderData.cFileName) + L"\\" + dllData.cFileName;
				entry.name = folderData.cFileName;
				entry.dllName = dllData.cFileName;
				entry.enabled.value = true;
				entry.enabled.animT = 1.0f;
				for (const PluginEntry& previous : previousEntries) {
					if (_wcsicmp(previous.key.c_str(), entry.key.c_str()) == 0) {
						entry.enabled.value = previous.enabled.value;
						entry.enabled.animT = previous.enabled.value ? 1.0f : 0.0f;
						entry.options = previous.options;
						break;
					}
				}
				ConfigurePluginDefaults(entry);
				pluginEntries.push_back(entry);
			} while (FindNextFileW(dllFind, &dllData));

			FindClose(dllFind);
		} while (FindNextFileW(folderFind, &folderData));

		FindClose(folderFind);
	}

	if (pluginEntries.empty()) {
		pluginListStatus = L"No native DLL plugins found";
	} else {
		pluginListStatus = std::to_wstring(pluginEntries.size()) + L" native DLL plugin(s)";
	}

	pluginListDirty = false;
}

void AetherApp::SendPluginSettings() {
	if (pluginListDirty)
		RefreshPluginList();

	if (!driver.isConnected)
		return;

	for (size_t i = 0; i < pluginEntries.size(); ++i) {
		SendPluginEnable(i);
	}
	for (size_t i = 0; i < pluginEntries.size(); ++i) {
		SendPluginOptions(i);
	}
}

void AetherApp::SendPluginEnable(size_t pluginIndex) {
	if (!driver.isConnected || pluginIndex >= pluginEntries.size())
		return;

	std::string key = WideToUtf8(pluginEntries[pluginIndex].key);
	driver.SendCommand("PluginEnable " + QuoteCommandArg(key) + " " + (pluginEntries[pluginIndex].enabled.value ? "on" : "off"));
}

void AetherApp::SendPluginOption(size_t pluginIndex, const PluginEntry::PluginOption& option) {
	if (!driver.isConnected || pluginIndex >= pluginEntries.size())
		return;

	std::string pluginKey = QuoteCommandArg(WideToUtf8(pluginEntries[pluginIndex].key));
	char cmd[256];
	if (option.kind == PluginEntry::PluginOption::ToggleOption) {
		sprintf_s(cmd, "PluginSet %s %s %d", pluginKey.c_str(), option.key.c_str(), option.toggle.value ? 1 : 0);
	}
	else {
		sprintf_s(cmd, "PluginSet %s %s %.6f", pluginKey.c_str(), option.key.c_str(), option.slider.value);
	}
	driver.SendCommand(cmd);
}

void AetherApp::SendPluginOptions(size_t pluginIndex) {
	if (!driver.isConnected || pluginIndex >= pluginEntries.size())
		return;

	for (const auto& option : pluginEntries[pluginIndex].options) {
		SendPluginOption(pluginIndex, option);
	}
}

void AetherApp::UpdatePluginCatalogInstallState() {
	for (auto& catalog : pluginCatalogEntries) {
		catalog.installed = false;
		catalog.needsUpdate = false;
		catalog.installedIdentity.clear();

		std::wstring catalogIdentity = catalog.name + L" " + Utf8ToWide(catalog.sourcePath);
		std::wstring catalogToken = NormalizePluginIdentity(catalog.name + L" " + Utf8ToWide(catalog.sourcePath));
		for (const auto& plugin : pluginEntries) {
			std::wstring pluginIdentity = plugin.key + L" " + plugin.name + L" " + plugin.dllName;
			if (CatalogNamesMatch(pluginIdentity, catalogIdentity)) {
				catalog.installed = true;
				catalog.installedIdentity = plugin.key;
				std::wstring pluginToken = NormalizePluginIdentity(plugin.name + L" " + plugin.dllName + L" " + plugin.key);
				catalog.needsUpdate = catalog.sourcePort && !catalogToken.empty() && !pluginToken.empty() && pluginToken != catalogToken;
				break;
			}
		}
		catalog.nativeAvailable = catalog.nativeAvailable || catalog.sourcePort || catalog.installed;
	}
}

void AetherApp::ClampPluginCatalogScroll() {
	if (pluginCatalogScrollY < 0.0f)
		pluginCatalogScrollY = 0.0f;

	float overlayH = Theme::Runtime::WindowHeight;
	float h = std::min(650.0f, overlayH - 54.0f);
	float listH = h - 148.0f;
	float visibleH = std::max(0.0f, listH - 52.0f);
	float contentH = (float)pluginCatalogEntries.size() * 30.0f + 8.0f;
	float maxScroll = contentH - visibleH;
	if (maxScroll < 0.0f)
		maxScroll = 0.0f;
	if (pluginCatalogScrollY > maxScroll)
		pluginCatalogScrollY = maxScroll;
}

bool AetherApp::RefreshPluginCatalog() {
	pluginCatalogEntries.clear();
	pluginCatalogStatus = L"Loading repository...";

	if (pluginListDirty)
		RefreshPluginList();

	std::wstring sourceOwner = pluginRepoOwner.empty() ? L"OpenTabletDriver" : pluginRepoOwner;
	std::wstring sourceName = pluginRepoName.empty() ? L"Plugin-Repository" : pluginRepoName;
	std::wstring sourceRef = pluginRepoRef.empty() ? L"master" : pluginRepoRef;
	std::wstring sourceTreePath =
		L"/repos/" + sourceOwner + L"/" + sourceName +
		L"/git/trees/" + sourceRef + L"?recursive=1";

	std::string sourceTreeJson;
	bool sourceTreeLoaded = HttpGetUtf8(L"api.github.com", sourceTreePath, sourceTreeJson);
	if (!sourceTreeLoaded && pluginManagerTab == 0 && pluginEntries.empty()) {
		pluginCatalogStatus = L"Failed to load GitHub repository tree";
		return false;
	}

	std::string sourcePrefix = pluginManagerTab == 0 ? "Plugins/" : "AetherOTDPorts/";
	std::vector<std::string> allPaths = sourceTreeLoaded ? JsonTreePaths(sourceTreeJson, false) : std::vector<std::string>();
	std::set<std::string> sourceFolders;
	for (const std::string& path : allPaths) {
		if (path.find(sourcePrefix) != 0)
			continue;
		size_t nextSlash = path.find('/', sourcePrefix.size());
		if (nextSlash == std::string::npos || nextSlash == sourcePrefix.size())
			continue;
		sourceFolders.insert(path.substr(sourcePrefix.size(), nextSlash - sourcePrefix.size()));
	}

	auto applySourceMetadata = [&](PluginCatalogEntry& entry) {
		const char* metadataNames[] = { "aether-plugin.json", "plugin.json", "metadata.json" };
		for (const char* metaName : metadataNames) {
			std::string metadataPath = entry.sourcePath + "/" + metaName;
			std::wstring rawPath = L"/" + sourceOwner + L"/" + sourceName + L"/" + sourceRef + L"/" + Utf8ToWide(metadataPath);
			std::string json;
			if (!HttpGetUtf8(L"raw.githubusercontent.com", rawPath, json))
				continue;

			std::wstring metaNameValue = Utf8ToWide(JsonStringValue(json, "Name"));
			std::wstring metaOwner = Utf8ToWide(JsonStringValue(json, "Owner"));
			std::wstring metaDescription = Utf8ToWide(JsonStringValue(json, "Description"));
			std::wstring metaVersion = Utf8ToWide(JsonStringValue(json, "PluginVersion"));
			std::wstring metaRepository = Utf8ToWide(JsonStringValue(json, "RepositoryUrl"));
			std::wstring metaWiki = Utf8ToWide(JsonStringValue(json, "WikiUrl"));
			std::wstring metaLicense = Utf8ToWide(JsonStringValue(json, "LicenseIdentifier"));
			if (!metaNameValue.empty()) entry.name = metaNameValue;
			if (!metaOwner.empty()) entry.owner = metaOwner;
			if (!metaDescription.empty()) entry.description = metaDescription;
			if (!metaVersion.empty()) entry.version = metaVersion;
			if (!metaRepository.empty()) entry.repositoryUrl = metaRepository;
			if (!metaWiki.empty()) entry.wikiUrl = metaWiki;
			if (!metaLicense.empty()) entry.license = metaLicense;
			break;
		}
	};

	auto findPortFolder = [&](const PluginCatalogEntry& entry) -> std::string {
		std::wstring identity = entry.name + L" " + entry.owner + L" " + entry.description;
		for (const std::string& folder : sourceFolders) {
			if (CatalogNamesMatch(identity, Utf8ToWide(folder)))
				return folder;
		}
		return std::string();
	};

	int loaded = 0;
	if (pluginManagerTab == 0) {
		for (const std::string& folder : sourceFolders) {
			PluginCatalogEntry entry;
			entry.name = Utf8ToWide(folder);
			entry.owner = sourceOwner;
			entry.description = L"Aether C++ filter source from Plugins.";
			entry.driverVersion = L"Aether";
			entry.sourcePath = sourcePrefix + folder;
			entry.sourcePort = true;
			entry.nativeAvailable = true;
			entry.repositoryUrl = L"https://github.com/" + sourceOwner + L"/" + sourceName + L"/tree/" + sourceRef + L"/" + Utf8ToWide(entry.sourcePath);
			applySourceMetadata(entry);
			pluginCatalogEntries.push_back(entry);
			loaded++;
		}

		for (const PluginEntry& plugin : pluginEntries) {
			bool alreadyListed = false;
			std::wstring pluginIdentity = plugin.key + L" " + plugin.name + L" " + plugin.dllName;
			for (const PluginCatalogEntry& entry : pluginCatalogEntries) {
				if (CatalogNamesMatch(pluginIdentity, entry.name + L" " + Utf8ToWide(entry.sourcePath))) {
					alreadyListed = true;
					break;
				}
			}
			if (alreadyListed)
				continue;

			PluginCatalogEntry local;
			local.name = plugin.name.empty() ? plugin.dllName : plugin.name;
			local.owner = L"Local";
			local.description = plugin.description.empty() ? L"Installed native Aether DLL filter." : plugin.description;
			local.version = L"Installed";
			local.driverVersion = L"Aether";
			local.downloadUrl = plugin.dllName;
			local.nativeAvailable = true;
			local.installed = true;
			pluginCatalogEntries.push_back(local);
		}
	}
	else {
		std::wstring otdOwner = L"OpenTabletDriver";
		std::wstring otdName = L"Plugin-Repository";
		std::wstring otdRef = L"master";
		std::wstring otdTreePath = L"/repos/" + otdOwner + L"/" + otdName + L"/git/trees/" + otdRef + L"?recursive=1";
		std::string otdTreeJson;
		if (!HttpGetUtf8(L"api.github.com", otdTreePath, otdTreeJson)) {
			pluginCatalogStatus = L"Failed to load OTD plugin repository";
			return false;
		}

		std::vector<std::string> paths = JsonTreePaths(otdTreeJson);
		std::set<std::wstring> names;
		int metadataRequestBudget = 96;
		for (const std::string& path : paths) {
			if (path.find("Repository/0.6.0.0/") == std::string::npos &&
				path.find("Repository/0.6.6.0/") == std::string::npos &&
				path.find("Repository/0.6.") == std::string::npos)
				continue;

			if (metadataRequestBudget-- <= 0)
				break;

			std::wstring rawPath = L"/" + otdOwner + L"/" + otdName + L"/" + otdRef + L"/" + Utf8ToWide(path);
			std::string json;
			if (!HttpGetUtf8(L"raw.githubusercontent.com", rawPath, json))
				continue;

			PluginCatalogEntry entry;
			entry.name = Utf8ToWide(JsonStringValue(json, "Name"));
			entry.owner = Utf8ToWide(JsonStringValue(json, "Owner"));
			entry.description = Utf8ToWide(JsonStringValue(json, "Description"));
			entry.version = Utf8ToWide(JsonStringValue(json, "PluginVersion"));
			entry.driverVersion = Utf8ToWide(JsonStringValue(json, "SupportedDriverVersion"));
			entry.downloadUrl = Utf8ToWide(JsonStringValue(json, "DownloadUrl"));
			entry.repositoryUrl = Utf8ToWide(JsonStringValue(json, "RepositoryUrl"));
			entry.wikiUrl = Utf8ToWide(JsonStringValue(json, "WikiUrl"));
			entry.license = Utf8ToWide(JsonStringValue(json, "LicenseIdentifier"));
			if (entry.name.empty())
				continue;
			std::wstring token = CatalogToken(entry.name);
			if (!names.insert(token).second)
				continue;

			std::string portFolder = findPortFolder(entry);
			if (!portFolder.empty()) {
				entry.sourcePath = sourcePrefix + portFolder;
				entry.sourcePort = true;
				entry.nativeAvailable = true;
				entry.repositoryUrl = L"https://github.com/" + sourceOwner + L"/" + sourceName + L"/tree/" + sourceRef + L"/" + Utf8ToWide(entry.sourcePath);
				applySourceMetadata(entry);
			}

			pluginCatalogEntries.push_back(entry);
			loaded++;
		}
	}

	UpdatePluginCatalogInstallState();
	if (pluginCatalogEntries.empty())
		pluginCatalogStatus = pluginManagerTab == 0
			? L"No Aether filters found in Plugins/ or local DLL plugins"
			: L"No OTD plugin metadata found";
	else
		pluginCatalogStatus = std::to_wstring(pluginCatalogEntries.size()) +
			(pluginManagerTab == 0 ? L" Aether filter(s)" : L" OTD plugin(s)");

	if (selectedCatalogIndex >= (int)pluginCatalogEntries.size())
		selectedCatalogIndex = (int)pluginCatalogEntries.size() - 1;
	if (selectedCatalogIndex < 0)
		selectedCatalogIndex = 0;
	pluginCatalogAnimT = 0.0f;

	return !pluginCatalogEntries.empty();
}

bool AetherApp::DeleteInstalledPlugin(size_t index) {
	if (index >= pluginEntries.size())
		return false;

	bool restartService = driver.isConnected;
	if (restartService) {
		driver.SendCommand("PluginEnable " + QuoteCommandArg(WideToUtf8(pluginEntries[index].key)) + " off");
		driver.SendCommand("PluginReload");
		Sleep(100);
		driver.Stop();
		Sleep(250);
	}

	size_t slash = pluginEntries[index].key.find(L'\\');
	std::wstring folderName = slash == std::wstring::npos ? pluginEntries[index].name : pluginEntries[index].key.substr(0, slash);
	std::wstring folderPath = GetPluginDirectory() + folderName;
	bool ok = DeleteDirectoryTree(folderPath);
	RefreshPluginList();
	if (restartService) {
		StartDriverService();
	}
	if (driver.isConnected) {
		driver.SendCommand("PluginReload");
		SendPluginSettings();
		driver.SendCommand("PluginList");
	}
	UpdatePluginCatalogInstallState();
	return ok;
}

bool AetherApp::InstallPluginWithDialog() {
	PrepareModalDialog();
	wchar_t filePath[MAX_PATH] = {};
	OPENFILENAMEW ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFilter = L"Aether Native Plugin (*.dll)\0*.dll\0All Files (*.*)\0*.*\0";
	ofn.lpstrFile = filePath;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrDefExt = L"dll";
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

	if (!GetOpenFileNameW(&ofn))
		return false;

	if (!driver.isConnected)
		StartDriverService();

	if (driver.isConnected) {
		std::string path = WideToUtf8(filePath);
		driver.SendCommand("PluginInstall \"" + path + "\"");
		driver.SendCommand("PluginList");
		Sleep(80);
		RefreshPluginList();
		SendPluginSettings();
		if (pluginManagerOpen && pluginManagerTab == 0)
			RefreshPluginCatalog();
		else
			UpdatePluginCatalogInstallState();
		return true;
	}

	RefreshPluginList();
	if (pluginManagerOpen && pluginManagerTab == 0)
		RefreshPluginCatalog();
	return false;
}

bool AetherApp::InstallPluginSourceWithDialog() {
	PrepareModalDialog();
	HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	bool shouldUninit = SUCCEEDED(initHr);

	IFileOpenDialog* dialog = nullptr;
	HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
	if (FAILED(hr)) {
		if (shouldUninit)
			CoUninitialize();
		pluginListStatus = L"Folder picker is not available";
		return false;
	}

	DWORD options = 0;
	dialog->GetOptions(&options);
	dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
	dialog->SetTitle(L"Select Aether plugin source folder");

	bool installed = false;
	if (SUCCEEDED(dialog->Show(hWnd))) {
		IShellItem* item = nullptr;
		if (SUCCEEDED(dialog->GetResult(&item))) {
			PWSTR folderPath = nullptr;
			if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &folderPath))) {
				std::wstring dllPath;
				std::wstring status;
				if (BuildAetherPluginSourceFolder(folderPath, dllPath, status)) {
					if (!driver.isConnected)
						StartDriverService();
					if (driver.isConnected) {
						driver.SendCommand("PluginInstall \"" + WideToUtf8(dllPath) + "\"");
						driver.SendCommand("PluginList");
						Sleep(100);
						RefreshPluginList();
						SendPluginSettings();
						if (pluginManagerOpen && pluginManagerTab == 0)
							RefreshPluginCatalog();
						else
							UpdatePluginCatalogInstallState();
						installed = true;
					}
					else {
						status = L"Build completed, but service is offline";
					}
				}
				pluginListStatus = status;
				pluginCatalogStatus = status;
				CoTaskMemFree(folderPath);
			}
			item->Release();
		}
	}

	dialog->Release();
	if (shouldUninit)
		CoUninitialize();
	return installed;
}

void AetherApp::OpenExternalUrl(const std::wstring& url) {
	if (url.empty()) {
		pluginCatalogStatus = L"No URL available";
		return;
	}

	HINSTANCE result = ShellExecuteW(hWnd, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	if ((INT_PTR)result <= 32)
		pluginCatalogStatus = L"Failed to open URL";
}

void AetherApp::ShowUpdateModal(const std::wstring& latestTag, const std::wstring& currentVersion, const std::wstring& releaseUrl) {
	updateLatestTag = latestTag;
	updateCurrentVersion = currentVersion;
	updateReleaseUrl = releaseUrl;
	updateModalOpen = true;
}

bool AetherApp::DownloadGitHubSourcePort(const PluginCatalogEntry& entry, std::wstring& folderPath, std::wstring& status) {
	if (entry.sourcePath.empty()) {
		status = L"Selected plugin has no source folder mapping";
		return false;
	}

	std::wstring repoOwner = pluginRepoOwner.empty() ? L"OpenTabletDriver" : pluginRepoOwner;
	std::wstring repoName = pluginRepoName.empty() ? L"Plugin-Repository" : pluginRepoName;
	std::wstring repoRef = pluginRepoRef.empty() ? L"master" : pluginRepoRef;
	std::wstring treePath = L"/repos/" + repoOwner + L"/" + repoName + L"/git/trees/" + repoRef + L"?recursive=1";

	std::string treeJson;
	if (!HttpGetUtf8(L"api.github.com", treePath, treeJson)) {
		status = L"Failed to read GitHub source tree";
		return false;
	}

	std::vector<std::string> paths = JsonTreePaths(treeJson, false);
	std::string prefix = entry.sourcePath + "/";
	std::wstring downloadRoot = GetPluginDirectory() + L"_sources\\" + SanitizePathSegment(entry.name) + L"\\";
	DeleteDirectoryTree(downloadRoot);
	std::error_code ec;
	std::filesystem::create_directories(downloadRoot, ec);
	if (ec) {
		status = L"Failed to create source download folder";
		return false;
	}

	int downloaded = 0;
	for (const std::string& path : paths) {
		if (path.find(prefix) != 0)
			continue;

		std::string relative = path.substr(prefix.size());
		if (relative.empty())
			continue;

		std::wstring rawPath = L"/" + repoOwner + L"/" + repoName + L"/" + repoRef + L"/" + Utf8ToWide(path);
		std::string body;
		if (!HttpGetUtf8(L"raw.githubusercontent.com", rawPath, body))
			continue;

		std::filesystem::path outputPath = std::filesystem::path(downloadRoot) / Utf8ToWide(relative);
		std::filesystem::create_directories(outputPath.parent_path(), ec);
		if (ec)
			continue;

		std::ofstream out(outputPath, std::ios::binary);
		if (!out.good())
			continue;
		out.write(body.data(), (std::streamsize)body.size());
		downloaded++;
	}

	if (downloaded == 0) {
		status = L"No source files were downloaded from the GitHub source folder";
		return false;
	}

	folderPath = downloadRoot;
	status = L"Downloaded " + std::to_wstring(downloaded) + L" source file(s)";
	return true;
}

bool AetherApp::InstallRepositoryPlugin(PluginCatalogEntry& entry) {
	bool replacingInstalled = entry.installed;
	std::wstring replaceKey = entry.installedIdentity;

	if (entry.sourcePort) {
		std::wstring sourceFolder;
		std::wstring status;
		if (!DownloadGitHubSourcePort(entry, sourceFolder, status)) {
			pluginCatalogStatus = status;
			return false;
		}

		std::wstring dllPath;
		if (!BuildAetherPluginSourceFolder(sourceFolder, dllPath, status)) {
			pluginCatalogStatus = status;
			return false;
		}

		if (!driver.isConnected)
			StartDriverService();
		if (!driver.isConnected) {
			pluginCatalogStatus = L"Source port built, but service is offline";
			return false;
		}

		if (replacingInstalled && !replaceKey.empty())
			driver.SendCommand("PluginEnable " + QuoteCommandArg(WideToUtf8(replaceKey)) + " off");

		driver.SendCommand("PluginInstall \"" + WideToUtf8(dllPath) + "\"");
		driver.SendCommand("PluginReload");
		driver.SendCommand("PluginList");
		Sleep(120);
		RefreshPluginList();
		SendPluginSettings();
		UpdatePluginCatalogInstallState();
		pluginCatalogStatus = replacingInstalled ? L"Rebuilt and replaced installed source port" : L"Built and installed source port";
		entry.installed = true;
		entry.needsUpdate = false;
		return true;
	}

	wchar_t userProfile[MAX_PATH] = {};
	GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH);
	std::wstring desktop111 = std::wstring(userProfile) + L"\\Desktop\\111\\";

	std::wstring sourceDir;
	std::wstring expectedDll;
	std::wstring identity = entry.name + L" " + entry.description + L" " + entry.downloadUrl;
	if (WideContainsNoCase(identity, L"Pleasant")) {
		sourceDir = desktop111 + L"PleasantAim";
		expectedDll = sourceDir + L"\\bin\\Release\\native\\PleasantAim.dll";
	}
	else if (WideContainsNoCase(identity, L"Hawku") && WideContainsNoCase(identity, L"Noise")) {
		sourceDir = desktop111 + L"OTDNativePorts\\TabletDriverFilters";
		expectedDll = sourceDir + L"\\bin\\Release\\TabletDriverFilters\\OTD_HawkuNoiseReduction.dll";
	}
	else if (WideContainsNoCase(identity, L"Hawku") || WideContainsNoCase(identity, L"Smoothing")) {
		sourceDir = desktop111 + L"OTDNativePorts\\TabletDriverFilters";
		expectedDll = sourceDir + L"\\bin\\Release\\TabletDriverFilters\\OTD_HawkuSmoothing.dll";
	}
	else if (WideContainsNoCase(identity, L"Devocub") || WideContainsNoCase(identity, L"Antichatter")) {
		sourceDir = desktop111 + L"OTDNativePorts\\TabletDriverFilters";
		expectedDll = sourceDir + L"\\bin\\Release\\TabletDriverFilters\\OTD_DevocubAntichatter.dll";
	}
	else if (WideContainsNoCase(identity, L"Bezier")) {
		sourceDir = desktop111 + L"AetherGUI\\AetherOTDPorts\\BezierInterpolatorAether";
		expectedDll = sourceDir + L"\\bin\\Release\\BezierInterpolatorAether.dll";
	}
	else if (WideContainsNoCase(identity, L"Radial") || WideContainsNoCase(identity, L"Follow")) {
		sourceDir = desktop111 + L"AetherGUI\\AetherOTDPorts\\RadialFollowAether";
		expectedDll = sourceDir + L"\\bin\\Release\\RadialFollowAether.dll";
	}

	if (sourceDir.empty()) {
		pluginCatalogStatus = L"No native Aether port is mapped yet. Use Build Source with an Aether plugin source folder.";
		return false;
	}
	if (!DirectoryExistsWide(sourceDir)) {
		pluginCatalogStatus = L"Native port source folder was not found";
		return false;
	}

	std::wstring status;
	std::wstring script = FindBuildScript(sourceDir);
	DWORD exitCode = 1;
	if (!script.empty()) {
		std::wstring commandLine = L"powershell.exe -ExecutionPolicy Bypass -File " + QuoteArg(script);
		if (!RunHiddenProcess(commandLine, sourceDir, exitCode) || exitCode != 0) {
			wchar_t buffer[128];
			swprintf_s(buffer, L"Native port build failed (%lu)", exitCode);
			pluginCatalogStatus = buffer;
			return false;
		}
	}
	else {
		std::wstring builtDll;
		if (!BuildAetherPluginSourceFolder(sourceDir, builtDll, status)) {
			pluginCatalogStatus = status;
			return false;
		}
		expectedDll = builtDll;
	}

	if (!FileExistsWide(expectedDll) || !DllHasAetherExports(expectedDll)) {
		pluginCatalogStatus = L"Native port did not produce a valid Aether DLL";
		return false;
	}

	if (!driver.isConnected)
		StartDriverService();
	if (!driver.isConnected) {
		pluginCatalogStatus = L"Native port built, but service is offline";
		return false;
	}

	if (replacingInstalled && !replaceKey.empty())
		driver.SendCommand("PluginEnable " + QuoteCommandArg(WideToUtf8(replaceKey)) + " off");
	driver.SendCommand("PluginInstall \"" + WideToUtf8(expectedDll) + "\"");
	driver.SendCommand("PluginReload");
	driver.SendCommand("PluginList");
	Sleep(120);
	RefreshPluginList();
	SendPluginSettings();
	UpdatePluginCatalogInstallState();
	pluginCatalogStatus = replacingInstalled ? L"Rebuilt and replaced native Aether port" : L"Installed native Aether port";
	entry.installed = true;
	entry.needsUpdate = false;
	return true;
}

void AetherApp::ClampScrollOffsets() {
	if (areaScrollY < 0) areaScrollY = 0;
	if (filterScrollY < 0) filterScrollY = 0;
	if (settingsScrollY < 0) settingsScrollY = 0;

	float visibleH = GetContentAreaBottom() - GetContentAreaTop();
	float maxScroll = areaContentH - visibleH;
	if (maxScroll < 0) maxScroll = 0;
	if (areaScrollY > maxScroll) areaScrollY = maxScroll;

	maxScroll = filterContentH - visibleH;
	if (maxScroll < 0) maxScroll = 0;
	if (filterScrollY > maxScroll) filterScrollY = maxScroll;

	maxScroll = settingsContentH - visibleH;
	if (maxScroll < 0) maxScroll = 0;
	if (settingsScrollY > maxScroll) settingsScrollY = maxScroll;
}

void AetherApp::DrawBackground() {
	float w = Theme::Runtime::WindowWidth;
	float h = Theme::Runtime::WindowHeight;

	
	
	
	D2D1_COLOR_F contentBg = LerpColor(Theme::BgDeep(), Theme::BgBase(), 0.85f);
	renderer.FillRect(0, 0, w, h, contentBg);

	bgAnimT += deltaTime;

	
	D2D1_COLOR_F dot = Theme::BorderNormal();
	dot.a = 0.04f;
	float startX = Theme::Size::SidebarWidth + 20.0f;
	float startY = Theme::Size::HeaderHeight + 18.0f;
	for (float gy = startY; gy < h - 36.0f; gy += 28.0f) {
		for (float gx = startX; gx < w - 12.0f; gx += 28.0f) {
			renderer.FillCircle(gx, gy, 0.6f, dot);
		}
	}

	if (particleStyle == 3) return;

	if (particleStyle == 0) {
		if (!twinkleStarsInitialized) {
			float minX = Theme::Size::SidebarWidth + 18.0f;
			float minY = Theme::Size::HeaderHeight + 14.0f;
			float spanX = std::max(1.0f, w - minX - 18.0f);
			float spanY = std::max(1.0f, h - minY - 38.0f);
			for (int i = 0; i < MAX_TWINKLE_STARS; i++) {
				TwinkleStar& ts = twinkleStars[i];
				ts.x = minX + (float)(rand() % (int)spanX);
				ts.y = minY + (float)(rand() % (int)spanY);
				ts.phase = (rand() % 628) / 100.0f;
				ts.speed = 0.7f + (rand() % 140) / 100.0f;
				ts.size = 0.55f + (rand() % 120) / 100.0f;
				ts.alpha = 0.04f + (rand() % 12) / 100.0f;
			}
			twinkleStarsInitialized = true;
		}

		for (int i = 0; i < MAX_TWINKLE_STARS; i++) {
			TwinkleStar& ts = twinkleStars[i];
			float pulse = sinf(bgAnimT * ts.speed + ts.phase) * 0.5f + 0.5f;
			float alpha = ts.alpha * (0.35f + pulse * 0.85f);
			D2D1_COLOR_F starCol = LerpColor(Theme::TextMuted(), Theme::AccentPrimary(), pulse * 0.35f);
			starCol.a = alpha;
			renderer.FillCircle(ts.x, ts.y, ts.size, starCol);
			if (ts.size > 1.2f && pulse > 0.68f) {
				D2D1_COLOR_F rayCol = starCol;
				rayCol.a = alpha * 0.55f;
				float ray = ts.size * (2.2f + pulse);
				renderer.DrawLine(ts.x - ray, ts.y, ts.x + ray, ts.y, rayCol, 0.7f);
				renderer.DrawLine(ts.x, ts.y - ray, ts.x, ts.y + ray, rayCol, 0.7f);
			}
		}

		starSpawnTimer -= deltaTime;
		if (starSpawnTimer <= 0.0f) {
			for (int i = 0; i < MAX_STARS; i++) {
				if (!stars[i].active) {
					ShootingStar& s = stars[i];
					s.active = true;
					bool fromTop = (rand() % 3) != 0;
					if (fromTop) {
						s.x = w * 0.1f + (rand() % (int)(w * 0.9f));
						s.y = -10.0f;
					} else {
						s.x = w + 10.0f;
						s.y = (float)(rand() % (int)(h * 0.4f));
					}
					float angle = 2.7f + (rand() % 80) / 100.0f;
					float speed = 100.0f + (float)(rand() % 220);
					s.vx = cosf(angle) * speed;
					s.vy = sinf(angle) * speed;
					s.maxLife = 1.8f + (rand() % 250) / 100.0f;
					s.life = s.maxLife;
					s.tailLen = 35.0f + (rand() % 60);
					s.brightness = 0.05f + (rand() % 10) / 100.0f;
					break;
				}
			}
			starSpawnTimer = 2.0f + (rand() % 400) / 100.0f;
		}

		for (int i = 0; i < MAX_STARS; i++) {
			ShootingStar& s = stars[i];
			if (!s.active) continue;

			s.x += s.vx * deltaTime;
			s.y += s.vy * deltaTime;
			s.life -= deltaTime;

			if (s.life <= 0 || s.x < -100 || s.x > w + 100 || s.y > h + 100) {
				s.active = false;
				continue;
			}

			float lifeT = s.life / s.maxLife;
			float fade = (lifeT > 0.85f) ? (1.0f - lifeT) / 0.15f : (lifeT < 0.25f) ? lifeT / 0.25f : 1.0f;
			float alpha = s.brightness * fade;

			float speed = sqrtf(s.vx * s.vx + s.vy * s.vy);
			float nx = (speed > 0.1f) ? s.vx / speed : 0;
			float ny = (speed > 0.1f) ? s.vy / speed : 0;

			
			D2D1_COLOR_F glowCol = Theme::AccentPrimary();
			glowCol.a = alpha * 0.3f;
			renderer.FillCircle(s.x, s.y, 4.0f, glowCol);

			
			int segments = 8;
			for (int seg = 0; seg < segments; seg++) {
				float t = (float)seg / (float)segments;
				float tx = s.x - nx * s.tailLen * t;
				float ty = s.y - ny * s.tailLen * t;
				float segAlpha = alpha * (1.0f - t * 0.9f);
				float segR = 1.4f * (1.0f - t * 0.7f);
				D2D1_COLOR_F starCol = LerpColor(Theme::AccentPrimary(), Theme::AccentSecondary(), t);
				starCol.a = segAlpha;
				renderer.FillCircle(tx, ty, segR, starCol);
			}

			
			D2D1_COLOR_F headCol = D2D1::ColorF(0xFFFFFF, alpha * 1.8f);
			renderer.FillCircle(s.x, s.y, 1.6f, headCol);
		}
	}

	
	
	
	else if (particleStyle == 1) {
		if (!firefliesInitialized) {
			for (int i = 0; i < MAX_FIREFLIES; i++) {
				Firefly& f = fireflies[i];
				f.active = true;
				f.baseX = Theme::Size::SidebarWidth + 30.0f + (float)(rand() % (int)(w - Theme::Size::SidebarWidth - 60));
				f.baseY = Theme::Size::HeaderHeight + 20.0f + (float)(rand() % (int)(h - Theme::Size::HeaderHeight - 80));
				f.x = f.baseX;
				f.y = f.baseY;
				f.phase = (rand() % 1000) / 100.0f;
				f.speed = 0.3f + (rand() % 100) / 100.0f;
				f.radius = 1.5f + (rand() % 20) / 10.0f;
				f.alpha = 0.0f;
				f.wanderAngle = (rand() % 628) / 100.0f;
			}
			firefliesInitialized = true;
		}

		for (int i = 0; i < MAX_FIREFLIES; i++) {
			Firefly& f = fireflies[i];
			if (!f.active) continue;

			f.phase += deltaTime * f.speed;
			f.wanderAngle += (((rand() % 100) - 50) / 500.0f) * deltaTime * 60.0f;

			
			f.x += cosf(f.wanderAngle) * 12.0f * deltaTime;
			f.y += sinf(f.wanderAngle) * 8.0f * deltaTime;

			
			float dx = f.baseX - f.x;
			float dy = f.baseY - f.y;
			f.x += dx * 0.3f * deltaTime;
			f.y += dy * 0.3f * deltaTime;

			
			float pulse = (sinf(f.phase * 1.5f) * 0.5f + 0.5f);
			float targetAlpha = pulse * 0.12f + 0.02f;
			f.alpha = Lerp(f.alpha, targetAlpha, deltaTime * 2.0f);

			
			D2D1_COLOR_F glowOuter = Theme::AccentPrimary();
			glowOuter.a = f.alpha * 0.3f;
			renderer.FillCircle(f.x, f.y, f.radius * 3.0f, glowOuter);

			
			D2D1_COLOR_F glowInner = Theme::AccentSecondary();
			glowInner.a = f.alpha * 0.7f;
			renderer.FillCircle(f.x, f.y, f.radius * 1.5f, glowInner);

			
			D2D1_COLOR_F core = D2D1::ColorF(0xFFFFFF, f.alpha * 1.2f);
			renderer.FillCircle(f.x, f.y, f.radius * 0.5f, core);
		}
	}

	else if (particleStyle == 2) {
		if (!snowInitialized) {
			for (int i = 0; i < MAX_SNOWFLAKES; i++) {
				Snowflake& s = snowflakes[i];
				s.active = true;
				s.x = Theme::Size::SidebarWidth + (float)(rand() % (int)(w - Theme::Size::SidebarWidth));
				s.y = (float)(rand() % (int)h);
				s.speed = 8.0f + (rand() % 25);
				s.drift = ((rand() % 100) - 50) / 50.0f * 4.0f;
				s.size = 0.8f + (rand() % 20) / 10.0f;
				s.alpha = 0.03f + (rand() % 8) / 100.0f;
				s.wobblePhase = (rand() % 628) / 100.0f;
			}
			snowInitialized = true;
		}

		for (int i = 0; i < MAX_SNOWFLAKES; i++) {
			Snowflake& s = snowflakes[i];
			if (!s.active) continue;

			s.wobblePhase += deltaTime * 1.5f;
			s.y += s.speed * deltaTime;
			s.x += (s.drift + sinf(s.wobblePhase) * 6.0f) * deltaTime;

			
			if (s.y > h + 10) {
				s.y = -5.0f;
				s.x = Theme::Size::SidebarWidth + (float)(rand() % (int)(w - Theme::Size::SidebarWidth));
			}
			if (s.x < Theme::Size::SidebarWidth - 5) s.x = w;
			if (s.x > w + 5) s.x = Theme::Size::SidebarWidth;

			
			D2D1_COLOR_F glow = Theme::AccentSecondary();
			glow.a = s.alpha * 0.4f;
			renderer.FillCircle(s.x, s.y, s.size * 2.5f, glow);

			
			D2D1_COLOR_F core = Theme::TextPrimary();
			core.a = s.alpha;
			renderer.FillCircle(s.x, s.y, s.size, core);
		}
	}
}

void AetherApp::DrawLogoBadge(float x, float y) {
	if (renderer.pLogoBitmap) {
		
		D2D1_COLOR_F tint = Theme::AccentPrimary();
		
		tint.r = tint.r * 0.6f + Theme::Base::TextPriR * 0.4f;
		tint.g = tint.g * 0.6f + Theme::Base::TextPriG * 0.4f;
		tint.b = tint.b * 0.6f + Theme::Base::TextPriB * 0.4f;
		tint.a = 1.0f;
		renderer.DrawBitmapTinted(renderer.pLogoBitmap, x - 1.0f, y - 3.0f, 38.0f, 38.0f, tint, 1.0f);
		return;
	}

	
	D2D1_COLOR_F glyphCol = Theme::AccentPrimary();
	glyphCol.r = glyphCol.r * 0.6f + Theme::Base::TextPriR * 0.4f;
	glyphCol.g = glyphCol.g * 0.6f + Theme::Base::TextPriG * 0.4f;
	glyphCol.b = glyphCol.b * 0.6f + Theme::Base::TextPriB * 0.4f;
	renderer.DrawText(L"\x2726", x, y, 32.0f, 32.0f, glyphCol, renderer.pFontTitle, Renderer::AlignCenter);
}

void AetherApp::DrawHeader() {
	float w = Theme::Runtime::WindowWidth;
	float h = Theme::Size::HeaderHeight;

	renderer.FillRect(0, 0, w, h, Theme::BgSurface());
	renderer.DrawLine(0, h, w, h, Theme::BorderSubtle());

	float titleX = Theme::Size::SidebarWidth + 16;
	DrawLogoBadge(titleX, 10);

	renderer.DrawText(L"AETHER", titleX + 42, -1, 170, h, Theme::TextPrimary(), renderer.pFontTitle);

	const wchar_t* tabName = L"Tablet Driver";
	if (sidebar.activeIndex >= 0 && sidebar.activeIndex < (int)sidebar.tabs.size())
		tabName = sidebar.tabs[sidebar.activeIndex].label;
	if (w > 640.0f) {
		renderer.DrawText(tabName, titleX + 135, 0, 160, h, Theme::TextMuted(), renderer.pFontSmall);
	}

	float pillW = 116.0f;
	float buttonW = 96.0f;
	float buttonX = w - buttonW - 14.0f;
	float pillX = buttonX - pillW - 10.0f;
	D2D1_COLOR_F pillBg = driver.isConnected ? Theme::AccentDim() : Theme::BgElevated();
	if (pillX > titleX + 250.0f) {
		renderer.FillRoundedRect(pillX, 11, pillW, 26, 13, pillBg);
		renderer.DrawRoundedRect(pillX, 11, pillW, 26, 13,
			driver.isConnected ? Theme::BorderAccent() : Theme::BorderSubtle(), 1.0f);
		renderer.DrawText(driver.isConnected ? L"\xE73E" : L"\xE895",
			pillX + 10, 11, 18, 26,
			driver.isConnected ? Theme::Success() : Theme::TextSecondary(),
			renderer.pFontIcon, Renderer::AlignCenter);
		renderer.DrawText(driver.isConnected ? L"Online" : L"Starting",
			pillX + 32, 11, pillW - 42, 26,
			driver.isConnected ? Theme::Success() : Theme::TextSecondary(),
			renderer.pFontSmall, Renderer::AlignLeft);
	}

	if (w > 820.0f && !displayTargets.empty()) {
		float chipW = 148.0f;
		float chipX = pillX - chipW - 8.0f;
		if (chipX > titleX + 300.0f) {
			std::wstring label = Ellipsize(displayTargets[selectedDisplayTarget].label, 18);
			renderer.FillRoundedRect(chipX, 11, chipW, 26, 13, Theme::BgElevated());
			renderer.DrawRoundedRect(chipX, 11, chipW, 26, 13, Theme::BorderSubtle(), 1.0f);
			renderer.DrawText(L"\xE7F4", chipX + 9, 11, 18, 26, Theme::AccentPrimary(), renderer.pFontIcon, Renderer::AlignCenter);
			renderer.DrawText(label.c_str(), chipX + 31, 11, chipW - 38, 26, Theme::TextSecondary(), renderer.pFontSmall, Renderer::AlignLeft);
		}
	}

	startStopBtn.x = buttonX;
	startStopBtn.y = 9;
	startStopBtn.width = buttonW;
	startStopBtn.label = driver.isConnected ? L"Stop" : L"Retry";
	startStopBtn.isPrimary = !driver.isConnected;

	if (startStopBtn.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
		if (driver.isConnected) {
			autoStartEnabled = false;
			driver.Stop();
		} else {
			autoStartEnabled = true;
			autoStartRetryTimer = 0.0f;
			StartDriverService();
		}
	}
	startStopBtn.Draw(renderer);
}

void AetherApp::DrawOverclockInfo(float x, float y, float w) {
	float h = 60.0f;
	float hz = Clamp(overclockHz.value, 125.0f, 2000.0f);
	float intervalMs = 1000.0f / hz;
	float norm = (hz - 125.0f) / (2000.0f - 125.0f);
	norm = Clamp(norm, 0.0f, 1.0f);

	renderer.FillRoundedRect(x, y, w, h, 6, Theme::BgElevated());
	renderer.DrawRoundedRect(x, y, w, h, 6, Theme::BorderSubtle());

	wchar_t line1[64];
	wchar_t line2[80];
	swprintf_s(line1, L"%s target: %.0f Hz", overclockEnabled.value ? L"Enabled" : L"Disabled", hz);
	swprintf_s(line2, L"Timer %.2f ms   USB/tablet polling can still cap actual rate", intervalMs);

	renderer.DrawText(line1, x + 12, y + 8, w - 24, 16,
		overclockEnabled.value ? Theme::AccentPrimary() : Theme::TextMuted(), renderer.pFontSmall);
	renderer.DrawText(line2, x + 12, y + 34, w - 24, 18, Theme::TextMuted(), renderer.pFontSmall);

	float meterX = x + 12;
	float meterY = y + 27;
	float meterW = w - 24;
	float meterH = 4;
	renderer.FillRoundedRect(meterX, meterY, meterW, meterH, 2, Theme::BgElevated());
	if (norm > 0.01f) {
		renderer.FillRoundedRect(meterX, meterY, meterW * norm, meterH, 2, Theme::AccentPrimary());
		renderer.FillRectGradientH(meterX, meterY, meterW * norm, meterH, Theme::AccentPrimary(), Theme::AccentSecondary());
	}
}

void AetherApp::DrawAreaPanel() {
	float cx = Theme::Size::SidebarWidth + Theme::Size::Padding;
	float cw = std::max(220.0f, Theme::Runtime::WindowWidth - Theme::Size::SidebarWidth - Theme::Size::Padding * 2);
	bool areaSingleColumn = cw < 520.0f;
	float hw = areaSingleColumn ? cw : (cw - Theme::Size::Padding) * 0.5f;
	float yStart = Theme::Size::HeaderHeight + Theme::Size::Padding;
	float y = yStart - areaScrollY;
	float visibleContentH = std::max(360.0f, GetContentAreaBottom() - yStart - Theme::Size::Padding);

	SectionHeader sec;
	auto drawValueControl = [&](Slider& control, float px, float py, float pw) -> bool {
		control.x = px;
		control.y = py;
		control.width = pw;
		if (area.customValues.value) {
			bool changed = control.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime);
			control.Draw(renderer);
			return changed;
		}
		bool changed = control.UpdateInput(mouseX, mouseY, mouseDown, mouseClicked, deltaTime);
		control.DrawInput(renderer);
		return changed;
	};
	auto drawInputControl = [&](Slider& control, float px, float py, float pw) -> bool {
		control.x = px;
		control.y = py;
		control.width = pw;
		bool changed = control.UpdateInput(mouseX, mouseY, mouseDown, mouseClicked, deltaTime);
		control.DrawInput(renderer);
		return changed;
	};
	float valueControlStep = area.customValues.value ? 42.0f : 48.0f;

	sec.Layout(cx, y, cw, L"SCREEN MAP");
	wchar_t resBuf[64];
	swprintf_s(resBuf, L"Desktop: %.0fx%.0f", detectedScreenW, detectedScreenH);
	renderer.DrawText(resBuf, cx + cw - 180, y, 180, 18, Theme::TextMuted(), renderer.pFontSmall, Renderer::AlignRight);
	y += sec.Draw(renderer);

	{
		float selectorH = 30.0f;
		float labelX = cx + 42.0f;
		float labelW = std::max(120.0f, cw - 132.0f);

		displayPrevBtn.Layout(cx, y, 32, 28, L"<", false);
		if (displayPrevBtn.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
			ApplyDisplayTarget(selectedDisplayTarget - 1);
		}
		displayPrevBtn.Draw(renderer);

		renderer.FillRoundedRect(labelX, y, labelW, 28, 6, Theme::BgElevated());
		renderer.DrawRoundedRect(labelX, y, labelW, 28, 6, Theme::BorderSubtle());

		const wchar_t* targetLabel = L"Full desktop";
		if (selectedDisplayTarget >= 0 && selectedDisplayTarget < (int)displayTargets.size())
			targetLabel = displayTargets[selectedDisplayTarget].label.c_str();
		renderer.DrawText(L"\xE7F4", labelX + 8, y, 28, 28, Theme::BrandHot(), renderer.pFontIcon, Renderer::AlignCenter);
		renderer.DrawText(targetLabel, labelX + 36, y, labelW - 44, 28, Theme::TextPrimary(), renderer.pFontSmall, Renderer::AlignLeft);

		displayNextBtn.Layout(labelX + labelW + 8, y, 32, 28, L">", false);
		if (displayNextBtn.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
			ApplyDisplayTarget(selectedDisplayTarget + 1);
		}
		displayNextBtn.Draw(renderer);

		y += selectorH + 8.0f;
	}

	float toggleRowY = y;
	area.customValues.x = cx;
	area.customValues.y = toggleRowY;
	bool customValuesChanged = area.customValues.Update(mouseX, mouseY, mouseClicked, deltaTime);
	if (customValuesChanged) {
		if (!area.customValues.value) {
			CenterScreenArea();
			ApplyAllSettings();
		}
		AutoSaveConfig();
	}
	area.customValues.Draw(renderer);

	if (areaSingleColumn) {
		y += 32.0f;
		area.autoCenter.x = cx;
		area.autoCenter.y = y;
	}
	else {
		area.autoCenter.x = cx + 190.0f;
		area.autoCenter.y = toggleRowY;
	}
	if (area.autoCenter.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
		if (area.autoCenter.value) {
			CenterScreenArea();
			ApplyAllSettings();
		}
		AutoSaveConfig();
	}
	area.autoCenter.Draw(renderer);

	valueControlStep = area.customValues.value ? 42.0f : 48.0f;
	y += 36.0f;

	{
		float previewMaxW = areaSingleColumn ? (cw - 8) : (hw - 20);
		float screenAspect = std::max(0.1f, detectedScreenW / std::max(1.0f, detectedScreenH));
		float previewW = previewMaxW;
		float previewH = previewW / screenAspect;
		if (!areaSingleColumn) {
			float previewMaxH = Clamp(visibleContentH * 0.34f, 240.0f, 340.0f);
			if (previewH > previewMaxH) {
				previewH = previewMaxH;
				previewW = previewH * screenAspect;
			}
		}
		float previewX = areaSingleColumn ? cx : cx + (previewMaxW - previewW) * 0.5f;
		float previewY = y;

		renderer.FillRoundedRect(previewX, previewY, previewW, previewH, 6, Theme::BgElevated());
		renderer.DrawRoundedRect(previewX, previewY, previewW, previewH, 6, Theme::BorderSubtle());

		float totalScreenW = detectedScreenW;
		float scaleX = previewW / totalScreenW;
		float scaleY = previewH / detectedScreenH;
		float scale = (scaleX < scaleY) ? scaleX : scaleY;

		float mappedX = previewX + area.screenX.value * scale;
		float mappedY = previewY + area.screenY.value * scale;
		float mappedW = area.screenWidth.value * scale;
		float mappedH = area.screenHeight.value * scale;

		if (mappedW > previewW) mappedW = previewW;
		if (mappedH > previewH) mappedH = previewH;

		bool hoveringScreenMap = PointInRect(mouseX, mouseY, mappedX, mappedY, mappedW, mappedH);
		if (hoveringScreenMap && mouseClicked && !isDraggingArea) {
			PushUndoCheckpoint();
			isDraggingArea = true;
			dragTarget = 1;
			dragStartMouseX = mouseX;
			dragStartMouseY = mouseY;
			dragStartValX = area.screenX.value;
			dragStartValY = area.screenY.value;
			dragScale = 1.0f / scale;
		}
		if (isDraggingArea && dragTarget == 1) {
			float dx = (mouseX - dragStartMouseX) * dragScale;
			float dy = (mouseY - dragStartMouseY) * dragScale;
			area.screenX.value = dragStartValX + dx;
			area.screenY.value = dragStartValY + dy;
			ClampScreenArea();
		}

		float zoneAlpha = (hoveringScreenMap || (isDraggingArea && dragTarget == 1)) ? 0.34f : 0.22f;
		D2D1_COLOR_F screenHot = Theme::BrandHot();
		D2D1_COLOR_F screenCold = Theme::AccentPrimary();
		screenHot.a = zoneAlpha;
		screenCold.a = zoneAlpha;
		renderer.FillRoundedRect(mappedX, mappedY, mappedW, mappedH, 2, Theme::AccentDim());
		renderer.FillRectGradientH(mappedX, mappedY, mappedW, mappedH, screenHot, screenCold);
		renderer.DrawRoundedRect(mappedX, mappedY, mappedW, mappedH, 2, Theme::BrandHot(), 1.2f);
		renderer.DrawRoundedRect(mappedX + 1, mappedY + 1, mappedW - 2, mappedH - 2, 2, Theme::BorderAccent(), 0.9f);

		float ratio = area.screenWidth.value / area.screenHeight.value;
		wchar_t ratioBuf[32];
		swprintf_s(ratioBuf, L"%.3f:1", ratio);
		renderer.DrawText(ratioBuf, mappedX, mappedY, mappedW, mappedH,
			Theme::TextAccent(), renderer.pFontSmall, Renderer::AlignCenter);

		bool screenChanged = false;
		float oldScreenW = area.screenWidth.value;
		float oldScreenH = area.screenHeight.value;
		float oldScreenX = area.screenX.value;
		float oldScreenY = area.screenY.value;
		bool screenWidthChanged = false;
		bool screenHeightChanged = false;
		bool screenXChanged = false;
		bool screenYChanged = false;
		if (areaSingleColumn) {
			y = previewY + previewH + 12;
			screenWidthChanged = drawValueControl(area.screenWidth, cx, y, cw);
			screenChanged |= screenWidthChanged;
			y += valueControlStep;
			screenHeightChanged = drawValueControl(area.screenHeight, cx, y, cw);
			screenChanged |= screenHeightChanged;
			y += valueControlStep;
			if (area.customValues.value) {
				screenXChanged = drawValueControl(area.screenX, cx, y, cw);
				screenChanged |= screenXChanged;
				y += valueControlStep;
				screenYChanged = drawValueControl(area.screenY, cx, y, cw);
				screenChanged |= screenYChanged;
				y += valueControlStep;
			}
		} else {
			float controlX = cx + hw + 8;
			float controlW = hw - 8;
			screenWidthChanged = drawValueControl(area.screenWidth, controlX, previewY, controlW);
			screenHeightChanged = drawValueControl(area.screenHeight, controlX, previewY + valueControlStep, controlW);
			screenChanged |= screenWidthChanged;
			screenChanged |= screenHeightChanged;

			int screenRows = 2;
			if (area.customValues.value) {
				screenXChanged = drawValueControl(area.screenX, controlX, previewY + valueControlStep * 2.0f, controlW);
				screenYChanged = drawValueControl(area.screenY, controlX, previewY + valueControlStep * 3.0f, controlW);
				screenChanged |= screenXChanged;
				screenChanged |= screenYChanged;
				screenRows = 4;
			}
			float controlsH = valueControlStep * (float)screenRows - 6.0f;
			y = std::max(previewY + previewH, previewY + controlsH) + 16;
		}

		ClampScreenArea();
		area.screenX.animValue = area.screenX.value;
		area.screenY.animValue = area.screenY.value;
		if ((screenWidthChanged || screenHeightChanged) && !screenXChanged && !screenYChanged && !isDraggingArea) {
			if (area.autoCenter.value) {
				CenterScreenArea();
			}
			else if (area.customValues.value) {
				float centerX = oldScreenX + oldScreenW * 0.5f;
				float centerY = oldScreenY + oldScreenH * 0.5f;
				area.screenX.value = centerX - area.screenWidth.value * 0.5f;
				area.screenY.value = centerY - area.screenHeight.value * 0.5f;
				ClampScreenArea();
			}
			else {
				CenterScreenArea();
			}
		}
		if (screenChanged) {
			area.screenWidth.animValue = area.screenWidth.value;
			area.screenHeight.animValue = area.screenHeight.value;
			area.screenX.animValue = area.screenX.value;
			area.screenY.animValue = area.screenY.value;
			if ((screenWidthChanged || screenHeightChanged) && !area.lockAspect.value && area.screenHeight.value > 1.0f) {
				area.aspectRatio.value = Clamp(area.screenWidth.value / area.screenHeight.value, area.aspectRatio.minVal, area.aspectRatio.maxVal);
				area.aspectRatio.animValue = area.aspectRatio.value;
			}
			ApplyAspectLock(false);
			ApplyAllSettings();
		}
	}

	sec.Layout(cx, y, cw, L"TABLET AREA");
	y += sec.Draw(renderer);

	{
		
		float fullTabletW = (driver.tabletWidth > 1.0f) ? driver.tabletWidth : 152.0f;
		float fullTabletH = (driver.tabletHeight > 1.0f) ? driver.tabletHeight : 95.0f;

		float previewMaxW = areaSingleColumn ? (cw - 8) : (hw - 20);
		float tabletAspect = std::max(0.1f, fullTabletW / std::max(1.0f, fullTabletH));
		float previewW = previewMaxW;
		float previewH = previewW / tabletAspect;
		if (!areaSingleColumn) {
			float previewMaxH = Clamp(visibleContentH * 0.29f, 190.0f, 285.0f);
			if (previewH > previewMaxH) {
				previewH = previewMaxH;
				previewW = previewH * tabletAspect;
			}
		}
		float previewX = areaSingleColumn ? cx : cx + (previewMaxW - previewW) * 0.5f;
		float previewY = y;

		renderer.FillRoundedRect(previewX, previewY, previewW, previewH, 6, Theme::BgElevated());
		renderer.DrawRoundedRect(previewX, previewY, previewW, previewH, 6, Theme::BorderSubtle());

		float scale = previewW / fullTabletW;
		float areaW = area.tabletWidth.value * scale;
		float areaH = area.tabletHeight.value * scale;

		float centerX = (area.tabletX.value > 0.01f) ? area.tabletX.value : fullTabletW * 0.5f;
		float centerY = (area.tabletY.value > 0.01f) ? area.tabletY.value : fullTabletH * 0.5f;
		float areaX = previewX + (centerX - area.tabletWidth.value * 0.5f) * scale;
		float areaY = previewY + (centerY - area.tabletHeight.value * 0.5f) * scale;

		if (areaW > previewW) areaW = previewW;
		if (areaH > previewH) areaH = previewH;

		bool hoveringTabletArea = PointInRect(mouseX, mouseY, areaX, areaY, areaW, areaH);
		if (hoveringTabletArea && mouseClicked && !isDraggingArea) {
			PushUndoCheckpoint();
			isDraggingArea = true;
			dragTarget = 2;
			dragStartMouseX = mouseX;
			dragStartMouseY = mouseY;
			dragStartValX = centerX;
			dragStartValY = centerY;
			dragScale = 1.0f / scale;
		}
		if (isDraggingArea && dragTarget == 2) {
			float dx = (mouseX - dragStartMouseX) * dragScale;
			float dy = (mouseY - dragStartMouseY) * dragScale;
			float newX = dragStartValX + dx;
			float newY = dragStartValY + dy;
			float halfW = area.tabletWidth.value * 0.5f;
			float halfH = area.tabletHeight.value * 0.5f;
			if (newX - halfW < 0) newX = halfW;
			if (newY - halfH < 0) newY = halfH;
			if (newX + halfW > fullTabletW) newX = fullTabletW - halfW;
			if (newY + halfH > fullTabletH) newY = fullTabletH - halfH;
			area.tabletX.value = newX;
			area.tabletY.value = newY;
			areaX = previewX + (newX - halfW) * scale;
			areaY = previewY + (newY - halfH) * scale;
		}

		float tabletAlpha = (hoveringTabletArea || (isDraggingArea && dragTarget == 2)) ? 0.34f : 0.22f;
		D2D1_COLOR_F tabHot = Theme::BrandHot();
		D2D1_COLOR_F tabCold = Theme::AccentSecondary();
		tabHot.a = tabletAlpha;
		tabCold.a = tabletAlpha;

		D2D1_MATRIX_3X2_F oldTransform = D2D1::Matrix3x2F::Identity();
		bool canRotatePreview = renderer.pRT != nullptr && areaW > 1.0f && areaH > 1.0f;
		if (canRotatePreview) {
			renderer.pRT->GetTransform(&oldTransform);
			float visualRotation = area.rotation.value;
			D2D1_POINT_2F center = D2D1::Point2F(areaX + areaW * 0.5f, areaY + areaH * 0.5f);
			renderer.pRT->SetTransform(D2D1::Matrix3x2F::Rotation(visualRotation, center) * oldTransform);
		}

		renderer.FillRoundedRect(areaX, areaY, areaW, areaH, 2, Theme::AccentDim());
		renderer.FillRectGradientH(areaX, areaY, areaW, areaH, tabHot, tabCold);
		renderer.DrawRoundedRect(areaX, areaY, areaW, areaH, 2, Theme::BrandHot(), 1.2f);
		renderer.DrawRoundedRect(areaX + 1, areaY + 1, areaW - 2, areaH - 2, 2, Theme::BorderAccent(), 0.9f);

		if (canRotatePreview) {
			renderer.pRT->SetTransform(oldTransform);
		}

		float tabRatio = area.tabletWidth.value / area.tabletHeight.value;
		wchar_t tabRatioBuf[32];
		swprintf_s(tabRatioBuf, L"%.3f:1", tabRatio);
		renderer.DrawText(tabRatioBuf, areaX, areaY, areaW, areaH,
			Theme::AccentSecondary(), renderer.pFontSmall, Renderer::AlignCenter);

		
		DrawLiveCursor(previewX, previewY, previewW, previewH, fullTabletW, fullTabletH);

		
		if (visualizerToggle.value) {
			DrawInputVisualizer(previewX, previewY, previewW, previewH);
		}

		bool tabletChanged = false;
		bool tabletWidthChanged = false;
		bool tabletHeightChanged = false;
		bool tabletXChanged = false;
		bool tabletYChanged = false;

		if (areaSingleColumn) {
			y = previewY + previewH + 12;
			tabletWidthChanged = drawValueControl(area.tabletWidth, cx, y, cw);
			y += valueControlStep;
			tabletHeightChanged = drawValueControl(area.tabletHeight, cx, y, cw);
			y += valueControlStep;
			tabletChanged = tabletWidthChanged || tabletHeightChanged;
			tabletXChanged = area.customValues.value
				? drawValueControl(area.tabletX, cx, y, cw)
				: drawInputControl(area.tabletX, cx, y, cw);
			tabletChanged |= tabletXChanged;
			y += area.customValues.value ? valueControlStep : 48.0f;
			tabletYChanged = area.customValues.value
				? drawValueControl(area.tabletY, cx, y, cw)
				: drawInputControl(area.tabletY, cx, y, cw);
			tabletChanged |= tabletYChanged;
			y += area.customValues.value ? valueControlStep : 48.0f;
		} else {
			float controlX = cx + hw + 8;
			float controlW = hw - 8;
			tabletWidthChanged = drawValueControl(area.tabletWidth, controlX, previewY, controlW);
			tabletHeightChanged = drawValueControl(area.tabletHeight, controlX, previewY + valueControlStep, controlW);
			tabletChanged = tabletWidthChanged || tabletHeightChanged;

			tabletXChanged = area.customValues.value
				? drawValueControl(area.tabletX, controlX, previewY + valueControlStep * 2.0f, controlW)
				: drawInputControl(area.tabletX, controlX, previewY + valueControlStep * 2.0f, controlW);
			tabletYChanged = area.customValues.value
				? drawValueControl(area.tabletY, controlX, previewY + valueControlStep * 3.0f, controlW)
				: drawInputControl(area.tabletY, controlX, previewY + valueControlStep * 3.0f, controlW);
			tabletChanged |= tabletXChanged;
			tabletChanged |= tabletYChanged;
			int tabletRows = 4;

			float controlsH = valueControlStep * (float)tabletRows - 6.0f;
			y = std::max(previewY + previewH, previewY + controlsH) + 16;
		}

		if (tabletWidthChanged || tabletHeightChanged || area.lockAspect.value) {
			ApplyAspectLock(tabletHeightChanged && !tabletWidthChanged);
		}
		if (tabletChanged) {
			ApplyAllSettings();
		}
	}

	DrawConfigManager(cx, y, cw);
	y += 8.0f;

	sec.Layout(cx, y, cw, L"OPTIONS");
	y += sec.Draw(renderer);

	float optionW = areaSingleColumn ? cw : hw - 8;
	float optionRightX = areaSingleColumn ? cx : cx + hw + 8;

	area.rotation.y = y; area.rotation.x = cx; area.rotation.width = optionW;
	if (area.rotation.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime)) {
		ApplyAllSettings();
	}
	area.rotation.Draw(renderer);

	if (areaSingleColumn)
		y += 48.0f;

	area.lockAspect.y = y; area.lockAspect.x = optionRightX;
	if (area.lockAspect.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
		ApplyAspectLock(false);
		ApplyAllSettings();
	}
	area.lockAspect.Draw(renderer);
	y += 36.0f;

	if (area.lockAspect.value) {
		area.aspectRatio.y = y;
		area.aspectRatio.x = cx;
		area.aspectRatio.width = cw;
		if (area.aspectRatio.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime)) {
			ApplyAspectLock(false);
			ApplyAllSettings();
		}
		area.aspectRatio.Draw(renderer);
		y += 48.0f;
	}

	forceFullArea.y = y; forceFullArea.x = cx;
	if (forceFullArea.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
		if (forceFullArea.value) {
			float fW = (driver.tabletWidth > 1.0f) ? driver.tabletWidth : 152.0f;
			float fH = (driver.tabletHeight > 1.0f) ? driver.tabletHeight : 95.25f;
			area.tabletWidth.value = fW;
			area.tabletHeight.value = fH;
			area.tabletX.value = fW * 0.5f;
			area.tabletY.value = fH * 0.5f;
			ApplyAspectLock(false);
		}
		ApplyAllSettings();
	}
	forceFullArea.Draw(renderer);

	areaClipping.y = y; areaClipping.x = cx + hw + 8;
	if (areaClipping.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
		ApplyAllSettings();
	}
	areaClipping.Draw(renderer);
	y += 36;

	areaLimiting.y = y; areaLimiting.x = cx;
	if (areaLimiting.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
		if (areaLimiting.value) areaClipping.value = true;
		ApplyAllSettings();
	}
	areaLimiting.Draw(renderer);

	visualizerToggle.y = y; visualizerToggle.x = cx + hw + 8;
	visualizerToggle.Update(mouseX, mouseY, mouseClicked, deltaTime);
	visualizerToggle.Draw(renderer);
	y += 36;

	sec.Layout(cx, y, cw, L"OUTPUT MODE");
	y += sec.Draw(renderer);

	outputMode.x = cx; outputMode.y = y;
	float modeBtnW = (cw - 8) / 3.0f;
	if (outputMode.Update(mouseX, mouseY, mouseClicked, deltaTime, modeBtnW) >= 0) {
		ApplyAllSettings();
	}
	outputMode.Draw(renderer, modeBtnW);

	
	if (outputMode.selected == 2 && vmultiCheckDone && !vmultiInstalled) {
		y += 32;
		renderer.FillRoundedRect(cx, y, cw, 28, 6, D2D1::ColorF(0.9f, 0.35f, 0.2f, 0.12f));
		renderer.DrawRoundedRect(cx, y, cw, 28, 6, D2D1::ColorF(0.9f, 0.35f, 0.2f, 0.3f));
		renderer.DrawText(L"\xE7BA", cx + 10, y, 18, 28, Theme::Warning(), renderer.pFontIcon, Renderer::AlignCenter);
		renderer.DrawText(L"Windows Ink requires VMulti driver. Install VMulti to use this mode.",
			cx + 32, y, cw - 44, 28, Theme::Warning(), renderer.pFontSmall);
	}
	y += 36;

	areaContentH = (y + areaScrollY) - yStart + 40;
}

void AetherApp::DrawSettingsPanel() {
	float cx = Theme::Size::SidebarWidth + Theme::Size::Padding;
	float cw = Theme::Runtime::WindowWidth - Theme::Size::SidebarWidth - Theme::Size::Padding * 2;
	float hw = (cw - Theme::Size::Padding) * 0.5f;
	float yStart = Theme::Size::HeaderHeight + Theme::Size::Padding;
	float y = yStart - settingsScrollY;

	SectionHeader sec;

	sec.Layout(cx, y, cw, L"BUTTON MAPPING");
	y += sec.Draw(renderer);

	buttonTip.x = cx; buttonTip.y = y; buttonTip.width = hw;
	if (buttonTip.Update(mouseX, mouseY, mouseClicked, deltaTime)) ApplyAllSettings();
	buttonTip.Draw(renderer);
	buttonBottom.x = cx + hw + 16; buttonBottom.y = y; buttonBottom.width = hw;
	if (buttonBottom.Update(mouseX, mouseY, mouseClicked, deltaTime)) ApplyAllSettings();
	buttonBottom.Draw(renderer);
	y += 34;
	buttonTop.x = cx; buttonTop.y = y; buttonTop.width = hw;
	if (buttonTop.Update(mouseX, mouseY, mouseClicked, deltaTime)) ApplyAllSettings();
	buttonTop.Draw(renderer);
	y += 44;

	sec.Layout(cx, y, cw, L"PEN SETTINGS");
	y += sec.Draw(renderer);

	tipThreshold.y = y; tipThreshold.x = cx; tipThreshold.width = hw - 8;
	if (tipThreshold.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime)) ApplyAllSettings();
	tipThreshold.Draw(renderer);
	y += 48;

	sec.Layout(cx, y, cw, L"UI SCALE"); y += sec.Draw(renderer);
	dpiScale.x = cx;
	dpiScale.y = y;
	dpiScale.width = cw;
	if (dpiScale.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime)) {
		ApplyDpiScale();
		AutoSaveConfig();
	}
	dpiScale.Draw(renderer);
	y += 52;

	sec.Layout(cx, y, cw, L"THEME"); y += sec.Draw(renderer);
	DrawThemeSelector(cx, y, cw);

	sec.Layout(cx, y, cw, L"PARTICLES"); y += sec.Draw(renderer);
	{
		const wchar_t* particleNames[] = { L"Stars", L"Fireflies", L"Snow", L"None" };
		int numStyles = 4;
		float pbtnW = (cw - (numStyles - 1) * 4.0f) / (float)numStyles;
		for (int i = 0; i < numStyles; i++) {
			float bx = cx + i * (pbtnW + 4.0f);
			bool isActive = (i == particleStyle);
			bool hovered = PointInRect(mouseX, mouseY, bx, y, pbtnW, 26);
			if (isActive) {
				renderer.FillRoundedRect(bx, y, pbtnW, 26, 6, Theme::AccentPrimary());
				renderer.DrawText(particleNames[i], bx, y, pbtnW, 26, D2D1::ColorF(0xFFFFFF), renderer.pFontSmall, Renderer::AlignCenter);
			} else {
				D2D1_COLOR_F bg = hovered ? Theme::BgHover() : Theme::BgElevated();
				renderer.FillRoundedRect(bx, y, pbtnW, 26, 6, bg);
				renderer.DrawRoundedRect(bx, y, pbtnW, 26, 6, Theme::BorderSubtle());
				D2D1_COLOR_F tc = hovered ? Theme::TextPrimary() : Theme::TextMuted();
				renderer.DrawText(particleNames[i], bx, y, pbtnW, 26, tc, renderer.pFontSmall, Renderer::AlignCenter);
			}
			if (hovered && mouseClicked) {
				particleStyle = i;
				AutoSaveConfig();
			}
		}
		y += 38;
	}

	settingsContentH = (y + settingsScrollY) - yStart + 40;
}

void AetherApp::SaveConfig(const std::wstring& path) {
	std::ofstream f(path);
	if (!f.is_open()) return;

	f << "# Aether Tablet Driver Config\n";
	f << "TabletWidth=" << area.tabletWidth.value << "\n";
	f << "TabletHeight=" << area.tabletHeight.value << "\n";
	f << "TabletX=" << area.tabletX.value << "\n";
	f << "TabletY=" << area.tabletY.value << "\n";
	f << "ScreenWidth=" << area.screenWidth.value << "\n";
	f << "ScreenHeight=" << area.screenHeight.value << "\n";
	f << "ScreenX=" << area.screenX.value << "\n";
	f << "ScreenY=" << area.screenY.value << "\n";
	f << "CustomValues=" << (int)area.customValues.value << "\n";
	f << "AutoCenter=" << (int)area.autoCenter.value << "\n";
	f << "Rotation=" << area.rotation.value << "\n";
	f << "OutputMode=" << outputMode.selected << "\n";
	f << "ButtonTip=" << buttonTip.selected << "\n";
	f << "ButtonBottom=" << buttonBottom.selected << "\n";
	f << "ButtonTop=" << buttonTop.selected << "\n";
	f << "ForceFullArea=" << (int)forceFullArea.value << "\n";
	f << "AreaClipping=" << (int)areaClipping.value << "\n";
	f << "AreaLimiting=" << (int)areaLimiting.value << "\n";
	f << "TipThreshold=" << tipThreshold.value << "\n";
	f << "Overclock=" << (int)overclockEnabled.value << "\n";
	f << "OverclockHz=" << overclockHz.value << "\n";
	f << "PenRateLimit=" << (int)penRateLimitEnabled.value << "\n";
	f << "PenRateLimitHz=" << penRateLimitHz.value << "\n";
	f << "LockAspect=" << (int)area.lockAspect.value << "\n";
	f << "AspectRatio=" << area.aspectRatio.value << "\n";
	f << "AccentR=" << Theme::Custom::AccentR << "\n";
	f << "AccentG=" << Theme::Custom::AccentG << "\n";
	f << "AccentB=" << Theme::Custom::AccentB << "\n";
	f << "UITheme=" << currentTheme << "\n";
	f << "DpiScale=" << dpiScale.value << "\n";

	f << "\n";
	f << "SmoothingEnabled=" << (int)filters.smoothingEnabled.value << "\n";
	f << "SmoothingLatency=" << filters.smoothingLatency.value << "\n";
	f << "SmoothingInterval=" << filters.smoothingInterval.value << "\n";

	f << "AntichatterEnabled=" << (int)filters.antichatterEnabled.value << "\n";
	f << "AntichatterStrength=" << filters.antichatterStrength.value << "\n";
	f << "AntichatterMultiplier=" << filters.antichatterMultiplier.value << "\n";
	f << "AntichatterOffsetX=" << filters.antichatterOffsetX.value << "\n";
	f << "AntichatterOffsetY=" << filters.antichatterOffsetY.value << "\n";

	f << "NoiseEnabled=" << (int)filters.noiseEnabled.value << "\n";
	f << "NoiseBuffer=" << filters.noiseBuffer.value << "\n";
	f << "NoiseThreshold=" << filters.noiseThreshold.value << "\n";
	f << "NoiseIterations=" << filters.noiseIterations.value << "\n";

	f << "VelCurveEnabled=" << (int)filters.velCurveEnabled.value << "\n";
	f << "VelCurveMinSpeed=" << filters.velCurveMinSpeed.value << "\n";
	f << "VelCurveMaxSpeed=" << filters.velCurveMaxSpeed.value << "\n";
	f << "VelCurveSmoothing=" << filters.velCurveSmoothing.value << "\n";
	f << "VelCurveSharpness=" << filters.velCurveSharpness.value << "\n";

	f << "SnapEnabled=" << (int)filters.snapEnabled.value << "\n";
	f << "SnapRadius=" << filters.snapRadius.value << "\n";
	f << "SnapSmooth=" << filters.snapSmooth.value << "\n";

	f << "ReconstructorEnabled=" << (int)filters.reconstructorEnabled.value << "\n";
	f << "ReconStrength=" << filters.reconStrength.value << "\n";
	f << "ReconVelSmooth=" << filters.reconVelSmooth.value << "\n";
	f << "ReconAccelCap=" << filters.reconAccelCap.value << "\n";
	f << "ReconPredTime=" << filters.reconPredTime.value << "\n";

	f << "AdaptiveEnabled=" << (int)filters.adaptiveEnabled.value << "\n";
	f << "AdaptiveProcessNoise=" << filters.adaptiveProcessNoise.value << "\n";
	f << "AdaptiveMeasNoise=" << filters.adaptiveMeasNoise.value << "\n";
	f << "AdaptiveVelWeight=" << filters.adaptiveVelWeight.value << "\n";

	f << "\n";
	f << "AetherEnabled=" << (int)aether.enabled.value << "\n";
	f << "AetherLagRemoval=" << (int)aether.lagRemovalEnabled.value << "\n";
	f << "AetherLagStrength=" << aether.lagRemovalStrength.value << "\n";
	f << "AetherStabilizer=" << (int)aether.stabilizerEnabled.value << "\n";
	f << "AetherStability=" << aether.stabilizerStability.value << "\n";
	f << "AetherSensitivity=" << aether.stabilizerSensitivity.value << "\n";
	f << "AetherSnapping=" << (int)aether.snappingEnabled.value << "\n";
	f << "AetherSnapInner=" << aether.snappingInner.value << "\n";
	f << "AetherSnapOuter=" << aether.snappingOuter.value << "\n";
	f << "AetherRhythmFlow=" << (int)aether.rhythmFlowEnabled.value << "\n";
	f << "AetherRhythmStrength=" << aether.rhythmFlowStrength.value << "\n";
	f << "AetherRhythmRelease=" << aether.rhythmFlowRelease.value << "\n";
	f << "AetherRhythmJitter=" << aether.rhythmFlowJitter.value << "\n";
	f << "AetherSuppression=" << (int)aether.suppressionEnabled.value << "\n";
	f << "AetherSuppressTime=" << aether.suppressionTime.value << "\n";
	f << "Visualizer=" << (int)visualizerToggle.value << "\n";
	f << "ParticleStyle=" << particleStyle << "\n";

	f << "\n";
	f << "PluginRepoOwner=" << WideToUtf8(pluginRepoOwner) << "\n";
	f << "PluginRepoName=" << WideToUtf8(pluginRepoName) << "\n";
	f << "PluginRepoRef=" << WideToUtf8(pluginRepoRef) << "\n";
	for (const PluginEntry& plugin : pluginEntries) {
		f << "PluginEnabled." << WideToUtf8(plugin.key) << "=" << (int)plugin.enabled.value << "\n";
		for (const auto& option : plugin.options) {
			f << "PluginOption." << WideToUtf8(plugin.key) << "." << option.key << "="
				<< (option.kind == PluginEntry::PluginOption::ToggleOption ? (option.toggle.value ? 1.0f : 0.0f) : option.slider.value)
				<< "\n";
		}
	}

	f.close();
}

void AetherApp::LoadConfig(const std::wstring& path) {
	std::ifstream f(path);
	if (!f.is_open()) return;

	float loadedAccentR = Theme::Custom::AccentR;
	float loadedAccentG = Theme::Custom::AccentG;
	float loadedAccentB = Theme::Custom::AccentB;
	bool hasLoadedAccent = false;
	std::string line;
	while (std::getline(f, line)) {
		if (line.empty() || line[0] == '#') continue;
		size_t eq = line.find('=');
		if (eq == std::string::npos) continue;
		std::string key = line.substr(0, eq);
		std::string rawValue = line.substr(eq + 1);
		if (key == "PluginRepoOwner") {
			pluginRepoOwner = Utf8ToWide(rawValue);
			continue;
		}
		if (key == "PluginRepoName") {
			pluginRepoName = Utf8ToWide(rawValue);
			continue;
		}
		if (key == "PluginRepoRef") {
			pluginRepoRef = Utf8ToWide(rawValue);
			continue;
		}

		float val = 0.0f;
		try {
			val = std::stof(rawValue);
		}
		catch (...) {
			continue;
		}

		if (key == "TabletWidth") area.tabletWidth.value = val;
		else if (key == "TabletHeight") area.tabletHeight.value = val;
		else if (key == "TabletX") area.tabletX.value = val;
		else if (key == "TabletY") area.tabletY.value = val;
		else if (key == "ScreenWidth") area.screenWidth.value = val;
		else if (key == "ScreenHeight") area.screenHeight.value = val;
		else if (key == "ScreenX") area.screenX.value = val;
		else if (key == "ScreenY") area.screenY.value = val;
		else if (key == "CustomValues") area.customValues.value = (val > 0.5f);
		else if (key == "AutoCenter") area.autoCenter.value = (val > 0.5f);
		else if (key == "Rotation") area.rotation.value = val;
		else if (key == "OutputMode") outputMode.selected = (int)val;
		else if (key == "ButtonTip") buttonTip.selected = (int)val;
		else if (key == "ButtonBottom") buttonBottom.selected = (int)val;
		else if (key == "ButtonTop") buttonTop.selected = (int)val;
		else if (key == "ForceFullArea") forceFullArea.value = (val > 0.5f);
		else if (key == "AreaClipping") areaClipping.value = (val > 0.5f);
		else if (key == "AreaLimiting") areaLimiting.value = (val > 0.5f);
		else if (key == "TipThreshold") tipThreshold.value = val;
		else if (key == "Overclock") overclockEnabled.value = (val > 0.5f);
		else if (key == "OverclockHz") overclockHz.value = val;
		else if (key == "PenRateLimit") penRateLimitEnabled.value = (val > 0.5f);
		else if (key == "PenRateLimitHz") penRateLimitHz.value = val;
		else if (key == "LockAspect") area.lockAspect.value = (val > 0.5f);
		else if (key == "AspectRatio") area.aspectRatio.value = Clamp(val, area.aspectRatio.minVal, area.aspectRatio.maxVal);
		else if (key == "AccentR") { loadedAccentR = Clamp(val, 0.0f, 1.0f); hasLoadedAccent = true; }
		else if (key == "AccentG") { loadedAccentG = Clamp(val, 0.0f, 1.0f); hasLoadedAccent = true; }
		else if (key == "AccentB") { loadedAccentB = Clamp(val, 0.0f, 1.0f); hasLoadedAccent = true; }
		else if (key == "UITheme") {
			int idx = (int)val;
			if (idx < 0 || idx >= uiThemeCount) idx = 0; 
			currentTheme = idx;
			Theme::ApplyTheme(uiThemes[idx]);
		}
		else if (key == "DpiScale") {
			dpiScale.value = Clamp(val, dpiScale.minVal, dpiScale.maxVal);
			dpiScale.animValue = dpiScale.value;
		}
		else if (key == "DpiScaleMode") {
			int idx = (int)val;
			float legacyValues[] = { GetSystemDpiScale() * 100.0f, 100.0f, 125.0f, 150.0f, 175.0f, 200.0f };
			if (idx < 0 || idx > 5) idx = 0;
			dpiScale.value = Clamp(legacyValues[idx], dpiScale.minVal, dpiScale.maxVal);
			dpiScale.animValue = dpiScale.value;
		}
		else if (key == "SmoothingEnabled") filters.smoothingEnabled.value = (val > 0.5f);
		else if (key == "SmoothingLatency") filters.smoothingLatency.value = val;
		else if (key == "SmoothingInterval") filters.smoothingInterval.value = val;
		else if (key == "AntichatterEnabled") filters.antichatterEnabled.value = (val > 0.5f);
		else if (key == "AntichatterStrength") filters.antichatterStrength.value = val;
		else if (key == "AntichatterMultiplier") filters.antichatterMultiplier.value = val;
		else if (key == "AntichatterOffsetX") filters.antichatterOffsetX.value = val;
		else if (key == "AntichatterOffsetY") filters.antichatterOffsetY.value = val;
		else if (key == "NoiseEnabled") filters.noiseEnabled.value = (val > 0.5f);
		else if (key == "NoiseBuffer") filters.noiseBuffer.value = val;
		else if (key == "NoiseThreshold") filters.noiseThreshold.value = val;
		else if (key == "NoiseIterations") filters.noiseIterations.value = val;
		else if (key == "VelCurveEnabled") filters.velCurveEnabled.value = (val > 0.5f);
		else if (key == "VelCurveMinSpeed") filters.velCurveMinSpeed.value = val;
		else if (key == "VelCurveMaxSpeed") filters.velCurveMaxSpeed.value = val;
		else if (key == "VelCurveSmoothing") filters.velCurveSmoothing.value = val;
		else if (key == "VelCurveSharpness") filters.velCurveSharpness.value = val;

		else if (key == "SnapEnabled") filters.snapEnabled.value = (val > 0.5f);
		else if (key == "SnapRadius") filters.snapRadius.value = val;
		else if (key == "SnapSmooth") filters.snapSmooth.value = val;
		else if (key == "ReconstructorEnabled") filters.reconstructorEnabled.value = (val > 0.5f);
		else if (key == "ReconStrength") filters.reconStrength.value = val;
		else if (key == "ReconVelSmooth") filters.reconVelSmooth.value = val;
		else if (key == "ReconAccelCap") filters.reconAccelCap.value = val;
		else if (key == "ReconPredTime") filters.reconPredTime.value = val;
		else if (key == "AdaptiveEnabled") filters.adaptiveEnabled.value = (val > 0.5f);
		else if (key == "AdaptiveProcessNoise") filters.adaptiveProcessNoise.value = val;
		else if (key == "AdaptiveMeasNoise") filters.adaptiveMeasNoise.value = val;
		else if (key == "AdaptiveVelWeight") filters.adaptiveVelWeight.value = val;
		else if (key == "AetherEnabled") aether.enabled.value = (val > 0.5f);
		else if (key == "AetherLagRemoval") aether.lagRemovalEnabled.value = (val > 0.5f);
		else if (key == "AetherLagStrength") aether.lagRemovalStrength.value = val;
		else if (key == "AetherStabilizer") aether.stabilizerEnabled.value = (val > 0.5f);
		else if (key == "AetherStability") aether.stabilizerStability.value = val;
		else if (key == "AetherSensitivity") aether.stabilizerSensitivity.value = val;
		else if (key == "AetherSnapping") aether.snappingEnabled.value = (val > 0.5f);
		else if (key == "AetherSnapInner") aether.snappingInner.value = val;
		else if (key == "AetherSnapOuter") aether.snappingOuter.value = val;
		else if (key == "AetherRhythmFlow") aether.rhythmFlowEnabled.value = (val > 0.5f);
		else if (key == "AetherRhythmStrength") aether.rhythmFlowStrength.value = val;
		else if (key == "AetherRhythmRelease") aether.rhythmFlowRelease.value = val;
		else if (key == "AetherRhythmJitter") aether.rhythmFlowJitter.value = val;
		else if (key == "AetherSuppression") aether.suppressionEnabled.value = (val > 0.5f);
		else if (key == "AetherSuppressTime") aether.suppressionTime.value = val;
		else if (key == "Visualizer") visualizerToggle.value = (val > 0.5f);
		else if (key == "ParticleStyle") { particleStyle = (int)val; if (particleStyle < 0 || particleStyle > 3) particleStyle = 0; }
		else if (key.rfind("PluginEnabled.", 0) == 0) {
			if (pluginListDirty)
				RefreshPluginList();

			std::wstring pluginKey = Utf8ToWide(key.substr(14));
			for (PluginEntry& plugin : pluginEntries) {
				if (_wcsicmp(plugin.key.c_str(), pluginKey.c_str()) == 0) {
					plugin.enabled.value = (val > 0.5f);
					plugin.enabled.animT = plugin.enabled.value ? 1.0f : 0.0f;
					break;
				}
			}
		}
		else if (key.rfind("PluginOption.", 0) == 0) {
			if (pluginListDirty)
				RefreshPluginList();

			std::string optionPath = key.substr(13);
			size_t sep = optionPath.rfind('.');
			if (sep != std::string::npos) {
				std::wstring pluginKey = Utf8ToWide(optionPath.substr(0, sep));
				std::string optionKey = optionPath.substr(sep + 1);
				for (PluginEntry& plugin : pluginEntries) {
					if (_wcsicmp(plugin.key.c_str(), pluginKey.c_str()) == 0) {
						for (auto& option : plugin.options) {
							if (option.key == optionKey) {
								if (option.kind == PluginEntry::PluginOption::ToggleOption) {
									option.toggle.value = (val > 0.5f);
									option.toggle.animT = option.toggle.value ? 1.0f : 0.0f;
								}
								else {
									option.slider.value = Clamp(val, option.slider.minVal, option.slider.maxVal);
									option.slider.animValue = option.slider.value;
								}
								break;
							}
						}
						break;
					}
				}
			}
		}
	}
	f.close();
	ClampScreenArea();
	area.aspectRatio.value = Clamp(area.aspectRatio.value, area.aspectRatio.minVal, area.aspectRatio.maxVal);
	area.aspectRatio.animValue = area.aspectRatio.value;
	if (!area.customValues.value)
		CenterScreenArea();
	ApplyAspectLock(false);
	ApplyDpiScale();
	if (hasLoadedAccent)
		Theme::Custom::SetAccent(loadedAccentR, loadedAccentG, loadedAccentB);
	accentPicker.SetRGB(Theme::Custom::AccentR, Theme::Custom::AccentG, Theme::Custom::AccentB);
	if (IsDefaultPluginRepoOwner(pluginRepoOwner))
		pluginRepoOwner.clear();
	
	{
		wchar_t hexBuf[16];
		swprintf_s(hexBuf, L"#%02X%02X%02X",
			(int)(Theme::Custom::AccentR * 255),
			(int)(Theme::Custom::AccentG * 255),
			(int)(Theme::Custom::AccentB * 255));
		wcscpy_s(hexColorInput.buffer, hexBuf);
		hexColorInput.cursor = (int)wcslen(hexBuf);
	}
	wcscpy_s(pluginRepoOwnerInput.buffer, pluginRepoOwner.c_str());
	pluginRepoOwnerInput.cursor = (int)pluginRepoOwner.length();
	wcscpy_s(pluginRepoNameInput.buffer, pluginRepoName.c_str());
	pluginRepoNameInput.cursor = (int)pluginRepoName.length();
	wcscpy_s(pluginRepoRefInput.buffer, pluginRepoRef.c_str());
	pluginRepoRefInput.cursor = (int)pluginRepoRef.length();
	SyncLoadedControlVisuals();
}

void AetherApp::DrawPluginFilterControls(float cx, float& y, float cw, float hw, float filterRightX, bool filterSingleColumn, bool& filterChanged) {
	if (pluginListDirty)
		RefreshPluginList();

	SectionHeader sec;
	float gap = 8.0f;
	float btnW = (cw - gap * 3.0f) / 4.0f;
	installPluginBtn.Layout(cx, y, btnW, 28, L"Install DLL", false);
	if (installPluginBtn.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
		InstallPluginWithDialog();
		UpdatePluginCatalogInstallState();
	}
	installPluginBtn.Draw(renderer);

	installSourcePluginBtn.Layout(cx + btnW + gap, y, btnW, 28, L"Build Source", false);
	if (installSourcePluginBtn.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
		InstallPluginSourceWithDialog();
		UpdatePluginCatalogInstallState();
	}
	installSourcePluginBtn.Draw(renderer);

	reloadPluginBtn.Layout(cx + (btnW + gap) * 2.0f, y, btnW, 28, L"Reload", false);
	if (reloadPluginBtn.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
		if (!driver.isConnected)
			StartDriverService();
		if (driver.isConnected)
			driver.SendCommand("PluginReload");
		RefreshPluginList();
		SendPluginSettings();
		UpdatePluginCatalogInstallState();
		if (driver.isConnected)
			driver.SendCommand("PluginList");
	}
	reloadPluginBtn.Draw(renderer);

	listPluginBtn.Layout(cx + (btnW + gap) * 3.0f, y, btnW, 28, L"List", false);
	if (listPluginBtn.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
		RefreshPluginList();
		selectedCatalogIndex = 0;
		RefreshPluginCatalog();
		pluginManagerOpen = true;
	}
	listPluginBtn.Draw(renderer);
	y += 42;

	renderer.DrawText(pluginListStatus.c_str(), cx, y, cw, 20, Theme::TextMuted(), renderer.pFontSmall);
	y += 24;

	if (pluginEntries.empty()) {
		renderer.FillRoundedRect(cx, y, cw, 30, 6, Theme::BgElevated());
		renderer.DrawRoundedRect(cx, y, cw, 30, 6, Theme::BorderSubtle());
		renderer.DrawText(L"No native Aether DLL filters installed", cx + 10, y, cw - 20, 30, Theme::TextMuted(), renderer.pFontSmall);
		y += 40;
		return;
	}

	for (size_t i = 0; i < pluginEntries.size(); ++i) {
		PluginEntry& plugin = pluginEntries[i];
		float itemT = Clamp((aboutAnimT + (float)i * 0.08f), 0.0f, 1.0f);
		float itemEase = 1.0f - (1.0f - itemT) * (1.0f - itemT);
		float itemX = cx + (1.0f - itemEase) * 18.0f;
		sec.Layout(itemX, y, cw, plugin.name.c_str());
		y += sec.Draw(renderer);

		plugin.enabled.y = y;
		plugin.enabled.x = itemX;
		if (plugin.enabled.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
			if (!driver.isConnected)
				StartDriverService();
			if (driver.isConnected) {
				SendPluginEnable(i);
				if (plugin.enabled.value)
					SendPluginOptions(i);
			}
			filterChanged = true;
			AutoSaveConfig();
		}
		plugin.enabled.Draw(renderer);
		renderer.DrawText(plugin.dllName.c_str(), itemX + cw * 0.55f, y, cw * 0.43f, Theme::Size::ToggleHeight, Theme::TextMuted(), renderer.pFontSmall, Renderer::AlignRight);
		y += 30;

		if (!plugin.description.empty()) {
			renderer.DrawText(plugin.description.c_str(), itemX, y, cw, 34, Theme::TextMuted(), renderer.pFontSmall);
			y += 36;
		}

		if (plugin.enabled.value && !plugin.options.empty()) {
			for (size_t optionIndex = 0; optionIndex < plugin.options.size(); ++optionIndex) {
				PluginEntry::PluginOption& option = plugin.options[optionIndex];
				if (option.kind == PluginEntry::PluginOption::ToggleOption) {
					option.toggle.x = itemX;
					option.toggle.y = y;
					option.toggle.label = option.label.c_str();
					if (option.toggle.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
						if (!driver.isConnected)
							StartDriverService();
						SendPluginOption(i, option);
						filterChanged = true;
						AutoSaveConfig();
					}
					option.toggle.Draw(renderer);
					y += 30;
				}
				else {
					option.slider.x = itemX;
					option.slider.y = y;
					option.slider.width = cw;
					option.slider.label = option.label.c_str();
					if (option.slider.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime)) {
						if (!driver.isConnected)
							StartDriverService();
						SendPluginOption(i, option);
						filterChanged = true;
						AutoSaveConfig();
					}
					option.slider.Draw(renderer);
					y += 50;
				}
			}
		}
		else if (plugin.enabled.value) {
			renderer.DrawText(L"This plugin exposes no editable Aether metadata yet.", itemX, y, cw, 20, Theme::TextMuted(), renderer.pFontSmall);
			y += 28;
		}

		y += 8;
	}
}

void AetherApp::DrawPluginSourceModal() {
	float overlayW = Theme::Runtime::WindowWidth;
	float overlayH = Theme::Runtime::WindowHeight;
	renderer.FillRect(0, 0, overlayW, overlayH, D2D1::ColorF(0, 0, 0, 0.55f));

	float w = std::min(460.0f, overlayW - 40.0f);
	float h = 250.0f;
	float x = (overlayW - w) * 0.5f;
	float y = (overlayH - h) * 0.5f;
	renderer.FillRoundedRect(x, y, w, h, 8, Theme::BgSurface());
	renderer.DrawRoundedRect(x, y, w, h, 8, Theme::BorderAccent());
	renderer.DrawText(L"Switch Repository Source", x + 18, y + 12, w - 36, 28, Theme::TextPrimary(), renderer.pFontBody);

	float inputX = x + 18;
	float inputW = w - 36;
	pluginRepoOwnerInput.x = inputX; pluginRepoOwnerInput.y = y + 54; pluginRepoOwnerInput.width = inputW;
	pluginRepoOwnerInput.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime);
	pluginRepoOwnerInput.Draw(renderer);
	pluginRepoNameInput.x = inputX; pluginRepoNameInput.y = y + 104; pluginRepoNameInput.width = inputW;
	pluginRepoNameInput.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime);
	pluginRepoNameInput.Draw(renderer);
	pluginRepoRefInput.x = inputX; pluginRepoRefInput.y = y + 154; pluginRepoRefInput.width = inputW;
	pluginRepoRefInput.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime);
	pluginRepoRefInput.Draw(renderer);

	float btnW = 120.0f;
	pluginManagerCancelSourceBtn.Layout(x + w - btnW * 2.0f - 28.0f, y + h - 42.0f, btnW, 28, L"Cancel", false);
	if (pluginManagerCancelSourceBtn.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
		pluginSourceEditorOpen = false;
	}
	pluginManagerCancelSourceBtn.Draw(renderer);
	pluginManagerApplySourceBtn.Layout(x + w - btnW - 18.0f, y + h - 42.0f, btnW, 28, L"Apply", true);
	if (pluginManagerApplySourceBtn.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
		pluginRepoOwner = pluginRepoOwnerInput.buffer;
		pluginRepoName = pluginRepoNameInput.buffer[0] ? pluginRepoNameInput.buffer : L"Plugin-Repository";
		pluginRepoRef = pluginRepoRefInput.buffer[0] ? pluginRepoRefInput.buffer : L"master";
		pluginSourceEditorOpen = false;
		RefreshPluginCatalog();
	}
	pluginManagerApplySourceBtn.Draw(renderer);
}

void AetherApp::DrawUpdateModal() {
	float overlayW = Theme::Runtime::WindowWidth;
	float overlayH = Theme::Runtime::WindowHeight;
	renderer.FillRect(0, 0, overlayW, overlayH, D2D1::ColorF(0, 0, 0, 0.52f));

	float w = std::min(520.0f, overlayW - 36.0f);
	float h = 260.0f;
	float x = (overlayW - w) * 0.5f;
	float y = (overlayH - h) * 0.5f;

	renderer.FillRoundedRect(x, y, w, h, 12, Theme::BgSurface());
	renderer.DrawRoundedRect(x, y, w, h, 12, Theme::BorderAccent(), 1.2f);

	D2D1_COLOR_F glow = Theme::AccentGlow();
	glow.a = 0.16f;
	renderer.FillCircle(x + 54, y + 54, 28, glow);
	renderer.FillCircle(x + 54, y + 54, 18, Theme::AccentDim());
	renderer.DrawText(L"\xE895", x + 42, y + 40, 24, 28, Theme::AccentPrimary(), renderer.pFontIcon, Renderer::AlignCenter);

	renderer.DrawText(L"AetherGUI update available", x + 92, y + 24, w - 116, 28, Theme::TextPrimary(), renderer.pFontBody);
	renderer.DrawText(L"A new GitHub release was found. You can open the release page now or continue using the current version.",
		x + 92, y + 56, w - 116, 46, Theme::TextMuted(), renderer.pFontSmall);

	renderer.FillRoundedRect(x + 18, y + 120, w - 36, 70, 8, Theme::BgElevated());
	renderer.DrawRoundedRect(x + 18, y + 120, w - 36, 70, 8, Theme::BorderSubtle());
	renderer.DrawText(L"Current", x + 34, y + 132, 120, 20, Theme::TextMuted(), renderer.pFontSmall);
	renderer.DrawText(updateCurrentVersion.empty() ? L"unknown" : updateCurrentVersion.c_str(), x + 160, y + 132, w - 200, 20, Theme::TextSecondary(), renderer.pFontSmall, Renderer::AlignRight);
	renderer.DrawText(L"Latest", x + 34, y + 160, 120, 20, Theme::TextMuted(), renderer.pFontSmall);
	renderer.DrawText(updateLatestTag.empty() ? L"unknown" : updateLatestTag.c_str(), x + 160, y + 160, w - 200, 20, Theme::AccentPrimary(), renderer.pFontSmall, Renderer::AlignRight);

	float btnY = y + h - 48;
	updateLaterBtn.Layout(x + w - 250, btnY, 100, 30, L"Later", false);
	if (updateLaterBtn.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
		updateModalOpen = false;
	}
	updateLaterBtn.Draw(renderer);

	updateOpenBtn.Layout(x + w - 140, btnY, 122, 30, L"Open Release", true);
	if (updateOpenBtn.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
		OpenExternalUrl(updateReleaseUrl);
		updateModalOpen = false;
	}
	updateOpenBtn.Draw(renderer);
}

void AetherApp::DrawPluginManagerModal() {
	float overlayW = Theme::Runtime::WindowWidth;
	float overlayH = Theme::Runtime::WindowHeight;
	renderer.FillRect(0, 0, overlayW, overlayH, D2D1::ColorF(0, 0, 0, 0.50f));

	float w = std::min(960.0f, overlayW - 36.0f);
	float h = std::min(650.0f, overlayH - 54.0f);
	float x = (overlayW - w) * 0.5f;
	float y = (overlayH - h) * 0.5f;
	renderer.FillRoundedRect(x, y, w, h, 8, Theme::BgSurface());
	renderer.DrawRoundedRect(x, y, w, h, 8, Theme::BorderAccent());

	renderer.DrawText(L"Plugin Manager", x + 18, y + 12, 200, 28, Theme::TextPrimary(), renderer.pFontBody);
	pluginManagerAetherTabBtn.Layout(x + 18, y + 46, 142, 30, L"Aether Filters", pluginManagerTab == 0);
	if (pluginManagerAetherTabBtn.Update(mouseX, mouseY, mouseClicked, deltaTime) && pluginManagerTab != 0) {
		aboutAnimT = 0.0f;
		pluginManagerTab = 0;
		selectedCatalogIndex = 0;
		pluginCatalogScrollY = 0.0f;
		pluginCatalogAnimDirection = -1;
		RefreshPluginCatalog();
	}
	pluginManagerAetherTabBtn.Draw(renderer);
	pluginManagerOtdTabBtn.Layout(x + 168, y + 46, 132, 30, L"OTD Ports", pluginManagerTab == 1);
	if (pluginManagerOtdTabBtn.Update(mouseX, mouseY, mouseClicked, deltaTime) && pluginManagerTab != 1) {
		aboutAnimT = 0.0f;
		pluginManagerTab = 1;
		selectedCatalogIndex = 0;
		pluginCatalogScrollY = 0.0f;
		pluginCatalogAnimDirection = 1;
		RefreshPluginCatalog();
	}
	pluginManagerOtdTabBtn.Draw(renderer);

	std::wstring sourceFolder = pluginManagerTab == 0 ? L"Plugins" : L"AetherOTDPorts";
	std::wstring source = IsDefaultPluginRepoOwner(pluginRepoOwner)
		? (L"Default GitHub catalog / " + sourceFolder + L" @ " + pluginRepoRef)
		: (pluginRepoOwner + L"/" + pluginRepoName + L" / " + sourceFolder + L" @ " + pluginRepoRef);
	renderer.DrawText(source.c_str(), x + 314, y + 49, w - 500, 24, Theme::TextMuted(), renderer.pFontSmall);

	pluginManagerSourceBtn.Layout(x + w - 366, y + 12, 108, 28, L"Source", false);
	if (pluginManagerSourceBtn.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
		std::wstring visibleOwner = IsDefaultPluginRepoOwner(pluginRepoOwner) ? L"" : pluginRepoOwner;
		wcscpy_s(pluginRepoOwnerInput.buffer, visibleOwner.c_str());
		pluginRepoOwnerInput.cursor = (int)visibleOwner.length();
		wcscpy_s(pluginRepoNameInput.buffer, pluginRepoName.c_str());
		pluginRepoNameInput.cursor = (int)pluginRepoName.length();
		wcscpy_s(pluginRepoRefInput.buffer, pluginRepoRef.c_str());
		pluginRepoRefInput.cursor = (int)pluginRepoRef.length();
		pluginSourceEditorOpen = true;
	}
	pluginManagerSourceBtn.Draw(renderer);
	pluginManagerRefreshBtn.Layout(x + w - 250, y + 12, 108, 28, L"Refresh", false);
	if (pluginManagerRefreshBtn.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
		RefreshPluginCatalog();
	}
	pluginManagerRefreshBtn.Draw(renderer);
	pluginManagerCloseBtn.Layout(x + w - 134, y + 12, 116, 28, L"Close", false);
	if (pluginManagerCloseBtn.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
		pluginManagerOpen = false;
	}
	pluginManagerCloseBtn.Draw(renderer);

	float listX = x + 14;
	float listY = y + 88;
	float listW = 280;
	float listH = h - 148;
	float detailX = listX + listW + 12;
	float detailW = w - listW - 40;
	float listBodyY = listY + 34.0f;
	float listBodyH = std::max(0.0f, listH - 52.0f);
	ClampPluginCatalogScroll();
	if (mouseClicked && PointInRect(mouseX, mouseY, x + 14.0f, listY, w - 28.0f, listH)) {
		isPluginCatalogDragScrolling = true;
		pluginCatalogDragStartY = mouseY;
		pluginCatalogDragStartOffset = pluginCatalogScrollY;
	}

	if (false && pluginManagerTab == 0) {
		if (pluginListDirty)
			RefreshPluginList();
		if (selectedPluginIndex >= (int)pluginEntries.size())
			selectedPluginIndex = (int)pluginEntries.size() - 1;
		if (selectedPluginIndex < 0)
			selectedPluginIndex = 0;

		renderer.FillRoundedRect(listX, listY, listW, listH, 6, Theme::BgDeep());
		renderer.DrawRoundedRect(listX, listY, listW, listH, 6, Theme::BorderSubtle());
		renderer.DrawText(L"Installed Filters", listX + 10, listY + 8, listW - 20, 20, Theme::TextSecondary(), renderer.pFontSmall);
		renderer.FillRoundedRect(detailX, listY, detailW, listH, 6, Theme::BgDeep());
		renderer.DrawRoundedRect(detailX, listY, detailW, listH, 6, Theme::BorderSubtle());

		float rowY = listY + 34;
		if (pluginEntries.empty()) {
			renderer.DrawText(L"No native Aether filters installed yet.", listX + 10, rowY, listW - 20, 44, Theme::TextMuted(), renderer.pFontSmall);
			renderer.DrawText(L"Install a DLL or build a C++ source folder to add a filter.", detailX + 14, listY + 18, detailW - 28, 36, Theme::TextMuted(), renderer.pFontSmall);
		}
		else {
			int visibleRows = (int)((listH - 42) / 30.0f);
			for (int i = 0; i < (int)pluginEntries.size() && i < visibleRows; ++i) {
				PluginEntry& plugin = pluginEntries[i];
				float ry = rowY + i * 30.0f;
				bool selected = i == selectedPluginIndex;
				bool hovered = PointInRect(mouseX, mouseY, listX + 6, ry, listW - 12, 26);
				renderer.FillRoundedRect(listX + 6, ry, listW - 12, 26, 5, selected ? Theme::AccentDim() : (hovered ? Theme::BgHover() : Theme::BgElevated()));
				if (selected)
					renderer.DrawRoundedRect(listX + 6, ry, listW - 12, 26, 5, Theme::BorderAccent());
				renderer.DrawText(plugin.name.c_str(), listX + 14, ry, listW - 64, 26, Theme::TextPrimary(), renderer.pFontSmall);
				renderer.DrawText(plugin.enabled.value ? L"ON" : L"OFF", listX + listW - 54, ry, 42, 26,
					plugin.enabled.value ? Theme::Success() : Theme::TextMuted(), renderer.pFontSmall, Renderer::AlignRight);
				if (hovered && mouseClicked)
					selectedPluginIndex = i;
			}

			PluginEntry& plugin = pluginEntries[selectedPluginIndex];
			float dy = listY + 14;
			renderer.DrawText(plugin.name.c_str(), detailX + 14, dy, detailW - 28, 26, Theme::TextPrimary(), renderer.pFontBody);
			dy += 40;
			renderer.DrawText(L"DLL", detailX + 14, dy, 120, 22, Theme::TextMuted(), renderer.pFontSmall);
			renderer.DrawText(plugin.dllName.c_str(), detailX + 150, dy, detailW - 170, 22, Theme::TextSecondary(), renderer.pFontSmall, Renderer::AlignRight);
			dy += 34;
			renderer.DrawText(L"Key", detailX + 14, dy, 120, 22, Theme::TextMuted(), renderer.pFontSmall);
			renderer.DrawText(plugin.key.c_str(), detailX + 150, dy, detailW - 170, 22, Theme::TextSecondary(), renderer.pFontSmall, Renderer::AlignRight);
			dy += 34;
			renderer.DrawText(L"Description", detailX + 14, dy, 120, 22, Theme::TextMuted(), renderer.pFontSmall);
			renderer.DrawText(plugin.description.c_str(), detailX + 150, dy, detailW - 170, 54, Theme::TextSecondary(), renderer.pFontSmall, Renderer::AlignRight);
			dy += 68;
			renderer.DrawText(L"Controls", detailX + 14, dy, 120, 22, Theme::TextMuted(), renderer.pFontSmall);
			renderer.DrawText(std::to_wstring(plugin.options.size()).c_str(), detailX + 150, dy, detailW - 170, 22, Theme::TextSecondary(), renderer.pFontSmall, Renderer::AlignRight);

			pluginManagerDeleteBtn.Layout(detailX + detailW - 124, y + h - 46, 112, 30, L"Uninstall", false);
			if (pluginManagerDeleteBtn.Update(mouseX, mouseY, mouseClicked, deltaTime))
				DeleteInstalledPlugin((size_t)selectedPluginIndex);
			pluginManagerDeleteBtn.Draw(renderer);
		}

		pluginManagerInstallBtn.Layout(x + 14, y + h - 46, 112, 30, L"Install DLL", true);
		if (pluginManagerInstallBtn.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
			InstallPluginWithDialog();
			UpdatePluginCatalogInstallState();
		}
		pluginManagerInstallBtn.Draw(renderer);
		pluginManagerInstallSourceBtn.Layout(x + 134, y + h - 46, 128, 30, L"Build Source", false);
		if (pluginManagerInstallSourceBtn.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
			InstallPluginSourceWithDialog();
			UpdatePluginCatalogInstallState();
		}
		pluginManagerInstallSourceBtn.Draw(renderer);
		renderer.DrawText(pluginListStatus.c_str(), x + 274, y + h - 46, w - 566, 30, Theme::TextMuted(), renderer.pFontSmall);
		return;
	}

	renderer.FillRoundedRect(listX, listY, listW, listH, 6, Theme::BgDeep());
	renderer.DrawRoundedRect(listX, listY, listW, listH, 6, Theme::BorderSubtle());
	renderer.DrawText(pluginManagerTab == 0 ? L"Aether Filters" : L"OTD Plugin Ports", listX + 10, listY + 8, listW - 20, 20, Theme::TextSecondary(), renderer.pFontSmall);

	if (pluginCatalogAnimT < 1.0f) {
		pluginCatalogAnimT += deltaTime * 7.5f;
		if (pluginCatalogAnimT > 1.0f) pluginCatalogAnimT = 1.0f;
	}
	float catalogEase = 1.0f - (1.0f - pluginCatalogAnimT) * (1.0f - pluginCatalogAnimT);
	float catalogSlideX = (1.0f - catalogEase) * 24.0f * (float)pluginCatalogAnimDirection;
	float catalogAlpha = catalogEase;

	float rowY = listBodyY;
	if (pluginCatalogEntries.empty()) {
		renderer.DrawText(pluginCatalogStatus.c_str(), listX + 10, rowY, listW - 20, 44, Theme::TextMuted(), renderer.pFontSmall);
	}
	else {
		float rowH = 30.0f;
		int firstIndex = (int)(pluginCatalogScrollY / rowH);
		float rowOffset = pluginCatalogScrollY - firstIndex * rowH;
		int visibleRows = (int)(listBodyH / rowH) + 2;
		for (int row = 0; row < visibleRows; ++row) {
			int i = firstIndex + row;
			if (i < 0 || i >= (int)pluginCatalogEntries.size())
				break;
			auto& entry = pluginCatalogEntries[i];
			float ry = rowY + row * rowH - rowOffset;
			if (ry < listBodyY || ry + 26.0f > listBodyY + listBodyH)
				continue;
			float rowDelay = std::min(1.0f, (float)row * 0.045f);
			float rowT = Clamp((pluginCatalogAnimT - rowDelay) / 0.72f, 0.0f, 1.0f);
			float rowEase = 1.0f - (1.0f - rowT) * (1.0f - rowT);
			float rowX = listX + 6 + catalogSlideX * (1.0f - rowEase);
			bool selected = i == selectedCatalogIndex;
			bool hovered = PointInRect(mouseX, mouseY, listX + 6, ry, listW - 12, 26);
			D2D1_COLOR_F rowBg = selected ? Theme::AccentDim() : (hovered ? Theme::BgHover() : Theme::BgElevated());
			rowBg.a *= catalogAlpha * (0.45f + rowEase * 0.55f);
			renderer.FillRoundedRect(rowX, ry, listW - 12, 26, 5, rowBg);
			if (selected) {
				D2D1_COLOR_F border = Theme::BorderAccent();
				border.a *= catalogAlpha;
				renderer.DrawRoundedRect(rowX, ry, listW - 12, 26, 5, border);
			}
			D2D1_COLOR_F nameColor = Theme::TextPrimary();
			nameColor.a *= catalogAlpha * rowEase;
			renderer.DrawText(entry.name.c_str(), rowX + 8, ry, listW - 64, 26, nameColor, renderer.pFontSmall);
			D2D1_COLOR_F stateColor = entry.installed ? Theme::Success() : (entry.sourcePort ? Theme::AccentPrimary() : Theme::TextMuted());
			stateColor.a *= catalogAlpha * rowEase;
			renderer.DrawText(entry.installed ? (entry.needsUpdate ? L"UPD" : L"ON") : (entry.sourcePort ? L"SRC" : L"TODO"), listX + listW - 54, ry, 42, 26,
				stateColor, renderer.pFontSmall, Renderer::AlignRight);
			if (hovered && mouseClicked)
				selectedCatalogIndex = i;
		}

		float contentH = (float)pluginCatalogEntries.size() * rowH + 8.0f;
		if (contentH > listBodyH + 1.0f) {
			float trackX = listX + listW - 7.0f;
			float trackY = listBodyY;
			float trackH = listBodyH;
			float thumbH = std::max(24.0f, trackH * (listBodyH / contentH));
			float maxScroll = contentH - listBodyH;
			float thumbY = trackY + (trackH - thumbH) * (maxScroll > 0.0f ? pluginCatalogScrollY / maxScroll : 0.0f);
			D2D1_COLOR_F trackCol = Theme::BorderSubtle();
			trackCol.a = 0.18f;
			renderer.FillRoundedRect(trackX, trackY, 4.0f, trackH, 2.0f, trackCol);
			D2D1_COLOR_F thumbCol = isPluginCatalogDragScrolling ? Theme::AccentPrimary() : Theme::TextMuted();
			thumbCol.a = isPluginCatalogDragScrolling ? 0.65f : 0.35f;
			renderer.FillRoundedRect(trackX, thumbY, 4.0f, thumbH, 2.0f, thumbCol);
		}
	}

	renderer.FillRoundedRect(detailX, listY, detailW, listH, 6, Theme::BgDeep());
	renderer.DrawRoundedRect(detailX, listY, detailW, listH, 6, Theme::BorderSubtle());
	if (!pluginCatalogEntries.empty() && selectedCatalogIndex >= 0 && selectedCatalogIndex < (int)pluginCatalogEntries.size()) {
		PluginCatalogEntry& entry = pluginCatalogEntries[selectedCatalogIndex];
		float dy = listY + 14;
		renderer.DrawText(entry.name.c_str(), detailX + 14, dy, detailW - 28, 26, Theme::TextPrimary(), renderer.pFontBody);
		dy += 40;
		renderer.DrawText(L"Owner", detailX + 14, dy, 120, 22, Theme::TextMuted(), renderer.pFontSmall);
		renderer.DrawText(entry.owner.c_str(), detailX + 150, dy, detailW - 170, 22, Theme::TextSecondary(), renderer.pFontSmall, Renderer::AlignRight);
		dy += 34;
		renderer.DrawText(L"Description", detailX + 14, dy, 120, 22, Theme::TextMuted(), renderer.pFontSmall);
		renderer.DrawText(entry.description.c_str(), detailX + 150, dy, detailW - 170, 54, Theme::TextSecondary(), renderer.pFontSmall, Renderer::AlignRight);
		dy += 68;
		renderer.DrawText(L"Driver Version", detailX + 14, dy, 160, 22, Theme::TextMuted(), renderer.pFontSmall);
		renderer.DrawText(entry.driverVersion.c_str(), detailX + 180, dy, detailW - 200, 22, Theme::TextSecondary(), renderer.pFontSmall, Renderer::AlignRight);
		dy += 34;
		renderer.DrawText(L"Plugin Version", detailX + 14, dy, 160, 22, Theme::TextMuted(), renderer.pFontSmall);
		renderer.DrawText(entry.version.c_str(), detailX + 180, dy, detailW - 200, 22, Theme::TextSecondary(), renderer.pFontSmall, Renderer::AlignRight);
		dy += 34;
		renderer.DrawText(L"License", detailX + 14, dy, 160, 22, Theme::TextMuted(), renderer.pFontSmall);
		renderer.DrawText(entry.license.c_str(), detailX + 180, dy, detailW - 200, 22, Theme::TextSecondary(), renderer.pFontSmall, Renderer::AlignRight);
		dy += 36;
		renderer.DrawText(L"Source Code Repository", detailX + 14, dy, 220, 28, Theme::TextMuted(), renderer.pFontSmall);
		pluginManagerSourceCodeBtn.Layout(detailX + detailW - 170, dy, 156, 28, L"Source Code", false);
		if (pluginManagerSourceCodeBtn.Update(mouseX, mouseY, mouseClicked, deltaTime))
			OpenExternalUrl(entry.repositoryUrl);
		pluginManagerSourceCodeBtn.Draw(renderer);
		dy += 42;
		renderer.DrawText(L"Wiki", detailX + 14, dy, 220, 28, Theme::TextMuted(), renderer.pFontSmall);
		pluginManagerWikiBtn.Layout(detailX + detailW - 170, dy, 156, 28, L"Wiki", false);
		if (pluginManagerWikiBtn.Update(mouseX, mouseY, mouseClicked, deltaTime))
			OpenExternalUrl(entry.wikiUrl);
		pluginManagerWikiBtn.Draw(renderer);
		dy += 42;
		const wchar_t* state = entry.installed
			? (entry.needsUpdate ? L"Installed, but GitHub source port differs. Reinstall will rebuild and replace it." : L"Installed")
			: (entry.sourcePort ? L"Ready to download, build, and install" : L"Port is not available yet. Needs an Aether C++ port.");
		renderer.DrawText(state, detailX + 14, dy, detailW - 28, 26,
			entry.installed ? (entry.needsUpdate ? Theme::Warning() : Theme::Success()) : (entry.sourcePort ? Theme::AccentPrimary() : Theme::Warning()), renderer.pFontSmall);

		if ((!entry.installed || entry.needsUpdate) && entry.sourcePort) {
			pluginManagerInstallRepoBtn.Layout(detailX + detailW - 124, y + h - 46, 112, 30, entry.installed ? L"Reinstall" : L"Install", true);
			if (pluginManagerInstallRepoBtn.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
				InstallRepositoryPlugin(entry);
			}
			pluginManagerInstallRepoBtn.Draw(renderer);
		}
		else if (entry.installed) {
			pluginManagerDeleteBtn.Layout(detailX + detailW - 124, y + h - 46, 112, 30, L"Uninstall", false);
			if (pluginManagerDeleteBtn.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
				std::wstring entryIdentity = entry.name + L" " + Utf8ToWide(entry.sourcePath);
				for (size_t i = 0; i < pluginEntries.size(); ++i) {
					std::wstring pluginIdentity = pluginEntries[i].key + L" " + pluginEntries[i].name + L" " + pluginEntries[i].dllName;
					if (CatalogNamesMatch(pluginIdentity, entryIdentity)) {
						DeleteInstalledPlugin(i);
						break;
					}
				}
				RefreshPluginCatalog();
			}
			pluginManagerDeleteBtn.Draw(renderer);
		}
	}
	else {
		renderer.DrawText(L"Press Refresh to load plugins from the selected GitHub repository.", detailX + 14, listY + 18, detailW - 28, 32, Theme::TextMuted(), renderer.pFontSmall);
	}

	if (pluginManagerTab == 0) {
		pluginManagerInstallBtn.Layout(x + 14, y + h - 46, 112, 30, L"Install DLL", true);
		if (pluginManagerInstallBtn.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
			InstallPluginWithDialog();
		}
		pluginManagerInstallBtn.Draw(renderer);
		pluginManagerInstallSourceBtn.Layout(x + 134, y + h - 46, 128, 30, L"Build Source", false);
		if (pluginManagerInstallSourceBtn.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
			InstallPluginSourceWithDialog();
		}
		pluginManagerInstallSourceBtn.Draw(renderer);
		renderer.DrawText(pluginCatalogStatus.c_str(), x + 274, y + h - 46, w - 560, 30, Theme::TextMuted(), renderer.pFontSmall);
	}
	else {
		renderer.DrawText(pluginCatalogStatus.c_str(), x + 14, y + h - 46, w - 280, 30, Theme::TextMuted(), renderer.pFontSmall);
	}
}
void AetherApp::DrawFilterPanel() {
	bool filterChanged = false;
	float cx = Theme::Size::SidebarWidth + Theme::Size::Padding;
	float cw = std::max(220.0f, Theme::Runtime::WindowWidth - Theme::Size::SidebarWidth - Theme::Size::Padding * 2);
	bool filterSingleColumn = cw < 640.0f;
	float filterGap = filterSingleColumn ? 0.0f : 16.0f;
	float hw = filterSingleColumn ? cw : (cw - filterGap) * 0.5f;
	float filterRightX = filterSingleColumn ? cx : cx + hw + filterGap;
	float yStart = Theme::Size::HeaderHeight + Theme::Size::Padding;
	float y = yStart - filterScrollY;

	SectionHeader sec;
	Slider* filterSliders[] = {
		&filters.smoothingLatency, &filters.smoothingInterval,
		&filters.antichatterStrength, &filters.antichatterMultiplier,
		&filters.antichatterOffsetX, &filters.antichatterOffsetY,
		&filters.noiseBuffer, &filters.noiseThreshold, &filters.noiseIterations,
		&filters.velCurveMinSpeed, &filters.velCurveMaxSpeed,
		&filters.velCurveSmoothing, &filters.velCurveSharpness,
		&filters.snapRadius, &filters.snapSmooth,
		&filters.reconStrength, &filters.reconVelSmooth,
		&filters.reconAccelCap, &filters.reconPredTime,
		&filters.adaptiveProcessNoise, &filters.adaptiveMeasNoise, &filters.adaptiveVelWeight
	};
	for (Slider* slider : filterSliders) {
		slider->width = hw;
	}
	bool smoothingPipelineActive = filters.smoothingEnabled.value || aether.enabled.value;

	sec.Layout(cx, y, cw, L"SMOOTHING"); y += sec.Draw(renderer);
	filters.smoothingEnabled.y = y; filters.smoothingEnabled.x = cx;
	filterChanged |= filters.smoothingEnabled.Update(mouseX, mouseY, mouseClicked, deltaTime); filters.smoothingEnabled.Draw(renderer); y += 30;

	if (filters.smoothingEnabled.value) {
		filters.smoothingLatency.y = y; filters.smoothingLatency.x = cx;
		filterChanged |= filters.smoothingLatency.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.smoothingLatency.Draw(renderer);
		if (filterSingleColumn) y += 48;
		filters.smoothingInterval.y = y; filters.smoothingInterval.x = filterRightX;
		filterChanged |= filters.smoothingInterval.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.smoothingInterval.Draw(renderer);
		y += 48;
	}

	sec.Layout(cx, y, cw, L"ANTICHATTER"); y += sec.Draw(renderer);
	filters.antichatterEnabled.y = y; filters.antichatterEnabled.x = cx;
	filterChanged |= filters.antichatterEnabled.Update(mouseX, mouseY, mouseClicked, deltaTime); filters.antichatterEnabled.Draw(renderer);
	if (!smoothingPipelineActive) {
		float warnX = cx + Theme::Size::ToggleWidth + 120;
		renderer.DrawText(L"\xE7BA", warnX, y + 3, 16, 16, Theme::Warning(), renderer.pFontIcon, Renderer::AlignCenter);
		renderer.DrawText(L"Requires Smoothing", warnX + 20, y, 200, Theme::Size::ToggleHeight, Theme::Warning(), renderer.pFontSmall);
	}
	y += 30;

	if (filters.antichatterEnabled.value) {
		filters.antichatterStrength.y = y; filters.antichatterStrength.x = cx;
		filterChanged |= filters.antichatterStrength.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.antichatterStrength.Draw(renderer);
		if (filterSingleColumn) y += 48;
		filters.antichatterMultiplier.y = y; filters.antichatterMultiplier.x = filterRightX;
		filterChanged |= filters.antichatterMultiplier.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.antichatterMultiplier.Draw(renderer);
		y += 48;
		filters.antichatterOffsetX.y = y; filters.antichatterOffsetX.x = cx;
		filterChanged |= filters.antichatterOffsetX.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.antichatterOffsetX.Draw(renderer);
		if (filterSingleColumn) y += 48;
		filters.antichatterOffsetY.y = y; filters.antichatterOffsetY.x = filterRightX;
		filterChanged |= filters.antichatterOffsetY.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.antichatterOffsetY.Draw(renderer);
		y += 48;
	}

	sec.Layout(cx, y, cw, L"NOISE REDUCTION"); y += sec.Draw(renderer);
	filters.noiseEnabled.y = y; filters.noiseEnabled.x = cx;
	filterChanged |= filters.noiseEnabled.Update(mouseX, mouseY, mouseClicked, deltaTime); filters.noiseEnabled.Draw(renderer); y += 30;

	if (filters.noiseEnabled.value) {
		filters.noiseBuffer.y = y; filters.noiseBuffer.x = cx;
		filterChanged |= filters.noiseBuffer.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.noiseBuffer.Draw(renderer);
		if (filterSingleColumn) y += 48;
		filters.noiseThreshold.y = y; filters.noiseThreshold.x = filterRightX;
		filterChanged |= filters.noiseThreshold.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.noiseThreshold.Draw(renderer);
		y += 48;
		filters.noiseIterations.y = y; filters.noiseIterations.x = cx;
		filterChanged |= filters.noiseIterations.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.noiseIterations.Draw(renderer);
		y += 48;
	}

	sec.Layout(cx, y, cw, L"PREDICTION"); y += sec.Draw(renderer);
	filters.velCurveEnabled.y = y; filters.velCurveEnabled.x = cx;
	filterChanged |= filters.velCurveEnabled.Update(mouseX, mouseY, mouseClicked, deltaTime); filters.velCurveEnabled.Draw(renderer);
	if (!smoothingPipelineActive) {
		float warnX = cx + Theme::Size::ToggleWidth + 120;
		renderer.DrawText(L"\xE7BA", warnX, y + 3, 16, 16, Theme::Warning(), renderer.pFontIcon, Renderer::AlignCenter);
		renderer.DrawText(L"Requires Smoothing", warnX + 20, y, 200, Theme::Size::ToggleHeight, Theme::Warning(), renderer.pFontSmall);
	}
	y += 30;

	if (filters.velCurveEnabled.value) {
		filters.velCurveMinSpeed.y = y; filters.velCurveMinSpeed.x = cx;
		filterChanged |= filters.velCurveMinSpeed.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.velCurveMinSpeed.Draw(renderer);
		if (filterSingleColumn) y += 48;
		filters.velCurveMaxSpeed.y = y; filters.velCurveMaxSpeed.x = filterRightX;
		filterChanged |= filters.velCurveMaxSpeed.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.velCurveMaxSpeed.Draw(renderer);
		y += 48;
		filters.velCurveSmoothing.y = y; filters.velCurveSmoothing.x = cx;
		filterChanged |= filters.velCurveSmoothing.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.velCurveSmoothing.Draw(renderer);
		if (filterSingleColumn) y += 48;
		filters.velCurveSharpness.y = y; filters.velCurveSharpness.x = filterRightX;
		filterChanged |= filters.velCurveSharpness.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.velCurveSharpness.Draw(renderer);
		y += 48;
	}

	sec.Layout(cx, y, cw, L"RECONSTRUCTOR"); y += sec.Draw(renderer);
	filters.reconstructorEnabled.y = y; filters.reconstructorEnabled.x = cx;
	filterChanged |= filters.reconstructorEnabled.Update(mouseX, mouseY, mouseClicked, deltaTime); filters.reconstructorEnabled.Draw(renderer); y += 30;

	if (filters.reconstructorEnabled.value) {
		
		renderer.FillRoundedRect(cx, y, cw, 24, 6, D2D1::ColorF(0.85f, 0.2f, 0.2f, 0.10f));
		renderer.DrawRoundedRect(cx, y, cw, 24, 6, D2D1::ColorF(0.85f, 0.2f, 0.2f, 0.25f));
		renderer.DrawText(L"\xE7BA", cx + 8, y, 16, 24, Theme::Error(), renderer.pFontIcon, Renderer::AlignCenter);
		renderer.DrawText(L"High values may cause instability. Use at your own risk.",
			cx + 28, y, cw - 36, 24, Theme::Error(), renderer.pFontSmall);
		y += 30;
		filters.reconStrength.y = y; filters.reconStrength.x = cx;
		filterChanged |= filters.reconStrength.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.reconStrength.Draw(renderer);
		if (filterSingleColumn) y += 48;
		filters.reconVelSmooth.y = y; filters.reconVelSmooth.x = filterRightX;
		filterChanged |= filters.reconVelSmooth.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.reconVelSmooth.Draw(renderer);
		y += 48;
		filters.reconAccelCap.y = y; filters.reconAccelCap.x = cx;
		filterChanged |= filters.reconAccelCap.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.reconAccelCap.Draw(renderer);
		if (filterSingleColumn) y += 48;
		filters.reconPredTime.y = y; filters.reconPredTime.x = filterRightX;
		filterChanged |= filters.reconPredTime.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.reconPredTime.Draw(renderer);
		y += 48;
	}

	sec.Layout(cx, y, cw, L"ADAPTIVE FILTER"); y += sec.Draw(renderer);
	filters.adaptiveEnabled.y = y; filters.adaptiveEnabled.x = cx;
	filterChanged |= filters.adaptiveEnabled.Update(mouseX, mouseY, mouseClicked, deltaTime); filters.adaptiveEnabled.Draw(renderer); y += 30;

	if (filters.adaptiveEnabled.value) {
		filters.adaptiveProcessNoise.y = y; filters.adaptiveProcessNoise.x = cx;
		filterChanged |= filters.adaptiveProcessNoise.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.adaptiveProcessNoise.Draw(renderer);
		if (filterSingleColumn) y += 48;
		filters.adaptiveMeasNoise.y = y; filters.adaptiveMeasNoise.x = filterRightX;
		filterChanged |= filters.adaptiveMeasNoise.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.adaptiveMeasNoise.Draw(renderer);
		y += 48;
		filters.adaptiveVelWeight.y = y; filters.adaptiveVelWeight.x = cx;
		filterChanged |= filters.adaptiveVelWeight.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); filters.adaptiveVelWeight.Draw(renderer);
		y += 48;
	}

	
	sec.Layout(cx, y, cw, L"AETHER SMOOTH"); y += sec.Draw(renderer);
	aether.enabled.y = y; aether.enabled.x = cx;
	filterChanged |= aether.enabled.Update(mouseX, mouseY, mouseClicked, deltaTime); aether.enabled.Draw(renderer); y += 30;

	if (aether.enabled.value) {
		
		aether.lagRemovalEnabled.y = y; aether.lagRemovalEnabled.x = cx;
		filterChanged |= aether.lagRemovalEnabled.Update(mouseX, mouseY, mouseClicked, deltaTime); aether.lagRemovalEnabled.Draw(renderer); y += 28;
		if (aether.lagRemovalEnabled.value) {
			aether.lagRemovalStrength.y = y; aether.lagRemovalStrength.x = cx; aether.lagRemovalStrength.width = hw;
			filterChanged |= aether.lagRemovalStrength.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); aether.lagRemovalStrength.Draw(renderer);
			y += 48;
		}

		
		aether.stabilizerEnabled.y = y; aether.stabilizerEnabled.x = cx;
		filterChanged |= aether.stabilizerEnabled.Update(mouseX, mouseY, mouseClicked, deltaTime); aether.stabilizerEnabled.Draw(renderer); y += 28;
		if (aether.stabilizerEnabled.value) {
			aether.stabilizerStability.y = y; aether.stabilizerStability.x = cx; aether.stabilizerStability.width = hw;
			filterChanged |= aether.stabilizerStability.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); aether.stabilizerStability.Draw(renderer);
			if (filterSingleColumn) y += 48;
			aether.stabilizerSensitivity.y = y; aether.stabilizerSensitivity.x = filterRightX; aether.stabilizerSensitivity.width = hw;
			filterChanged |= aether.stabilizerSensitivity.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); aether.stabilizerSensitivity.Draw(renderer);
			y += 48;
		}

		
		aether.snappingEnabled.y = y; aether.snappingEnabled.x = cx;
		filterChanged |= aether.snappingEnabled.Update(mouseX, mouseY, mouseClicked, deltaTime); aether.snappingEnabled.Draw(renderer); y += 28;
		if (aether.snappingEnabled.value) {
			aether.snappingInner.y = y; aether.snappingInner.x = cx; aether.snappingInner.width = hw;
			filterChanged |= aether.snappingInner.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); aether.snappingInner.Draw(renderer);
			if (filterSingleColumn) y += 48;
			aether.snappingOuter.y = y; aether.snappingOuter.x = filterRightX; aether.snappingOuter.width = hw;
			filterChanged |= aether.snappingOuter.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); aether.snappingOuter.Draw(renderer);
			y += 48;
		}

		
		aether.rhythmFlowEnabled.y = y; aether.rhythmFlowEnabled.x = cx;
		filterChanged |= aether.rhythmFlowEnabled.Update(mouseX, mouseY, mouseClicked, deltaTime); aether.rhythmFlowEnabled.Draw(renderer); y += 28;
		if (aether.rhythmFlowEnabled.value) {
			aether.rhythmFlowStrength.y = y; aether.rhythmFlowStrength.x = cx; aether.rhythmFlowStrength.width = hw;
			filterChanged |= aether.rhythmFlowStrength.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); aether.rhythmFlowStrength.Draw(renderer);
			if (filterSingleColumn) y += 48;
			aether.rhythmFlowRelease.y = y; aether.rhythmFlowRelease.x = filterRightX; aether.rhythmFlowRelease.width = hw;
			filterChanged |= aether.rhythmFlowRelease.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); aether.rhythmFlowRelease.Draw(renderer);
			y += 48;
			aether.rhythmFlowJitter.y = y; aether.rhythmFlowJitter.x = cx; aether.rhythmFlowJitter.width = hw;
			filterChanged |= aether.rhythmFlowJitter.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); aether.rhythmFlowJitter.Draw(renderer);
			y += 48;
		}

		
		aether.suppressionEnabled.y = y; aether.suppressionEnabled.x = cx;
		filterChanged |= aether.suppressionEnabled.Update(mouseX, mouseY, mouseClicked, deltaTime); aether.suppressionEnabled.Draw(renderer); y += 28;
		if (aether.suppressionEnabled.value) {
			aether.suppressionTime.y = y; aether.suppressionTime.x = cx; aether.suppressionTime.width = hw;
			filterChanged |= aether.suppressionTime.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime); aether.suppressionTime.Draw(renderer);
			y += 48;
		}
	}

	
	sec.Layout(cx, y, cw, L"PLUGINS"); y += sec.Draw(renderer);
	DrawPluginFilterControls(cx, y, cw, hw, filterRightX, filterSingleColumn, filterChanged);

	sec.Layout(cx, y, cw, L"OVERCLOCK"); y += sec.Draw(renderer);
	overclockEnabled.y = y; overclockEnabled.x = cx;
	if (overclockEnabled.Update(mouseX, mouseY, mouseClicked, deltaTime)) filterChanged = true;
	overclockEnabled.Draw(renderer);
	y += 34;

	if (overclockEnabled.value) {
		overclockHz.y = y; overclockHz.x = cx; overclockHz.width = hw;
		if (overclockHz.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime)) filterChanged = true;
		overclockHz.Draw(renderer);
		y += 42;

		DrawOverclockInfo(cx, y, cw);
		y += 76;
	}

	penRateLimitEnabled.y = y; penRateLimitEnabled.x = cx;
	if (penRateLimitEnabled.Update(mouseX, mouseY, mouseClicked, deltaTime)) filterChanged = true;
	penRateLimitEnabled.Draw(renderer);
	y += 34;

	if (penRateLimitEnabled.value) {
		penRateLimitHz.y = y; penRateLimitHz.x = cx; penRateLimitHz.width = hw;
		if (penRateLimitHz.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime)) filterChanged = true;
		penRateLimitHz.Draw(renderer);
		y += 48;
	}

	if (filterChanged) {
		ApplyAllSettings();
	}

	filterContentH = (y + filterScrollY) - yStart + 64;
}

void AetherApp::DrawConsolePanel() {
	float cx = Theme::Size::SidebarWidth + Theme::Size::Padding;
	float cw = Theme::Runtime::WindowWidth - Theme::Size::SidebarWidth - Theme::Size::Padding * 2;
	float y = Theme::Size::HeaderHeight + Theme::Size::Padding;
	float inputH = 36;
	float consoleH = Theme::Runtime::WindowHeight - y - 50 - inputH;

	renderer.FillRoundedRect(cx, y, cw, consoleH, Theme::Size::CornerRadius, Theme::BgElevated());
	renderer.DrawRoundedRect(cx, y, cw, consoleH, Theme::Size::CornerRadius, Theme::BorderSubtle());

	auto lines = driver.GetLogLines();
	float lineH = 16;
	float textY = y + 8;
	int maxVisible = (int)(consoleH / lineH) - 1;
	int startIdx = (int)lines.size() - maxVisible;
	if (startIdx < 0) startIdx = 0;

	for (int i = startIdx; i < (int)lines.size(); i++) {
		if (textY > y + consoleH - 8) break;

		std::wstring wline(lines[i].begin(), lines[i].end());
		D2D1_COLOR_F color = Theme::TextSecondary();

		if (lines[i].find("[ERROR]") != std::string::npos) color = Theme::Error();
		else if (lines[i].find("[WARNING]") != std::string::npos) color = Theme::Warning();
		else if (lines[i].find("[STATUS]") != std::string::npos) color = Theme::AccentPrimary();

		renderer.DrawText(wline.c_str(), cx + 12, textY, cw - 24, lineH, color, renderer.pFontMono);
		textY += lineH;
	}

	if (lines.empty()) {
		renderer.DrawText(L"No output \x2014 start the driver to see logs here",
			cx + 12, y + consoleH * 0.5f - 10, cw - 24, 20,
			Theme::TextMuted(), renderer.pFontBody, Renderer::AlignCenter);
	}

	float inputY = y + consoleH + 6;
	consoleInput.x = cx; consoleInput.y = inputY; consoleInput.width = cw;
	consoleInput.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime);
	consoleInput.Draw(renderer);
}

void AetherApp::DrawAboutPanel() {
	float cx = Theme::Size::SidebarWidth + Theme::Size::Padding;
	float cw = Theme::Runtime::WindowWidth - Theme::Size::SidebarWidth - Theme::Size::Padding * 2;
	float y = Theme::Size::HeaderHeight + 38;

	aboutAnimT += deltaTime;
	float logoSize = 38.0f;
	float centerX = cx + cw * 0.5f - 1.0f;
	float floatY = y + sinf(aboutAnimT * 1.2f) * 4.0f;
	float logoX = centerX - logoSize * 0.5f;
	float logoY = floatY;
	float breath = 1.0f + 0.03f * sinf(aboutAnimT * 1.8f);

	D2D1_POINT_2F logoCtr = D2D1::Point2F(centerX, floatY + logoSize * 0.5f - 3.0f);

	D2D1_COLOR_F glowOuter = Theme::AccentGlow();
	glowOuter.a = 0.08f + 0.06f * sinf(aboutAnimT * 1.8f);
	renderer.DrawCircle(logoCtr.x, logoCtr.y, 26.0f * breath, glowOuter, 1.0f);

	D2D1_COLOR_F glowInner = Theme::BrandHot();
	glowInner.a = 0.10f + 0.08f * sinf(aboutAnimT * 2.2f + 0.5f);
	renderer.FillCircle(logoCtr.x, logoCtr.y, 20.0f * breath, glowInner);

	for (int i = 0; i < 3; i++) {
		float angle = aboutAnimT * 0.8f + i * 2.094f;
		float orbitR = 24.0f * breath;
		float dotX = logoCtr.x + cosf(angle) * orbitR;
		float dotY = logoCtr.y + sinf(angle) * orbitR;
		float dotAlpha = 0.3f + 0.2f * sinf(aboutAnimT * 2.5f + i * 1.0f);
		D2D1_COLOR_F dotColor = Theme::AccentSecondary();
		dotColor.a = dotAlpha;
		renderer.FillCircle(dotX, dotY, 2.0f, dotColor);
	}

	D2D1_MATRIX_3X2_F oldTransform;
	renderer.pRT->GetTransform(&oldTransform);
	renderer.pRT->SetTransform(
		D2D1::Matrix3x2F::Scale(breath, breath, logoCtr) * oldTransform);
	DrawLogoBadge(logoX, logoY);
	renderer.pRT->SetTransform(oldTransform);

	y += 36;
	renderer.DrawText(L"AETHER", cx, y, cw, 40, Theme::TextPrimary(), renderer.pFontTitle, Renderer::AlignCenter);
	renderer.FillRectGradientH(cx + cw * 0.5f - 72.0f, y + 39.0f, 144.0f, 2.0f, Theme::BrandHot(), Theme::AccentPrimary());
	y += 54;
	renderer.DrawText(L"High-performance tablet driver for creative workflows", cx, y, cw, 20, Theme::TextSecondary(), renderer.pFontBody, Renderer::AlignCenter);
	y += 30;
	renderer.DrawText(L"Made by q1xlf (known as sophia)", cx, y, cw, 20, Theme::TextAccent(), renderer.pFontSmall, Renderer::AlignCenter);
	y += 50;

	renderer.DrawText(L"Filters: Smoothing \x2022 Antichatter \x2022 Noise Reduction", cx, y, cw, 18, Theme::TextSecondary(), renderer.pFontSmall, Renderer::AlignCenter);
	y += 22;
	renderer.DrawText(L"Reconstructor \x2022 Adaptive Filter \x2022 Aether Smooth", cx, y, cw, 18, Theme::TextAccent(), renderer.pFontSmall, Renderer::AlignCenter);
	y += 22;
	renderer.DrawText(L"Live Cursor \x2022 Input Visualizer \x2022 Theme Presets \x2022 Config Manager", cx, y, cw, 18, Theme::TextMuted(), renderer.pFontSmall, Renderer::AlignCenter);
}

void AetherApp::DrawStatusBar() {
	float w = Theme::Runtime::WindowWidth;
	float h = 28;
	float y = Theme::Runtime::WindowHeight - h;

	renderer.FillRect(0, y, w, h, Theme::BgSurface());
	renderer.DrawLine(0, y, w, y, Theme::BorderSubtle());

	D2D1_COLOR_F statusColor = driver.isConnected ? Theme::Success() : Theme::TextMuted();
	renderer.FillCircle(18, y + h * 0.5f, 3.0f, statusColor);
	const wchar_t* statusText = driver.isConnected ? L"Connected" : L"Not connected";
	renderer.DrawText(statusText, 28, y, 120, h, statusColor, renderer.pFontSmall);

	if (driver.isConnected) {
		std::wstring name(driver.tabletName.begin(), driver.tabletName.end());
		renderer.DrawText(name.c_str(), 150, y, 240, h, Theme::TextMuted(), renderer.pFontSmall);
	}

	if (w > 560.0f) {
		const wchar_t* modes[] = { L"Absolute", L"Relative", L"Windows Ink" };
		const wchar_t* mode = modes[(outputMode.selected >= 0 && outputMode.selected < 3) ? outputMode.selected : 0];
		renderer.DrawText(L"Mode", w - 330, y, 44, h, Theme::TextMuted(), renderer.pFontSmall, Renderer::AlignRight);
		renderer.DrawText(mode, w - 280, y, 86, h, Theme::TextSecondary(), renderer.pFontSmall, Renderer::AlignLeft);
	}

	if (w > 720.0f) {
		wchar_t hzText[64];
		swprintf_s(hzText, L"%.0f Hz", overclockHz.value);
		renderer.DrawText(L"Overclock", w - 178, y, 74, h, Theme::TextMuted(), renderer.pFontSmall, Renderer::AlignRight);
		renderer.DrawText(overclockEnabled.value ? hzText : L"off", w - 96, y, 82, h,
			overclockEnabled.value ? Theme::AccentPrimary() : Theme::TextMuted(),
			renderer.pFontSmall, Renderer::AlignLeft);
	}

	
	UpdateHzMeter();
	if (w > 560.0f && measuredHz > 1.0f) {
		wchar_t hzBuf[32];
		swprintf_s(hzBuf, L"%.0f Hz", measuredHz);
		float penX = (penRateLimitEnabled.value && w > 900.0f) ? (w - 535.0f) : (w - 420.0f);
		renderer.DrawText(L"Pen", penX, y, 30, h, Theme::TextMuted(), renderer.pFontSmall, Renderer::AlignRight);
		D2D1_COLOR_F hzCol = (measuredHz > 200) ? Theme::Success() : (measuredHz > 50) ? Theme::AccentPrimary() : Theme::Warning();
		renderer.DrawText(hzBuf, penX + 35.0f, y, 60, h, hzCol, renderer.pFontSmall, Renderer::AlignLeft);

		if (penRateLimitEnabled.value && w > 900.0f) {
			wchar_t capBuf[40];
			swprintf_s(capBuf, L"Cap %.0f Hz", penRateLimitHz.value);
			D2D1_COLOR_F capColor = Theme::TextMuted();
			if (penRateLimitHz.value <= measuredHz + 1.0f)
				capColor = Theme::AccentSecondary();
			renderer.DrawText(capBuf, penX + 104.0f, y, 105, h, capColor, renderer.pFontSmall, Renderer::AlignLeft);
		}
	}
}




void AetherApp::DrawLiveCursor(float previewX, float previewY, float previewW, float previewH, float fullW, float fullH) {
	if (!driver.penActive.load()) return;

	float px = driver.penX.load();
	float py = driver.penY.load();
	float pressure = driver.penPressure.load();

	
	
	float dotX = previewX + (px / fullW) * previewW;
	float dotY = previewY + (py / fullH) * previewH;

	
	dotX = Clamp(dotX, previewX, previewX + previewW);
	dotY = Clamp(dotY, previewY, previewY + previewH);

	
	liveCursorPulseT += deltaTime * 3.0f;
	float pulse = 1.0f + 0.15f * sinf(liveCursorPulseT);

	
	D2D1_COLOR_F glow = Theme::AccentPrimary();
	glow.a = 0.15f + 0.1f * sinf(liveCursorPulseT);
	renderer.FillCircle(dotX, dotY, 8.0f * pulse, glow);

	
	D2D1_COLOR_F ring = Theme::AccentSecondary();
	ring.a = 0.5f;
	renderer.DrawCircle(dotX, dotY, 5.0f * pulse, ring, 1.0f);

	
	float dotR = 2.0f + pressure * 2.0f;
	renderer.FillCircle(dotX, dotY, dotR, Theme::AccentPrimary());

	
	renderer.FillCircle(dotX, dotY, 1.2f, D2D1::ColorF(0xFFFFFF, 0.9f));
}




void AetherApp::DrawInputVisualizer(float x, float y, float w, float h) {
	std::vector<DriverBridge::TrailPoint> points;
	{
		std::lock_guard<std::mutex> lock(driver.trailMutex);
		points = driver.trail;
	}

	if (points.size() < 2) return;

	
	float fullW = (driver.tabletWidth > 1.0f) ? driver.tabletWidth : 152.0f;
	float fullH = (driver.tabletHeight > 1.0f) ? driver.tabletHeight : 95.0f;

	
	int count = (int)points.size();
	int start = (count > 200) ? count - 200 : 0;

	
	while (start < count && points[start].age > 2.5f) start++;

	int visibleCount = count - start;
	if (visibleCount < 2) return;

	for (int i = start + 1; i < count; i++) {
		
		if (points[i].age > 2.5f || points[i - 1].age > 2.5f) continue;

		float t = (float)(i - start) / (float)(visibleCount); 

		
		float ageFade = 1.0f - Clamp(points[i].age / 2.5f, 0.0f, 1.0f);

		
		float x1 = x + (points[i - 1].x / fullW) * w;
		float y1 = y + (points[i - 1].y / fullH) * h;
		float x2 = x + (points[i].x / fullW) * w;
		float y2 = y + (points[i].y / fullH) * h;

		
		x1 = Clamp(x1, x, x + w); y1 = Clamp(y1, y, y + h);
		x2 = Clamp(x2, x, x + w); y2 = Clamp(y2, y, y + h);

		
		float alpha = t * 0.6f * ageFade;
		D2D1_COLOR_F col = Theme::AccentPrimary();
		col.a = alpha;

		float lineW = (0.5f + t * 1.5f) * ageFade;
		renderer.DrawLine(x1, y1, x2, y2, col, lineW);
	}

	
	if (count > 0 && points[count - 1].age < 0.5f) {
		const auto& last = points[count - 1];
		float hx = x + (last.x / fullW) * w;
		float hy = y + (last.y / fullH) * h;
		renderer.FillCircle(hx, hy, 2.5f, Theme::AccentSecondary());
	}
}




void AetherApp::GetThemeSlotColor(Theme::ThemeData& t, int slot, float& r, float& g, float& b) {
	switch (slot) {
	case 0: r = t.bgDeep[0]; g = t.bgDeep[1]; b = t.bgDeep[2]; break;
	case 1: r = t.bgBase[0]; g = t.bgBase[1]; b = t.bgBase[2]; break;
	case 2: r = t.bgSurface[0]; g = t.bgSurface[1]; b = t.bgSurface[2]; break;
	case 3: r = t.bgElevated[0]; g = t.bgElevated[1]; b = t.bgElevated[2]; break;
	case 4: r = t.textPri[0]; g = t.textPri[1]; b = t.textPri[2]; break;
	case 5: r = t.accent[0]; g = t.accent[1]; b = t.accent[2]; break;
	}
}

void AetherApp::SetThemeSlotColor(Theme::ThemeData& t, int slot, float r, float g, float b) {
	switch (slot) {
	case 0: t.bgDeep[0]=r; t.bgDeep[1]=g; t.bgDeep[2]=b; break;
	case 1: t.bgBase[0]=r; t.bgBase[1]=g; t.bgBase[2]=b; break;
	case 2: t.bgSurface[0]=r; t.bgSurface[1]=g; t.bgSurface[2]=b; break;
	case 3: t.bgElevated[0]=r; t.bgElevated[1]=g; t.bgElevated[2]=b; break;
	case 4: t.textPri[0]=r; t.textPri[1]=g; t.textPri[2]=b;
		
		t.textSec[0]=r*0.71f; t.textSec[1]=g*0.71f; t.textSec[2]=b*0.71f;
		t.textMut[0]=r*0.48f; t.textMut[1]=g*0.48f; t.textMut[2]=b*0.48f;
		break;
	case 5: t.accent[0]=r; t.accent[1]=g; t.accent[2]=b; break;
	}
}

void AetherApp::ResetThemeToDefault(int themeIndex) {
	if (themeIndex >= 0 && themeIndex < uiThemeCount && uiThemeDefaults[themeIndex]) {
		uiThemes[themeIndex] = *uiThemeDefaults[themeIndex];
		if (themeIndex == currentTheme) {
			Theme::ApplyTheme(uiThemes[themeIndex]);
			accentPicker.SetRGB(uiThemes[themeIndex].accent[0], uiThemes[themeIndex].accent[1], uiThemes[themeIndex].accent[2]);
			if (hWnd) { extern void ApplyAetherWindowTheme(HWND); ApplyAetherWindowTheme(hWnd); }
		}
	}
}




void AetherApp::DrawThemeSelector(float x, float& y, float w) {
	int cols = 4;
	float cardW = (w - (cols - 1) * 8.0f) / (float)cols;
	float cardH = 64.0f;
	float gap = 8.0f;

	for (int i = 0; i < uiThemeCount; i++) {
		int col = i % cols;
		int row = i / cols;
		float cx = x + col * (cardW + gap);
		float cy = y + row * (cardH + gap);

		Theme::ThemeData& t = uiThemes[i];

		bool hovered = PointInRect(mouseX, mouseY, cx, cy, cardW, cardH);
		themeHoverT[i] = Lerp(themeHoverT[i], hovered ? 1.0f : 0.0f, deltaTime * Theme::Anim::SpeedFast);
		bool isActive = (i == currentTheme);

		
		D2D1_COLOR_F cardBg = D2D1::ColorF(t.bgSurface[0], t.bgSurface[1], t.bgSurface[2]);
		renderer.FillRoundedRect(cx, cy, cardW, cardH, 8, cardBg);

		
		D2D1_COLOR_F accentCol = D2D1::ColorF(t.accent[0], t.accent[1], t.accent[2]);
		D2D1_COLOR_F bgCol = D2D1::ColorF(t.bgBase[0], t.bgBase[1], t.bgBase[2]);
		renderer.FillRoundedRect(cx + 4, cy + 4, cardW - 8, 12, 4, bgCol);
		renderer.FillRoundedRect(cx + 4, cy + 4, (cardW - 8) * 0.5f, 12, 4, accentCol);

		
		D2D1_COLOR_F nameCol = D2D1::ColorF(t.textPri[0], t.textPri[1], t.textPri[2]);
		renderer.DrawText(t.name, cx + 8, cy + 22, cardW - 40, 20, nameCol, renderer.pFontSmall, Renderer::AlignLeft);

		
		float dotY = cy + 46;
		float dotR = 5.0f;
		float dotGap = 13.0f;
		float dotStartX = cx + 10;

		for (int s = 0; s < THEME_SLOT_COUNT; s++) {
			float dCX = dotStartX + s * dotGap;
			float dCY = dotY;
			float sr, sg, sb;
			GetThemeSlotColor(t, s, sr, sg, sb);

			
			D2D1_COLOR_F dotCol = D2D1::ColorF(sr, sg, sb);
			renderer.FillCircle(dCX, dCY, dotR, dotCol);
			renderer.DrawCircle(dCX, dCY, dotR, D2D1::ColorF(0.5f, 0.5f, 0.5f, 0.3f), 0.5f);

			
			bool dotHovered = PointInRect(mouseX, mouseY, dCX - dotR, dCY - dotR, dotR * 2, dotR * 2);
			if (dotHovered) {
				renderer.DrawCircle(dCX, dCY, dotR + 2, Theme::AccentPrimary(), 1.5f);
				Tooltip::Show(mouseX, mouseY, themeSlotNames[s], deltaTime);
			}

			
			if (dotHovered && mouseClicked) {
				
				if (!isActive) {
					currentTheme = i;
					Theme::ApplyTheme(t);
					accentPicker.SetRGB(t.accent[0], t.accent[1], t.accent[2]);
					if (hWnd) { extern void ApplyAetherWindowTheme(HWND); ApplyAetherWindowTheme(hWnd); }
				}
				
				editingTheme = i;
				editingSlot = s;
				slotPicker.SetRGB(sr, sg, sb);
			}
		}

		
		if (isActive) {
			renderer.DrawRoundedRect(cx, cy, cardW, cardH, 8, Theme::AccentPrimary(), 2.0f);
			
			float badgeR = 10.0f;
			float badgeCX = cx + cardW - badgeR - 4;
			float badgeCY = cy + badgeR + 4;
			renderer.FillCircle(badgeCX, badgeCY, badgeR, Theme::AccentPrimary());
			renderer.DrawCircle(badgeCX, badgeCY, badgeR, D2D1::ColorF(0xFFFFFF, 0.3f), 1.0f);
			renderer.DrawText(L"\x2713", badgeCX - 7, badgeCY - 7, 14, 14, D2D1::ColorF(0xFFFFFF), renderer.pFontSmall, Renderer::AlignCenter);

			
			float resetX = cx + cardW - 38;
			float resetY = cy + 24;
			float resetW = 32, resetH = 16;
			bool resetHovered = PointInRect(mouseX, mouseY, resetX, resetY, resetW, resetH);
			D2D1_COLOR_F resetBg = resetHovered ? Theme::BgHover() : D2D1::ColorF(0, 0, 0, 0);
			if (resetHovered) {
				renderer.FillRoundedRect(resetX, resetY, resetW, resetH, 3, resetBg);
				Tooltip::Show(mouseX, mouseY, L"Reset this theme to default colors", deltaTime);
			}
			renderer.DrawText(L"Reset", resetX, resetY, resetW, resetH, 
				resetHovered ? Theme::TextPrimary() : Theme::TextMuted(), renderer.pFontSmall, Renderer::AlignCenter);
			if (resetHovered && mouseClicked) {
				ResetThemeToDefault(i);
				editingTheme = -1;
				editingSlot = -1;
				AutoSaveConfig();
			}
		} else {
			D2D1_COLOR_F border = LerpColor(D2D1::ColorF(t.borderSub[0], t.borderSub[1], t.borderSub[2], t.borderSub[3]),
				accentCol, themeHoverT[i] * 0.5f);
			renderer.DrawRoundedRect(cx, cy, cardW, cardH, 8, border);
		}

		
		if (hovered && mouseClicked && !isActive) {
			
			bool clickedDot = false;
			for (int s = 0; s < THEME_SLOT_COUNT; s++) {
				float dCX = dotStartX + s * dotGap;
				if (PointInRect(mouseX, mouseY, dCX - dotR, dotY - dotR, dotR * 2, dotR * 2)) {
					clickedDot = true; break;
				}
			}
			if (!clickedDot) {
				currentTheme = i;
				Theme::ApplyTheme(t);
				accentPicker.SetRGB(t.accent[0], t.accent[1], t.accent[2]);
				wchar_t hexBuf[16];
				swprintf_s(hexBuf, L"#%02X%02X%02X", (int)(t.accent[0] * 255), (int)(t.accent[1] * 255), (int)(t.accent[2] * 255));
				wcscpy_s(hexColorInput.buffer, hexBuf);
				hexColorInput.cursor = (int)wcslen(hexBuf);
				if (hWnd) { extern void ApplyAetherWindowTheme(HWND); ApplyAetherWindowTheme(hWnd); }
				editingTheme = -1;
				editingSlot = -1;
				AutoSaveConfig();
			}
		}
	}

	int rows = (uiThemeCount + cols - 1) / cols;
	y += rows * (cardH + gap) + 8.0f;

	
	if (editingTheme >= 0 && editingTheme < uiThemeCount && editingSlot >= 0) {
		Theme::ThemeData& et = uiThemes[editingTheme];

		
		float pickerPanelW = w;
		float pickerPanelH = slotPicker.GetTotalHeight() + 72;
		renderer.FillRoundedRect(x, y, pickerPanelW, pickerPanelH, 8, Theme::BgElevated());
		renderer.DrawRoundedRect(x, y, pickerPanelW, pickerPanelH, 8, Theme::BorderAccent());

		
		wchar_t editLabel[128];
		swprintf_s(editLabel, L"Editing: %s \x2022 %s", et.name, themeSlotNames[editingSlot]);
		renderer.DrawText(editLabel, x + 12, y + 6, pickerPanelW - 80, 20, Theme::TextAccent(), renderer.pFontSmall);

		
		{
			float closeX = x + pickerPanelW - 60, closeY = y + 6, closeW = 48, closeH = 20;
			bool closeHovered = PointInRect(mouseX, mouseY, closeX, closeY, closeW, closeH);
			if (closeHovered) renderer.FillRoundedRect(closeX, closeY, closeW, closeH, 4, Theme::BgHover());
			renderer.DrawText(L"Close", closeX, closeY, closeW, closeH,
				closeHovered ? Theme::TextPrimary() : Theme::TextMuted(), renderer.pFontSmall, Renderer::AlignCenter);
			if (closeHovered && mouseClicked) {
				editingTheme = -1; editingSlot = -1;
			}
		}

		
		slotPicker.x = x + 12;
		slotPicker.y = y + 30;
		slotPicker.width = pickerPanelW * 0.45f;
		if (slotPicker.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime)) {
			float r, g, b;
			slotPicker.GetRGB(r, g, b);
			SetThemeSlotColor(et, editingSlot, r, g, b);
			
			if (editingTheme == currentTheme) {
				Theme::ApplyTheme(et);
				if (editingSlot == 5) {
					accentPicker.SetRGB(r, g, b);
					wchar_t hexBuf[16];
					swprintf_s(hexBuf, L"#%02X%02X%02X", (int)(r * 255), (int)(g * 255), (int)(b * 255));
					wcscpy_s(hexColorInput.buffer, hexBuf);
					hexColorInput.cursor = (int)wcslen(hexBuf);
				}
				if (hWnd) { extern void ApplyAetherWindowTheme(HWND); ApplyAetherWindowTheme(hWnd); }
			}
			AutoSaveConfig();
		}
		slotPicker.Draw(renderer);

		
		{
			float hexLabelY = slotPicker.y + slotPicker.GetTotalHeight() + 4;
			renderer.DrawText(L"Hex:", x + 12, hexLabelY, 30, 24, Theme::TextMuted(), renderer.pFontSmall);
			slotHexInput.x = x + 44;
			slotHexInput.y = hexLabelY;
			slotHexInput.width = slotPicker.width - 34;
			slotHexInput.Update(mouseX, mouseY, mouseDown, mouseClicked, deltaTime);
			slotHexInput.Draw(renderer);

			
			if (!slotHexInput.focused) {
				float pr, pg, pb;
				slotPicker.GetRGB(pr, pg, pb);
				wchar_t hb[16];
				swprintf_s(hb, L"#%02X%02X%02X", (int)(pr*255), (int)(pg*255), (int)(pb*255));
				wcscpy_s(slotHexInput.buffer, hb);
				slotHexInput.cursor = (int)wcslen(hb);
			}
		}

		
		float slotBtnX = x + pickerPanelW * 0.5f + 20;
		float slotBtnY = y + 32;
		for (int s = 0; s < THEME_SLOT_COUNT; s++) {
			float sr, sg, sb;
			GetThemeSlotColor(et, s, sr, sg, sb);
			float bx = slotBtnX, by = slotBtnY + s * 26.0f;
			float bw = pickerPanelW * 0.45f - 20, bh = 22.0f;

			bool slotActive = (s == editingSlot);
			bool slotHov = PointInRect(mouseX, mouseY, bx, by, bw, bh);

			D2D1_COLOR_F slotBg = slotActive ? Theme::AccentDim() : (slotHov ? Theme::BgHover() : D2D1::ColorF(0, 0, 0, 0));
			renderer.FillRoundedRect(bx, by, bw, bh, 4, slotBg);

			
			renderer.FillCircle(bx + 10, by + bh * 0.5f, 5, D2D1::ColorF(sr, sg, sb));
			renderer.DrawCircle(bx + 10, by + bh * 0.5f, 5, Theme::BorderSubtle(), 0.5f);

			
			D2D1_COLOR_F lblCol = slotActive ? Theme::AccentPrimary() : (slotHov ? Theme::TextPrimary() : Theme::TextSecondary());
			renderer.DrawText(themeSlotNames[s], bx + 22, by, bw - 22, bh, lblCol, renderer.pFontSmall);

			
			if (slotHov && mouseClicked) {
				editingSlot = s;
				slotPicker.SetRGB(sr, sg, sb);
			}
		}

		y += pickerPanelH + 8.0f;
	}
}




void AetherApp::DrawConfigManager(float x, float& y, float w) {
	RefreshConfigFiles();

	SectionHeader sec;
	sec.Layout(x, y, w, L"CONFIGS");
	y += sec.Draw(renderer);

	float buttonW = 96.0f;
	float gap = 8.0f;
	float buttonY = y;
	float saveX = x + w - buttonW * 2.0f - gap;
	float loadX = x + w - buttonW;

	saveConfigBtn.Layout(saveX, buttonY, buttonW, 28, L"Save As", false);
	if (saveConfigBtn.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
		SaveConfigWithDialog();
	}
	saveConfigBtn.Draw(renderer);

	loadConfigBtn.Layout(loadX, buttonY, buttonW, 28, L"Load File", false);
	if (loadConfigBtn.Update(mouseX, mouseY, mouseClicked, deltaTime)) {
		LoadConfigWithDialog();
	}
	loadConfigBtn.Draw(renderer);

	std::wstring activeName = L"Last session";
	if (!activeConfigPath.empty()) {
		size_t slash = activeConfigPath.find_last_of(L"\\/");
		activeName = (slash == std::wstring::npos) ? activeConfigPath : activeConfigPath.substr(slash + 1);
	}
	renderer.DrawText(activeName.c_str(), x + 4, y, saveX - x - 12, 28, Theme::TextSecondary(), renderer.pFontSmall);
	y += 36;

	if (configEntries.empty()) {
		renderer.FillRoundedRect(x, y, w, 30, 6, Theme::BgElevated());
		renderer.DrawRoundedRect(x, y, w, 30, 6, Theme::BorderSubtle());
		renderer.DrawText(L"No configs in config folder", x + 10, y, w - 20, 30, Theme::TextMuted(), renderer.pFontSmall);
		y += 38;
		return;
	}

	float rowH = 30.0f;
	for (int i = 0; i < (int)configEntries.size(); i++) {
		float rowY = y + i * (rowH + 6.0f);
		bool selected = (i == selectedConfigIndex);
		bool hovered = PointInRect(mouseX, mouseY, x, rowY, w, rowH);
		D2D1_COLOR_F bg = selected ? Theme::AccentDim() : (hovered ? Theme::BgHover() : Theme::BgElevated());
		renderer.FillRoundedRect(x, rowY, w, rowH, 6, bg);
		renderer.DrawRoundedRect(x, rowY, w, rowH, 6, selected ? Theme::BorderAccent() : Theme::BorderSubtle());

		float miniW = 54.0f;
		float loadBtnX = x + w - miniW - 8.0f;
		float saveBtnX = loadBtnX - miniW - 6.0f;
		float btnY = rowY + 4.0f;
		float btnH = rowH - 8.0f;
		bool saveHovered = PointInRect(mouseX, mouseY, saveBtnX, btnY, miniW, btnH);
		bool loadHovered = PointInRect(mouseX, mouseY, loadBtnX, btnY, miniW, btnH);

		renderer.DrawText(configEntries[i].name.c_str(), x + 10, rowY, saveBtnX - x - 18, rowH,
			selected ? Theme::TextPrimary() : Theme::TextSecondary(), renderer.pFontSmall);

		D2D1_COLOR_F saveBg = saveHovered ? Theme::BgHover() : Theme::BgSurface();
		renderer.FillRoundedRect(saveBtnX, btnY, miniW, btnH, 5, saveBg);
		renderer.DrawRoundedRect(saveBtnX, btnY, miniW, btnH, 5, Theme::BorderSubtle());
		renderer.DrawText(L"Save", saveBtnX, btnY, miniW, btnH, Theme::TextPrimary(), renderer.pFontSmall, Renderer::AlignCenter);

		D2D1_COLOR_F loadBg = loadHovered ? Theme::AccentPrimary() : Theme::BgSurface();
		renderer.FillRoundedRect(loadBtnX, btnY, miniW, btnH, 5, loadBg);
		renderer.DrawRoundedRect(loadBtnX, btnY, miniW, btnH, 5, loadHovered ? Theme::BorderAccent() : Theme::BorderSubtle());
		renderer.DrawText(L"Load", loadBtnX, btnY, miniW, btnH,
			loadHovered ? D2D1::ColorF(0xFFFFFF) : Theme::TextPrimary(), renderer.pFontSmall, Renderer::AlignCenter);

		if (mouseClicked) {
			if (saveHovered) {
				selectedConfigIndex = i;
				activeConfigPath = configEntries[i].path;
				SaveConfig(activeConfigPath);
				RefreshConfigFiles();
				break;
			}
			else if (loadHovered) {
				selectedConfigIndex = i;
				activeConfigPath = configEntries[i].path;
				LoadConfig(activeConfigPath);
				ApplyAllSettings();
				RefreshConfigFiles();
				break;
			}
			else if (hovered) {
				selectedConfigIndex = i;
			}
		}
	}

	y += (float)configEntries.size() * (rowH + 6.0f) + 4.0f;
}

void AetherApp::UpdateHzMeter() {
	if (!driver.penActive.load()) {
		measuredHz = 0;
		return;
	}
	float serviceHz = driver.penHz.load();
	if (serviceHz > 0.1f) {
		
		measuredHz = Lerp(measuredHz, serviceHz, deltaTime * 3.0f);
	}
}
