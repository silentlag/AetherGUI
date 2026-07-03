#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <atomic>

#if defined(_WIN32)
#include <windows.h>
#endif

struct HotkeyBinding {
	int id;
	int modifiers;
	int vk;
	std::string action;
	bool registered;
};

class HotkeyManager {
public:
	HotkeyManager();
	~HotkeyManager();

	void Start();
	void Stop();

	bool Bind(int id, int modifiers, int vk, const std::string& action);
	void Clear();
	void ClearId(int id);
	std::vector<HotkeyBinding> GetBindings();

private:
	std::vector<HotkeyBinding> bindings;
	std::mutex bindMutex;
	std::atomic<bool> running{false};
	std::atomic<bool> dirty{false};
#if defined(_WIN32)
	DWORD threadId;
	HANDLE threadHandle;
	HANDLE wakeEvent;
	static DWORD WINAPI ThreadProc(LPVOID param);
	void Run();
	void ReRegister();
	int ExecuteAction(const std::string& action);
#endif
};
