#pragma once

#include <string>

std::wstring Utf8ToWideService(const std::string& text);
std::string WideToUtf8Service(const std::wstring& text);
std::wstring GetAetherServiceDirectory();
std::wstring GetAetherPluginDirectory();
bool EnsureAetherPluginDirectory();
bool InstallAetherPluginDll(const std::wstring& sourcePath, std::wstring* installedPath);
