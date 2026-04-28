#include "stdafx.h"
#include "TabletFilterAdaptive.h"
#include <chrono>
#include <cstring>

#define LOG_MODULE "Adaptive"
#include "Logger.h"




TabletFilterAdaptive::TabletFilterAdaptive() {
	
	processNoise = 0.02;         
	processNoiseVelocity = 0.1;  
	measurementNoise = 0.5;      
	velocityWeight = 1.0;

	hasInitialized = false;
	lastTimestamp = 0;

	
	memset(state, 0, sizeof(state));

	
	memset(P, 0, sizeof(P));
	for (int i = 0; i < 4; i++) {
		P[i][i] = 1000.0;
	}
}




TabletFilterAdaptive::~TabletFilterAdaptive() {
}




double TabletFilterAdaptive::GetCurrentTimeMs() {
	auto now = std::chrono::high_resolution_clock::now();
	auto duration = now.time_since_epoch();
	return std::chrono::duration_cast<std::chrono::microseconds>(duration).count() / 1000.0;
}




void TabletFilterAdaptive::InitState(double x, double y) {
	state[0] = x;    
	state[1] = y;    
	state[2] = 0.0;  
	state[3] = 0.0;  

	memset(P, 0, sizeof(P));
	for (int i = 0; i < 4; i++) {
		P[i][i] = 1000.0;
	}

	hasInitialized = true;
}









void TabletFilterAdaptive::Predict(double dt) {
	double vw = velocityWeight;

	
	state[0] += state[2] * dt * vw;
	state[1] += state[3] * dt * vw;
	

	
	
	double dt2 = dt * dt * vw * vw;

	
	
	P[0][0] += 2.0 * dt * vw * P[0][2] + dt2 * P[2][2] + processNoise;
	P[0][1] += dt * vw * (P[0][3] + P[2][1]) + dt2 * P[2][3];
	P[0][2] += dt * vw * P[2][2];
	P[0][3] += dt * vw * P[2][3];

	P[1][0] += dt * vw * (P[1][2] + P[0][3]) + dt2 * P[2][3]; 
	P[1][1] += 2.0 * dt * vw * P[1][3] + dt2 * P[3][3] + processNoise;
	P[1][2] += dt * vw * P[3][2];
	P[1][3] += dt * vw * P[3][3];

	P[2][0] += dt * vw * P[2][2];
	P[2][1] += dt * vw * P[2][3];
	P[2][2] += processNoiseVelocity;
	

	P[3][0] += dt * vw * P[3][2];
	P[3][1] += dt * vw * P[3][3];
	
	P[3][3] += processNoiseVelocity;
}








void TabletFilterAdaptive::UpdateMeasurement(double mx, double my) {
	
	double innovX = mx - state[0];
	double innovY = my - state[1];

	
	double S00 = P[0][0] + measurementNoise;
	double S01 = P[0][1];
	double S10 = P[1][0];
	double S11 = P[1][1] + measurementNoise;

	
	double det = S00 * S11 - S01 * S10;
	if (fabs(det) < 1e-12) det = 1e-12; 

	double invDet = 1.0 / det;
	double Si00 = S11 * invDet;
	double Si01 = -S01 * invDet;
	double Si10 = -S10 * invDet;
	double Si11 = S00 * invDet;

	
	
	
	double K[4][2];
	for (int i = 0; i < 4; i++) {
		K[i][0] = P[i][0] * Si00 + P[i][1] * Si10;
		K[i][1] = P[i][0] * Si01 + P[i][1] * Si11;
	}

	
	for (int i = 0; i < 4; i++) {
		state[i] += K[i][0] * innovX + K[i][1] * innovY;
	}

	
	
	double Pnew[4][4];
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			double kh = 0;
			if (j < 2) {
				kh = K[i][j]; 
			}
			double identity = (i == j) ? 1.0 : 0.0;
			Pnew[i][j] = 0;
			for (int k = 0; k < 4; k++) {
				double ikh = (i == k) ? 1.0 : 0.0;
				if (k < 2) ikh -= K[i][k];
				Pnew[i][j] += ikh * P[k][j];
			}
		}
	}
	memcpy(P, Pnew, sizeof(P));
}





void TabletFilterAdaptive::Reset(Vector2D pos) {
	InitState(pos.x, pos.y);
	position.Set(pos);
	target.Set(pos);
	lastTimestamp = GetCurrentTimeMs();
}

void TabletFilterAdaptive::SetTarget(Vector2D vector, double h) {
	target.Set(vector);
}

void TabletFilterAdaptive::SetPosition(Vector2D vector, double h) {
	position.Set(vector);
}

bool TabletFilterAdaptive::GetPosition(Vector2D *outputVector) {
	outputVector->x = position.x;
	outputVector->y = position.y;
	return true;
}

void TabletFilterAdaptive::Update() {
	double now = GetCurrentTimeMs();

	if (!hasInitialized) {
		InitState(target.x, target.y);
		lastTimestamp = now;
		position.Set(target);
		return;
	}

	double dt = now - lastTimestamp;
	if (dt <= 0) dt = 1.0; 
	if (dt > 100.0) {
		
		Reset(target);
		return;
	}

	lastTimestamp = now;

	
	
	double savedProcessNoise = processNoise;
	double savedProcessNoiseVel = processNoiseVelocity;

	
	double predX = state[0] + state[2] * dt * velocityWeight;
	double predY = state[1] + state[3] * dt * velocityWeight;
	double innovX = target.x - predX;
	double innovY = target.y - predY;
	double innovMag = sqrt(innovX * innovX + innovY * innovY);

	
	
	double innovThreshold = measurementNoise * 2.0;
	if (innovMag > innovThreshold) {
		double boost = 1.0 + (innovMag - innovThreshold) * 0.5;
		if (boost > 10.0) boost = 10.0;
		processNoise *= boost;
		processNoiseVelocity *= boost;
	}

	
	Predict(dt);

	
	processNoise = savedProcessNoise;
	processNoiseVelocity = savedProcessNoiseVel;

	
	UpdateMeasurement(target.x, target.y);

	
	position.x = state[0];
	position.y = state[1];
}
