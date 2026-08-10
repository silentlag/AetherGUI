#include "stdafx.h"
#include "TabletFilterReconstructor.h"
#include "Platform.h"
#include <chrono>
#include <cmath>

#define LOG_MODULE "Reconstructor"
#include "Logger.h"

TabletFilterReconstructor::TabletFilterReconstructor() {
	reconstructionStrength = 0.3;
	velocitySmoothing = 0.5;

	useInverseEma = false;
	emaWeight = 1.0;

	predictionRatio = 0.0;

	smoothedSpeed = 0.0;
	isFirstReport = true;
	hasPrevTmp = false;
	lastTimestamp = 0.0;

	prevStepX = 0.0;
	prevStepY = 0.0;

	position.Set(0, 0);
	target.Set(0, 0);
	prevTarget.Set(0, 0);
	prevTmp.Set(0, 0);
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
	prevTmp.Set(pos);
	smoothedSpeed = 0.0;
	isFirstReport = true;
	hasPrevTmp = false;
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

	if (reconstructionStrength <= 0.001 && !useInverseEma && predictionRatio <= 0.001) {
		position.Set(target);
		prevTarget.Set(target);
		return;
	}

	double now = GetCurrentTimeMs();

	if (isFirstReport) {
		position.Set(target);
		prevTarget.Set(target);
		prevTmp.Set(target);
		smoothedSpeed = 0.0;
		prevStepX = 0.0;
		prevStepY = 0.0;
		lastTimestamp = now;
		isFirstReport = false;
		hasPrevTmp = false;
		return;
	}

	double dtMs = now - lastTimestamp;
	lastTimestamp = now;
	if (dtMs <= 0.0 || dtMs > 100.0) dtMs = 1.0;
	double dtSec = dtMs / 1000.0;

	double instStep = target.Distance(prevTarget);
	double instSpeed = instStep / dtSec;

	double alpha = 1.0 - velocitySmoothing;
	if (alpha < 0.0) alpha = 0.0;
	if (alpha > 1.0) alpha = 1.0;
	smoothedSpeed = smoothedSpeed * velocitySmoothing + instSpeed * alpha;

	double velocityScale = smoothedSpeed / 50.0;
	if (velocityScale < 0.0) velocityScale = 0.0;
	if (velocityScale > 1.0) velocityScale = 1.0;

	const double kSpeedDeadZone = 0.5;
	if (smoothedSpeed < kSpeedDeadZone && predictionRatio <= 0.001) {
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
		curvGain = 0.3 + 0.7 * cosA * cosA;
	}

	double gain = 1.0 + reconstructionStrength * velocityScale * curvGain;

	double dx = stepX * gain;
	double dy = stepY * gain;

	double maxLead = instStep * (1.0 + reconstructionStrength * 2.0) + 0.01;
	double leadDist = sqrt(dx * dx + dy * dy);
	if (leadDist > maxLead && leadDist > 0.0) {
		double scale = maxLead / leadDist;
		dx *= scale;
		dy *= scale;
	}

	double reconX = prevTarget.x + dx;
	double reconY = prevTarget.y + dy;

	if (useInverseEma && emaWeight < 1.0) {
		double w = emaWeight;
		if (w < 0.001) w = 0.001;
		double ex = (reconX - prevTmp.x) / w;
		double ey = (reconY - prevTmp.y) / w;

		double lead = sqrt(ex * ex + ey * ey);
		double maxLeadE = instStep / w + 0.01;
		if (lead > maxLeadE && lead > 0.0) {
			double sc = maxLeadE / lead;
			ex *= sc;
			ey *= sc;
		}
		reconX = prevTmp.x + ex;
		reconY = prevTmp.y + ey;
	}

	if (predictionRatio > 0.001 && hasPrevTmp) {
		double vx = reconX - prevTmp.x;
		double vy = reconY - prevTmp.y;
		double lead = predictionRatio * dtSec * smoothedSpeed;
		double vMag = sqrt(vx * vx + vy * vy);
		if (vMag > 1e-9) {
			double nx = vx / vMag;
			double ny = vy / vMag;
			reconX += nx * lead;
			reconY += ny * lead;
		} else {
			reconX += stepX * predictionRatio;
			reconY += stepY * predictionRatio;
		}
	}
	prevTmp.Set(reconX, reconY);
	hasPrevTmp = true;

	position.x = reconX;
	position.y = reconY;

	prevStepX = stepX;
	prevStepY = stepY;
	prevTarget.Set(target);
}
