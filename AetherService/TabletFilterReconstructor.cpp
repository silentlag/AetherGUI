#include "stdafx.h"
#include "TabletFilterReconstructor.h"
#include "Platform.h"
#include <chrono>

#define LOG_MODULE "Reconstructor"
#include "Logger.h"

TabletFilterReconstructor::TabletFilterReconstructor() {
	reconstructionStrength = 0.5;
	velocitySmoothing = 0.6;

	useInverseEma = false;
	emaWeight = 0.5;

	smoothedSpeed = 0.0;
	isFirstReport = true;
	lastTimestamp = 0.0;

	prevStepX = 0.0;
	prevStepY = 0.0;

	position.Set(0, 0);
	target.Set(0, 0);
	prevTarget.Set(0, 0);
}

TabletFilterReconstructor::~TabletFilterReconstructor() {
}

double TabletFilterReconstructor::GetCurrentTimeMs() {
	return (double)platform::MonotonicNs() / 1.0e6;
}

void TabletFilterReconstructor::Reset(Vector2D pos) {
	position.Set(pos);
	target.Set(pos);
	prevTarget.Set(pos);
	smoothedSpeed = 0.0;
	isFirstReport = true;
	lastTimestamp = GetCurrentTimeMs();
	prevStepX = 0.0;
	prevStepY = 0.0;
}

void TabletFilterReconstructor::SetTarget(Vector2D vector, double h) {
	target.Set(vector);
}

void TabletFilterReconstructor::SetPosition(Vector2D vector, double h) {
	position.Set(vector);
}

bool TabletFilterReconstructor::GetPosition(Vector2D *outputVector) {
	outputVector->x = position.x;
	outputVector->y = position.y;
	return true;
}

void TabletFilterReconstructor::Update() {

	if (reconstructionStrength <= 0.001 && !useInverseEma) {
		position.Set(target);
		prevTarget.Set(target);
		return;
	}

	double now = GetCurrentTimeMs();

	if (isFirstReport) {
		position.Set(target);
		prevTarget.Set(target);
		smoothedSpeed = 0.0;
		prevStepX = 0.0;
		prevStepY = 0.0;
		lastTimestamp = now;
		isFirstReport = false;
		return;
	}

	if (useInverseEma) {
		double w = emaWeight;
		if (w < 0.15) w = 0.15;
		if (w > 1.0)  w = 1.0;

		double dx = (target.x - prevTarget.x) / w;
		double dy = (target.y - prevTarget.y) / w;

		double instStep = target.Distance(prevTarget);
		double maxLead = instStep / w + 0.01;
		double leadDist = sqrt(dx * dx + dy * dy);
		if (leadDist > maxLead && leadDist > 0.0) {
			double scale = maxLead / leadDist;
			dx *= scale;
			dy *= scale;
		}

		position.x = prevTarget.x + dx;
		position.y = prevTarget.y + dy;
		prevTarget.Set(target);
		return;
	}

	double dtSec;
	double instSpeed;
	double instStep = target.Distance(prevTarget);
	if (hasHostTiming) {
		dtSec = hostDtSec;
		if (dtSec <= 0.0 || dtSec > 0.1) dtSec = 0.001;
		instSpeed = hostRawSpeed;
		lastTimestamp = now;
	} else {
		double dtMs = now - lastTimestamp;
		lastTimestamp = now;
		if (dtMs <= 0.0 || dtMs > 100.0) dtMs = 1.0;
		dtSec = dtMs / 1000.0;
		instSpeed = instStep / dtSec;
	}

	double alpha = 1.0 - velocitySmoothing;
	if (alpha < 0.0) alpha = 0.0;
	if (alpha > 1.0) alpha = 1.0;
	smoothedSpeed = smoothedSpeed * velocitySmoothing + instSpeed * alpha;

	double velocityScale = smoothedSpeed / 100.0;
	if (velocityScale < 0.0) velocityScale = 0.0;
	if (velocityScale > 1.0) velocityScale = 1.0;

	const double kSpeedDeadZone = 0.5;
	if (smoothedSpeed < kSpeedDeadZone) {
		position.Set(target);
		prevTarget.Set(target);
		prevStepX = 0.0;
		prevStepY = 0.0;
		return;
	}

	double stepX = target.x - prevTarget.x;
	double stepY = target.y - prevTarget.y;

	double curvGain = 1.0;
	double nStep = sqrt(stepX * stepX + stepY * stepY);
	double nPrev = sqrt(prevStepX * prevStepX + prevStepY * prevStepY);
	if (nStep > 1e-9 && nPrev > 1e-9) {
		double cosA = (stepX * prevStepX + stepY * prevStepY) / (nStep * nPrev);
		if (cosA < 0.0) cosA = 0.0;
		curvGain = cosA * cosA;
	}

	double gain = 1.0 + reconstructionStrength * velocityScale * curvGain;

	double dx = stepX * gain;
	double dy = stepY * gain;

	double maxLead = instStep * (1.0 + reconstructionStrength) + 0.01;
	double leadDist = sqrt(dx * dx + dy * dy);
	if (leadDist > maxLead && leadDist > 0.0) {
		double scale = maxLead / leadDist;
		dx *= scale;
		dy *= scale;
	}

	position.x = prevTarget.x + dx;
	position.y = prevTarget.y + dy;

	prevStepX = stepX;
	prevStepY = stepY;
	prevTarget.Set(target);
}
