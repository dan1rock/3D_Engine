#include "MeshCollider.h"
#include "PhysicsEngine.h"
#include "ConvexMeshManager.h"
#include "Entity.h"
#include "MeshRenderer.h"
#include <string>

MeshCollider::MeshCollider()
{
}

MeshCollider::~MeshCollider()
{
}

// Повертає вказівник на геометрію коллайдера
void* MeshCollider::getGeometry(Vector3& scale, bool convex)
{
	// Якщо геометрія вже існує і масштаб не змінився, повертаємо її
	if (mGeometry) {
		if (scale == mScale) {
			return mGeometry;
		}

		delete mGeometry;
		mGeometry = nullptr;
	}

	mScale = scale;

	// Якщо меш не завантажено, намагаємося його взяти з MeshRenderer
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

	// Створюємо геометрію в залежності від типу меша
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

// Встановлює меш для коллайдера з файлу
void MeshCollider::setConvexMesh(const wchar_t* fullPath)
{
	mConvexMesh = PhysicsEngine::get()->getConvexMeshManager()->createConvexMeshFromFile(fullPath);
}
