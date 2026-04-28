#pragma once

#include "Vector2D.h"
#include "TabletFilter.h"

class TabletFilterReconstructor : public TabletFilter {
public:

	
	static const int HISTORY_SIZE = 16;
	struct HistoryEntry {
		Vector2D position;
		double timestamp;
	};
	HistoryEntry history[HISTORY_SIZE];
	int historyIndex;
	int historyCount;

	
	Vector2D velocity;
	Vector2D acceleration;
	Vector2D smoothedVelocity;

	
	Vector2D position;
	Vector2D target;

	
	double reconstructionStrength;   
	double velocitySmoothing;        
	double accelerationCap;          
	double predictionTimeMs;         

	
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
