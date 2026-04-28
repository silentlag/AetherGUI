#include "stdafx.h"
#include "TabletFilterAetherSmooth.h"

#define LOG_MODULE "AetherSmooth"
#include "Logger.h"




TabletFilterAetherSmooth::TabletFilterAetherSmooth() {
	
	flowFirstTime = true;
	velocity = 0;

	
	isFirstReport = true;

	

	
	enableAntismoothing = true;
	antismoothing = 0.6;

	
	enableSmoothing = true;
	stability = 1.0;
	speedSensitivity = 0.015;

	
	enableRadialFollow = true;
	radialInner = 0.5;
	radialOuter = 3.0;

	
	enableDebounce = true;
	debounceMs = 5.0;

	lastTime = std::chrono::high_resolution_clock::now();
	debounceTime = std::chrono::high_resolution_clock::now();
}




TabletFilterAetherSmooth::~TabletFilterAetherSmooth() {
}




void TabletFilterAetherSmooth::Reset(Vector2D pos) {
	lastPos.Set(pos);
	lastAntismoothPos.Set(pos);
	lastSmoothedPos.Set(pos);
	prevProcessedPos.Set(pos);
	debouncePos.Set(pos);
	position.Set(pos);
	target.Set(pos);
	velocity = 0;
	isFirstReport = true;
	AdaptiveFlowReset(pos);
	lastTime = std::chrono::high_resolution_clock::now();
	debounceTime = std::chrono::high_resolution_clock::now();
}




void TabletFilterAetherSmooth::SetTarget(Vector2D vector, double h) {
	target.x = vector.x;
	target.y = vector.y;
	z = h;
}




void TabletFilterAetherSmooth::SetPosition(Vector2D vector, double h) {
	position.x = vector.x;
	position.y = vector.y;
	z = h;
}




bool TabletFilterAetherSmooth::GetPosition(Vector2D *outputVector) {
	outputVector->x = position.x;
	outputVector->y = position.y;
	return true;
}




double TabletFilterAetherSmooth::CalculateAlpha(double dt, double fc) {
	double tau = 1.0 / (2.0 * M_PI * fc);
	return 1.0 / (1.0 + tau / dt);
}




void TabletFilterAetherSmooth::AdaptiveFlowReset(Vector2D pos) {
	xPrev.Set(pos);
	dxPrev.x = 0;
	dxPrev.y = 0;
	flowFirstTime = true;
}





Vector2D TabletFilterAetherSmooth::AdaptiveFlow(Vector2D x, double dt, double minCutoff, double beta, double dCutoff) {
	if (flowFirstTime) {
		xPrev.Set(x);
		dxPrev.x = 0;
		dxPrev.y = 0;
		flowFirstTime = false;
		return x;
	}

	if (dt <= 0) return xPrev;

	
	Vector2D dx;
	dx.x = (x.x - xPrev.x) / dt;
	dx.y = (x.y - xPrev.y) / dt;

	
	double alphaD = CalculateAlpha(dt, dCutoff);
	Vector2D dxFiltered;
	dxFiltered.x = alphaD * dx.x + (1.0 - alphaD) * dxPrev.x;
	dxFiltered.y = alphaD * dx.y + (1.0 - alphaD) * dxPrev.y;
	dxPrev.Set(dxFiltered);

	
	velocity = dxFiltered.Length();

	
	double cutoff = minCutoff + beta * velocity;

	
	double alpha = CalculateAlpha(dt, cutoff);
	Vector2D xFiltered;
	xFiltered.x = alpha * x.x + (1.0 - alpha) * xPrev.x;
	xFiltered.y = alpha * x.y + (1.0 - alpha) * xPrev.y;
	xPrev.Set(xFiltered);

	return xFiltered;
}





void TabletFilterAetherSmooth::Update() {

	
	auto now = std::chrono::high_resolution_clock::now();
	double dt = (now - lastTime).count() / 1000000000.0; 
	lastTime = now;
	if (dt <= 0 || dt > 0.1) dt = 0.001;

	Vector2D currentPos = target;

	
	if (isFirstReport) {
		Reset(currentPos);
		isFirstReport = false;
		position.Set(currentPos);
		return;
	}

	
	double velocityScale = velocity / 100.0;
	if (velocityScale < 0) velocityScale = 0;
	if (velocityScale > 1) velocityScale = 1;

	double effectiveAntismoothing = 1.0 + (antismoothing - 1.0) * velocityScale;
	double safeAntismoothing = effectiveAntismoothing;
	if (safeAntismoothing < 0.6) safeAntismoothing = 0.6;

	Vector2D processedPos = currentPos;

	
	if (enableAntismoothing) {
		processedPos.x = lastAntismoothPos.x + (currentPos.x - lastAntismoothPos.x) / safeAntismoothing;
		processedPos.y = lastAntismoothPos.y + (currentPos.y - lastAntismoothPos.y) / safeAntismoothing;
		lastAntismoothPos.Set(currentPos);
	}

	
	if (enableSmoothing) {
		processedPos = AdaptiveFlow(processedPos, dt, stability, speedSensitivity, 1.0);
	}

	
	if (enableRadialFollow) {
		double dist = processedPos.Distance(lastSmoothedPos);
		double range = radialOuter - radialInner;
		double factor = 0;
		if (range > 0.0001) {
			factor = (dist - radialInner) / range;
		}
		if (factor < 0) factor = 0;
		if (factor > 1) factor = 1;

		
		factor = factor * factor * (3.0 - 2.0 * factor);

		
		processedPos.x = lastSmoothedPos.x + (processedPos.x - lastSmoothedPos.x) * factor;
		processedPos.y = lastSmoothedPos.y + (processedPos.y - lastSmoothedPos.y) * factor;
	}
	lastSmoothedPos.Set(processedPos);

	
	if (enableDebounce) {
		double debounceElapsed = (now - debounceTime).count() / 1000000.0; 
		bool isMovingIntentional = velocity > 0.05 || processedPos.DistanceSq(debouncePos) > 0.04; 

		if (isMovingIntentional) {
			debouncePos.Set(processedPos);
			debounceTime = now;
		}
		else if (debounceElapsed < debounceMs) {
			processedPos.Set(debouncePos);
		}
	}

	
	prevProcessedPos.Set(processedPos);
	lastPos.Set(currentPos);
	position.Set(processedPos);
}
