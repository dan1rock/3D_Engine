#pragma once
#include "Collider.h"

class ConvexMesh;

class MeshCollider : public Collider
{
public:
	MeshCollider();
	~MeshCollider() override;

	void* getGeometry(Vector3& scale, bool convex = true) override;

	void setConvexMesh(const wchar_t* fullPath);

protected:
	MeshCollider* instantiate() const override {
		return new MeshCollider(*this);
	}

private:
	ConvexMesh* mConvexMesh = nullptr;
};

