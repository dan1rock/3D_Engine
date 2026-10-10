#include "ConvexMesh.h"
#include "PhysicsEngine.h"
#include "GraphicsEngine.h"
#include "MeshManager.h"
#include "Vector3.h"
#include "Vector2.h"
#include "Mesh.h"
#include "AssetCache.h"
#include <vector>
#include <PxPhysics.h>

ConvexMesh::ConvexMesh(const wchar_t* fullPath) : Resource(fullPath)
{
}

ConvexMesh::~ConvexMesh()
{
	if (mConvexMesh)
	{
		static_cast<PxConvexMesh*>(mConvexMesh)->release();
		mConvexMesh = nullptr;
	}
	if (mTriangleMesh)
	{
		static_cast<PxTriangleMesh*>(mTriangleMesh)->release();
		mTriangleMesh = nullptr;
	}
}

// Повертає вказівник на опуклий меш, створює його при необхідності
void* ConvexMesh::getConvexMesh(Mesh* sourceMesh)
{
	if (mConvexMesh == nullptr) createConvexMesh(sourceMesh);
    return mConvexMesh;
}

// Повертає вказівник на трикутний меш, створює його при необхідності
void* ConvexMesh::getTriangleMesh(Mesh* sourceMesh)
{
	if (mTriangleMesh == nullptr) createTriangleMesh(sourceMesh);
	return mTriangleMesh;
}

// Повертає відбиток, за яким готові фізичні дані в кеші відповідають файлу моделі та версії PhysX
std::string ConvexMesh::cacheStamp()
{
	std::string stamp = Mesh::sourceStamp(getFullPath());

	if (stamp.empty()) return stamp;

	return stamp + "|" + PhysicsEngine::cookingStamp();
}

// Повертає меш з геометрією для готування: вказаний, якщо він має вершини, або завантажений з того самого файлу
Mesh* ConvexMesh::geometrySource(Mesh* sourceMesh)
{
	// Геометрія вже є в оперативній пам'яті, тому файл читати не потрібно
	if (sourceMesh && !sourceMesh->getPositions().empty()) return sourceMesh;

	// Без готового меша геометрію дає той самий завантажувач моделей, що й для малювання, з перетвореннями вузлів
	Mesh* loaded = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(getFullPath().c_str());

	return loaded && !loaded->getPositions().empty() ? loaded : nullptr;
}

// Створює опуклий меш з кешу готових даних або з геометрії вказаного меша чи моделі з того самого файлу
void ConvexMesh::createConvexMesh(Mesh* sourceMesh)
{
	// Готування повторюється лише тоді, коли модель змінилась або кешу ще немає
	std::wstring cache = AssetCache::pathOf(L"Physics", getFullPath(), L".convex");
	std::string stamp = cacheStamp();
	std::vector<unsigned char> cooked;

	if (AssetCache::read(cache, stamp, cooked))
	{
		mConvexMesh = PhysicsEngine::get()->createConvexMesh(cooked);

		if (mConvexMesh) return;
	}

	Mesh* source = geometrySource(sourceMesh);

	if (source == nullptr) return;

	const std::vector<Vector3>& positions = source->getPositions();
	std::vector<PxVec3> points;

	points.reserve(positions.size());

	for (const Vector3& position : positions) {
		points.emplace_back(PxVec3(position.x, position.y, position.z));
	}

	mConvexMesh = PhysicsEngine::get()->cookConvexMesh(points, &cooked);

	AssetCache::write(cache, stamp, cooked.data(), cooked.size());
}

// Створює трикутний меш з кешу готових даних або з геометрії вказаного меша чи моделі з того самого файлу
void ConvexMesh::createTriangleMesh(Mesh* sourceMesh)
{
	// Готування великої моделі триває найдовше з усього завантаження, тож готові дані беруться з кешу
	std::wstring cache = AssetCache::pathOf(L"Physics", getFullPath(), L".triangles");
	std::string stamp = cacheStamp();
	std::vector<unsigned char> cooked;

	if (AssetCache::read(cache, stamp, cooked))
	{
		mTriangleMesh = PhysicsEngine::get()->createTriangleMesh(cooked);

		if (mTriangleMesh) return;
	}

	Mesh* source = geometrySource(sourceMesh);

	if (source == nullptr) return;

	const std::vector<Vector3>& positions = source->getPositions();
	const std::vector<unsigned int>& meshIndices = source->getIndices();

	std::vector<PxVec3> points;
	std::vector<PxU32> indices(meshIndices.begin(), meshIndices.end());

	points.reserve(positions.size());

	for (const Vector3& position : positions) {
		points.emplace_back(PxVec3(position.x, position.y, position.z));
	}

	mTriangleMesh = PhysicsEngine::get()->cookTriangleMesh(points, indices, &cooked);

	AssetCache::write(cache, stamp, cooked.data(), cooked.size());
}
