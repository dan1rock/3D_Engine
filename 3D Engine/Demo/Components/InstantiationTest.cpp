#include "InstantiationTest.h"
#include "Prefab.h"
#include "Input.h"
#include "SceneIO.h"

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
		Entity* newObject = prefab->instantiate();
		newObject->getTransform()->setPosition(mPosition);

		mGameObjects.push_back(newObject);

		mPosition.x += 1.0f;
	}
	if (Input::getKeyDown('O'))
	{
		if (!mGameObjects.empty())
		{
			Entity* objectToDelete = mGameObjects.back();
			mGameObjects.pop_back();
			objectToDelete->destroy();

			mPosition.x -= 1.0f;
		}
	}
}

// Записує власні поля та посилання у файл сцени
void InstantiationTest::serialize(SceneWriter& writer) const
{
	writer.writeRef("prefab", prefab);
}

// Відновлює власні поля та посилання з файлу сцени
void InstantiationTest::deserialize(const SceneReader& reader)
{
	prefab = reader.readPrefab("prefab");
}
