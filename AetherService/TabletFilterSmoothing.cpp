#include "stdafx.h"
#include "TabletFilterSmoothing.h"

TabletFilterSmoothing::TabletFilterSmoothing() {
	latency = 2.0;
	weight = 1.000;
	threshold = 0.9;

	AntichatterEnabled = true;
	antichatterStrength = 3;
	antichatterMultiplier = 1;
	antichatterOffsetX = 0.0;
	antichatterOffsetY = 0.0;

	prev_target.x = 0.0;
	prev_target.y = 0.0;

	PredictionEnabled = true;
	PredictionSharpness = 1.0;
	PredictionStrength = 1.1;
	PredictionOffsetX = 3.0;
	PredictionOffsetY = 0.3;
}

TabletFilterSmoothing::~TabletFilterSmoothing() {
}

void TabletFilterSmoothing::Reset(Vector2D position) {
	target.Set(position);
	prev_target.Set(position);
	calculated_target.Set(position);
	this->position.Set(position);
}

void TabletFilterSmoothing::SetTarget(Vector2D vector, double h) {
	this->target.x = vector.x;
	this->target.y = vector.y;
	this->z = h;
}

void TabletFilterSmoothing::SetPosition(Vector2D vector, double h) {
	position.x = vector.x;
	position.y = vector.y;
	z = h;
}

bool TabletFilterSmoothing::GetPosition(Vector2D *outputVector) {
	outputVector->x = position.x;
	outputVector->y = position.y;
	return true;
}

void TabletFilterSmoothing::Update() {

	double deltaX, deltaY, distance, weightModifier, predictionModifier;

	if (PredictionEnabled)
	{

		if (prev_target.x != target.x or prev_target.y != target.y)
		{

			deltaX = target.x - prev_target.x;
			deltaY = target.y - prev_target.y;
			double distSq = deltaX * deltaX + deltaY * deltaY;
			distance = (distSq > 0) ? sqrt(distSq) : 0;
			predictionModifier = 1 / cosh((distance - PredictionOffsetX) * PredictionSharpness) * PredictionStrength + PredictionOffsetY;

			deltaX *= predictionModifier;
			deltaY *= predictionModifier;

			calculated_target.x = target.x + deltaX;
			calculated_target.y = target.y + deltaY;

			prev_target.x = target.x;
			prev_target.y = target.y;
		}
	}
	else {
		calculated_target.x = target.x;
		calculated_target.y = target.y;
	}

	deltaX = calculated_target.x - position.x;
	deltaY = calculated_target.y - position.y;
	double distSq = deltaX * deltaX + deltaY * deltaY;
	distance = (distSq > 0.0) ? sqrt(distSq) : 0.0;

	if (!AntichatterEnabled) {
		position.x += deltaX * weight;
		position.y += deltaY * weight;
	}
	else {
		// Original formula, kept verbatim so the slider feel matches what
		// users tuned against. Two safety touches vs the legacy code:
		//   1. compute pow() once per packet instead of twice (the legacy
		//      version recomputed the whole expression for the clamp).
		//   2. clamp the input to pow() away from zero so distance == 0 (pen
		//      perfectly still between samples) doesn't push pow into +inf and
		//      then rely on the post-clamp to hide it.
		double base = distance + antichatterOffsetX;
		if (base < 1e-6) base = 1e-6;
		double shaped = pow(base, -antichatterStrength) * antichatterMultiplier;
		double denom  = shaped + antichatterOffsetY;
		if (denom < 0.0) denom = 0.0;

		weightModifier = (denom > 1e-9) ? (weight / denom) : 1.0;
		if (weightModifier > 1.0) weightModifier = 1.0;
		else if (weightModifier < 0.0) weightModifier = 0.0;

		position.x += deltaX * weightModifier;
		position.y += deltaY * weightModifier;
	}
}

void TabletFilterSmoothing::SetPosition(double x, double y, double h) {
	this->position.x = x;
	this->position.y = y;
	this->z = h;
}

double TabletFilterSmoothing::GetLatency(double filterWeight, double interval, double threshold) {
	double target = 1 - threshold;
	double stepCount = -log(1 / target) / log(1 - filterWeight);
	return stepCount * interval;
}
double TabletFilterSmoothing::GetLatency(double filterWeight) {
	return this->GetLatency(filterWeight, timerInterval, threshold);
}
double TabletFilterSmoothing::GetLatency() {
	return this->GetLatency(weight, timerInterval, threshold);
}

double TabletFilterSmoothing::GetWeight(double latency, double interval, double threshold) {
	double stepCount = latency / interval;
	double target = 1 - threshold;
	return 1 - 1 / pow(1 / target, 1 / stepCount);
}
double TabletFilterSmoothing::GetWeight(double latency) {
	return this->GetWeight(latency, this->timerInterval, this->threshold);
}

void TabletFilterSmoothing::SetLatency(double latency) {
	if (latency <= 0) {
		this->weight = 1.0;
		this->latency = 0;
		return;
	}
	this->weight = GetWeight(latency);
	this->latency = latency;
}

