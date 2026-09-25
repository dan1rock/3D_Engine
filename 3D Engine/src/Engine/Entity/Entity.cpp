#include "Entity.h"
#include <algorithm>
#include "EntityManager.h"
#include "RigidBody.h"

#include <iostream>

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

// Повертає ім'я об'єкта, яке показує редактор
const std::string& Entity::getName() const
{
	return mName;
}

// Встановлює ім'я об'єкта
void Entity::setName(const std::string& name)
{
	mName = name;
}

// Повертає список усіх компонентів об'єкта
const std::list<Component*>& Entity::getComponentList() const
{
	return mComponents;
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

    if (it == mComponents.end()) return false;

    mComponents.erase(it);

	// Об'єкт тримає фізичне тіло окремо, і Transform передає в нього кожну зміну позиції,
	// тож без цього наступне ж переміщення звернулося б до знищеного тіла
	if (component == mRigidBody) mRigidBody = nullptr;

	// Сусіди дізнаються про видалення, поки компонент ще живий: так вони можуть його порівняти
	for (Component* other : mComponents)
	{
		other->onComponentRemoved(component);
	}

	// Лише прибрати вказівник зі списку замало: компонент лишився б зареєстрованим у менеджері
	// і далі оновлювався, малювався та брав участь у фізиці. Деструктор знімає реєстрацію
	// та звільняє ресурси. Викликати це під час обходу компонентів менеджером не можна
	delete component;

    return true;
}

// Створює копію об'єкта разом з усіма його компонентами
Entity* Entity::instantiate()  
{  
   Entity* newObject = new Entity();

   newObject->mTransform.setLocalPosition(mTransform.getLocalPosition());
   newObject->mTransform.setLocalScale(mTransform.getLocalScale());
   newObject->mTransform.setLocalRotation(mTransform.getLocalRotation());

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

void Entity::setParent(Entity* parent, bool keepWorldTransform)
{
	if (mParent == parent) return;

	// Власний нащадок не може стати батьком: вийшов би цикл, і оновлення трансформацій не скінчилося б
	for (Entity* ancestor = parent; ancestor; ancestor = ancestor->mParent)
	{
		if (ancestor == this) return;
	}

	// Світова матриця потрібна до зміни батька, бо після неї вона вже рахується від нового
	Matrix world = *mTransform.getMatrix();

	if (mParent) {
		mParent->mChildren.remove(this);
	}

	mParent = parent;

	if (mParent) {
		mParent->mChildren.push_back(this);
	}

	if (!keepWorldTransform)
	{
		if (mParent) mTransform.updateGlobalMatrix();
		return;
	}

	// Об'єкт без батька і так зберігає саме світову трансформацію
	if (mParent == nullptr) return;

	// Світова матриця дочірнього — це локальна, помножена на батьківську, тож локальну дає
	// множення на обернену батьківську. Через сетери, щоб узгодити й окремі значення
	Matrix parentInverse = *mParent->getTransform()->getMatrix();
	parentInverse.inverse();

	Matrix local = world * parentInverse;

	mTransform.setLocalScale(local.getScale());
	mTransform.setLocalRotation(local.getRotation());
	mTransform.setLocalPosition(local.getTranslation());
}

// Переставляє дочірній об'єкт перед іншим дочірнім, а за nullptr — у кінець
void Entity::moveChildBefore(Entity* child, Entity* before)
{
	if (child == before) return;

	auto it = std::find(mChildren.begin(), mChildren.end(), child);

	if (it == mChildren.end()) return;

	mChildren.erase(it);

	auto position = before ? std::find(mChildren.begin(), mChildren.end(), before) : mChildren.end();

	mChildren.insert(position, child);
}

std::list<Entity*>* Entity::getChildren()
{
	return &mChildren;
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
