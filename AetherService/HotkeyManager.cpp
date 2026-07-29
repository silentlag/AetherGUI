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
	running.store(true);
	LOG_INFO("Hotkey manager (disabled on service side - handled by GUI LL hook).\n");
}

void HotkeyManager::Stop() {
	running.store(false);
#if defined(_WIN32)
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
	return true;
}

void HotkeyManager::Clear() {
	std::lock_guard<std::mutex> lock(bindMutex);
	bindings.clear();
}

void HotkeyManager::ClearId(int id) {
	std::lock_guard<std::mutex> lock(bindMutex);
	for (auto it = bindings.begin(); it != bindings.end(); ++it) {
		if (it->id == id) { bindings.erase(it); break; }
	}
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
}

int HotkeyManager::ExecuteAction(const std::string& action) {
	if (action.empty()) return 0;
	CommandLine* cmd = new CommandLine(action);
	bool ok = ProcessCommand(cmd);
	delete cmd;
	return ok ? 1 : 0;
}

void HotkeyManager::Run() {
}

#endif
