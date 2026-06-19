#pragma once

#include "Vector2D.h"
#include "TabletFilter.h"

class TabletFilterJitterStabilizer : public TabletFilter {
public:

	double radius;

	double releaseSpeed;

	TabletFilterJitterStabilizer();
	~TabletFilterJitterStabilizer();

	void SetTarget(Vector2D vector, double h) override;
	void SetPosition(Vector2D vector, double h) override;
	bool GetPosition(Vector2D *outputVector) override;
	void Update() override;
	void Reset(Vector2D position) override;

private:
	Vector2D position;
	Vector2D target;
	Vector2D latched;
	bool     hasLatch;
};
