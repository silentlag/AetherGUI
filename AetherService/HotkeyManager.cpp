#include "stdafx.h"
#include "HotkeyManager.h"
#include "CommandLine.h"
#include "ProcessCommand.h"

#define LOG_MODULE "Hotkey"
#include "Logger.h"

HotkeyManager::HotkeyManager() {
#if defined(_WIN32)
	threadId = 0;
	threadHandle = NULL;
	wakeEvent = NULL;
#endif
}

HotkeyManager::~HotkeyManager() {
	Stop();
}

void HotkeyManager::Start() {
#if defined(_WIN32)
	if (running.load()) return;
	wakeEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
	running.store(true);
	dirty.store(true);
	threadHandle = CreateThread(NULL, 0, ThreadProc, this, 0, &threadId);
#endif
}

void HotkeyManager::Stop() {
#if defined(_WIN32)
	if (!running.load()) return;
	running.store(false);
	if (threadId) PostThreadMessageW(threadId, WM_QUIT, 0, 0);
	if (wakeEvent) SetEvent(wakeEvent);
	if (threadHandle) {
		WaitForSingleObject(threadHandle, 1000);
		CloseHandle(threadHandle);
		threadHandle = NULL;
	}
	if (wakeEvent) { CloseHandle(wakeEvent); wakeEvent = NULL; }
	threadId = 0;
#endif
}

bool HotkeyManager::Bind(int id, int modifiers, int vk, const std::string& action) {
	if (id < 1 || id > 32) return false;
	{
		std::lock_guard<std::mutex> lock(bindMutex);
		bool found = false;
		for (auto& b : bindings) {
			if (b.id == id) {
				b.modifiers = modifiers;
				b.vk = vk;
				b.action = action;
				b.registered = false;
				found = true;
				break;
			}
		}
		if (!found) {
			HotkeyBinding b;
			b.id = id;
			b.modifiers = modifiers;
			b.vk = vk;
			b.action = action;
			b.registered = false;
			bindings.push_back(b);
		}
	}
	dirty.store(true);
#if defined(_WIN32)
	if (threadId) PostThreadMessageW(threadId, WM_USER, 0, 0);
#endif
	return true;
}

void HotkeyManager::Clear() {
	{
		std::lock_guard<std::mutex> lock(bindMutex);
		bindings.clear();
	}
	dirty.store(true);
#if defined(_WIN32)
	if (threadId) PostThreadMessageW(threadId, WM_USER, 0, 0);
#endif
}

void HotkeyManager::ClearId(int id) {
	{
		std::lock_guard<std::mutex> lock(bindMutex);
		for (auto it = bindings.begin(); it != bindings.end(); ++it) {
			if (it->id == id) { bindings.erase(it); break; }
		}
	}
	dirty.store(true);
#if defined(_WIN32)
	if (threadId) PostThreadMessageW(threadId, WM_USER, 0, 0);
#endif
}

std::vector<HotkeyBinding> HotkeyManager::GetBindings() {
	std::lock_guard<std::mutex> lock(bindMutex);
	return bindings;
}

#if defined(_WIN32)

DWORD WINAPI HotkeyManager::ThreadProc(LPVOID param) {
	HotkeyManager* self = (HotkeyManager*)param;
	self->Run();
	return 0;
}

void HotkeyManager::ReRegister() {
	std::vector<HotkeyBinding> snapshot;
	{
		std::lock_guard<std::mutex> lock(bindMutex);
		snapshot = bindings;
	}
	for (auto& b : snapshot) {
		if (!b.registered) {
			RegisterHotKey(NULL, b.id, b.modifiers | MOD_NOREPEAT, b.vk);
			b.registered = true;
			LOG_INFO("Hotkey %d registered (mods=0x%X vk=0x%X) -> %s\n", b.id, b.modifiers, b.vk, b.action.c_str());
		}
	}
	{
		std::lock_guard<std::mutex> lock(bindMutex);
		bindings = snapshot;
	}
}

int HotkeyManager::ExecuteAction(const std::string& action) {
	if (action.empty()) return 0;
	CommandLine* cmd = new CommandLine(action);
	bool ok = ProcessCommand(cmd);
	delete cmd;
	return ok ? 1 : 0;
}

void HotkeyManager::Run() {
	MSG msg;
	PeekMessageW(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);
	ReRegister();
	dirty.store(false);

	while (running.load()) {
		BOOL r = GetMessageW(&msg, NULL, 0, 0);
		if (r <= 0) break;
		if (msg.message == WM_HOTKEY) {
			int id = (int)msg.wParam;
			std::string action;
			{
				std::lock_guard<std::mutex> lock(bindMutex);
				for (auto& b : bindings) {
					if (b.id == id) { action = b.action; break; }
				}
			}
			if (!action.empty()) {
				LOG_INFO("Hotkey %d fired: %s\n", id, action.c_str());
				ExecuteAction(action);
			}
		}
		else if (msg.message == WM_USER) {
			if (dirty.exchange(false)) ReRegister();
		}
	}

	std::vector<HotkeyBinding> snapshot;
	{
		std::lock_guard<std::mutex> lock(bindMutex);
		snapshot = bindings;
	}
	for (auto& b : snapshot) {
		UnregisterHotKey(NULL, b.id);
	}
}

#endif
