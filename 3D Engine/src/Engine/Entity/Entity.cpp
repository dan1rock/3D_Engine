#include "Entity.h"
#include "EntityManager.h"
#include "RigidBody.h"

// Конструктор класу GameObject, реєструє об'єкт у EntityManager та ініціалізує трансформацію за замовчуванням
Entity::Entity()
{
	EntityManager::get()->registerEntity(this);
    mTransform.setOwner(this);
	mTransform.setPosition(Vector3(0.0f, 0.0f, 0.0f));
	mTransform.setScale(Vector3(1.0f, 1.0f, 1.0f));
	mTransform.setRotation(Vector3(0.0f, 0.0f, 0.0f));
}

// Конструктор класу GameObject з початковою позицією
Entity::Entity(Vector3 position)
{
    EntityManager::get()->registerEntity(this);
    mTransform.setOwner(this);
	mTransform.setPosition(position);
	mTransform.setScale(Vector3(1.0f, 1.0f, 1.0f));
	mTransform.setRotation(Vector3(0.0f, 0.0f, 0.0f));
}

// Деструктор класу GameObject, видаляє всі компоненти та знімає реєстрацію об'єкта
Entity::~Entity()
{
    for (auto* component : mComponents) {
        delete component;
    }

    mComponents.clear();

	if (mParent) {
		mParent->mChildren.remove(this);
		mParent = nullptr;
	}

    EntityManager::get()->unregisterEntity(this);
}

// Повертає вказівник на компонент Transform цього об'єкта
Transform* Entity::getTransform()
{
    return &mTransform;
}

// Знищує об'єкт
void Entity::destroy()
{
	auto it = mChildren.begin();

	while (it != mChildren.end()) {
		Entity* child = *it++;
		child->destroy();
	}
    
    delete this;
}

// Видаляє компонент з об'єкта, якщо він існує
bool Entity::removeComponent(Component* component)
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
Entity* Entity::instantiate()  
{  
   Entity* newObject = new Entity();

   newObject->mTransform.setPosition(mTransform.getPosition());
   newObject->mTransform.setScale(mTransform.getScale());
   newObject->mTransform.setRotation(mTransform.getRotation());

   newObject->mTransform.setLocalPosition(mTransform.getLocalPosition());
   newObject->mTransform.setLocalScale(mTransform.getLocalScale());
   newObject->mTransform.setLocalRotation(mTransform.getLocalRotation());

   for (auto* component : mComponents) {
       auto* newComponent = component->instantiate();
       newComponent->setOwner(newObject);
       static_cast<Component*>(newComponent)->registerComponent();
       newObject->mComponents.push_back(newComponent);
       newObject->mComponents.back()->awake();
    newObject->mRigidBody = newObject->getComponent<RigidBody>();
   }

   for (auto* child : mChildren) {
	   Entity* newChild = child->instantiate();
	   newChild->setParent(newObject);
   }

   return newObject;
}

Entity* Entity::getParent()
{
    return mParent;
}

void Entity::setParent(Entity* parent)
{
	if (mParent == parent) return;
	if (mParent) {
		mParent->mChildren.remove(this);
	}

	mParent = parent;
	if (mParent) {
		mParent->mChildren.push_back(this);
	}
}

bool Entity::isActive()
{
	if (!isActiveSelf) return false;

	Entity* parent = getParent();

    while (parent) {
		if (!parent->isActiveSelf) {
			return false;
		}
		parent = parent->getParent();
    }

    return true;
}
