#include "stdafx.h"
#include "TabletFilterReconstructor.h"
#include <chrono>

#define LOG_MODULE "Reconstructor"
#include "Logger.h"




TabletFilterReconstructor::TabletFilterReconstructor() {
	reconstructionStrength = 0.5;
	velocitySmoothing = 0.6;
	accelerationCap = 50.0;
	predictionTimeMs = 5.0;

	historyIndex = 0;
	historyCount = 0;
	lastTimestamp = 0;

	velocity.Set(0, 0);
	acceleration.Set(0, 0);
	smoothedVelocity.Set(0, 0);
}




TabletFilterReconstructor::~TabletFilterReconstructor() {
}





double TabletFilterReconstructor::GetCurrentTimeMs() {
	static LARGE_INTEGER freq = {};
	if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	return (double)now.QuadPart / (double)freq.QuadPart * 1000.0;
}




void TabletFilterReconstructor::AddToHistory(Vector2D pos, double time) {
	history[historyIndex].position.Set(pos);
	history[historyIndex].timestamp = time;
	historyIndex = (historyIndex + 1) % HISTORY_SIZE;
	if (historyCount < HISTORY_SIZE) {
		historyCount++;
	}
}




void TabletFilterReconstructor::EstimateVelocityAndAcceleration() {
	if (historyCount < 3) return;

	
	int idx0 = (historyIndex - 1 + HISTORY_SIZE) % HISTORY_SIZE; 
	int idx1 = (historyIndex - 2 + HISTORY_SIZE) % HISTORY_SIZE; 
	int idx2 = (historyIndex - 3 + HISTORY_SIZE) % HISTORY_SIZE; 

	Vector2D &p0 = history[idx0].position;
	Vector2D &p1 = history[idx1].position;
	Vector2D &p2 = history[idx2].position;

	double t0 = history[idx0].timestamp;
	double t1 = history[idx1].timestamp;
	double t2 = history[idx2].timestamp;

	double dt01 = t0 - t1;
	double dt12 = t1 - t2;

	
	if (dt01 < 0.001) dt01 = 0.001;
	if (dt12 < 0.001) dt12 = 0.001;

	
	Vector2D rawVelocity;
	rawVelocity.x = (p0.x - p1.x) / dt01;
	rawVelocity.y = (p0.y - p1.y) / dt01;

	
	Vector2D prevVelocity;
	prevVelocity.x = (p1.x - p2.x) / dt12;
	prevVelocity.y = (p1.y - p2.y) / dt12;

	
	double alpha = 1.0 - velocitySmoothing;
	smoothedVelocity.x = smoothedVelocity.x * velocitySmoothing + rawVelocity.x * alpha;
	smoothedVelocity.y = smoothedVelocity.y * velocitySmoothing + rawVelocity.y * alpha;

	
	double dtAvg = (dt01 + dt12) * 0.5;
	if (dtAvg < 0.001) dtAvg = 0.001;
	acceleration.x = (rawVelocity.x - prevVelocity.x) / dtAvg;
	acceleration.y = (rawVelocity.y - prevVelocity.y) / dtAvg;

	
	double accelMag = acceleration.Length();
	if (accelMag > accelerationCap) {
		double scale = accelerationCap / accelMag;
		acceleration.x *= scale;
		acceleration.y *= scale;
	}
}





void TabletFilterReconstructor::Reset(Vector2D pos) {
	position.Set(pos);
	target.Set(pos);
	velocity.Set(0, 0);
	acceleration.Set(0, 0);
	smoothedVelocity.Set(0, 0);
	historyIndex = 0;
	historyCount = 0;
	lastTimestamp = 0;
}

void TabletFilterReconstructor::SetTarget(Vector2D vector, double h) {
	target.Set(vector);

	double now = GetCurrentTimeMs();
	AddToHistory(vector, now);
	EstimateVelocityAndAcceleration();
	lastTimestamp = now;
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
	if (historyCount < 3 || reconstructionStrength <= 0.001) {
		
		position.Set(target);
		return;
	}

	
	
	double dt = predictionTimeMs * reconstructionStrength;

	double correctedX = target.x + smoothedVelocity.x * dt + 0.5 * acceleration.x * dt * dt * reconstructionStrength;
	double correctedY = target.y + smoothedVelocity.y * dt + 0.5 * acceleration.y * dt * dt * reconstructionStrength;

	
	
	double velMag = smoothedVelocity.Length();
	double maxCorrection = velMag * predictionTimeMs * 2.0;
	if (maxCorrection < 0.01) maxCorrection = 0.01;
	if (maxCorrection > 5.0) maxCorrection = 5.0; 

	double dx = correctedX - target.x;
	double dy = correctedY - target.y;
	double correctionDist = sqrt(dx * dx + dy * dy);

	if (correctionDist > maxCorrection) {
		double scale = maxCorrection / correctionDist;
		correctedX = target.x + dx * scale;
		correctedY = target.y + dy * scale;
	}

	position.x = correctedX;
	position.y = correctedY;
}
