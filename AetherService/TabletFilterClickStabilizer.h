#pragma once

#include <chrono>

#include "Vector2D.h"
#include "TabletFilter.h"

class TabletFilterClickStabilizer : public TabletFilter {
public:
	bool   enabled;
	double clickStabilizeMs;

	TabletFilterClickStabilizer();
	~TabletFilterClickStabilizer();

	void SetTarget(Vector2D vector, double h) override;
	void SetPosition(Vector2D vector, double h) override;
	bool GetPosition(Vector2D *outputVector) override;
	void SetReportState(BYTE buttons, double pressure, double hoverDistance) override;
	void Update() override;
	void Reset(Vector2D position) override;

private:
	Vector2D position;
	Vector2D target;
	double   z;

	Vector2D latchedPos;
	bool     wasTipDown;
	std::chrono::high_resolution_clock::time_point latchStart;
};
