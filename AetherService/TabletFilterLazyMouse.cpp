#include "stdafx.h"
#include "TabletFilterLazyMouse.h"

#define LOG_MODULE "LazyMouse"
#include "Logger.h"

TabletFilterLazyMouse::TabletFilterLazyMouse() {
	radius = 3.0;
	smooth = 0.5;
	hasLatch = false;
	hasReport = false;
	prevButtons = 0;
	lastButtons = 0;
	position.Set(0, 0);
	target.Set(0, 0);
	cursor.Set(0, 0);
}

TabletFilterLazyMouse::~TabletFilterLazyMouse() {
}

void TabletFilterLazyMouse::Reset(Vector2D pos) {
	position.Set(pos);
	target.Set(pos);
	cursor.Set(pos);
	hasLatch = true;
}

void TabletFilterLazyMouse::SetTarget(Vector2D vector, double h) {
	target.Set(vector);
}

void TabletFilterLazyMouse::SetPosition(Vector2D vector, double h) {
	position.Set(vector);
}

void TabletFilterLazyMouse::SetReportState(BYTE buttons, double pressure, double hoverDistance) {
	lastButtons = buttons;
	hasReport = true;
}

bool TabletFilterLazyMouse::GetPosition(Vector2D *outputVector) {
	outputVector->x = position.x;
	outputVector->y = position.y;
	return true;
}

void TabletFilterLazyMouse::Update() {
	if (radius <= 0.01) {
		position.Set(target);
		cursor.Set(target);
		return;
	}

	bool tipDown = (lastButtons & 0x01) != 0;
	bool prevTipDown = (prevButtons & 0x01) != 0;

	if (!hasLatch || (tipDown && !prevTipDown)) {
		cursor.Set(target);
		position.Set(target);
		hasLatch = true;
		prevButtons = lastButtons;
		return;
	}

	Vector2D d;
	d.x = target.x - cursor.x;
	d.y = target.y - cursor.y;
	double dist = sqrt(d.x * d.x + d.y * d.y);

	if (dist > radius) {
		double inv = 1.0 / dist;
		double excess = dist - radius;
		cursor.x += d.x * inv * excess;
		cursor.y += d.y * inv * excess;
	}

	double a = smooth;
	if (a < 0.0) a = 0.0;
	if (a > 0.95) a = 0.95;
	position.x = position.x * a + cursor.x * (1.0 - a);
	position.y = position.y * a + cursor.y * (1.0 - a);

	prevButtons = lastButtons;
}
