#pragma once

#include <string>

std::wstring Utf8ToWideService(const std::string& text);
std::string WideToUtf8Service(const std::wstring& text);
std::wstring GetAetherServiceDirectory();
std::wstring GetAetherPluginDirectory();
bool EnsureAetherPluginDirectory();
bool InstallAetherPluginDll(const std::wstring& sourcePath, std::wstring* installedPath);

struct PluginSecurityPolicy {
	double clampRadiusMm;
	bool   buttonGate;
	bool   pressureGate;
	bool   allowlistEnabled;
};
extern PluginSecurityPolicy g_pluginSecurity;

std::string ComputeFileSha256Hex(const std::wstring& path);
bool IsPluginHashAllowed(const std::string& sha256HexLower);
bool PluginAllowlistCheck(const std::wstring& dllPath);
void ReloadPluginAllowlist();
