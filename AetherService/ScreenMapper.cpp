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

	// Default Virtual Screen Area
	areaVirtualScreen.width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	areaVirtualScreen.height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
	areaVirtualScreen.x = 0;
	areaVirtualScreen.y = 0;

	// Default Tablet Area
	areaTablet.width = 80;
	areaTablet.height = 45;
	areaTablet.x = 10;
	areaTablet.y = 10;


	// Default Screen Area
	areaScreen.width = 1920;
	areaScreen.height = 1080;
	areaScreen.x = 0;
	areaScreen.y = 0;
	areaClipping = true;
	areaLimiting = false;

	// Default Rotation Matrix
	rotationMatrix[0] = 1;
	rotationMatrix[1] = 0;
	rotationMatrix[2] = 0;
	rotationMatrix[3] = 1;


}

//
// Create rotation matrix
//
void ScreenMapper::SetRotation(double angle) {
	angle *= M_PI / 180;
	rotationMatrix[0] = cos(angle);
	rotationMatrix[1] = -sin(angle);
	rotationMatrix[2] = sin(angle);
	rotationMatrix[3] = cos(angle);
}

//
// Get rotated tablet position
//
bool ScreenMapper::GetRotatedTabletPosition(double *x, double *y) {
	double mapX, mapY;
	double tmpX, tmpY;

	mapX = (*x);
	mapY = (*y);

	// Offset tablet so that the center is at zero
	mapX -= tablet->settings.width / 2.0;
	mapY -= tablet->settings.height / 2.0;

	// Rotate
	tmpX = mapX;
	tmpY = mapY;
	mapX = tmpX * rotationMatrix[0] + tmpY * rotationMatrix[1];
	mapY = tmpX * rotationMatrix[2] + tmpY * rotationMatrix[3];

	// Offset back to center from zero
	mapX += tablet->settings.width / 2.0;
	mapY += tablet->settings.height / 2.0;

	// Set pointer values
	*x = mapX;
	*y = mapY;

	return true;
}

//
// Get screen position. Return values between 0 and 1
//
bool ScreenMapper::GetScreenPosition(double *x, double *y) {
	double mapX, mapY;
	double tmpX, tmpY;

	mapX = (*x);
	mapY = (*y);

	// areaTablet.x/y is the CENTER of the tablet area (set by GUI)
	double centerX = areaTablet.x;
	double centerY = areaTablet.y;
	mapX -= centerX;
	mapY -= centerY;

	// Apply rotation around the center
	tmpX = mapX;
	tmpY = mapY;
	mapX = tmpX * rotationMatrix[0] + tmpY * rotationMatrix[1];
	mapY = tmpX * rotationMatrix[2] + tmpY * rotationMatrix[3];

	// Scale to normalized [0..1]
	mapX = mapX / areaTablet.width + 0.5;
	mapY = mapY / areaTablet.height + 0.5;


	// Scale to screen area size
	mapX *= (areaScreen.width);
	mapY *= (areaScreen.height);

	// Offset screen area
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
