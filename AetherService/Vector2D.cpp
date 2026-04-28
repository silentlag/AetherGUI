#include "stdafx.h"
#include "Vector2D.h"




Vector2D::Vector2D() {
	x = 0;
	y = 0;
}




Vector2D::~Vector2D() {
}

void Vector2D::Set(double x, double y) {
	this->x = x;
	this->y = y;
}

void Vector2D::Set(Vector2D vector) {
	Set(vector.x, vector.y);
}

void Vector2D::Add(double x, double y) {
	this->x += x;
	this->y += y;
}

void Vector2D::Add(Vector2D vector) {
	Add(vector.x, vector.y);
}

void Vector2D::Subtract(Vector2D vector) {
	this->x -= vector.x;
	this->y -= vector.y;
}

void Vector2D::Multiply(double value) {
	this->x *= value;
	this->y *= value;
}

double Vector2D::Distance(Vector2D target) {
	double dx = target.x - this->x;
	double dy = target.y - this->y;
	return sqrt(dx * dx + dy * dy);
}

double Vector2D::DistanceSq(Vector2D target) {
	double dx = target.x - this->x;
	double dy = target.y - this->y;
	return dx * dx + dy * dy;
}

double Vector2D::Length() {
	return sqrt(x * x + y * y);
}

double Vector2D::LengthSq() {
	return x * x + y * y;
}

void Vector2D::CopyTo(Vector2D *target) {
	target->x = this->x;
	target->y = this->y;
}
