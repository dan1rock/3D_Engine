#include "GameObject.h"
#include "EntityManager.h"
#include "RigidBody.h"

// Конструктор класу GameObject, реєструє об'єкт у EntityManager та ініціалізує трансформацію за замовчуванням
GameObject::GameObject()
{
	EntityManager::get()->registerGameObject(this);
    mTransform.setOwner(this);
	mTransform.setPosition(Vector3(0.0f, 0.0f, 0.0f));
	mTransform.setScale(Vector3(1.0f, 1.0f, 1.0f));
	mTransform.setRotation(Vector3(0.0f, 0.0f, 0.0f));
}

// Конструктор класу GameObject з початковою позицією
GameObject::GameObject(Vector3 position)
{
    EntityManager::get()->registerGameObject(this);
    mTransform.setOwner(this);
	mTransform.setPosition(position);
	mTransform.setScale(Vector3(1.0f, 1.0f, 1.0f));
	mTransform.setRotation(Vector3(0.0f, 0.0f, 0.0f));
}

// Деструктор класу GameObject, видаляє всі компоненти та знімає реєстрацію об'єкта
GameObject::~GameObject()
{
    for (auto* component : mComponents) {
        delete component;
    }

    mComponents.clear();

    EntityManager::get()->unregisterGameObject(this);
}

// Повертає вказівник на компонент Transform цього об'єкта
Transform* GameObject::getTransform()
{
    return &mTransform;
}

// Знищує об'єкт
void GameObject::destroy()
{
    delete this;
}

// Видаляє компонент з об'єкта, якщо він існує
bool GameObject::removeComponent(Component* component)
{
    auto it = std::find_if(
        mComponents.begin(),
        mComponents.end(),
        [component](const Component* ptr) {
            return ptr == component;
        });

    if (it != mComponents.end()) {
        mComponents.erase(it);
        return true;
    }
    return false;
}

// Створює копію об'єкта разом з усіма його компонентами
GameObject* GameObject::instantiate()  
{  
   GameObject* newObject = new GameObject();

   newObject->mTransform.setPosition(mTransform.getPosition());
   newObject->mTransform.setScale(mTransform.getScale());
   newObject->mTransform.setRotation(mTransform.getRotation());

   for (auto* component : mComponents) {
       auto* newComponent = component->instantiate();
       newComponent->setOwner(newObject);
       static_cast<Component*>(newComponent)->registerComponent();
       newObject->mComponents.push_back(newComponent);
       newObject->mComponents.back()->awake();
    newObject->mRigidBody = newObject->getComponent<RigidBody>();
   }

   return newObject;
}
