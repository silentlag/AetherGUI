#pragma once

#define _USE_MATH_DEFINES
#include <cmath>
#include <math.h>
#include <chrono>

#include "Vector2D.h"
#include "TabletFilter.h"

class TabletFilterAetherSmooth : public TabletFilter {
public:

	
	Vector2D xPrev;
	Vector2D dxPrev;
	bool flowFirstTime;

	
	Vector2D lastPos;
	Vector2D lastAntismoothPos;
	Vector2D lastSmoothedPos;
	Vector2D prevProcessedPos;
	Vector2D debouncePos;
	bool isFirstReport;
	double velocity;

	
	std::chrono::high_resolution_clock::time_point lastTime;
	std::chrono::high_resolution_clock::time_point debounceTime;

	

	
	bool enableAntismoothing;
	double antismoothing;

	
	bool enableSmoothing;
	double stability;        
	double speedSensitivity; 

	
	bool enableRadialFollow;
	double radialInner;
	double radialOuter;

	
	bool enableDebounce;
	double debounceMs;

	
	TabletFilterAetherSmooth();
	~TabletFilterAetherSmooth();

	
	void SetTarget(Vector2D vector, double h);
	void SetPosition(Vector2D vector, double h);
	bool GetPosition(Vector2D *outputVector);
	void Update();
	void Reset(Vector2D position);

private:
	Vector2D position;
	Vector2D target;
	double z;

	
	Vector2D AdaptiveFlow(Vector2D x, double dt, double minCutoff, double beta, double dCutoff);
	void AdaptiveFlowReset(Vector2D pos);
	double CalculateAlpha(double dt, double fc);
};
