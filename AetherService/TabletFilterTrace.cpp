#include "stdafx.h"
#include "TabletFilterTrace.h"
#include <cmath>

#define LOG_MODULE "Trace"
#include "Logger.h"

TabletFilterTrace::TabletFilterTrace() {
	cornerSharpness = 2.0;
	lineSmooth = 0.5;
	position.Set(0, 0);
	target.Set(0, 0);
	prevTarget.Set(0, 0);
	prevPrevTarget.Set(0, 0);
	hasHistory = false;
	isEnabled = false;
}

TabletFilterTrace::~TabletFilterTrace() {
}

void TabletFilterTrace::Reset(Vector2D pos) {
	position.Set(pos);
	target.Set(pos);
	prevTarget.Set(pos);
	prevPrevTarget.Set(pos);
	hasHistory = false;
}

void TabletFilterTrace::SetTarget(Vector2D vector, double h) {
	target.Set(vector);
}

void TabletFilterTrace::SetPosition(Vector2D vector, double h) {
	position.Set(vector);
}

bool TabletFilterTrace::GetPosition(Vector2D *outputVector) {
	outputVector->x = position.x;
	outputVector->y = position.y;
	return true;
}

void TabletFilterTrace::Update() {
	if (!hasHistory) {
		prevPrevTarget.Set(prevTarget);
		prevTarget.Set(target);
		position.Set(target);
		hasHistory = true;
		return;
	}

	double v1x = prevTarget.x - prevPrevTarget.x;
	double v1y = prevTarget.y - prevPrevTarget.y;
	double v2x = target.x - prevTarget.x;
	double v2y = target.y - prevTarget.y;

	double len1 = sqrt(v1x * v1x + v1y * v1y);
	double len2 = sqrt(v2x * v2x + v2y * v2y);

	double curvature = 0.0;
	if (len1 > 0.01 && len2 > 0.01) {
		double dot = (v1x * v2x + v1y * v2y) / (len1 * len2);
		if (dot > 1.0) dot = 1.0;
		if (dot < -1.0) dot = -1.0;
		curvature = 1.0 - dot;
	}

	double sharpThreshold = cornerSharpness * 0.2;
	double factor;
	if (curvature >= sharpThreshold) {
		factor = 0.0;
	} else {
		factor = lineSmooth * (1.0 - curvature / sharpThreshold);
		if (factor < 0.0) factor = 0.0;
		if (factor > 0.98) factor = 0.98;
	}

	position.x = position.x + (target.x - position.x) * (1.0 - factor);
	position.y = position.y + (target.y - position.y) * (1.0 - factor);

	prevPrevTarget.Set(prevTarget);
	prevTarget.Set(target);
}
