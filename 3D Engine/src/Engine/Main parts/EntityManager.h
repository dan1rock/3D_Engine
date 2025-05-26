#pragma once
#include <list>
#include <unordered_map>

class Component;
class Renderer;
class Camera;
class Entity;
class Material;
class RigidBody;

class EntityManager
{
public:
	EntityManager();
	~EntityManager();

	// Реєструє Entity у менеджері
	void registerEntity(Entity* gameObject);
	// Видаляє Entity з менеджера
	void unregisterEntity(Entity* gameObject);
	// Реєструє компонент у менеджері
	void registerComponent(Component* component);
	// Видаляє компонент з менеджера
	void unregisterComponent(Component* component);
	// Реєструє рендер-компонент у менеджері
	void registerRenderer(Renderer* renderer);
	// Видаляє рендер-компонент з менеджера
	void unregisterRenderer(Renderer* renderer);
	// Реєструє камеру у менеджері
	void registerCamera(Camera* camera);
	// Видаляє камеру з менеджера
	void unregisterCamera(Camera* camera);
	// Реєструє матеріал у менеджері
	void registerMaterial(Material* material);
	// Видаляє матеріал з менеджера
	void unregisterMaterial(Material* material);
	// Реєструє фізичне тіло у менеджері
	void registerRigidBody(RigidBody* rigidBody);
	// Видаляє фізичне тіло з менеджера
	void unregisterRigidBody(RigidBody* rigidBody);

	// Повертає фізичне тіло за вказаним актором
	RigidBody* getRigidBody(void* actor);

	// Оновлює всі компоненти
	void updateComponents();
	// Виконує фіксоване оновлення для всіх компонентів
	void fixedUpdateComponents();
	// Оновлює всі рендер-компоненти
	void updateRenderers();
	// Оновлює всі камери
	void updateCameras();

	// Викликається на початку завантаження сцени
	void onSceneLoadStart();
	// Викликається після завершення завантаження сцени
	void onSceneLoadFinished();

	// Повертає єдиний екземпляр менеджера сутностей (синглтон)
	static EntityManager* get();

private:
	std::list<Entity*> mEntities = {};
	std::list<Component*> mComponents = {};
	std::list<Renderer*> mRenderers = {};
	std::list<Camera*> mCameras = {};
	std::list<Material*> mMaterials = {};
	std::unordered_map<void*, RigidBody*> mRigidBodies = {};
};
