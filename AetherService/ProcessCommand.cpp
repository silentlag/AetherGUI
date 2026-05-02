#include "stdafx.h"
#include "ProcessCommand.h"
#include "AetherPluginManager.h"

#define LOG_MODULE ""
#include "Logger.h"



static std::string LowerService(std::string text) {
	transform(text.begin(), text.end(), text.begin(), ::tolower);
	return text;
}

static bool TryParsePluginIndex(const std::string& value, int* index) {
	if (value.empty())
		return false;

	char* end = NULL;
	long parsed = strtol(value.c_str(), &end, 10);
	if (end == value.c_str() || *end != '\0')
		return false;

	*index = (int)parsed;
	return true;
}

static std::wstring FileNameFromPath(const std::wstring& path) {
	size_t slash = path.find_last_of(L"\\/");
	return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

static std::string PluginRelativeKey(TabletFilterPlugin* plugin) {
	if (plugin == NULL)
		return "";

	std::wstring pluginPath = plugin->path;
	std::wstring root = GetAetherPluginDirectory();
	std::wstring lowerPath = pluginPath;
	std::wstring lowerRoot = root;
	transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::towlower);
	transform(lowerRoot.begin(), lowerRoot.end(), lowerRoot.begin(), ::towlower);

	std::wstring relative = pluginPath;
	if (lowerPath.find(lowerRoot) == 0)
		relative = pluginPath.substr(root.size());
	for (wchar_t& ch : relative) {
		if (ch == L'/')
			ch = L'\\';
	}
	return WideToUtf8Service(relative);
}

static int FindPluginFilterIndex(const std::string& selector) {
	if (tablet == NULL)
		return -1;

	int parsedIndex = -1;
	if (TryParsePluginIndex(selector, &parsedIndex))
		return parsedIndex;

	std::string needle = LowerService(selector);
	for (size_t i = 0; i < tablet->pluginFilters.size(); i++) {
		TabletFilterPlugin* plugin = tablet->pluginFilters[i];
		std::string key = LowerService(PluginRelativeKey(plugin));
		std::string name = LowerService(plugin != NULL ? plugin->name : "");
		std::string dllName = LowerService(WideToUtf8Service(FileNameFromPath(plugin != NULL ? plugin->path : L"")));
		if (needle == key || needle == name || needle == dllName)
			return (int)i;
	}

	return -1;
}



bool ProcessCommand(CommandLine *cmd) {

	LOG_INFO(">> %s\n", cmd->line.c_str());

	
	
	
	if (cmd->is("Tablet")) {

		
		if (cmd->valueCount == 3) {
			string guid = cmd->GetString(0, "");
			int stringId = cmd->GetInt(1, 0);
			string stringMatch = cmd->GetString(2, "");
			if (tablet == NULL) {
				tablet = new Tablet(guid, stringId, stringMatch);
				if (tablet->isOpen) {
					LOG_INFO("Tablet found!\n");
				}
				else {
					LOG_WARNING("Can't open USB tablet '%s' %d '%s'\n", guid.c_str(), stringId, stringMatch.c_str());
					delete tablet;
					tablet = NULL;
				}
			}
		}

		
		else if (cmd->valueCount >= 4 && cmd->valueCount <= 9) {
			USHORT vendorID = cmd->GetInt(0, 0);
			USHORT productID = cmd->GetInt(1, 0);
			USHORT usagePage = cmd->GetInt(2, 0);
			USHORT usage = cmd->GetInt(3, 0);
			int inputReportLength = cmd->GetInt(4, 0);
			int stringId = cmd->GetInt(5, 0);
			string stringMatch = cmd->GetString(6, "");
			int stringId2 = cmd->GetInt(7, 0);
			string stringMatch2 = cmd->GetString(8, "");
			if (tablet == NULL) {
				tablet = new Tablet(vendorID, productID, usagePage, usage, inputReportLength, stringId, stringMatch, stringId2, stringMatch2);
				if (tablet->isOpen) {
					LOG_INFO("Tablet found! HID usage 0x%04X 0x%04X inputLen %d\n",
						tablet->hidDevice->usagePage,
						tablet->hidDevice->usage,
						tablet->hidDevice->inputReportLength);
				}
				else {
					if (inputReportLength > 0) {
						if (stringId > 0 && !stringMatch.empty()) {
							LOG_WARNING("Can't open HID tablet 0x%04X 0x%04X 0x%04X 0x%04X inputLen %d string %d '%s' string2 %d '%s'\n",
								vendorID, productID, usagePage, usage, inputReportLength, stringId, stringMatch.c_str(), stringId2, stringMatch2.c_str());
						}
						else {
							LOG_WARNING("Can't open HID tablet 0x%04X 0x%04X 0x%04X 0x%04X inputLen %d\n",
								vendorID, productID, usagePage, usage, inputReportLength);
						}
					}
					else {
						LOG_WARNING("Can't open HID tablet 0x%04X 0x%04X 0x%04X 0x%04X\n", vendorID, productID, usagePage, usage);
					}
					delete tablet;
					tablet = NULL;
				}
			}
		}
		else {
			if (tablet != NULL) {
				if (tablet->usbDevice != NULL) {
					USBDevice *usb = tablet->usbDevice;
					LOG_INFO("Tablet = USB(\"%s\", %d, \"%s\")\n",
						usb->guid.c_str(),
						usb->stringId,
						usb->stringMatch.c_str()
					);
				}
				else if (tablet->hidDevice != NULL) {
					HIDDevice *hid = tablet->hidDevice;
					LOG_INFO("Tablet = HID(0x%04X 0x%04X 0x%04X 0x%04X)\n",
						hid->vendorId,
						hid->productId,
						hid->usagePage,
						hid->usage
					);
				}
			}
			else {
				LOG_INFO("Tablet is not defined.\n");
			}
		}
	}

	
	else if (cmd->is("CheckTablet")) {
		if (!CheckTablet()) {
			LOG_ERROR("Tablet not found!\n");
			LOG_ERROR("Check the list of supported tablets from the GitHub page.\n");
			LOG_ERROR("Check the AetherGUI repository for supported tablets.\n");
			CleanupAndExit(1);
		}
	}

	
	else if (cmd->is("HIDList")) {
		HANDLE hidHandle = 0;
		HIDDevice *hid = new HIDDevice();
		hid->debugEnabled = true;
		hid->OpenDevice(&hidHandle, 1, 1, 1, 1);
		if (hidHandle != NULL) {
			CloseHandle(hidHandle);
		}
		delete hid;
	}


	
	else if (cmd->is("HID2")) {

		if (cmd->valueCount == 4) {
			USHORT vendorID = cmd->GetInt(0, 0);
			USHORT productID = cmd->GetInt(1, 0);
			USHORT usagePage = cmd->GetInt(2, 0);
			USHORT usage = cmd->GetInt(3, 0);
			if (tablet->hidDevice2 == NULL) {
				tablet->hidDevice2 = new HIDDevice(vendorID, productID, usagePage, usage);
				if (tablet->hidDevice2->isOpen) {
					LOG_INFO("HID Device found!\n");
				}
				else {
					LOG_ERROR("Can't open HID device 0x%04X 0x%04X 0x%04X 0x%04X\n", vendorID, productID, usagePage, usage);
					delete tablet->hidDevice2;
					tablet->hidDevice2 = NULL;
				}
			}
		}
	}

	
	else if (cmd->is("Name")) {
		if (tablet == NULL) return false;
		tablet->name = cmd->GetString(0, tablet->name);
		LOG_INFO("Tablet name = '%s'\n", tablet->name.c_str());
	}

	
	else if (cmd->is("ReportId")) {
		if (tablet == NULL) return false;
		tablet->settings.reportId = cmd->GetInt(0, tablet->settings.reportId);
		LOG_INFO("Tablet report id = %d\n", tablet->settings.reportId);
	}

	
	else if (cmd->is("ReportLength")) {
		if (tablet == NULL) return false;
		tablet->settings.reportLength = cmd->GetInt(0, tablet->settings.reportLength);
		LOG_INFO("Tablet report length = %d\n", tablet->settings.reportLength);
	}

	
	else if (cmd->is("ReportOffset")) {
		if (tablet == NULL) return false;
		tablet->settings.reportOffset = cmd->GetInt(0, tablet->settings.reportOffset);
		if (tablet->settings.reportOffset < 0) tablet->settings.reportOffset = 0;
		LOG_INFO("Tablet report offset = %d\n", tablet->settings.reportOffset);
	}

	
	else if (cmd->is("DetectMask")) {
		if (tablet == NULL) return false;
		tablet->settings.detectMask = cmd->GetInt(0, tablet->settings.detectMask);
		LOG_INFO("Tablet detect mask = %02X\n", tablet->settings.detectMask);
	}

	
	else if (cmd->is("IgnoreMask")) {
		if (tablet == NULL) return false;
		tablet->settings.ignoreMask = cmd->GetInt(0, tablet->settings.ignoreMask);
		LOG_INFO("Tablet ignore mask = %02X\n", tablet->settings.ignoreMask);
	}

	
	else if (cmd->is("MaxX")) {
		if (tablet == NULL) return false;
		tablet->settings.maxX = cmd->GetInt(0, tablet->settings.maxX);
		tablet->settings.invMaxX = 1.0 / (double)tablet->settings.maxX;
		LOG_INFO("Tablet max X = %d\n", tablet->settings.maxX);
	}

	
	else if (cmd->is("MaxY")) {
		if (tablet == NULL) return false;
		tablet->settings.maxY = cmd->GetInt(0, tablet->settings.maxY);
		tablet->settings.invMaxY = 1.0 / (double)tablet->settings.maxY;
		LOG_INFO("Tablet max Y = %d\n", tablet->settings.maxY);
	}

	
	else if (cmd->is("MaxPressure")) {
		if (tablet == NULL) return false;
		tablet->settings.maxPressure = cmd->GetInt(0, tablet->settings.maxPressure);
		LOG_INFO("Tablet max pressure = %d\n", tablet->settings.maxPressure);
	}

	
	else if (cmd->is("ClickPressure")) {
		if (tablet == NULL) return false;
		tablet->settings.clickPressure = cmd->GetInt(0, tablet->settings.clickPressure);
		LOG_INFO("Tablet click pressure = %d\n", tablet->settings.clickPressure);
	}

	
	else if (cmd->is("TipThreshold") || cmd->is("TipActivationThreshold")) {
		if (tablet == NULL) return false;
		double percent = cmd->GetDouble(0, 0.0);
		if (percent < 0.0) percent = 0.0;
		if (percent > 100.0) percent = 100.0;

		int maxPressure = tablet->settings.maxPressure;
		if (maxPressure < 1) maxPressure = 1;

		if (percent <= 0.0) {
			tablet->settings.clickPressure = 0;
		}
		else {
			tablet->settings.clickPressure = (int)round(maxPressure * percent / 100.0);
			if (tablet->settings.clickPressure < 1) tablet->settings.clickPressure = 1;
		}

		LOG_INFO("Tablet tip threshold = %0.2f%% (%d pressure)\n",
			percent, tablet->settings.clickPressure);
	}


	
	else if (cmd->is("KeepTipDown")) {
		if (tablet == NULL) return false;
		tablet->settings.keepTipDown = cmd->GetInt(0, tablet->settings.keepTipDown);
		LOG_INFO("Tablet pen tip keep down = %d packets\n", tablet->settings.keepTipDown);
	}


	
	else if (cmd->is("Width")) {
		if (tablet == NULL) return false;
		tablet->settings.width = cmd->GetDouble(0, tablet->settings.width);
		LOG_INFO("Tablet width = %0.2f mm\n", tablet->settings.width);
	}

	
	else if (cmd->is("Height")) {
		if (tablet == NULL) return false;
		tablet->settings.height = cmd->GetDouble(0, tablet->settings.height);
		LOG_INFO("Tablet height = %0.2f mm\n", tablet->settings.height);
	}

	
	else if (cmd->is("Skew")) {
		if (tablet == NULL) return false;
		tablet->settings.skew = cmd->GetDouble(0, tablet->settings.skew);
		LOG_INFO("Tablet skew = Shift X-axis %0.2f mm per Y-axis mm\n", tablet->settings.skew);
	}

	
	else if (cmd->is("Type")) {
		if (tablet == NULL) return false;

		
		if (cmd->GetStringLower(0, "") == "wacomintuos") {
			tablet->settings.type = TabletSettings::TypeWacomIntuos;
		}

		
		else if (cmd->GetStringLower(0, "") == "wacom4100") {
			tablet->settings.type = TabletSettings::TypeWacom4100;
		}

		
		else if (cmd->GetStringLower(0, "") == "wacombamboo") {
			tablet->settings.type = TabletSettings::TypeWacomBamboo;
		}

		
		else if (cmd->GetStringLower(0, "") == "wacomintuos4") {
			tablet->settings.type = TabletSettings::TypeWacomIntuos4;
		}

		
		else if (cmd->GetStringLower(0, "") == "wacomintuosv2") {
			tablet->settings.type = TabletSettings::TypeWacomIntuosV2;
		}

		
		else if (cmd->GetStringLower(0, "") == "wacomintuosv3") {
			tablet->settings.type = TabletSettings::TypeWacomIntuosV3;
		}

		
		else if (cmd->GetStringLower(0, "") == "uclogic") {
			tablet->settings.type = TabletSettings::TypeUCLogic;
		}

		
		else if (cmd->GetStringLower(0, "") == "uclogicv1") {
			tablet->settings.type = TabletSettings::TypeUCLogicV1;
		}

		
		else if (cmd->GetStringLower(0, "") == "uclogicv2") {
			tablet->settings.type = TabletSettings::TypeUCLogicV2;
		}

		
		else if (cmd->GetStringLower(0, "") == "inspiroy") {
			tablet->settings.type = TabletSettings::TypeInspiroy;
		}

		
		else if (cmd->GetStringLower(0, "") == "giano" || cmd->GetStringLower(0, "") == "huiongiano") {
			tablet->settings.type = TabletSettings::TypeGiano;
		}

		
		else if (cmd->GetStringLower(0, "") == "xppen") {
			tablet->settings.type = TabletSettings::TypeXPPen;
		}

		
		else if (cmd->GetStringLower(0, "") == "xppenoffsetpressure") {
			tablet->settings.type = TabletSettings::TypeXPPenOffsetPressure;
		}

		
		else if (cmd->GetStringLower(0, "") == "xppengen2") {
			tablet->settings.type = TabletSettings::TypeXPPenGen2;
		}

		
		else if (cmd->GetStringLower(0, "") == "xppenoffsetaux") {
			tablet->settings.type = TabletSettings::TypeXPPenOffsetAux;
		}

		
		else if (cmd->GetStringLower(0, "") == "wacomdrivers") {
			tablet->settings.type = TabletSettings::TypeWacomDrivers;
		}

		
		else if (cmd->GetStringLower(0, "") == "acepen") {
			tablet->settings.type = TabletSettings::TypeAcepen;
		}

		
		else if (cmd->GetStringLower(0, "") == "bosto") {
			tablet->settings.type = TabletSettings::TypeBosto;
		}

		
		else if (cmd->GetStringLower(0, "") == "floogoo" || cmd->GetStringLower(0, "") == "fma") {
			tablet->settings.type = TabletSettings::TypeFlooGoo;
		}

		
		else if (cmd->GetStringLower(0, "") == "genius") {
			tablet->settings.type = TabletSettings::TypeGenius;
		}

		
		else if (cmd->GetStringLower(0, "") == "geniusv2") {
			tablet->settings.type = TabletSettings::TypeGeniusV2;
		}

		
		else if (cmd->GetStringLower(0, "") == "lifetec") {
			tablet->settings.type = TabletSettings::TypeLifetec;
		}

		
		else if (cmd->GetStringLower(0, "") == "robotpen") {
			tablet->settings.type = TabletSettings::TypeRobotPen;
		}

		
		else if (cmd->GetStringLower(0, "") == "veikk") {
			tablet->settings.type = TabletSettings::TypeVeikk;
		}

		
		else if (cmd->GetStringLower(0, "") == "veikka15") {
			tablet->settings.type = TabletSettings::TypeVeikkA15;
		}

		
		else if (cmd->GetStringLower(0, "") == "veikkv1") {
			tablet->settings.type = TabletSettings::TypeVeikkV1;
		}

		
		else if (cmd->GetStringLower(0, "") == "veikktilt") {
			tablet->settings.type = TabletSettings::TypeVeikkTilt;
		}

		
		else if (cmd->GetStringLower(0, "") == "woodpad" || cmd->GetStringLower(0, "") == "viewsonicwoodpad") {
			tablet->settings.type = TabletSettings::TypeWoodPad;
		}

		
		else if (cmd->GetStringLower(0, "") == "xencelabs") {
			tablet->settings.type = TabletSettings::TypeXenceLabs;
		}

		
		else if (cmd->GetStringLower(0, "") == "xenx") {
			tablet->settings.type = TabletSettings::TypeXENX;
		}

		
		else if (cmd->GetStringLower(0, "") == "wacomgraphire") {
			tablet->settings.type = TabletSettings::TypeWacomGraphire;
		}

		
		else if (cmd->GetStringLower(0, "") == "wacombamboopad") {
			tablet->settings.type = TabletSettings::TypeWacomBambooPad;
		}

		
		else if (cmd->GetStringLower(0, "") == "wacomcintiqv1") {
			tablet->settings.type = TabletSettings::TypeWacomCintiqV1;
		}

		
		else if (cmd->GetStringLower(0, "") == "wacompl") {
			tablet->settings.type = TabletSettings::TypeWacomPL;
		}

		
		else if (cmd->GetStringLower(0, "") == "wacomptu") {
			tablet->settings.type = TabletSettings::TypeWacomPTU;
		}

		LOG_INFO("Tablet type = %d\n", tablet->settings.type);
	}


	
	else if (cmd->is("InitFeature") && cmd->valueCount > 0) {
		if (tablet == NULL) return false;
		vector<BYTE> report;
		report.resize(cmd->valueCount);
		for (int i = 0; i < (int)cmd->valueCount; i++) {
			report[i] = cmd->GetInt(i, 0);
		}
		tablet->initFeatureReports.push_back(report);
		LOG_INFOBUFFER(report.data(), (int)report.size(), "Tablet init feature report: ");
	}

	
	else if ((cmd->is("InitString") || cmd->is("InitializationString")) && cmd->valueCount > 0) {
		if (tablet == NULL) return false;
		for (int i = 0; i < (int)cmd->valueCount && tablet->initStringCount < 8; i++) {
			tablet->initStringIds[tablet->initStringCount++] = cmd->GetInt(i, 0);
		}
		LOG_INFO("Tablet init string count = %d\n", tablet->initStringCount);
	}

	
	else if (cmd->is("InitReport") && cmd->valueCount > 0) {
		if (tablet == NULL) return false;
		vector<BYTE> report;
		report.resize(cmd->valueCount);
		for (int i = 0; i < (int)cmd->valueCount; i++) {
			report[i] = cmd->GetInt(i, 0);
		}
		tablet->initOutputReports.push_back(report);
		LOG_INFOBUFFER(report.data(), (int)report.size(), "Tablet init report: ");

	}


	
	
	
	else if ((cmd->is("SetFeature") || cmd->is("Feature")) && cmd->valueCount > 1) {
		if (tablet == NULL) return false;
		if (tablet->hidDevice == NULL) return false;
		int length = cmd->GetInt(0, 1);
		BYTE *buffer = new BYTE[length];
		for (int i = 0; i < length; i++) {
			buffer[i] = cmd->GetInt(i + 1, 0);
		}
		LOG_INFOBUFFER(buffer, length, "Set Feature Report (%d): ", length);
		tablet->hidDevice->SetFeature(buffer, length);
		LOG_INFO("HID Feature set!\n");
		delete[] buffer;
	}

	
	
	
	else if (cmd->is("GetFeature") && cmd->valueCount > 1) {
		if (tablet == NULL) return false;
		if (tablet->hidDevice == NULL) return false;
		int length = cmd->GetInt(0, 1);
		BYTE *buffer = new BYTE[length];
		for (int i = 0; i < length; i++) {
			buffer[i] = cmd->GetInt(i + 1, 0);
		}
		LOG_INFOBUFFER(buffer, length, "Get Feature Report (%d): ", length);
		tablet->hidDevice->GetFeature(buffer, length);
		LOG_INFOBUFFER(buffer, length, "Result Feature Report (%d): ", length);
		delete[] buffer;
	}


	
	
	
	else if ((cmd->is("OutputReport") || cmd->is("Report")) && cmd->valueCount > 1) {
		if (tablet == NULL) return false;
		if (tablet->hidDevice == NULL) return false;
		int length = cmd->GetInt(0, 1);
		BYTE *buffer = new BYTE[length];
		for (int i = 0; i < length; i++) {
			buffer[i] = cmd->GetInt(i + 1, 0);
		}
		LOG_INFOBUFFER(buffer, length, "Sending HID Report: ");
		tablet->hidDevice->Write(buffer, length);
		LOG_INFO("Report sent!\n");
		delete[] buffer;
	}

	
	
	
	else if (cmd->is("TabletArea") || cmd->is("Area")) {
		if (!CheckTablet()) return true;
		mapper->areaTablet.width = cmd->GetDouble(0, mapper->areaTablet.width);
		mapper->areaTablet.height = cmd->GetDouble(1, mapper->areaTablet.height);
		mapper->areaTablet.x = cmd->GetDouble(2, mapper->areaTablet.x);
		mapper->areaTablet.y = cmd->GetDouble(3, mapper->areaTablet.y);

		LogTabletArea("Tablet area");
	}

	
	else if (cmd->is("ButtonMap") || cmd->is("Buttons")) {
		if (!CheckTablet()) return true;
		char buttonMapBuffer[32];
		int index = 0;
		for (int i = 0; i < 8; i++) {
			tablet->buttonMap[i] = cmd->GetInt(i, tablet->buttonMap[i]);
			index += sprintf_s(buttonMapBuffer + index, 32 - index, "%d ", tablet->buttonMap[i]);
		}
		LOG_INFO("Button Map = %s\n", buttonMapBuffer);
	}

	
	else if (cmd->is("MouseWheelSpeed")) {
		if (!CheckTablet()) return true;
		int MouseWheelSpeed = (int)cmd->GetInt(0, tablet->settings.mouseWheelSpeed);
		tablet->settings.mouseWheelSpeed = MouseWheelSpeed;
		LOG_INFO("Mouse Wheel Speed = %d\n", MouseWheelSpeed);
	}

	
	else if (cmd->is("Overclock") || cmd->is("TabletOverclock")) {
		if (!CheckTablet()) return true;

		bool enabled = cmd->GetBoolean(0, true);
		int targetHz = cmd->GetInt(1, 1000);
		if (targetHz < 125) targetHz = 125;
		if (targetHz > 2000) targetHz = 2000;

		if (enabled) {
			timeBeginPeriod(1);
			SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
			SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
			if (tabletThread != NULL) {
				SetThreadPriority(tabletThread->native_handle(), THREAD_PRIORITY_HIGHEST);
			}

			double interval = 1000.0 / (double)targetHz;
			if (interval < 0.5) interval = 0.5;

			
			
			tablet->smoothing.timerInterval = interval;
			tablet->smoothing.SetLatency(tablet->smoothing.latency);

			
			for (int fi = 0; fi < tablet->filterTimedCount; fi++) {
				tablet->filterTimed[fi]->timerInterval = interval;
			}

			overclockActive = true;
			overclockTargetHz = (double)targetHz;

			
			
			tablet->smoothing.StopTimer();
			if (tablet->filterTimedCount > 0 && tablet->filterTimed[0]->callback != NULL) {
				StartOverclockTimer((double)targetHz);
			}

			LOG_INFO("Overclock = on (%d Hz target, %0.3f ms timer interval)\n", targetHz, interval);
			LOG_INFO("Overclock: high-resolution timer sends reports at the target output rate.\n");
		}
		else {
			overclockActive = false;
			StopOverclockTimer();
			if (tablet->smoothing.timerInterval < 1.0) {
				tablet->smoothing.timerInterval = 1.0;
				tablet->smoothing.SetLatency(tablet->smoothing.latency);
			}
			if (tablet->filterTimedCount > 0 && tablet->filterTimed[0]->callback != NULL) {
				tablet->smoothing.StartTimer();
			}
			RefreshTimedOutputTimer();
			timeEndPeriod(1);
			SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
			SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
			if (tabletThread != NULL) {
				SetThreadPriority(tabletThread->native_handle(), THREAD_PRIORITY_NORMAL);
			}
			LOG_INFO("Overclock = off\n");
		}
	}

	
	else if (cmd->is("PenRateLimit") || cmd->is("PenRate") || cmd->is("ReportRateLimit")) {
		string first = cmd->GetStringLower(0, "");
		bool hasExplicitState = (first == "on" || first == "off" || first == "true" || first == "false" || first == "0" || first == "1");
		bool enabled = hasExplicitState ? cmd->GetBoolean(0, penRateLimitActive) : true;
		double targetHz = hasExplicitState ? cmd->GetDouble(1, penRateLimitHz) : cmd->GetDouble(0, penRateLimitHz);

		if (targetHz < 30.0) targetHz = 30.0;
		if (targetHz > 1000.0) targetHz = 1000.0;

		penRateLimitActive = enabled;
		penRateLimitHz = targetHz;
		ResetPenRateLimiter();
		RefreshTimedOutputTimer();

		if (penRateLimitActive) {
			LOG_INFO("Pen rate limit = on (%0.0f Hz independent output target)\n", penRateLimitHz);
		}
		else {
			LOG_INFO("Pen rate limit = off\n");
		}
	}


	
	else if (cmd->is("ScreenArea") || cmd->is("Screen")) {
		if (!CheckTablet()) return true;
		mapper->areaScreen.width = cmd->GetDouble(0, mapper->areaScreen.width);
		mapper->areaScreen.height = cmd->GetDouble(1, mapper->areaScreen.height);
		mapper->areaScreen.x = cmd->GetDouble(2, mapper->areaScreen.x);
		mapper->areaScreen.y = cmd->GetDouble(3, mapper->areaScreen.y);

		double requestedW = mapper->areaScreen.width;
		double requestedH = mapper->areaScreen.height;
		double requestedX = mapper->areaScreen.x;
		double requestedY = mapper->areaScreen.y;
		mapper->ClampScreenAreaToVirtualScreen();

		LOG_INFO("Screen area = (w=%0.2f, h=%0.2f, x=%0.2f, y=%0.2f)\n",
			mapper->areaScreen.width,
			mapper->areaScreen.height,
			mapper->areaScreen.x,
			mapper->areaScreen.y
		);
		if (requestedW != mapper->areaScreen.width || requestedH != mapper->areaScreen.height ||
			requestedX != mapper->areaScreen.x || requestedY != mapper->areaScreen.y) {
			LOG_INFO("Screen area was adjusted to fit current desktop bounds (%0.0fx%0.0f at %+0.0f,%+0.0f).\n",
				mapper->areaVirtualScreen.width, mapper->areaVirtualScreen.height,
				mapper->areaVirtualScreen.x, mapper->areaVirtualScreen.y);
		}
	}

	
	else if (cmd->is("AreaClipping") || cmd->is("Clipping")) {
		if (!CheckTablet()) return true;
		mapper->areaClipping = cmd->GetBoolean(0, mapper->areaClipping);
		LOG_INFO("Area clipping = %s\n", mapper->areaClipping ? "on" : "off");
	}

	
	else if (cmd->is("AreaLimiting") || cmd->is("Limiting")) {
		if (!CheckTablet()) return true;
		mapper->areaLimiting = cmd->GetBoolean(0, mapper->areaLimiting);
		if (mapper->areaLimiting) mapper->areaClipping = true; 
		LOG_INFO("Area limiting = %s\n", mapper->areaLimiting ? "on" : "off");
	}

	
	else if (cmd->is("DesktopSize") || cmd->is("Desktop")) {
		if (!CheckTablet()) return true;
		mapper->areaVirtualScreen.width = cmd->GetDouble(0, mapper->areaVirtualScreen.width);
		mapper->areaVirtualScreen.height = cmd->GetDouble(1, mapper->areaVirtualScreen.height);
		mapper->areaVirtualScreen.x = 0;
		mapper->areaVirtualScreen.y = 0;
		mapper->ClampScreenAreaToVirtualScreen(false);
		LOG_INFO("Desktop size = (%0.2f px x %0.2f px)\n",
			mapper->areaVirtualScreen.width,
			mapper->areaVirtualScreen.height
		);
	}

	else if (cmd->is("StaticMonitorInfo") || cmd->is("MonitorInfo")) {
		if (vmulti != NULL) {
			double primaryWidth = cmd->GetDouble(0, vmulti->monitorInfo.primaryWidth);
			double primaryHeight = cmd->GetDouble(1, vmulti->monitorInfo.primaryHeight);
			double virtualWidth = cmd->GetDouble(2, primaryWidth);
			double virtualHeight = cmd->GetDouble(3, primaryHeight);
			double virtualX = cmd->GetDouble(4, 0);
			double virtualY = cmd->GetDouble(5, 0);
			vmulti->SetMonitorInfo(primaryWidth, primaryHeight, virtualWidth, virtualHeight, virtualX, virtualY);
			if (mapper != NULL) {
				mapper->areaVirtualScreen.width = virtualWidth;
				mapper->areaVirtualScreen.height = virtualHeight;
				mapper->areaVirtualScreen.x = virtualX;
				mapper->areaVirtualScreen.y = virtualY;
				mapper->ClampScreenAreaToVirtualScreen(false);
			}
			LOG_INFO("Static monitor info = primary=%0.0fx%0.0f virtual=%0.0fx%0.0f offset=%+0.0f,%+0.0f\n",
				vmulti->monitorInfo.primaryWidth, vmulti->monitorInfo.primaryHeight,
				vmulti->monitorInfo.virtualWidth, vmulti->monitorInfo.virtualHeight,
				vmulti->monitorInfo.virtualX, vmulti->monitorInfo.virtualY);
		}
	}

	
	else if (cmd->is("TabletMove") || cmd->is("Move") && cmd->valueCount > 0) {
		if (!CheckTablet()) return true;
		string border;
		double offset;
		bool moved = true;

		for (int i = 0; i < cmd->valueCount; i += 2) {

			border = cmd->GetStringLower(i, "");
			offset = cmd->GetDouble(i + 1, 0.0);
			moved = true;

			if (border == "top") {
				mapper->areaTablet.y = offset;
			}
			else if (border == "bottom") {
				mapper->areaTablet.y = tablet->settings.height - mapper->areaTablet.height - offset;
			}
			else if (border == "left") {
				mapper->areaTablet.x = offset;
			}
			else if (border == "right") {
				mapper->areaTablet.x = tablet->settings.width - mapper->areaTablet.width - offset;
			}
			else {
				moved = false;
			}
			if (moved) {
				LOG_INFO("Tablet area moved to %s border with %0.2f mm margin.\n", border.c_str(), offset);
				LogTabletArea("  New tablet area");
			}
		}
	}

	
	
	
	else if (cmd->is("Rotate")) {
		if (!CheckTablet()) return true;
		double value = cmd->GetDouble(0, 0);
		mapper->SetRotation(value);
		LOG_INFO("Rotation matrix = [%f,%f,%f,%f]\n",
			mapper->rotationMatrix[0],
			mapper->rotationMatrix[1],
			mapper->rotationMatrix[2],
			mapper->rotationMatrix[3]
		);
	}


	
	
	
	else if (cmd->is("Sensitivity")) {
		if (!CheckTablet()) return true;
		vmulti->relativeData.sensitivity = cmd->GetDouble(0, vmulti->relativeData.sensitivity);
		LOG_INFO("Relative mode sensitivity = %0.2f px/mm\n", vmulti->relativeData.sensitivity);
	}

	
	
	
	else if (cmd->is("ResetDistance")) {
		if (!CheckTablet()) return true;
		vmulti->relativeData.resetDistance = cmd->GetDouble(0, vmulti->relativeData.resetDistance);
		LOG_INFO("Relative mode reset distance = %0.2f mm\n", vmulti->relativeData.resetDistance);
	}


	
	
	
	else if (cmd->is("Mode")) {
		string mode = cmd->GetStringLower(0, "");

		
		if (mode.compare(0, 3, "abs") == 0) {
			if (vmulti->mode != VMulti::ModeAbsoluteMouse)
				vmulti->ResetReport();
			vmulti->mode = VMulti::ModeAbsoluteMouse;
			LOG_INFO("Output Mode = Absolute\n");
		}

		
		else if (mode.compare(0, 3, "rel") == 0) {
			if (vmulti->mode != VMulti::ModeRelativeMouse)
				vmulti->ResetReport();
			vmulti->mode = VMulti::ModeRelativeMouse;
			LOG_INFO("Output Mode = Relative\n");
		}

		
		else if (mode.compare(0, 3, "dig") == 0 || mode.compare(0, 3, "pen") == 0) {
			if (vmulti->mode != VMulti::ModeDigitizer)
				vmulti->ResetReport();
			vmulti->mode = VMulti::ModeDigitizer;
			LOG_INFO("Output Mode = Digitizer\n");
		}

		
		else if (mode.compare(0, 4, "send") == 0) {
			if (vmulti->mode != VMulti::ModeSendInput)
				vmulti->ResetReport();
			vmulti->mode = VMulti::ModeSendInput;
			vmulti->UpdateMonitorInfo();
			LOG_INFO("Output Mode = SendInput\n");
		}

		else {
			LOG_ERROR("Unknown output mode '%s'\n", mode.c_str());
		}

	}


	
	
	
	else if (cmd->is("Smoothing")) {
		if (!CheckTablet()) return true;
		double latency = cmd->GetDouble(0, tablet->smoothing.GetLatency());
		double threshold = cmd->GetDouble(1, tablet->smoothing.threshold);

		string stringValue = cmd->GetStringLower(0, "");

		
		if (stringValue == "off" || stringValue == "false") {
			latency = 0;
		}

		
		if (latency < 0) latency = 1;
		if (latency > 1000) latency = 1000;
		if (threshold < 0.001) threshold = 0.001;
		if (threshold > 0.999) threshold = 0.999;

		
		tablet->smoothing.threshold = threshold;

		
		tablet->smoothing.SetLatency(latency);

		
		if (tablet->smoothing.weight < 1.0) {
			tablet->smoothing.isEnabled = true;
			LOG_INFO("Smoothing = %0.2f ms to reach %0.0f%% (weight = %f)\n", latency, tablet->smoothing.threshold * 100, tablet->smoothing.weight);
		}
		else {
			tablet->smoothing.isEnabled = false;
			LOG_INFO("Smoothing = off\n");
		}
		RefreshTimedOutputTimer();
	}


	
	
	
	else if (cmd->is("AntichatterEnabled")) {
		bool AntichatterEnabled = (bool)cmd->GetInt(0, tablet->smoothing.AntichatterEnabled);
		tablet->smoothing.AntichatterEnabled = AntichatterEnabled;
		LOG_INFO("Filter Antichatter Enabled = %d \n", tablet->smoothing.AntichatterEnabled);
		RefreshTimedOutputTimer();
	}
	else if (cmd->is("AntichatterStrength")) {
		double antichatterStrength = cmd->GetDouble(0, tablet->smoothing.antichatterStrength);
		tablet->smoothing.antichatterStrength = antichatterStrength;
		LOG_INFO("Filter Antichatter Stregth = %0.2f \n", tablet->smoothing.antichatterStrength);
	}
	else if (cmd->is("AntichatterMultiplier")) {
		double antichatterMultiplier = cmd->GetDouble(0, tablet->smoothing.antichatterMultiplier);
		tablet->smoothing.antichatterMultiplier = antichatterMultiplier;
		LOG_INFO("Filter Antichatter Multiplier = %0.2f \n", tablet->smoothing.antichatterMultiplier);
	}
	else if (cmd->is("AntichatterOffsetX")) {
		double antichatterOffsetX = cmd->GetDouble(0, tablet->smoothing.antichatterOffsetX);
		tablet->smoothing.antichatterOffsetX = antichatterOffsetX;
		LOG_INFO("Filter Antichatter Offset X = %0.2f cm\n", tablet->smoothing.antichatterOffsetX);
	}
	else if (cmd->is("AntichatterOffsetY")) {
		double antichatterOffsetY = cmd->GetDouble(0, tablet->smoothing.antichatterOffsetY);
		tablet->smoothing.antichatterOffsetY = antichatterOffsetY;
		LOG_INFO("Filter Antichatter Offset Y = %0.2f cm\n", tablet->smoothing.antichatterOffsetY);
	}

	
	
	
	else if (cmd->is("PredictionEnabled")) {
		bool PredictionEnabled = (bool)cmd->GetInt(0, tablet->smoothing.PredictionEnabled);
		tablet->smoothing.PredictionEnabled = PredictionEnabled;
		LOG_INFO("Filter Prediction Enabled = %d \n", tablet->smoothing.PredictionEnabled);
	}
	else if (cmd->is("PredictionSharpness")) {
		double PredictionSharpness = cmd->GetDouble(0, tablet->smoothing.PredictionSharpness);
		tablet->smoothing.PredictionSharpness = PredictionSharpness;
		LOG_INFO("Filter Prediction Sharpness = %0.2f \n", tablet->smoothing.PredictionSharpness);
	}
	else if (cmd->is("PredictionStrength")) {
		double PredictionStrength = cmd->GetDouble(0, tablet->smoothing.PredictionStrength);
		tablet->smoothing.PredictionStrength = PredictionStrength;
		LOG_INFO("Filter Prediction Strength = %0.2f x\n", tablet->smoothing.PredictionStrength);
	}
	else if (cmd->is("PredictionOffsetX")) {
		double PredictionOffsetX = cmd->GetDouble(0, tablet->smoothing.PredictionOffsetX);
		tablet->smoothing.PredictionOffsetX = PredictionOffsetX;
		LOG_INFO("Filter Prediction Offset X = %0.2f cm\n", tablet->smoothing.PredictionOffsetX);
	}
	else if (cmd->is("PredictionOffsetY")) {
		double PredictionOffsetY = cmd->GetDouble(0, tablet->smoothing.PredictionOffsetY);
		tablet->smoothing.PredictionOffsetY = PredictionOffsetY;
		LOG_INFO("Filter Prediction Offset Y = %0.2f cm\n", tablet->smoothing.PredictionOffsetY);
	}



	
	
	
	else if (cmd->is("SmoothingInterval")) {
		int interval = cmd->GetInt(0, (int)round(tablet->smoothing.timerInterval));

		
		if (interval > 100) interval = 100;

		
		if (interval < 1) interval = 1;

		if (overclockActive) {
			LOG_INFO("Smoothing Interval = %d ignored while Overclock is active (timer stays %d ms)\n",
				interval, (int)round(tablet->smoothing.timerInterval));
			return true;
		}

		
		if (interval != (int)round(tablet->smoothing.timerInterval)) {
			tablet->smoothing.timerInterval = interval;
			tablet->smoothing.SetLatency(tablet->smoothing.latency);
			RefreshTimedOutputTimer();
		}

		LOG_INFO("Smoothing Interval = %d (%0.2f Hz, %0.2f ms, %f)\n", interval, 1000.0 / interval, tablet->smoothing.latency, tablet->smoothing.weight);

	}


	
	
	
	else if (cmd->is("Noise")) {

		string stringValue = cmd->GetStringLower(0, "");

		
		if (stringValue == "off" || stringValue == "false") {
			tablet->noise.isEnabled = false;
			LOG_INFO("Noise Reduction = off\n");

			
		}
		else {

			int length = cmd->GetInt(0, tablet->noise.buffer.length);
			double distanceThreshold = cmd->GetDouble(1, tablet->noise.distanceThreshold);
			int iterations = cmd->GetInt(2, tablet->noise.iterations);

			
			if (length < 0) length = 0;
			else if (length > 50) length = 50;

			if (distanceThreshold < 0) distanceThreshold = 0;
			else if (distanceThreshold > 100) distanceThreshold = 100;

			if (iterations < 1) iterations = 1;
			else if (iterations > 100) iterations = 100;

			
			tablet->noise.buffer.SetLength(length);
			tablet->noise.distanceThreshold = distanceThreshold;
			tablet->noise.iterations = iterations;

			
			if (tablet->noise.buffer.length > 0) {
				tablet->noise.isEnabled = true;
				LOG_INFO("Noise Reduction = %d packets, %0.3f mm threshold, %d iterations\n", length, distanceThreshold, iterations);
			}
			else {
				tablet->noise.isEnabled = false;
				LOG_INFO("Noise Reduction = off\n");
			}

		}




	}

	
	
	
	else if (cmd->is("Reconstructor") || cmd->is("Recon")) {
		if (!CheckTablet()) return true;

		string stringValue = cmd->GetStringLower(0, "");

		
		if (stringValue == "off" || stringValue == "false") {
			tablet->reconstructor.isEnabled = false;
			LOG_INFO("Reconstructor = off\n");
		}
		else {
			double strength = cmd->GetDouble(0, tablet->reconstructor.reconstructionStrength);
			double velSmooth = cmd->GetDouble(1, tablet->reconstructor.velocitySmoothing);
			double accelCap = cmd->GetDouble(2, tablet->reconstructor.accelerationCap);
			double predTime = cmd->GetDouble(3, tablet->reconstructor.predictionTimeMs);

			
			if (strength < 0) strength = 0;
			else if (strength > 2.0) strength = 2.0;

			if (velSmooth < 0) velSmooth = 0;
			else if (velSmooth > 0.99) velSmooth = 0.99;

			if (accelCap < 1) accelCap = 1;
			else if (accelCap > 200) accelCap = 200;

			if (predTime < 0.1) predTime = 0.1;
			else if (predTime > 50) predTime = 50;

			
			tablet->reconstructor.reconstructionStrength = strength;
			tablet->reconstructor.velocitySmoothing = velSmooth;
			tablet->reconstructor.accelerationCap = accelCap;
			tablet->reconstructor.predictionTimeMs = predTime;

			if (strength > 0.001) {
				tablet->reconstructor.isEnabled = true;
				LOG_INFO("Reconstructor = strength %0.2f, velSmooth %0.2f, accelCap %0.1f, predTime %0.1f ms\n",
					strength, velSmooth, accelCap, predTime);
			}
			else {
				tablet->reconstructor.isEnabled = false;
				LOG_INFO("Reconstructor = off\n");
			}
		}
	}

	
	
	
	else if (cmd->is("Adaptive")) {
		if (!CheckTablet()) return true;

		string stringValue = cmd->GetStringLower(0, "");

		
		if (stringValue == "off" || stringValue == "false") {
			tablet->adaptive.isEnabled = false;
			LOG_INFO("Adaptive = off\n");
		}
		else {
			double procNoise = cmd->GetDouble(0, tablet->adaptive.processNoise);
			double measNoise = cmd->GetDouble(1, tablet->adaptive.measurementNoise);
			double velWeight = cmd->GetDouble(2, tablet->adaptive.velocityWeight);

			
			if (procNoise < 0.001) procNoise = 0.001;
			else if (procNoise > 10.0) procNoise = 10.0;

			if (measNoise < 0.001) measNoise = 0.001;
			else if (measNoise > 50.0) measNoise = 50.0;

			if (velWeight < 0) velWeight = 0;
			else if (velWeight > 5.0) velWeight = 5.0;

			
			tablet->adaptive.processNoise = procNoise;
			tablet->adaptive.measurementNoise = measNoise;
			tablet->adaptive.velocityWeight = velWeight;

			tablet->adaptive.isEnabled = true;
			LOG_INFO("Adaptive = processNoise %0.4f, measurementNoise %0.4f, velocityWeight %0.2f\n",
				procNoise, measNoise, velWeight);
		}
	}


	
	
	
	else if (cmd->is("AetherSmooth") || cmd->is("Aether")) {
		if (!CheckTablet()) return true;

		string stringValue = cmd->GetStringLower(0, "");

		
		if (stringValue == "off" || stringValue == "false") {
			tablet->aetherSmooth.isEnabled = false;
			LOG_INFO("Aether Smooth = off\n");
		}
		else {
			tablet->aetherSmooth.isEnabled = true;
			LOG_INFO("Aether Smooth = on\n");
		}
	}

	
	else if (cmd->is("AetherLagRemoval") || cmd->is("AS_LagRemoval")) {
		if (!CheckTablet()) return true;
		tablet->aetherSmooth.enableAntismoothing = cmd->GetBoolean(0, tablet->aetherSmooth.enableAntismoothing);
		tablet->aetherSmooth.antismoothing = cmd->GetDouble(1, tablet->aetherSmooth.antismoothing);
		LOG_INFO("Aether Lag Removal = %s, strength = %0.3f\n",
			tablet->aetherSmooth.enableAntismoothing ? "on" : "off",
			tablet->aetherSmooth.antismoothing);
	}

	
	else if (cmd->is("AetherStabilizer") || cmd->is("AS_Stabilizer")) {
		if (!CheckTablet()) return true;
		tablet->aetherSmooth.enableSmoothing = cmd->GetBoolean(0, tablet->aetherSmooth.enableSmoothing);
		tablet->aetherSmooth.stability = cmd->GetDouble(1, tablet->aetherSmooth.stability);
		tablet->aetherSmooth.speedSensitivity = cmd->GetDouble(2, tablet->aetherSmooth.speedSensitivity);
		LOG_INFO("Aether Stabilizer = %s, stability = %0.3f, sensitivity = %0.4f\n",
			tablet->aetherSmooth.enableSmoothing ? "on" : "off",
			tablet->aetherSmooth.stability,
			tablet->aetherSmooth.speedSensitivity);
	}

	
	else if (cmd->is("AetherSnapping") || cmd->is("AS_Snapping")) {
		if (!CheckTablet()) return true;
		tablet->aetherSmooth.enableRadialFollow = cmd->GetBoolean(0, tablet->aetherSmooth.enableRadialFollow);
		tablet->aetherSmooth.radialInner = cmd->GetDouble(1, tablet->aetherSmooth.radialInner);
		tablet->aetherSmooth.radialOuter = cmd->GetDouble(2, tablet->aetherSmooth.radialOuter);
		LOG_INFO("Aether Snapping = %s, inner = %0.2f, outer = %0.2f\n",
			tablet->aetherSmooth.enableRadialFollow ? "on" : "off",
			tablet->aetherSmooth.radialInner,
			tablet->aetherSmooth.radialOuter);
	}

	
	else if (cmd->is("AetherRhythmFlow") || cmd->is("AS_RhythmFlow")) {
		if (!CheckTablet()) return true;
		tablet->aetherSmooth.enableRhythmFlow = cmd->GetBoolean(0, tablet->aetherSmooth.enableRhythmFlow);
		tablet->aetherSmooth.rhythmStrength = cmd->GetDouble(1, tablet->aetherSmooth.rhythmStrength);
		tablet->aetherSmooth.rhythmTurnRelease = cmd->GetDouble(2, tablet->aetherSmooth.rhythmTurnRelease);
		tablet->aetherSmooth.rhythmJitter = cmd->GetDouble(3, tablet->aetherSmooth.rhythmJitter);

		if (tablet->aetherSmooth.rhythmStrength < 0) tablet->aetherSmooth.rhythmStrength = 0;
		if (tablet->aetherSmooth.rhythmStrength > 1) tablet->aetherSmooth.rhythmStrength = 1;
		if (tablet->aetherSmooth.rhythmTurnRelease < 0) tablet->aetherSmooth.rhythmTurnRelease = 0;
		if (tablet->aetherSmooth.rhythmTurnRelease > 1) tablet->aetherSmooth.rhythmTurnRelease = 1;
		if (tablet->aetherSmooth.rhythmJitter < 0) tablet->aetherSmooth.rhythmJitter = 0;
		if (tablet->aetherSmooth.rhythmJitter > 1.5) tablet->aetherSmooth.rhythmJitter = 1.5;

		LOG_INFO("Aether Rhythm Flow = %s, flow = %0.2f, release = %0.2f, jitter = %0.2f mm\n",
			tablet->aetherSmooth.enableRhythmFlow ? "on" : "off",
			tablet->aetherSmooth.rhythmStrength,
			tablet->aetherSmooth.rhythmTurnRelease,
			tablet->aetherSmooth.rhythmJitter);
	}

	
	else if (cmd->is("AetherSuppression") || cmd->is("AS_Suppress")) {
		if (!CheckTablet()) return true;
		tablet->aetherSmooth.enableDebounce = cmd->GetBoolean(0, tablet->aetherSmooth.enableDebounce);
		tablet->aetherSmooth.debounceMs = cmd->GetDouble(1, tablet->aetherSmooth.debounceMs);
		LOG_INFO("Aether Suppression = %s, time = %0.1f ms\n",
			tablet->aetherSmooth.enableDebounce ? "on" : "off",
			tablet->aetherSmooth.debounceMs);
	}

	else if (cmd->is("PluginInstall") || cmd->is("InstallPlugin")) {
		string source = cmd->GetString(0, "");
		if (source.empty()) {
			LOG_ERROR("PluginInstall requires a DLL path.\n");
			return true;
		}

		if (tablet != NULL) {
			lock_guard<mutex> lock(tabletStateMutex);
			tablet->ClearPluginFilters();
		}

		std::wstring installedPath;
		if (InstallAetherPluginDll(Utf8ToWideService(source), &installedPath)) {
			LOG_INFO("Plugin installed. Reloading plugins...\n");
			if (tablet != NULL) {
				lock_guard<mutex> lock(tabletStateMutex);
				tablet->ReloadPluginFilters(GetAetherPluginDirectory());
			}
		}
	}

	else if (cmd->is("PluginReload") || cmd->is("PluginsReload") || cmd->is("ReloadPlugins")) {
		EnsureAetherPluginDirectory();
		if (!CheckTablet()) return true;

		lock_guard<mutex> lock(tabletStateMutex);
		tablet->ReloadPluginFilters(GetAetherPluginDirectory());
	}

	else if (cmd->is("PluginList") || cmd->is("PluginsList") || cmd->is("ListPlugins")) {
		if (!CheckTablet()) return true;

		if (tablet->pluginFilters.empty()) {
			LOG_INFO("No Aether plugins loaded. Folder: %ls\n", GetAetherPluginDirectory().c_str());
		}
		else {
			for (size_t i = 0; i < tablet->pluginFilters.size(); i++) {
				TabletFilterPlugin* plugin = tablet->pluginFilters[i];
				LOG_INFO("Plugin %d: %s [%s] key=%s\n", (int)i, plugin->name.c_str(), plugin->isEnabled ? "on" : "off", PluginRelativeKey(plugin).c_str());
			}
		}
	}

	else if (cmd->is("PluginEnable") || cmd->is("EnablePlugin")) {
		if (!CheckTablet()) return true;
		string selector = cmd->GetString(0, "");
		int index = FindPluginFilterIndex(selector);
		if (index < 0 || index >= (int)tablet->pluginFilters.size()) {
			LOG_DEBUG("Plugin not loaded yet, ignoring enable state: %s\n", selector.c_str());
			return true;
		}

		bool enabled = cmd->GetBoolean(1, true);
		tablet->pluginFilters[index]->isEnabled = enabled;
		LOG_INFO("Plugin %s = %s\n", PluginRelativeKey(tablet->pluginFilters[index]).c_str(), enabled ? "on" : "off");
	}

	else if (cmd->is("PluginSet") || cmd->is("SetPlugin")) {
		if (!CheckTablet()) return true;
		string selector = cmd->GetString(0, "");
		int index = FindPluginFilterIndex(selector);
		if (index < 0 || index >= (int)tablet->pluginFilters.size()) {
			LOG_DEBUG("Plugin not loaded yet, ignoring option: %s\n", selector.c_str());
			return true;
		}

		string key = cmd->GetString(1, "");
		if (key.empty()) {
			LOG_ERROR("PluginSet requires option name.\n");
			return true;
		}

		string raw = cmd->GetString(2, "");
		if (raw.empty()) {
			LOG_ERROR("PluginSet requires option value.\n");
			return true;
		}

		char* parseEnd = NULL;
		double numberValue = strtod(raw.c_str(), &parseEnd);
		bool isNumber = parseEnd != raw.c_str() && *parseEnd == '\0';
		bool applied = false;
		if (isNumber)
			applied = tablet->pluginFilters[index]->SetDoubleOption(key, numberValue);
		if (!applied)
			applied = tablet->pluginFilters[index]->SetStringOption(key, raw);

		if (applied) {
			LOG_INFO("Plugin %d option %s = %s\n", index, key.c_str(), raw.c_str());
		}
		else {
			LOG_WARNING("Plugin %d did not accept option %s\n", index, key.c_str());
		}
	}

	else if (cmd->is("PluginDir") || cmd->is("PluginsDir")) {
		EnsureAetherPluginDirectory();
		LOG_INFO("Plugin directory: %ls\n", GetAetherPluginDirectory().c_str());
	}


	else if (cmd->is("UpdateMonitorInfo")) {
		if (vmulti != NULL) {
			vmulti->UpdateMonitorInfo();
			LOG_INFO("Monitor info updated: primary=%0.0fx%0.0f virtual=%0.0fx%0.0f offset=%+0.0f,%+0.0f\n",
				vmulti->monitorInfo.primaryWidth, vmulti->monitorInfo.primaryHeight,
				vmulti->monitorInfo.virtualWidth, vmulti->monitorInfo.virtualHeight,
				vmulti->monitorInfo.virtualX, vmulti->monitorInfo.virtualY);
		}
	}

	else if (cmd->is("Debug")) {
		if (!CheckTablet()) return true;
		tablet->debugEnabled = cmd->GetBoolean(0, tablet->debugEnabled);
		
		LOG_INFO("Tablet debug = %s\n", tablet->debugEnabled ? "True" : "False");
	}


	
	else if (cmd->is("Log") && cmd->valueCount > 0) {
		string logPath = cmd->GetString(0, "log.txt");
		if (!cmd->GetBoolean(0, true)) {
			logger.CloseLogFile();
			LOG_INFO("Log file '%s' closed.\n", logger.logFilename.c_str());
		}
		else if (logger.OpenLogFile(logPath)) {
			LOG_INFO("Log file '%s' opened.\n", logPath.c_str());
		}
		else {
			LOG_ERROR("Cant open log file!\n");
		}
	}


	
	else if (cmd->is("Wait")) {
		int waitTime = cmd->GetInt(0, 0);
		Sleep(waitTime);
	}

	
	else if (cmd->is("LogDirect")) {
		logger.ProcessMessages();
		logger.directPrint = cmd->GetBoolean(0, logger.directPrint);
		logger.ProcessMessages();

		LOG_INFO("Log direct print = %s\n", logger.directPrint ? "True" : "False");
	}


	
	else if (cmd->is("Output")) {
		vmulti->outputEnabled = cmd->GetBoolean(0, vmulti->outputEnabled);
		LOG_INFO("Output enabled = %s\n", vmulti->outputEnabled ? "True" : "False");
	}

	
	else if (cmd->is("Info")) {
		if (!CheckTablet()) return true;
		LogInformation();
	}

	
	else if (cmd->is("Status")) {
		if (!CheckTablet()) return true;
		LogStatus();
	}

	
	else if (cmd->is("Benchmark") || cmd->is("Bench")) {
		if (!CheckTablet()) return true;

		int timeLimit;
		int packetCount = cmd->GetInt(0, 200);

		
		if (packetCount < 10) packetCount = 10;
		if (packetCount > 1000) packetCount = 1000;

		
		timeLimit = packetCount * 10;
		if (timeLimit < 1000) timeLimit = 1000;

		
		LOG_DEBUG("Tablet benchmark starting in 3 seconds!\n");
		LOG_DEBUG("Keep the pen stationary on top of the tablet!\n");
		Sleep(3000);
		LOG_DEBUG("Benchmark started!\n");

		
		tablet->benchmark.Start(packetCount);

		
		for (int i = 0; i < timeLimit / 100; i++) {
			Sleep(100);

			
			if (tablet->benchmark.packetCounter <= 0) {

				double width = tablet->benchmark.maxX - tablet->benchmark.minX;
				double height = tablet->benchmark.maxY - tablet->benchmark.minY;
				LOG_DEBUG("\n");
				LOG_DEBUG("Benchmark result (%d positions):\n", tablet->benchmark.totalPackets);
				LOG_DEBUG("  Tablet: %s\n", tablet->name.c_str());
				LOG_DEBUG("  Area: %0.2f mm x %0.2f mm (%0.0f px x %0.0f px)\n",
					mapper->areaTablet.width,
					mapper->areaTablet.height,
					mapper->areaScreen.width,
					mapper->areaScreen.height
				);
				LOG_DEBUG("  X range: %0.3f mm <-> %0.3f mm\n", tablet->benchmark.minX, tablet->benchmark.maxX);
				LOG_DEBUG("  Y range: %0.3f mm <-> %0.3f mm\n", tablet->benchmark.minY, tablet->benchmark.maxY);
				LOG_DEBUG("  Width: %0.3f mm (%0.2f px)\n",
					width,
					mapper->areaScreen.width / mapper->areaTablet.width * width
				);
				LOG_DEBUG("  Height: %0.3f mm (%0.2f px)\n",
					height,
					mapper->areaScreen.height / mapper->areaTablet.height* height
				);
				LOG_DEBUG("\n");
				LOG_STATUS("BENCHMARK %d %0.3f %0.3f %s\n", tablet->benchmark.totalPackets, width, height, tablet->name.c_str());
				break;
			}
		}

		
		if (tablet->benchmark.packetCounter > 0) {
			LOG_ERROR("Benchmark failed!\n");
			LOG_ERROR("Not enough packets captured in %0.2f seconds!\n",
				timeLimit / 1000.0
			);
		}


	}


	
	else if (cmd->is("Include")) {
		string filename = cmd->GetString(0, "");
		if (filename == "") {
			LOG_ERROR("Invalid filename '%s'!\n", filename.c_str());
		}
		else {
			if (ReadCommandFile(filename)) {
			}
			else {
				LOG_ERROR("Can't open file '%s'\n", filename.c_str());
			}
		}
	}


	
	else if (cmd->is("Exit") || cmd->is("Quit")) {
		LOG_INFO("Bye!\n");
		CleanupAndExit(0);
	}


	
	else if (cmd->isValid) {
		LOG_WARNING("Unknown command: %s\n", cmd->line.c_str());
	}

	return true;
}






bool ReadCommandFile(string filename) {
	CommandLine *cmd;
	ifstream file;
	string line = "";

	
	file.open(filename);
	if (!file.is_open()) {
		return false;
	}


	LOG_INFO("\\ Reading '%s'\n", filename.c_str());

	
	while (!file.eof()) {
		getline(file, line);
		if (line.length() == 0) continue;
		cmd = new CommandLine(line);

		
		
		
		if (cmd->is("Tablet") && tablet != NULL && tablet->IsConfigured()) {
			LOG_INFO(">> %s\n", cmd->line.c_str());
			LOG_INFO("Tablet is already defined!\n");
			delete cmd;
			break;
		}
		ProcessCommand(cmd);
		delete cmd;
	}
	file.close();

	LOG_INFO("/ End of '%s'\n", filename.c_str());

	return true;
}






void LogInformation() {
	char stringBuffer[64];
	int maxLength = sizeof(stringBuffer) - 1;
	int stringIndex = 0;

	LOG_INFO("\n");
	LOG_INFO("Tablet: %s\n", tablet->name.c_str());
	LOG_INFO("  Width = %0.2f mm\n", tablet->settings.width);
	LOG_INFO("  Height = %0.2f mm\n", tablet->settings.height);
	LOG_INFO("  Max X = %d\n", tablet->settings.maxX);
	LOG_INFO("  Max Y = %d\n", tablet->settings.maxY);
	LOG_INFO("  Max Pressure = %d\n", tablet->settings.maxPressure);
	LOG_INFO("  Click Pressure = %d\n", tablet->settings.clickPressure);
	LOG_INFO("  Keep Tip Down = %d packets\n", tablet->settings.keepTipDown);
	LOG_INFO("  Report Id = %02X\n", tablet->settings.reportId);
	LOG_INFO("  Report Length = %d bytes\n", tablet->settings.reportLength);
	LOG_INFO("  Detect Mask = 0x%02X\n", tablet->settings.detectMask);
	LOG_INFO("  Ignore Mask = 0x%02X\n", tablet->settings.ignoreMask);

	for (int i = 0; i < 8; i++) {
		stringIndex += sprintf_s(stringBuffer + stringIndex, maxLength - stringIndex, "%d ", tablet->buttonMap[i]);
	}
	LOG_INFO("  Button Map = %s\n", stringBuffer);


	if (tablet->initFeatureLength > 0) {
		LOG_INFOBUFFER(tablet->initFeature, tablet->initFeatureLength, "  Tablet init feature report: ");
	}
	if (tablet->initReportLength > 0) {
		LOG_INFOBUFFER(tablet->initReport, tablet->initReportLength, "  Tablet init report: ");
	}
	LOG_INFO("\n");
	LOG_INFO("Area:\n");
	LOG_INFO("  Desktop = %0.0fpx x %0.0fpx\n",
		mapper->areaVirtualScreen.width, mapper->areaVirtualScreen.height
	);
	LOG_INFO("  Screen Map = %0.0fpx x %0.0fpx @ X%+0.0fpx, Y%+0.0fpx\n",
		mapper->areaScreen.width, mapper->areaScreen.height,
		mapper->areaScreen.x, mapper->areaScreen.y
	);
	LOG_INFO("  Tablet =  %0.2fmm x %0.2fmm @ Min(%0.2fmm, %0.2fmm) Max(%0.2fmm, %0.2fmm)\n",
		mapper->areaTablet.width, mapper->areaTablet.height,
		mapper->areaTablet.x, mapper->areaTablet.y,
		mapper->areaTablet.width + mapper->areaTablet.x, mapper->areaTablet.height + mapper->areaTablet.y
	);
	LOG_INFO("  Area clipping = %s\n", mapper->areaClipping ? "on" : "off");
	LOG_INFO("  Rotation matrix = [%f,%f,%f,%f]\n",
		mapper->rotationMatrix[0],
		mapper->rotationMatrix[1],
		mapper->rotationMatrix[2],
		mapper->rotationMatrix[3]
	);
	LOG_INFO("\n");
}




void LogStatus() {
	LOG_STATUS("TABLET %s\n", tablet->name.c_str());

	if (tablet->hidDevice != NULL) {
		LOG_STATUS("HID %04X %04X %04X %04X\n",
			tablet->hidDevice->vendorId,
			tablet->hidDevice->productId,
			tablet->hidDevice->usagePage,
			tablet->hidDevice->usage
		);
	}
	else if (tablet->usbDevice != NULL) {
		LOG_STATUS("USB %d %s\n",
			tablet->usbDevice->stringId,
			tablet->usbDevice->stringMatch.c_str()
		);
	}
	LOG_STATUS("WIDTH %0.5f\n", tablet->settings.width);
	LOG_STATUS("HEIGHT %0.5f\n", tablet->settings.height);
	LOG_STATUS("MAX_X %d\n", tablet->settings.maxX);
	LOG_STATUS("MAX_Y %d\n", tablet->settings.maxY);
	LOG_STATUS("MAX_PRESSURE %d\n", tablet->settings.maxPressure);
	LOG_STATUS("PEN_RATE_LIMIT %s %0.0f\n", penRateLimitActive ? "on" : "off", penRateLimitHz);
}




void LogTabletArea(string text) {
	LOG_INFO("%s: (%0.2f mm x %0.2f mm X+%0.2f mm Y+%0.2f mm)\n",
		text.c_str(),
		mapper->areaTablet.width,
		mapper->areaTablet.height,
		mapper->areaTablet.x,
		mapper->areaTablet.y
	);
}


bool CheckTablet() {
	if (tablet == NULL) {
		return false;
	}
	return true;
}
