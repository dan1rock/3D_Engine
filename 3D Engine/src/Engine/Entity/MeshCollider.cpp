#include "MeshCollider.h"
#include "PhysicsEngine.h"
#include "ConvexMeshManager.h"
#include "Entity.h"
#include "MeshRenderer.h"
#include "Mesh.h"
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
	// Якщо геометрія вже існує, масштаб не змінився і вид геометрії той самий, повертаємо її
	if (mGeometry) {
		if (scale == mScale && convex == mGeometryIsConvex) {
			return mGeometry;
		}

		delete mGeometry;
		mGeometry = nullptr;
	}

	mScale = scale;
	mGeometryIsConvex = convex;

	// Меш рендерера віддає свою геометрію фізиці, щоб не читати той самий файл вдруге
	Mesh* sourceMesh = nullptr;

	// Якщо меш не завантажено, намагаємося його взяти з MeshRenderer
	MeshRenderer* meshRenderer = mOwner->getComponent<MeshRenderer>();

	if (meshRenderer) {
		sourceMesh = meshRenderer->getMesh();
	}

	if (!mConvexMesh) {
		if (sourceMesh) {
			mConvexMesh = PhysicsEngine::get()->getConvexMeshManager()->createConvexMeshFromFile(sourceMesh->getFullPath().c_str());
		}
		else {
			return nullptr;
		}
	}

	PxMeshScale meshScale(PxVec3(scale.x, scale.y, scale.z), PxQuat(PxIdentity));

	// Створюємо геометрію в залежності від типу меша
	if (!convex)
	{
		mGeometry = new PxTriangleMeshGeometry(static_cast<PxTriangleMesh*>(mConvexMesh->getTriangleMesh(sourceMesh)), meshScale);
	}
	else
	{
		mGeometry = new PxConvexMeshGeometry(static_cast<PxConvexMesh*>(mConvexMesh->getConvexMesh(sourceMesh)), meshScale);
	}

	return mGeometry;
}

// Встановлює меш для коллайдера з файлу
void MeshCollider::setConvexMesh(const wchar_t* fullPath)
{
	mConvexMesh = PhysicsEngine::get()->getConvexMeshManager()->createConvexMeshFromFile(fullPath);
}
