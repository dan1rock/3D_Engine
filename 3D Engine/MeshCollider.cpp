#include "MeshCollider.h"
#include "PhysicsEngine.h"
#include "ConvexMeshManager.h"
#include "GameObject.h"
#include "MeshRenderer.h"
#include <string>

MeshCollider::MeshCollider()
{
}

MeshCollider::~MeshCollider()
{
}

void* MeshCollider::getGeometry(Vector3& scale, bool convex)
{
	if (mGeometry) {
		if (scale == mScale) {
			return mGeometry;
		}

		delete mGeometry;
		mGeometry = nullptr;
	}

	mScale = scale;

	if (!mConvexMesh) {
		MeshRenderer* meshRenderer = mOwner->getComponent<MeshRenderer>();
		if (meshRenderer) {
			mConvexMesh = PhysicsEngine::get()->getConvexMeshManager()->createConvexMeshFromFile(meshRenderer->getMesh()->getFullPath().c_str());
		}
		else {
			return nullptr;
		}
	}

	PxMeshScale meshScale(PxVec3(scale.x, scale.y, scale.z), PxQuat(PxIdentity));

	if (!convex)
	{
		mGeometry = new PxTriangleMeshGeometry(static_cast<PxTriangleMesh*>(mConvexMesh->getTriangleMesh()), meshScale);
	}
	else
	{
		mGeometry = new PxConvexMeshGeometry(static_cast<PxConvexMesh*>(mConvexMesh->getConvexMesh()), meshScale);
	}

	return mGeometry;
}

void MeshCollider::setConvexMesh(const wchar_t* fullPath)
{
	mConvexMesh = PhysicsEngine::get()->getConvexMeshManager()->createConvexMeshFromFile(fullPath);
}
