#pragma once

#include "Vector2D.h"
#include "TabletFilter.h"

class TabletFilterReconstructor : public TabletFilter {
public:

	// History buffer for position reconstruction
	static const int HISTORY_SIZE = 16;
	struct HistoryEntry {
		Vector2D position;
		double timestamp;
	};
	HistoryEntry history[HISTORY_SIZE];
	int historyIndex;
	int historyCount;

	// Estimated velocity and acceleration
	Vector2D velocity;
	Vector2D acceleration;
	Vector2D smoothedVelocity;

	// Output
	Vector2D position;
	Vector2D target;

	// Parameters
	double reconstructionStrength;   // 0.0 - 2.0, how aggressively to compensate latency
	double velocitySmoothing;        // 0.0 - 1.0, smoothing applied to velocity estimation
	double accelerationCap;          // max acceleration magnitude (prevents overshot on direction changes)
	double predictionTimeMs;         // how far ahead to extrapolate (ms)

	// Timing
	double lastTimestamp;

	TabletFilterReconstructor();
	~TabletFilterReconstructor();

	void SetTarget(Vector2D vector, double h);
	void SetPosition(Vector2D vector, double h);
	bool GetPosition(Vector2D *outputVector);
	void Update();
	void Reset(Vector2D position);

	double GetCurrentTimeMs();
	void AddToHistory(Vector2D pos, double time);
	void EstimateVelocityAndAcceleration();
};
