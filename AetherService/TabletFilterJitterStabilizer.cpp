#include "stdafx.h"
#include "TabletFilterJitterStabilizer.h"

#define LOG_MODULE "JitterStabilizer"
#include "Logger.h"

TabletFilterJitterStabilizer::TabletFilterJitterStabilizer() {
	radius = 0.1;
	releaseSpeed = 15.0;
	hasLatch = false;
	position.Set(0, 0);
	target.Set(0, 0);
	latched.Set(0, 0);
}

TabletFilterJitterStabilizer::~TabletFilterJitterStabilizer() {
}

void TabletFilterJitterStabilizer::Reset(Vector2D pos) {
	position.Set(pos);
	target.Set(pos);
	latched.Set(pos);
	hasLatch = true;
}

void TabletFilterJitterStabilizer::SetTarget(Vector2D vector, double h) {
	target.Set(vector);
}

void TabletFilterJitterStabilizer::SetPosition(Vector2D vector, double h) {
	position.Set(vector);
}

bool TabletFilterJitterStabilizer::GetPosition(Vector2D *outputVector) {
	outputVector->x = position.x;
	outputVector->y = position.y;
	return true;
}

void TabletFilterJitterStabilizer::Update() {
	if (!hasLatch) {
		latched.Set(target);
		hasLatch = true;
		position.Set(target);
		return;
	}

	double speed = hasHostTiming ? hostRawSpeed : 1e9;

	if (speed >= releaseSpeed) {
		latched.Set(target);
		position.Set(target);
		return;
	}

	double dist = target.Distance(latched);
	if (dist > radius) {

		latched.Set(target);
		position.Set(target);
		return;
	}

	position.Set(latched);
}
