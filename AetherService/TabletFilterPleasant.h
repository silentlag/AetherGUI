#pragma once

#define _USE_MATH_DEFINES
#include <cmath>
#include <math.h>
#include <chrono>

#include "Vector2D.h"
#include "TabletFilter.h"

class TabletFilterPleasant : public TabletFilter {
public:

	// === 1€ Filter (OneFilter / PleasantFilter core) ===
	Vector2D xPrev;
	Vector2D dxPrev;
	bool oneFilterFirstTime;

	// === Filter state ===
	Vector2D lastPos;
	Vector2D lastAntismoothPos;
	Vector2D lastSmoothedPos;
	Vector2D prevProcessedPos;
	Vector2D debouncePos;
	bool isFirstReport;
	double velocity;

	// Timing
	std::chrono::high_resolution_clock::time_point lastTime;
	std::chrono::high_resolution_clock::time_point debounceTime;

	// === Parameters ===

	// [1] Lag Removal (Antismoothing)
	bool enableAntismoothing;
	double antismoothing;

	// [2] Stabilizer (1€ Filter)
	bool enableSmoothing;
	double stability;       // minCutoff
	double speedSensitivity; // beta

	// [3] Dynamic Snapping (Radial Follow)
	bool enableRadialFollow;
	double radialInner;
	double radialOuter;

	// [4] Suppression (Debounce)
	bool enableDebounce;
	double debounceMs;

	// Constructor / Destructor
	TabletFilterPleasant();
	~TabletFilterPleasant();

	// TabletFilter interface
	void SetTarget(Vector2D vector, double h);
	void SetPosition(Vector2D vector, double h);
	bool GetPosition(Vector2D *outputVector);
	void Update();
	void Reset(Vector2D position);

private:
	Vector2D position;
	Vector2D target;
	double z;

	// 1€ Filter methods
	Vector2D OneEuroFilter(Vector2D x, double dt, double minCutoff, double beta, double dCutoff);
	void OneEuroReset(Vector2D pos);
	double CalculateAlpha(double dt, double fc);
};
