#include "InstantiationTest.h"
#include "GameObject.h"
#include "Input.h"
#include "GraphicsEngine.h"
#include "TextureManager.h"
#include "MeshManager.h"
#include "MeshRenderer.h"
#include "Material.h"

InstantiationTest::InstantiationTest()
{
}

InstantiationTest::~InstantiationTest()
{
}

void InstantiationTest::update()
{
	if (Input::getKey('I'))
	{
		if (mMaterial == nullptr)
		{
			Texture* penguinTexture = GraphicsEngine::get()->getTextureManager()->createTextureFromFile(L"Assets\\Textures\\penguin.png");
			mMaterial = new Material();
			mMaterial->addTexture(penguinTexture);
		}

		Mesh* penguinMesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\penguin.obj");

		GameObject* newObject = new GameObject(mPosition);
		newObject->addComponent<MeshRenderer>(penguinMesh, mMaterial);

		mGameObjects.push_back(newObject);

		mPosition.x += 1.0f;
	}
	if (Input::getKey('O'))
	{
		if (!mGameObjects.empty())
		{
			GameObject* objectToDelete = mGameObjects.front();
			mGameObjects.pop_front();
			objectToDelete->destroy();

			mPosition.x -= 1.0f;
		}
	}
}
