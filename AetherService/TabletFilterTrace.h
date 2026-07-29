#pragma once

#include "Vector2D.h"
#include "TabletFilter.h"

class TabletFilterTrace : public TabletFilter {
public:

	double cornerSharpness;
	double lineSmooth;

	Vector2D position;
	Vector2D target;
	Vector2D prevTarget;
	Vector2D prevPrevTarget;
	bool hasHistory;

	TabletFilterTrace();
	~TabletFilterTrace();

	void SetTarget(Vector2D vector, double h) override;
	void SetPosition(Vector2D vector, double h) override;
	bool GetPosition(Vector2D *outputVector) override;
	void Update() override;
	void Reset(Vector2D position) override;
};
