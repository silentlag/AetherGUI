#include "stdafx.h"
#include "ScreenMapper.h"

#define LOG_MODULE "ScreenMapper"
#include "Logger.h"

#include <stdio.h>

#define _USE_MATH_DEFINES
#include <cmath>
#include <math.h>

ScreenMapper::ScreenMapper(Tablet *t) {
	this->tablet = t;

	
	RefreshVirtualScreen();

	
	areaTablet.width = 80;
	areaTablet.height = 45;
	areaTablet.x = 10;
	areaTablet.y = 10;


	
	areaScreen.width = 1920;
	areaScreen.height = 1080;
	areaScreen.x = 0;
	areaScreen.y = 0;
	areaClipping = true;
	areaLimiting = false;

	
	rotationMatrix[0] = 1;
	rotationMatrix[1] = 0;
	rotationMatrix[2] = 0;
	rotationMatrix[3] = 1;


}




void ScreenMapper::SetRotation(double angle) {
	angle *= M_PI / 180;
	rotationMatrix[0] = cos(angle);
	rotationMatrix[1] = -sin(angle);
	rotationMatrix[2] = sin(angle);
	rotationMatrix[3] = cos(angle);
}




void ScreenMapper::RefreshVirtualScreen() {
	int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
	int top = GetSystemMetrics(SM_YVIRTUALSCREEN);
	int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

	areaVirtualScreen.x = (double)left;
	areaVirtualScreen.y = (double)top;
	areaVirtualScreen.width = (double)(width > 0 ? width : GetSystemMetrics(SM_CXSCREEN));
	areaVirtualScreen.height = (double)(height > 0 ? height : GetSystemMetrics(SM_CYSCREEN));

	if (areaVirtualScreen.width < 1) areaVirtualScreen.width = 1920;
	if (areaVirtualScreen.height < 1) areaVirtualScreen.height = 1080;
}




void ScreenMapper::ClampScreenAreaToVirtualScreen(bool refreshVirtualScreen) {
	if (refreshVirtualScreen) RefreshVirtualScreen();

	if (areaScreen.width < 1) areaScreen.width = 1;
	if (areaScreen.height < 1) areaScreen.height = 1;
	if (areaScreen.width > areaVirtualScreen.width) areaScreen.width = areaVirtualScreen.width;
	if (areaScreen.height > areaVirtualScreen.height) areaScreen.height = areaVirtualScreen.height;

	double minX = areaVirtualScreen.x;
	double minY = areaVirtualScreen.y;
	double maxX = areaVirtualScreen.x + areaVirtualScreen.width - areaScreen.width;
	double maxY = areaVirtualScreen.y + areaVirtualScreen.height - areaScreen.height;

	if (areaScreen.x < minX) areaScreen.x = minX;
	if (areaScreen.y < minY) areaScreen.y = minY;
	if (areaScreen.x > maxX) areaScreen.x = maxX;
	if (areaScreen.y > maxY) areaScreen.y = maxY;
}




bool ScreenMapper::GetRotatedTabletPosition(double *x, double *y) {
	double mapX, mapY;
	double tmpX, tmpY;

	mapX = (*x);
	mapY = (*y);

	
	mapX -= tablet->settings.width / 2.0;
	mapY -= tablet->settings.height / 2.0;

	
	tmpX = mapX;
	tmpY = mapY;
	mapX = tmpX * rotationMatrix[0] + tmpY * rotationMatrix[1];
	mapY = tmpX * rotationMatrix[2] + tmpY * rotationMatrix[3];

	
	mapX += tablet->settings.width / 2.0;
	mapY += tablet->settings.height / 2.0;

	
	*x = mapX;
	*y = mapY;

	return true;
}




bool ScreenMapper::GetScreenPosition(double *x, double *y) {
	double mapX, mapY;
	double tmpX, tmpY;

	mapX = (*x);
	mapY = (*y);

	
	double centerX = areaTablet.x;
	double centerY = areaTablet.y;
	mapX -= centerX;
	mapY -= centerY;

	
	tmpX = mapX;
	tmpY = mapY;
	mapX = tmpX * rotationMatrix[0] + tmpY * rotationMatrix[1];
	mapY = tmpX * rotationMatrix[2] + tmpY * rotationMatrix[3];

	
	mapX = mapX / areaTablet.width + 0.5;
	mapY = mapY / areaTablet.height + 0.5;


	
	mapX *= (areaScreen.width);
	mapY *= (areaScreen.height);

	
	mapX += areaScreen.x;
	mapY += areaScreen.y;

	if (areaLimiting) {
		if (mapX < areaScreen.x || mapX > areaScreen.x + areaScreen.width ||
			mapY < areaScreen.y || mapY > areaScreen.y + areaScreen.height) {
			return false;
		}
	}

	if (areaClipping || areaLimiting) {
		if (mapX < areaScreen.x) mapX = areaScreen.x;
		if (mapY < areaScreen.y) mapY = areaScreen.y;
		if (mapX > areaScreen.x + areaScreen.width - 1) mapX = areaScreen.x + areaScreen.width - 1;
		if (mapY > areaScreen.y + areaScreen.height - 1) mapY = areaScreen.y + areaScreen.height - 1;
	}

	*x = mapX;
	*y = mapY;

	return 1;
}
