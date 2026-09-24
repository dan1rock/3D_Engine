#include "Properties.h"
#include "Entity.h"
#include "Prefab.h"
#include "Transform.h"

// Скорочення, щоб компонент не писав крок і вид посилання там, де вони й так очевидні
void PropertyVisitor::reference(const char* name, Prefab*& value)
{
	Entity* entity = value;

	reference(name, entity, ReferenceKind::PrefabOnly);

	// Обхідник міг підставити звичайний об'єкт, тому перевіряємо тип перед привласненням
	value = dynamic_cast<Prefab*>(entity);
}

// Скорочення, щоб компонент не писав крок і вид посилання там, де вони й так очевидні
void PropertyVisitor::reference(const char* name, Transform*& value)
{
	// Посилання зберігається на сам об'єкт, а не на його трансформацію: так його видно за іменем
	Entity* entity = value ? value->getOwner() : nullptr;

	reference(name, entity, ReferenceKind::Any);

	value = entity ? entity->getTransform() : nullptr;
}
