#pragma once

#include "Vector2D.h"
#include "TabletFilter.h"

class TabletFilterAdaptive : public TabletFilter {
public:

	
	double state[4];

	
	double P[4][4];

	
	double processNoise;        
	double processNoiseVelocity; 

	
	double measurementNoise;    

	
	double velocityWeight;

	
	double lastTimestamp;
	bool hasInitialized;

	
	Vector2D position;
	Vector2D target;

	TabletFilterAdaptive();
	~TabletFilterAdaptive();

	void SetTarget(Vector2D vector, double h);
	void SetPosition(Vector2D vector, double h);
	bool GetPosition(Vector2D *outputVector);
	void Update();
	void Reset(Vector2D position);

	double GetCurrentTimeMs();
	void Predict(double dt);
	void UpdateMeasurement(double mx, double my);
	void InitState(double x, double y);
};
