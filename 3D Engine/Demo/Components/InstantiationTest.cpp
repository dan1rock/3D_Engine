#include "InstantiationTest.h"
#include "Prefab.h"
#include "Input.h"

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
		GameObject* newObject = prefab->instantiate();
		newObject->getTransform()->setPosition(mPosition);

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
