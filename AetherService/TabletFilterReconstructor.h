#pragma once

#include "Vector2D.h"
#include "TabletFilter.h"

class TabletFilterReconstructor : public TabletFilter {
public:

	double reconstructionStrength;

	double velocitySmoothing;

	bool useInverseEma;
	double emaWeight;

	Vector2D position;
	Vector2D target;

	Vector2D prevTarget;
	double smoothedSpeed;
	bool isFirstReport;
	double lastTimestamp;

	TabletFilterReconstructor();
	~TabletFilterReconstructor();

	void SetTarget(Vector2D vector, double h);
	void SetPosition(Vector2D vector, double h);
	bool GetPosition(Vector2D *outputVector);
	void Update();
	void Reset(Vector2D position);

	double GetCurrentTimeMs();
};
