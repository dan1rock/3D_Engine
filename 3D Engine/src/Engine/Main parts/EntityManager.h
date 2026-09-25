#pragma once
#include <list>
#include <unordered_map>

class Component;
class Renderer;
class Camera;
class DirectionalLight;
class Entity;
class Material;
class RigidBody;
class Frustum;

class EntityManager
{
public:
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
	// Реєструє напрямлене світло у менеджері
	void registerLight(DirectionalLight* light);
	// Видаляє напрямлене світло з менеджера
	void unregisterLight(DirectionalLight* light);
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

	// Повертає всі зареєстровані об'єкти сцени для редактора та збереження
	const std::list<Entity*>& getEntities() const;
	// Повертає всі зареєстровані рендер-компоненти, зокрема приховані образи префабів
	const std::list<Renderer*>& getRenderers() const;
	// Перевіряє, чи об'єкт ще існує у сцені, не розіменовуючи вказівник
	bool isAlive(Entity* entity) const;

	// Оновлює всі компоненти
	void updateComponents();
	// Виконує фіксоване оновлення для всіх компонентів
	void fixedUpdateComponents();
	// Оновлює всі рендер-компоненти
	void updateRenderers();
	// Рендерить глибину рендер-компонентів, які кидають тінь у вказану піраміду видимості
	void renderShadowCasters(const Frustum& frustum);
	// Оновлює всі камери
	void updateCameras();
	// Оновлює всі джерела напрямленого світла
	void updateLights();

	// Ставить кореневий об'єкт перед іншим кореневим у порядку сцени, а за nullptr — у кінець
	void moveRootBefore(Entity* root, Entity* before);
	// Упорядковує список об'єктів так, як їх показує дерево сцени: кожен батько, а за ним
	// його нащадки. Від цього порядку залежить і файл сцени, і знімок для режиму гри
	void sortByHierarchy();

	// Вмикає або вимикає відсікання об'єктів за пірамідою видимості
	void setFrustumCullingEnabled(bool enabled);
	// Перевіряє, чи увімкнено відсікання за пірамідою видимості
	bool isFrustumCullingEnabled();

	// Повертає кількість рендер-компонентів, намальованих в останньому кадрі
	int getVisibleRendererCount();
	// Повертає загальну кількість активних рендер-компонентів
	int getActiveRendererCount();

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
	std::list<DirectionalLight*> mLights = {};
	std::list<Material*> mMaterials = {};
	std::unordered_map<void*, RigidBody*> mRigidBodies = {};

	bool mFrustumCullingEnabled = true;

	int mVisibleRenderers = 0;
	int mActiveRenderers = 0;
};
