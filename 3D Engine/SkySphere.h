#pragma once
#include "RenderObject.h"

class SkySphere : public RenderObject
{
public:
	SkySphere();
	~SkySphere();

	virtual void render() override;

private:
	virtual void init() override;
};
