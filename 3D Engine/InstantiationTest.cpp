#include "InstantiationTest.h"
#include "GameObject.h"
#include "Input.h"
#include "GraphicsEngine.h"
#include "TextureManager.h"
#include "MeshManager.h"
#include "MeshRenderer.h"
#include "RigidBody.h"
#include "MeshCollider.h"
#include "Material.h"
#include "LevitationTest.h"

InstantiationTest::InstantiationTest()
{
}

InstantiationTest::~InstantiationTest()
{
}

void InstantiationTest::update()
{
	if (Input::getKeyDown('I'))
	{
		if (mMaterial == nullptr)
		{
			Texture* penguinTexture = GraphicsEngine::get()->getTextureManager()->createTextureFromFile(L"Assets\\Textures\\rabbit.png");
			mMaterial = new Material();
			mMaterial->addTexture(penguinTexture);
		}

		Mesh* penguinMesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\rabbit.obj");

		GameObject* newObject = new GameObject(mPosition);
		newObject->addComponent<MeshRenderer>(penguinMesh, mMaterial);
		newObject->addComponent<MeshCollider>();
		newObject->addComponent<RigidBody>(1.0f, 2.0f);
		LevitationTest* l = newObject->addComponent<LevitationTest>();
		l->force = 200.0f;
		l->damping = 10.0f;
		l->maxDistance = 2.0f;


		mGameObjects.push_back(newObject);

		mPosition.x += 1.0f;
	}
	if (Input::getKeyDown('O'))
	{
		if (!mGameObjects.empty())
		{
			GameObject* objectToDelete = mGameObjects.back();
			mGameObjects.pop_back();
			objectToDelete->destroy();

			mPosition.x -= 1.0f;
		}
	}
}
