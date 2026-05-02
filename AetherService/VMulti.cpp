#include "stdafx.h"
#include "VMulti.h"

#define LOG_MODULE "VMulti"
#include "Logger.h"

#ifndef MOUSEEVENTF_MOVE_NOCOALESCE
#define MOUSEEVENTF_MOVE_NOCOALESCE 0x2000
#endif


VMulti::VMulti() {
	isOpen = false;
	debugEnabled = false;
	lastButtons = 0;
	pendingButtons = 0;
	buttonsChanged = false;
	monitorInfoLocked = false;

	mode = ModeAbsoluteMouse;

	
	reportAbsoluteMouse.vmultiId = 0x40;
	reportAbsoluteMouse.reportLength = 7;
	reportAbsoluteMouse.reportId = 3;
	reportAbsoluteMouse.buttons = 0;
	reportAbsoluteMouse.wheel = 0;

	
	reportRelativeMouse.vmultiId = 0x40;
	reportRelativeMouse.reportLength = 5;
	reportRelativeMouse.reportId = 4;
	reportRelativeMouse.buttons = 0;
	reportRelativeMouse.x = 0;
	reportRelativeMouse.y = 0;
	reportRelativeMouse.wheel = 0;

	
	reportDigitizer.vmultiId = 0x40;
	reportDigitizer.reportLength = 8;
	reportDigitizer.reportId = 5;
	reportDigitizer.buttons = 0;
	reportDigitizer.pressure = 0;

	
	memset(&relativeData, 0, sizeof(relativeData));
	relativeData.sensitivity = 1;
	relativeData.resetDistance = 400;
	relativeData.firstReport = true;

	
	UpdateMonitorInfo();

	
	memset(reportBuffer, 0, 65);
	memset(lastReportBuffer, 0, 65);

	
	hidDevice = new HIDDevice(0x00FF, 0xBACC, 0xFF00, 0x0001);
	if (hidDevice->isOpen) {
		isOpen = true;
		outputEnabled = true;
	}
	else {
		delete hidDevice;
		hidDevice = NULL;
		
		isOpen = true;
		outputEnabled = true;
	}
}


VMulti::~VMulti() {
	if (hidDevice != NULL)
		delete hidDevice;
}




bool VMulti::HasReportChanged() {
	
	int cmpLen;
	switch (mode) {
		case ModeDigitizer: cmpLen = sizeof(reportDigitizer); break;
		case ModeRelativeMouse: cmpLen = sizeof(INPUT); break;
		case ModeAbsoluteMouse: cmpLen = sizeof(INPUT); break;
		case ModeSendInput: cmpLen = sizeof(INPUT); break;
		default: cmpLen = 65; break;
	}
	return memcmp(reportBuffer, lastReportBuffer, cmpLen) != 0;
}




void VMulti::ResetRelativeData(double x, double y) {
	relativeData.targetPosition.Set(x, y);
	relativeData.lastPosition.Set(x, y);
	relativeData.currentPosition.x = (int)x;
	relativeData.currentPosition.y = (int)y;
	relativeData.accumX = 0;
	relativeData.accumY = 0;
}

void VMulti::InvalidateRelativeData() {
	relativeData.accumX = 0;
	relativeData.accumY = 0;
	relativeData.firstReport = true;
}

void VMulti::UpdateMonitorInfo() {
	if (monitorInfoLocked) {
		return;
	}
	monitorInfo.primaryWidth = GetSystemMetrics(SM_CXSCREEN);
	monitorInfo.primaryHeight = GetSystemMetrics(SM_CYSCREEN);
	monitorInfo.virtualWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	monitorInfo.virtualHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
	monitorInfo.virtualX = GetSystemMetrics(SM_XVIRTUALSCREEN);
	monitorInfo.virtualY = GetSystemMetrics(SM_YVIRTUALSCREEN);
}

void VMulti::SetMonitorInfo(double primaryWidth, double primaryHeight, double virtualWidth, double virtualHeight, double virtualX, double virtualY) {
	monitorInfo.primaryWidth = primaryWidth;
	monitorInfo.primaryHeight = primaryHeight;
	monitorInfo.virtualWidth = virtualWidth;
	monitorInfo.virtualHeight = virtualHeight;
	monitorInfo.virtualX = virtualX;
	monitorInfo.virtualY = virtualY;
	monitorInfoLocked = true;
}

void VMulti::CreateReport(BYTE buttons, double x, double y, double pressure) {
	double dx, dy, distance;

	
	buttonsChanged = (buttons != pendingButtons);
	pendingButtons = buttons;

	
	
	
	if (mode == VMulti::ModeAbsoluteMouse) {
		INPUT input = { 0 };
		input.type = INPUT_MOUSE;
		input.mi.mouseData = 0;
		double normX = (x - monitorInfo.virtualX) / monitorInfo.virtualWidth;
		double normY = (y - monitorInfo.virtualY) / monitorInfo.virtualHeight;
		if (normX < 0) normX = 0; if (normX > 1) normX = 1;
		if (normY < 0) normY = 0; if (normY > 1) normY = 1;
		input.mi.dx = (LONG)floor(normX * 65535.0);
		input.mi.dy = (LONG)floor(normY * 65535.0);
		input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_MOVE_NOCOALESCE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;

		if ((buttons & 0x01) && !(lastButtons & 0x01)) input.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
		else if (!(buttons & 0x01) && (lastButtons & 0x01)) input.mi.dwFlags |= MOUSEEVENTF_LEFTUP;
		if ((buttons & 0x02) && !(lastButtons & 0x02)) input.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
		else if (!(buttons & 0x02) && (lastButtons & 0x02)) input.mi.dwFlags |= MOUSEEVENTF_RIGHTUP;
		if ((buttons & 0x04) && !(lastButtons & 0x04)) input.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN;
		else if (!(buttons & 0x04) && (lastButtons & 0x04)) input.mi.dwFlags |= MOUSEEVENTF_MIDDLEUP;

		lastButtons = buttons;
		memcpy(reportBuffer, &input, sizeof(INPUT));
	}

	
	
	
	else if (mode == VMulti::ModeRelativeMouse) {

		
		if (relativeData.firstReport) {
			relativeData.firstReport = false;
			ResetRelativeData(x, y);
			dx = 0;
			dy = 0;
		} else {
			dx = x - relativeData.lastPosition.x;
			dy = y - relativeData.lastPosition.y;
			distance = sqrt(dx * dx + dy * dy);

			
			
			if (distance > relativeData.resetDistance) {
				ResetRelativeData(x, y);
				dx = 0;
				dy = 0;
			}
		}

		dx *= relativeData.sensitivity;
		dy *= relativeData.sensitivity;

		
		
		relativeData.accumX += dx;
		relativeData.accumY += dy;

		int intMoveX = (int)relativeData.accumX;
		int intMoveY = (int)relativeData.accumY;

		
		relativeData.accumX -= intMoveX;
		relativeData.accumY -= intMoveY;

		
		if (intMoveX > 500) intMoveX = 500;
		if (intMoveX < -500) intMoveX = -500;
		if (intMoveY > 500) intMoveY = 500;
		if (intMoveY < -500) intMoveY = -500;

		relativeData.lastPosition.Set(x, y);

		INPUT input = { 0 };
		input.type = INPUT_MOUSE;
		input.mi.dx = (LONG)intMoveX;
		input.mi.dy = (LONG)intMoveY;
		input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_MOVE_NOCOALESCE;

		if ((buttons & 0x01) && !(lastButtons & 0x01)) input.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
		else if (!(buttons & 0x01) && (lastButtons & 0x01)) input.mi.dwFlags |= MOUSEEVENTF_LEFTUP;
		if ((buttons & 0x02) && !(lastButtons & 0x02)) input.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
		else if (!(buttons & 0x02) && (lastButtons & 0x02)) input.mi.dwFlags |= MOUSEEVENTF_RIGHTUP;
		if ((buttons & 0x04) && !(lastButtons & 0x04)) input.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN;
		else if (!(buttons & 0x04) && (lastButtons & 0x04)) input.mi.dwFlags |= MOUSEEVENTF_MIDDLEUP;

		lastButtons = buttons;
		memcpy(reportBuffer, &input, sizeof(INPUT));

		if (debugEnabled) {
			LOG_DEBUG("Relative SendInput: dx=%d dy=%d\n", intMoveX, intMoveY);
		}
	}

	
	
	
	else if (mode == VMulti::ModeDigitizer) {
		reportDigitizer.buttons = buttons | 0x20;
		double digNormX = x / monitorInfo.primaryWidth;
		double digNormY = y / monitorInfo.primaryHeight;
		if (digNormX < 0) digNormX = 0; if (digNormX > 1) digNormX = 1;
		if (digNormY < 0) digNormY = 0; if (digNormY > 1) digNormY = 1;
		reportDigitizer.x = (USHORT)(digNormX * 32767.0 + 0.5);
		reportDigitizer.y = (USHORT)(digNormY * 32767.0 + 0.5);
		reportDigitizer.pressure = (USHORT)(pressure * 2047.0);
		if (reportDigitizer.pressure > 2047) {
			reportDigitizer.pressure = 2047;
		}
		memcpy(reportBuffer, &reportDigitizer, sizeof(reportDigitizer));
		if (debugEnabled) {
			LOG_DEBUGBUFFER(&reportDigitizer, 10, "VMulti Digitizer: ");
		}
	}

	
	
	
	else if (mode == ModeSendInput) {

		if (debugEnabled) {
			LOG_DEBUG("%0.0f,%0.0f | %0.0f,%0.0f | %0.0f,%0.0f\n",
				monitorInfo.primaryWidth, monitorInfo.primaryHeight,
				monitorInfo.virtualWidth, monitorInfo.virtualHeight,
				monitorInfo.virtualX, monitorInfo.virtualY
			);
		}
		INPUT input = { 0 };
		input.type = INPUT_MOUSE;
		input.mi.mouseData = 0;
		double sendNormX = (x - monitorInfo.virtualX) / monitorInfo.virtualWidth;
		double sendNormY = (y - monitorInfo.virtualY) / monitorInfo.virtualHeight;
		if (sendNormX < 0) sendNormX = 0; if (sendNormX > 1) sendNormX = 1;
		if (sendNormY < 0) sendNormY = 0; if (sendNormY > 1) sendNormY = 1;
		input.mi.dx = (LONG)floor(sendNormX * 65535.0);
		input.mi.dy = (LONG)floor(sendNormY * 65535.0);
		input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_MOVE_NOCOALESCE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;

		
		if ((buttons & 0x01) && !(lastButtons & 0x01)) input.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
		else if (!(buttons & 0x01) && (lastButtons & 0x01)) input.mi.dwFlags |= MOUSEEVENTF_LEFTUP;

		
		if ((buttons & 0x02) && !(lastButtons & 0x02)) input.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
		else if (!(buttons & 0x02) && (lastButtons & 0x02)) input.mi.dwFlags |= MOUSEEVENTF_RIGHTUP;

		
		if ((buttons & 0x04) && !(lastButtons & 0x04)) input.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN;
		else if (!(buttons & 0x04) && (lastButtons & 0x04)) input.mi.dwFlags |= MOUSEEVENTF_MIDDLEUP;

		lastButtons = buttons;

		memcpy(reportBuffer, &input, sizeof(INPUT));
	}


}






int VMulti::ResetReport() {
	if (!outputEnabled) return true;

	
	if (mode == ModeAbsoluteMouse) {
		INPUT input = { 0 };
		input.type = INPUT_MOUSE;
		input.mi.dwFlags = MOUSEEVENTF_LEFTUP | MOUSEEVENTF_RIGHTUP | MOUSEEVENTF_MIDDLEUP;
		memcpy(reportBuffer, &input, sizeof(INPUT));
		memcpy(lastReportBuffer, reportBuffer, 65);
		lastButtons = 0;
		return 0;
	}
	else if (mode == ModeRelativeMouse) {
		INPUT input = { 0 };
		input.type = INPUT_MOUSE;
		input.mi.dwFlags = MOUSEEVENTF_LEFTUP | MOUSEEVENTF_RIGHTUP | MOUSEEVENTF_MIDDLEUP;
		memcpy(reportBuffer, &input, sizeof(INPUT));
		memcpy(lastReportBuffer, reportBuffer, 65);
		lastButtons = 0;
		relativeData.accumX = 0;
		relativeData.accumY = 0;
		relativeData.firstReport = true;
		return 0;

		
	}
	else if (mode == ModeDigitizer) {
		reportDigitizer.buttons = 0;
		reportDigitizer.pressure = 0;
		memcpy(reportBuffer, &reportDigitizer, sizeof(reportDigitizer));

		
	}
	else if (mode == ModeSendInput) {
		INPUT input = { 0 };
		input.type = INPUT_MOUSE;
		input.mi.mouseData = 0;
		input.mi.dx = 0;
		input.mi.dy = 0;
		input.mi.dwFlags = MOUSEEVENTF_LEFTUP | MOUSEEVENTF_RIGHTUP | MOUSEEVENTF_MIDDLEUP;
		memcpy(reportBuffer, &input, sizeof(INPUT));
		memcpy(lastReportBuffer, reportBuffer, 65);
		return 0;
	}

	memcpy(lastReportBuffer, reportBuffer, 65);
	if (hidDevice == NULL) return 0;
	return hidDevice->Write(reportBuffer, 65);
}





int VMulti::WriteReport() {
	if (!outputEnabled) return true;

	memcpy(lastReportBuffer, reportBuffer, 65);
	buttonsChanged = false;

	if (mode == ModeSendInput || mode == ModeRelativeMouse || mode == ModeAbsoluteMouse) {
		return SendInput(1, (LPINPUT)reportBuffer, sizeof(INPUT));
	}
	else {
		
		if (hidDevice == NULL) return 0;
		return hidDevice->Write(reportBuffer, 65);
	}
}
