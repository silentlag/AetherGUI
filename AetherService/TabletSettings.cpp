#include "stdafx.h"
#include "TabletSettings.h"





TabletSettings::TabletSettings() {
	
	reportId = 0;
	reportLength = 8;
	reportOffset = 0;
	detectMask = 0x00;
	ignoreMask = 0x00;
	maxX = 1;
	maxY = 1;
	maxPressure = 1;
	clickPressure = 0;
	width = 1;
	height = 1;
	skew = 0;
	type = TabletNormal;
	mouseWheelSpeed = 50;
	invMaxX = 1.0;
	invMaxY = 1.0;
}




TabletSettings::~TabletSettings() {
}
