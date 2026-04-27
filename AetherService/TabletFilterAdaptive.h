#pragma once

#include "Vector2D.h"
#include "TabletFilter.h"

class TabletFilterAdaptive : public TabletFilter {
public:

	// State vector: [x, y, vx, vy]
	double state[4];

	// Error covariance matrix P (4x4)
	double P[4][4];

	// Process noise covariance Q (4x4 diagonal)
	double processNoise;        // Q scalar for position
	double processNoiseVelocity; // Q scalar for velocity

	// Measurement noise covariance R (2x2 diagonal)
	double measurementNoise;    // R scalar

	// Velocity model weight (how much the velocity model is trusted)
	double velocityWeight;

	// Last update timestamp
	double lastTimestamp;
	bool hasInitialized;

	// Output
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
