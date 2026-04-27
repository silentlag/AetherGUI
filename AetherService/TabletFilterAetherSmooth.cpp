#include "stdafx.h"
#include "TabletFilterAetherSmooth.h"

#define LOG_MODULE "AetherSmooth"
#include "Logger.h"

//
// Constructor
//
TabletFilterAetherSmooth::TabletFilterAetherSmooth() {
	// Adaptive Flow state
	flowFirstTime = true;
	velocity = 0;

	// First report flag
	isFirstReport = true;

	// Default parameters

	// [1] Lag Removal
	enableAntismoothing = true;
	antismoothing = 0.6;

	// [2] Stabilizer
	enableSmoothing = true;
	stability = 1.0;
	speedSensitivity = 0.015;

	// [3] Dynamic Snapping
	enableRadialFollow = true;
	radialInner = 0.5;
	radialOuter = 3.0;

	// [4] Suppression
	enableDebounce = true;
	debounceMs = 5.0;

	lastTime = std::chrono::high_resolution_clock::now();
	debounceTime = std::chrono::high_resolution_clock::now();
}

//
// Destructor
//
TabletFilterAetherSmooth::~TabletFilterAetherSmooth() {
}

//
// Reset
//
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

//
// Set target position (called each packet)
//
void TabletFilterAetherSmooth::SetTarget(Vector2D vector, double h) {
	target.x = vector.x;
	target.y = vector.y;
	z = h;
}

//
// Set position
//
void TabletFilterAetherSmooth::SetPosition(Vector2D vector, double h) {
	position.x = vector.x;
	position.y = vector.y;
	z = h;
}

//
// Get position
//
bool TabletFilterAetherSmooth::GetPosition(Vector2D *outputVector) {
	outputVector->x = position.x;
	outputVector->y = position.y;
	return true;
}

//
// Adaptive Flow: Calculate alpha
//
double TabletFilterAetherSmooth::CalculateAlpha(double dt, double fc) {
	double tau = 1.0 / (2.0 * M_PI * fc);
	return 1.0 / (1.0 + tau / dt);
}

//
// Adaptive Flow: Reset
//
void TabletFilterAetherSmooth::AdaptiveFlowReset(Vector2D pos) {
	xPrev.Set(pos);
	dxPrev.x = 0;
	dxPrev.y = 0;
	flowFirstTime = true;
}

//
// Adaptive Flow: Process
//   Velocity-adaptive low-pass — smooths at rest, transparent at speed
//
Vector2D TabletFilterAetherSmooth::AdaptiveFlow(Vector2D x, double dt, double minCutoff, double beta, double dCutoff) {
	if (flowFirstTime) {
		xPrev.Set(x);
		dxPrev.x = 0;
		dxPrev.y = 0;
		flowFirstTime = false;
		return x;
	}

	if (dt <= 0) return xPrev;

	// Derivative
	Vector2D dx;
	dx.x = (x.x - xPrev.x) / dt;
	dx.y = (x.y - xPrev.y) / dt;

	// Low-pass on derivative
	double alphaD = CalculateAlpha(dt, dCutoff);
	Vector2D dxFiltered;
	dxFiltered.x = alphaD * dx.x + (1.0 - alphaD) * dxPrev.x;
	dxFiltered.y = alphaD * dx.y + (1.0 - alphaD) * dxPrev.y;
	dxPrev.Set(dxFiltered);

	// Update velocity
	velocity = dxFiltered.Length();

	// Adaptive cutoff
	double cutoff = minCutoff + beta * velocity;

	// Low-pass on position
	double alpha = CalculateAlpha(dt, cutoff);
	Vector2D xFiltered;
	xFiltered.x = alpha * x.x + (1.0 - alpha) * xPrev.x;
	xFiltered.y = alpha * x.y + (1.0 - alpha) * xPrev.y;
	xPrev.Set(xFiltered);

	return xFiltered;
}


//
// Update — main Aether Smooth pipeline
//
void TabletFilterAetherSmooth::Update() {

	// Delta time
	auto now = std::chrono::high_resolution_clock::now();
	double dt = (now - lastTime).count() / 1000000000.0; // seconds
	lastTime = now;
	if (dt <= 0 || dt > 0.1) dt = 0.001;

	Vector2D currentPos = target;

	// First report — initialize
	if (isFirstReport) {
		Reset(currentPos);
		isFirstReport = false;
		position.Set(currentPos);
		return;
	}

	// Velocity scale for antismoothing
	double velocityScale = velocity / 100.0;
	if (velocityScale < 0) velocityScale = 0;
	if (velocityScale > 1) velocityScale = 1;

	double effectiveAntismoothing = 1.0 + (antismoothing - 1.0) * velocityScale;
	double safeAntismoothing = effectiveAntismoothing;
	if (safeAntismoothing < 0.6) safeAntismoothing = 0.6;

	Vector2D processedPos = currentPos;

	// === [1] Lag Removal ===
	if (enableAntismoothing) {
		processedPos.x = lastAntismoothPos.x + (currentPos.x - lastAntismoothPos.x) / safeAntismoothing;
		processedPos.y = lastAntismoothPos.y + (currentPos.y - lastAntismoothPos.y) / safeAntismoothing;
		lastAntismoothPos.Set(currentPos);
	}

	// === [2] Stabilizer (Adaptive Flow) ===
	if (enableSmoothing) {
		processedPos = AdaptiveFlow(processedPos, dt, stability, speedSensitivity, 1.0);
	}

	// === [3] Dynamic Snapping ===
	if (enableRadialFollow) {
		double dist = processedPos.Distance(lastSmoothedPos);
		double range = radialOuter - radialInner;
		double factor = 0;
		if (range > 0.0001) {
			factor = (dist - radialInner) / range;
		}
		if (factor < 0) factor = 0;
		if (factor > 1) factor = 1;

		// Smoothstep
		factor = factor * factor * (3.0 - 2.0 * factor);

		// Lerp
		processedPos.x = lastSmoothedPos.x + (processedPos.x - lastSmoothedPos.x) * factor;
		processedPos.y = lastSmoothedPos.y + (processedPos.y - lastSmoothedPos.y) * factor;
	}
	lastSmoothedPos.Set(processedPos);

	// === [4] Suppression ===
	if (enableDebounce) {
		double debounceElapsed = (now - debounceTime).count() / 1000000.0; // ms
		bool isMovingIntentional = velocity > 0.05 || processedPos.DistanceSq(debouncePos) > 0.04; // 0.2^2 = 0.04

		if (isMovingIntentional) {
			debouncePos.Set(processedPos);
			debounceTime = now;
		}
		else if (debounceElapsed < debounceMs) {
			processedPos.Set(debouncePos);
		}
	}

	// Output
	prevProcessedPos.Set(processedPos);
	lastPos.Set(currentPos);
	position.Set(processedPos);
}
