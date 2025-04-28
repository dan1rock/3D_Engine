#pragma once
#include "RenderObject.h"

class SkySphere : public RenderObject
{
public:
	SkySphere(Matrix* cameraMat);
	~SkySphere();

	virtual void render() override;

private:
	virtual void init() override;

	Matrix* cameraMat = nullptr;
};
