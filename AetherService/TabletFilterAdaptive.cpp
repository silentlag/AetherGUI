#include "stdafx.h"
#include "TabletFilterAdaptive.h"
#include <chrono>
#include <cstring>

#define LOG_MODULE "Adaptive"
#include "Logger.h"

//
// Constructor
//
TabletFilterAdaptive::TabletFilterAdaptive() {
	// Default parameters — tuned for typical tablet use
	processNoise = 0.02;         // Low = smoother, High = more responsive
	processNoiseVelocity = 0.1;  // Velocity model noise
	measurementNoise = 0.5;      // Low = trust sensor more, High = trust model more
	velocityWeight = 1.0;

	hasInitialized = false;
	lastTimestamp = 0;

	// Zero state
	memset(state, 0, sizeof(state));

	// Initialize P to identity * large value (high initial uncertainty)
	memset(P, 0, sizeof(P));
	for (int i = 0; i < 4; i++) {
		P[i][i] = 1000.0;
	}
}

//
// Destructor
//
TabletFilterAdaptive::~TabletFilterAdaptive() {
}

//
// Get current time in milliseconds
//
double TabletFilterAdaptive::GetCurrentTimeMs() {
	auto now = std::chrono::high_resolution_clock::now();
	auto duration = now.time_since_epoch();
	return std::chrono::duration_cast<std::chrono::microseconds>(duration).count() / 1000.0;
}

//
// Initialize state with first measurement
//
void TabletFilterAdaptive::InitState(double x, double y) {
	state[0] = x;    // x position
	state[1] = y;    // y position
	state[2] = 0.0;  // x velocity
	state[3] = 0.0;  // y velocity

	memset(P, 0, sizeof(P));
	for (int i = 0; i < 4; i++) {
		P[i][i] = 1000.0;
	}

	hasInitialized = true;
}

//
// Predict step: x_pred = F * x, P_pred = F * P * F' + Q
// State transition matrix F (constant velocity model):
// [1  0  dt  0 ]
// [0  1  0   dt]
// [0  0  1   0 ]
// [0  0  0   1 ]
//
void TabletFilterAdaptive::Predict(double dt) {
	double vw = velocityWeight;

	// Predict state
	state[0] += state[2] * dt * vw;
	state[1] += state[3] * dt * vw;
	// Velocity stays the same in constant velocity model

	// Predict covariance P = F * P * F' + Q
	// Since F is sparse, we can do this efficiently
	double dt2 = dt * dt * vw * vw;

	// Update P with state transition
	// P[0][0] += 2*dt*P[0][2] + dt^2*P[2][2] + Q_pos
	P[0][0] += 2.0 * dt * vw * P[0][2] + dt2 * P[2][2] + processNoise;
	P[0][1] += dt * vw * (P[0][3] + P[2][1]) + dt2 * P[2][3];
	P[0][2] += dt * vw * P[2][2];
	P[0][3] += dt * vw * P[2][3];

	P[1][0] += dt * vw * (P[1][2] + P[0][3]) + dt2 * P[2][3]; // symmetric fix
	P[1][1] += 2.0 * dt * vw * P[1][3] + dt2 * P[3][3] + processNoise;
	P[1][2] += dt * vw * P[3][2];
	P[1][3] += dt * vw * P[3][3];

	P[2][0] += dt * vw * P[2][2];
	P[2][1] += dt * vw * P[2][3];
	P[2][2] += processNoiseVelocity;
	// P[2][3] unchanged

	P[3][0] += dt * vw * P[3][2];
	P[3][1] += dt * vw * P[3][3];
	// P[3][2] unchanged
	P[3][3] += processNoiseVelocity;
}

//
// Update step with measurement
// Measurement matrix H:
// [1 0 0 0]
// [0 1 0 0]
// (we only measure position, not velocity)
//
void TabletFilterAdaptive::UpdateMeasurement(double mx, double my) {
	// Innovation (measurement residual): y = z - H * x_pred
	double innovX = mx - state[0];
	double innovY = my - state[1];

	// Innovation covariance: S = H * P * H' + R
	double S00 = P[0][0] + measurementNoise;
	double S01 = P[0][1];
	double S10 = P[1][0];
	double S11 = P[1][1] + measurementNoise;

	// Inverse of 2x2 matrix S
	double det = S00 * S11 - S01 * S10;
	if (fabs(det) < 1e-12) det = 1e-12; // prevent division by zero

	double invDet = 1.0 / det;
	double Si00 = S11 * invDet;
	double Si01 = -S01 * invDet;
	double Si10 = -S10 * invDet;
	double Si11 = S00 * invDet;

	// Adaptive gain: K = P * H' * S^-1
	// K is 4x2 matrix, H' is 4x2 = [[1,0],[0,1],[0,0],[0,0]]
	// So K[i][j] = P[i][0]*Si[0][j] + P[i][1]*Si[1][j]
	double K[4][2];
	for (int i = 0; i < 4; i++) {
		K[i][0] = P[i][0] * Si00 + P[i][1] * Si10;
		K[i][1] = P[i][0] * Si01 + P[i][1] * Si11;
	}

	// Update state: x = x + K * innovation
	for (int i = 0; i < 4; i++) {
		state[i] += K[i][0] * innovX + K[i][1] * innovY;
	}

	// Update covariance: P = (I - K * H) * P
	// Since H = [[1,0,0,0],[0,1,0,0]], K*H is 4x4 where (K*H)[i][j] = K[i][0]*(j==0) + K[i][1]*(j==1)
	double Pnew[4][4];
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			double kh = 0;
			if (j < 2) {
				kh = K[i][j]; // K[i][0] for j=0, K[i][1] for j=1
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

//
// TabletFilter interface
//

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
	if (dt <= 0) dt = 1.0; // fallback to 1ms
	if (dt > 100.0) {
		// Too long without update — reset
		Reset(target);
		return;
	}

	lastTimestamp = now;

	// Adaptive process noise: detect sudden direction changes (flicks)
	// by comparing predicted velocity direction with measurement innovation
	double savedProcessNoise = processNoise;
	double savedProcessNoiseVel = processNoiseVelocity;

	// Innovation before prediction
	double predX = state[0] + state[2] * dt * velocityWeight;
	double predY = state[1] + state[3] * dt * velocityWeight;
	double innovX = target.x - predX;
	double innovY = target.y - predY;
	double innovMag = sqrt(innovX * innovX + innovY * innovY);

	// If innovation is large relative to expected noise, temporarily boost Q
	// This helps with fast flicks in osu! where the model lags behind
	double innovThreshold = measurementNoise * 2.0;
	if (innovMag > innovThreshold) {
		double boost = 1.0 + (innovMag - innovThreshold) * 0.5;
		if (boost > 10.0) boost = 10.0;
		processNoise *= boost;
		processNoiseVelocity *= boost;
	}

	// Predict step
	Predict(dt);

	// Restore original noise values
	processNoise = savedProcessNoise;
	processNoiseVelocity = savedProcessNoiseVel;

	// Update step with new measurement
	UpdateMeasurement(target.x, target.y);

	// Output the estimated position
	position.x = state[0];
	position.y = state[1];
}
