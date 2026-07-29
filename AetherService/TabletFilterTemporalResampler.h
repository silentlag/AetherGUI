#pragma once

#include "Vector2D.h"
#include "TabletFilter.h"
#include <cmath>

class TabletFilterTemporalResampler : public TabletFilter {
public:

	double predictionRatio;
	double followRadius;
	double smoothingLatency;
	double reverseEma;
	bool   extraFrames;

	TabletFilterTemporalResampler();
	~TabletFilterTemporalResampler();

	void SetTarget(Vector2D vector, double h) override;
	void SetPosition(Vector2D vector, double h) override;
	bool GetPosition(Vector2D *outputVector) override;
	void Update() override;
	void Reset(Vector2D position) override;

private:

	Vector2D position;
	Vector2D target;
	double   pressure;

	double   rpsAvg;
	double   tOffset;
	double   lastReportTimeS;
	double   lastConsumeTimeS;
	bool     hasPrevReport;

	Vector2D bE;
	Vector2D bP;
	Vector2D sC;
	double   sPressure;

	Vector2D smoothedPoints[3];
	Vector2D stablePointsPos[3];
	double   stablePointsPressure[3];

	Vector2D kfX[4];
	double   kfPx[4][4];
	double   kfPy[4][4];
	Vector2D kfLastMeasured;
	bool     kfInitialized;

	void   KfReset(Vector2D pos);
	Vector2D KfUpdate(Vector2D measured, double dt);

	Vector2D Trajectory(double t, Vector2D v3, Vector2D v2, Vector2D v1);

	void   ResetValues(Vector2D p0, double p);

	double  GetCurrentTimeS();
};
