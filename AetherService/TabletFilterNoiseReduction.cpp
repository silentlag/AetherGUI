#include "stdafx.h"
#include "TabletFilterNoiseReduction.h"





TabletFilterNoiseReduction::TabletFilterNoiseReduction() {
	distanceThreshold = 0;
	iterations = 10;
}




TabletFilterNoiseReduction::~TabletFilterNoiseReduction() {
}






void TabletFilterNoiseReduction::Reset(Vector2D position) {
	lastTarget.Set(position);
	buffer.Reset();
}


void TabletFilterNoiseReduction::SetTarget(Vector2D targetVector, double h) {
	lastTarget.Set(targetVector);
	buffer.Add(targetVector);
}


void TabletFilterNoiseReduction::SetPosition(Vector2D vector, double h) {
	position.x = vector.x;
	position.y = vector.y;
}


bool TabletFilterNoiseReduction::GetPosition(Vector2D *outputVector) {
	outputVector->x = position.x;
	outputVector->y = position.y;
	return true;
}


void TabletFilterNoiseReduction::Update() {

	
	if (buffer.count == 1) {
		position.x = buffer[0]->x;
		position.y = buffer[0]->y;
		return;
	}

	
	GetGeometricMedianVector(&position, iterations);

	
	if (buffer.isValid) {
		double distance = lastTarget.Distance(position);
		if (distance > distanceThreshold) {
			buffer.Reset();
			position.Set(lastTarget);
		}
	}
}




bool TabletFilterNoiseReduction::GetAverageVector(Vector2D *output) {
	double x, y;
	if (!buffer.isValid) return false;

	x = y = 0;
	for (int i = 0; i < buffer.count; i++) {
		x += buffer[i]->x;
		y += buffer[i]->y;
	}
	output->x = x / buffer.count;
	output->y = y / buffer.count;
	return true;
}




bool TabletFilterNoiseReduction::GetGeometricMedianVector(Vector2D *output, int iterations) {

	Vector2D candidate, next;
	double minimumDistance = 0.001;

	double denominator, dx, dy, distance, weight;
	int i;

	
	if (GetAverageVector(&candidate)) {
	}
	else {
		return false;
	}

	
	for (int iteration = 0; iteration < iterations; iteration++) {

		denominator = 0;

		
		for (i = 0; i < buffer.count; i++) {
			dx = candidate.x - buffer[i]->x;
			dy = candidate.y - buffer[i]->y;
			distance = sqrt(dx*dx + dy * dy);
			if (distance > minimumDistance) {
				denominator += 1.0 / distance;
			}
			else {
				denominator += 1.0 / minimumDistance;
			}
		}

		
		next.x = 0;
		next.y = 0;

		
		for (i = 0; i < buffer.count; i++) {
			dx = candidate.x - buffer[i]->x;
			dy = candidate.y - buffer[i]->y;
			distance = sqrt(dx*dx + dy * dy);
			if (distance > minimumDistance) {
				weight = 1.0 / distance;
			}
			else {
				weight = 1.0 / minimumDistance;
			}

			next.x += buffer[i]->x * weight / denominator;
			next.y += buffer[i]->y * weight / denominator;
		}

		
		candidate.x = next.x;
		candidate.y = next.y;
	}

	
	output->x = candidate.x;
	output->y = candidate.y;

	return true;

}

