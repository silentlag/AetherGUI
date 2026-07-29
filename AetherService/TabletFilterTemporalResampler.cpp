#include "stdafx.h"
#include "TabletFilterTemporalResampler.h"
#include "Platform.h"
#include <cmath>

#define LOG_MODULE "TemporalResampler"
#include "Logger.h"

TabletFilterTemporalResampler::TabletFilterTemporalResampler() {
	predictionRatio = 0.5;
	followRadius = 0.0;
	smoothingLatency = 0.0;
	reverseEma = 1.0;
	extraFrames = true;

	rpsAvg = 200.0;
	tOffset = 0.0;
	lastReportTimeS = 0.0;
	lastConsumeTimeS = 0.0;
	hasPrevReport = false;

	pressure = 0.0;
	sPressure = 0.0;

	kfInitialized = false;
}

TabletFilterTemporalResampler::~TabletFilterTemporalResampler() {
}

double TabletFilterTemporalResampler::GetCurrentTimeS() {
	return (double)platform::MonotonicNs() / 1.0e9;
}

void TabletFilterTemporalResampler::Reset(Vector2D pos) {
	position.Set(pos);
	target.Set(pos);
	pressure = 0.0;
	ResetValues(pos, 0.0);
}

void TabletFilterTemporalResampler::ResetValues(Vector2D p0, double p) {
	for (int i = 0; i < 3; i++) {
		smoothedPoints[i].Set(p0);
		stablePointsPos[i].Set(p0);
		stablePointsPressure[i] = p;
	}
	bE.Set(p0);
	bP.Set(p0);
	sC.Set(p0);
	sPressure = p;
	rpsAvg = 200.0;
	tOffset = 0.0;
	hasPrevReport = false;
	KfReset(p0);
	lastReportTimeS = GetCurrentTimeS();
}

static inline void Kalman1D(
	double& x0, double& x1, double& x2, double& x3,
	double P[4][4],
	double measuredPos, double measuredVel, double dt,
	double Q, double R) {

	double dt2 = dt * dt;
	double dt3 = dt2 * dt;

	double A[4][4] = {
		{1,  dt,        0.5*dt2,   dt3/6.0},
		{0,  1,         dt,        0.5*dt2},
		{0,  0,         1,         dt     },
		{0,  0,         0,         1      }
	};

	double px0 = x0 + x1*dt + 0.5*x2*dt2 + x3*dt3/6.0;
	double px1 = x1 + x2*dt + 0.5*x3*dt2;
	double px2 = x2 + x3*dt;
	double px3 = x3;

	double AP[4][4];
	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++) {
			double s = 0;
			for (int k = 0; k < 4; k++) s += A[i][k] * P[k][j];
			AP[i][j] = s;
		}
	double newP[4][4];
	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++) {
			double s = 0;
			for (int k = 0; k < 4; k++) s += AP[i][k] * A[j][k];
			newP[i][j] = s;
		}
	
	newP[0][0] += Q; newP[1][1] += Q; newP[2][2] += Q; newP[3][3] += Q;

	double innov0 = measuredPos - px0;
	double innov1 = measuredVel - px1;

	double S00 = newP[0][0] + R;
	double S01 = newP[0][1];
	double S11 = newP[1][1] + R;

	double detS = S00 * S11 - S01 * S01;
	if (std::abs(detS) < 1e-18) detS = 1e-18;
	double invS00 =  S11 / detS;
	double invS01 = -S01  / detS;
	double invS11 =  S00  / detS;

	double K[4][2];
	for (int i = 0; i < 4; i++) {
		K[i][0] = newP[i][0] * invS00 + newP[i][1] * invS01;
		K[i][1] = newP[i][0] * invS01 + newP[i][1] * invS11;
	}

	x0 = px0 + K[0][0]*innov0 + K[0][1]*innov1;
	x1 = px1 + K[1][0]*innov0 + K[1][1]*innov1;
	x2 = px2 + K[2][0]*innov0 + K[2][1]*innov1;
	x3 = px3 + K[3][0]*innov0 + K[3][1]*innov1;

	for (int i = 0; i < 4; i++) {
		double kh0 = K[i][0];
		double kh1 = K[i][1];
		for (int j = 0; j < 4; j++) {
			newP[i][j] = newP[i][j] - kh0 * newP[0][j] - kh1 * newP[1][j];
		}
	}

	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++)
			P[i][j] = newP[i][j];
}

void TabletFilterTemporalResampler::KfReset(Vector2D pos) {
	for (int i = 0; i < 4; i++) {
		kfX[i].Set(0, 0);
		for (int j = 0; j < 4; j++) { kfPx[i][j] = 0.0; kfPy[i][j] = 0.0; }
	}
	kfX[0].Set(pos);
	for (int i = 0; i < 4; i++) { kfPx[i][i] = 1.0; kfPy[i][i] = 1.0; }
	kfLastMeasured.Set(pos);
	kfInitialized = true;
}

Vector2D TabletFilterTemporalResampler::KfUpdate(Vector2D measured, double dt) {
	if (!kfInitialized) {
		KfReset(measured);
		return measured;
	}
	if (dt <= 0.0 || dt > 0.1) dt = 0.001;

	double measuredVelX = (measured.x - kfLastMeasured.x) / dt;
	double measuredVelY = (measured.y - kfLastMeasured.y) / dt;

	double predX = kfX[0].x + kfX[1].x * dt;
	double predY = kfX[0].y + kfX[1].y * dt;
	double innovDistX = measured.x - predX;
	double innovDistY = measured.y - predY;
	double innovMag = sqrt(innovDistX * innovDistX + innovDistY * innovDistY);

	double speed = sqrt(measuredVelX * measuredVelX + measuredVelY * measuredVelY);

	kfLastMeasured.Set(measured);

	double Q = 0.5 + 2.5 * predictionRatio;
	double R = 0.0002 + 0.02 / (1.0 + speed * 0.5);

	double scale = 1.0;
	if (innovMag > 0.5) {
		scale = 0.5 / innovMag;
		measured.x = predX + innovDistX * scale;
		measured.y = predY + innovDistY * scale;
		measuredVelX *= scale;
		measuredVelY *= scale;
		R *= 4.0;
	}

	double Px[4][4], Py[4][4];
	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++) {
			Px[i][j] = kfPx[i][j];
			Py[i][j] = kfPy[i][j];
		}

	Kalman1D(kfX[0].x, kfX[1].x, kfX[2].x, kfX[3].x, Px, measured.x, measuredVelX, dt, Q, R);
	Kalman1D(kfX[0].y, kfX[1].y, kfX[2].y, kfX[3].y, Py, measured.y, measuredVelY, dt, Q, R);

	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++) {
			kfPx[i][j] = Px[i][j];
			kfPy[i][j] = Py[i][j];
		}

	Vector2D result;
	result.x = kfX[0].x;
	result.y = kfX[0].y;
	return result;
}

Vector2D TabletFilterTemporalResampler::Trajectory(double t, Vector2D v3, Vector2D v2, Vector2D v1) {
	Vector2D mid;
	mid.x = 0.5 * (v1.x + v3.x);
	mid.y = 0.5 * (v1.y + v3.y);
	Vector2D accel;
	accel.x = 2.0 * (mid.x - v2.x);
	accel.y = 2.0 * (mid.y - v2.y);
	Vector2D vel;
	vel.x = 2.0 * v2.x - v3.x - mid.x;
	vel.y = 2.0 * v2.y - v3.y - mid.y;

	double accelMag = accel.x * accel.x + accel.y * accel.y;
	if (accelMag > 0.001) {
		int floorVal = (int)floor(t);
		Vector2D _vel;
		_vel.x = vel.x + accel.x * floorVal;
		_vel.y = vel.y + accel.y * floorVal;

		const int steps = 256;
		const double dtStep = 1.0 / steps;
		double arcArr[steps];
		double arcTar = 0.0;
		for (int s = 0; s < steps; s++) {
			arcArr[s] = arcTar;
			double vx = _vel.x + s * dtStep * accel.x;
			double vy = _vel.y + s * dtStep * accel.y;
			arcTar += sqrt(vx * vx + vy * vy);
		}

		double target = arcTar * (t - floorVal);
		for (int s = 0; s < steps; s++) {
			if (arcArr[s] >= target) {
				t = s * dtStep + floorVal;
				break;
			}
		}
	}

	Vector2D result;
	result.x = v3.x + t * vel.x + 0.5 * t * t * accel.x;
	result.y = v3.y + t * vel.y + 0.5 * t * t * accel.y;
	return result;
}

void TabletFilterTemporalResampler::SetTarget(Vector2D vector, double h) {
	target.Set(vector);
	pressure = h;
}

void TabletFilterTemporalResampler::SetPosition(Vector2D vector, double h) {
	position.Set(vector);
}

bool TabletFilterTemporalResampler::GetPosition(Vector2D *outputVector) {
	outputVector->x = position.x;
	outputVector->y = position.y;
	return true;
}

void TabletFilterTemporalResampler::Update() {
	double nowS = GetCurrentTimeS();

	double consumeDelta;
	if (!hasPrevReport) {
		consumeDelta = 1.0 / rpsAvg;
		lastConsumeTimeS = nowS;
		hasPrevReport = true;
	} else {
		consumeDelta = nowS - lastConsumeTimeS;
		lastConsumeTimeS = nowS;
	}

	if (consumeDelta < 0.0001) {
		return;
	}
	if (consumeDelta > 0.03) {
		consumeDelta = 1.0 / rpsAvg;
		lastConsumeTimeS = nowS - consumeDelta;
	}

	Vector2D real = target;

	double alpha = 1.0 - exp(-2.0 * consumeDelta);
	rpsAvg += (1.0 / consumeDelta - rpsAvg) * alpha;
	double secAvg = 1.0 / rpsAvg;
	double msAvg = 1000.0 * secAvg;

	Vector2D smoothed = real;
	if (reverseEma < 1.0) {
		double r = reverseEma;
		if (r < 0.2) r = 0.2;
		double gain = (1.0 - r) / r;
		if (gain > 4.0) gain = 4.0;
		Vector2D prev;
		prev.x = smoothed.x - bE.x;
		prev.y = smoothed.y - bE.y;
		double dist = sqrt(prev.x * prev.x + prev.y * prev.y);
		double cap = 0.5;
		if (dist * gain > cap) gain = cap / dist;
		smoothed.x = bE.x + prev.x * (1.0 + gain);
		smoothed.y = bE.y + prev.y * (1.0 + gain);
	}
	bE.Set(real);

	if (smoothingLatency > 0.0) {
		double sw = exp(msAvg / -smoothingLatency);
		smoothed.x += (sC.x - smoothed.x) * sw;
		smoothed.y += (sC.y - smoothed.y) * sw;
		pressure += (sPressure - pressure) * sw;
	}
	sC.Set(smoothed);
	sPressure = pressure;

	for (int i = 2; i > 0; i--) smoothedPoints[i] = smoothedPoints[i-1];
	smoothedPoints[0] = smoothed;

	Vector2D predict = smoothedPoints[0];
	if (predictionRatio > 0.0) {
		predict = KfUpdate(smoothedPoints[0], secAvg);

		double followUnits = followRadius;
		double predictCoe = 1.0;
		if (followUnits > 0.0) {
			double dx = smoothedPoints[0].x - smoothedPoints[2].x;
			double dy = smoothedPoints[0].y - smoothedPoints[2].y;
			double dist = sqrt(dx*dx + dy*dy);
			predictCoe = dist / followUnits - 1.0;
			if (predictCoe < 0.0) predictCoe = 0.0;
			if (predictCoe > 1.0) predictCoe = 1.0;
		}
		double sw = (smoothingLatency > 0.0) ? exp(msAvg / -smoothingLatency) : 0.0;
		predictCoe += (1.0 - predictCoe) * sw;

		double blend = 1.0 - predictCoe * predictionRatio;
		predict.x += (smoothedPoints[0].x - predict.x) * blend;
		predict.y += (smoothedPoints[0].y - predict.y) * blend;
	}

	if (followRadius > 0.0) {
		double dx = predict.x - bP.x;
		double dy = predict.y - bP.y;
		double travel = sqrt(dx*dx + dy*dy);
		if (travel > 0.0) {
			double k = travel / followRadius - 1.0;
			if (k < 0.0) k = 0.0;
			if (k > 1.0) k = 1.0;
			predict.x = bP.x + dx * k;
			predict.y = bP.y + dy * k;
		}
	}

	tOffset += secAvg - consumeDelta;
	tOffset *= exp(-5.0 * consumeDelta);
	if (tOffset > secAvg) tOffset = secAvg;
	if (tOffset < -secAvg) tOffset = -secAvg;

	lastReportTimeS = nowS + tOffset;

	for (int i = 2; i > 0; i--) {
		stablePointsPos[i] = stablePointsPos[i-1];
		stablePointsPressure[i] = stablePointsPressure[i-1];
	}
	stablePointsPos[0] = predict;
	stablePointsPressure[0] = pressure;

	bP.Set(predict);

	double t = 1.0 + (nowS - lastReportTimeS) * rpsAvg;
	if (t < 0.0) t = 0.0;
	if (t > 3.0) t = 3.0;

	Vector2D interp = Trajectory(t, stablePointsPos[2], stablePointsPos[1], stablePointsPos[0]);
	position.Set(interp);
}
