#include "Editor.h"
#include "ComponentRegistry.h"
#include "SceneSerializer.h"
#include "InspectorVisitor.h"

#include "EntityManager.h"
#include "Entity.h"
#include "Transform.h"
#include "Renderer.h"
#include "MeshRenderer.h"
#include "Camera.h"
#include "DirectionalLight.h"
#include "Material.h"
#include "Mesh.h"
#include "MeshManager.h"
#include "TextureManager.h"
#include "GraphicsEngine.h"
#include "ShadowMap.h"
#include "PostProcessing.h"
#include "Input.h"

#include "imgui.h"

#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>
#include <iostream>

namespace filesystem = std::experimental::filesystem;

// Шлях, за яким редактор зберігає та завантажує сцену
static const char* SCENE_PATH = "Assets\\Scenes\\Scene.json";

// Розставляє панель у типове місце, поки користувач не пересунув її сам
static void placeWindow(float x, float y, float width, float height)
{
	ImVec2 display = ImGui::GetIO().DisplaySize;

	// Від'ємні значення відлічуються від правого чи нижнього краю вікна
	if (x < 0.0f) x += display.x;
	if (y < 0.0f) y += display.y;
	if (width <= 0.0f) width += display.x;
	if (height <= 0.0f) height += display.y;

	ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_FirstUseEver);
}

// Повертає вікно у межі екрана. Розміри та позиції панелей зберігаються в imgui.ini, тому
// після запуску у меншому вікні панель може опинитися за краєм екрана разом з усіма полями
static void keepWindowOnScreen()
{
	ImVec2 display = ImGui::GetIO().DisplaySize;
	ImVec2 pos = ImGui::GetWindowPos();
	ImVec2 size = ImGui::GetWindowSize();

	// Спершу звужуємо саме вікно: інакше зсув не допоміг би
	ImVec2 fitted = size;

	if (fitted.x > display.x) fitted.x = display.x;
	if (fitted.y > display.y) fitted.y = display.y;

	if (fitted.x != size.x || fitted.y != size.y) ImGui::SetWindowSize(fitted);

	ImVec2 moved = pos;

	if (moved.x + fitted.x > display.x) moved.x = display.x - fitted.x;
	if (moved.y + fitted.y > display.y) moved.y = display.y - fitted.y;
	if (moved.x < 0.0f) moved.x = 0.0f;
	if (moved.y < 0.0f) moved.y = 0.0f;

	if (moved.x != pos.x || moved.y != pos.y) ImGui::SetWindowPos(moved);
}

// Повертає єдиний екземпляр редактора (синглтон)
Editor* Editor::get()
{
	static Editor instance;
	return &instance;
}

Editor::Editor()
{
}

// Збирає списки доступних ресурсів для випадних списків інспектора
void Editor::init()
{
	ComponentRegistry::registerEngineTypes();

	// Перелік ресурсів читається один раз під час запуску
	struct Folder { const wchar_t* path; std::vector<std::wstring>* paths; std::vector<std::string>* names; };

	Folder folders[] = {
		{ L"Assets\\Meshes", &mMeshPaths, &mMeshNames },
		{ L"Assets\\Textures", &mTexturePaths, &mTextureNames },
	};

	for (const Folder& folder : folders)
	{
		std::error_code error;

		if (!filesystem::exists(folder.path, error)) continue;

		for (const auto& entry : filesystem::directory_iterator(folder.path, error))
		{
			if (!filesystem::is_regular_file(entry.path(), error)) continue;

			std::wstring extension = entry.path().extension().wstring();

			// До списку потрапляють лише ті типи файлів, які рушій уміє завантажити
			bool isMesh = extension == L".obj";
			bool isTexture = extension == L".png" || extension == L".jpg"
				|| extension == L".jpeg" || extension == L".bmp";

			if (folder.paths == &mMeshPaths && !isMesh) continue;
			if (folder.paths == &mTexturePaths && !isTexture) continue;

			std::wstring wide = entry.path().wstring();

			folder.paths->push_back(wide);
			folder.names->push_back(std::string(wide.begin(), wide.end()));
		}
	}

	std::cout << "Editor: found " << mMeshPaths.size() << " meshes and "
		<< mTexturePaths.size() << " textures" << std::endl;
}

// Малює інтерфейс редактора та оновлює його камеру
void Editor::update()
{
	if (Input::getKeyDown(VK_F1)) mEnabled = !mEnabled;

	if (!mEnabled) return;

	// Поза режимом гри сценою керує камера редактора
	if (!mPlaying)
	{
		if (!Input::getMouseButton(MB_Right) && Input::isCursorHidden())
		{
			Input::hideCursor(false);
		}

		mCamera.update();
	}

	drawToolbar();
	drawHierarchy();
	drawInspector();
	drawAssets();

	updateSelection();

	// Гарячі клавіші працюють лише тоді, коли ввід не перехоплює поле тексту
	ImGuiIO& io = ImGui::GetIO();

	if (!io.WantCaptureKeyboard)
	{
		if (Input::getKeyDown('F')) focusSelected();
		if (Input::getKeyDown(VK_DELETE)) deleteSelected();

		// W, E, R перемикають режим маніпулятора, як у Unity. Поки тримають праву кнопку,
		// ці ж клавіші ведуть камеру, тому режим тоді не міняється
		if (!Input::getMouseButton(MB_Right))
		{
			if (Input::getKeyDown('W')) mGizmo.mode = GizmoMode::Translate;
			if (Input::getKeyDown('E')) mGizmo.mode = GizmoMode::Rotate;
			if (Input::getKeyDown('R')) mGizmo.mode = GizmoMode::Scale;
			if (Input::getKeyDown('X')) mGizmo.local = !mGizmo.local;
		}
	}
}

// Перевіряє, чи сцена зараз програється, а не редагується
bool Editor::isPlaying() const
{
	return mPlaying;
}

// Перевіряє, чи показано інтерфейс редактора
bool Editor::isEnabled() const
{
	return mEnabled;
}

// Вмикає або вимикає інтерфейс редактора
void Editor::setEnabled(bool enabled)
{
	mEnabled = enabled;
}

// Повертає камеру редактора
EditorCamera& Editor::getCamera()
{
	return mCamera;
}

// Малює верхню панель з кнопками режиму гри та збереження сцени
void Editor::drawToolbar()
{
	placeWindow(10.0f, 10.0f, 300.0f, 300.0f);

	ImGui::Begin("Editor");
	keepWindowOnScreen();

	if (mPlaying)
	{
		if (ImGui::Button("Stop", ImVec2(70, 0))) stop();
	}
	else
	{
		if (ImGui::Button("Play", ImVec2(70, 0))) play();
	}

	ImGui::SameLine();
	ImGui::TextUnformatted(mPlaying ? "Playing" : "Editing");

	ImGui::Separator();

	if (ImGui::Button("Save Scene"))
	{
		std::error_code error;
		filesystem::create_directories("Assets\\Scenes", error);

		if (SceneSerializer::saveToFile(SCENE_PATH))
			std::cout << "Scene saved to " << SCENE_PATH << std::endl;
		else
			std::cout << "Failed to save scene" << std::endl;
	}

	ImGui::SameLine();

	if (ImGui::Button("Load Scene"))
	{
		mSelected = nullptr;

		if (!SceneSerializer::loadFromFile(SCENE_PATH))
			std::cout << "No saved scene at " << SCENE_PATH << std::endl;
	}

	ImGui::Separator();

	ImGui::Text("Camera");
	ImGui::DragFloat("Move Speed", &mCamera.moveSpeed, 0.5f, 1.0f, 200.0f);

	ImGui::Separator();

	// Налаштування рендерингу зручно мати поруч, бо вони впливають на весь кадр
	if (ImGui::CollapsingHeader("Rendering"))
	{
		ShadowMap* shadows = GraphicsEngine::get()->getShadowMap();
		PostProcessing* post = GraphicsEngine::get()->getPostProcessing();

		if (shadows)
		{
			bool enabled = shadows->isEnabled();
			if (ImGui::Checkbox("Shadows", &enabled)) shadows->setEnabled(enabled);
		}

		if (post)
		{
			bool bloom = post->isBloomEnabled();
			if (ImGui::Checkbox("Bloom", &bloom)) post->setBloomEnabled(bloom);

			bool vignette = post->isVignetteEnabled();
			if (ImGui::Checkbox("Vignette", &vignette)) post->setVignetteEnabled(vignette);
		}

		UINT anisotropy = GraphicsEngine::get()->getAnisotropy();
		int level = (int)anisotropy;

		if (ImGui::SliderInt("Anisotropy", &level, 1, 16))
		{
			GraphicsEngine::get()->setAnisotropy((UINT)level);
		}
	}

	ImGui::Separator();
	ImGui::TextUnformatted("F1 hide editor, F focus, Del delete");
	ImGui::TextUnformatted("Click to select, W/E/R move/rotate/scale, X local");
	ImGui::TextUnformatted("Right mouse + WASDQE to fly");

	ImGui::End();
}

// Малює дерево об'єктів сцени
void Editor::drawHierarchy()
{
	placeWindow(10.0f, 320.0f, 300.0f, -540.0f);

	ImGui::Begin("Hierarchy");
	keepWindowOnScreen();

	drawCreateMenu();

	ImGui::Separator();

	// Копія списку потрібна, бо створення чи видалення змінює його під час обходу
	std::vector<Entity*> roots;

	for (Entity* entity : EntityManager::get()->getEntities())
	{
		if (entity->getParent() == nullptr) roots.push_back(entity);
	}

	for (Entity* entity : roots)
	{
		drawEntityNode(entity);
	}

	ImGui::End();
}

// Малює один вузол дерева разом з його дочірніми об'єктами
void Editor::drawEntityNode(Entity* entity)
{
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

	if (entity == mSelected) flags |= ImGuiTreeNodeFlags_Selected;
	if (entity->getChildren()->empty()) flags |= ImGuiTreeNodeFlags_Leaf;

	// Неактивні об'єкти показуються приглушеним кольором
	bool active = entity->isActiveSelf;

	if (!active) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.55f, 1.0f));

	bool open = ImGui::TreeNodeEx((void*)entity, flags, "%s", entity->getName().c_str());

	if (!active) ImGui::PopStyleColor();

	if (ImGui::IsItemClicked()) mSelected = entity;

	if (open)
	{
		std::vector<Entity*> children(entity->getChildren()->begin(), entity->getChildren()->end());

		for (Entity* child : children)
		{
			drawEntityNode(child);
		}

		ImGui::TreePop();
	}
}

// Малює меню створення нового об'єкта
void Editor::drawCreateMenu()
{
	if (ImGui::Button("Create", ImVec2(-1.0f, 0.0f)))
	{
		ImGui::OpenPopup("CreateEntity");
	}

	if (!ImGui::BeginPopup("CreateEntity")) return;

	// Нові об'єкти з'являються перед камерою редактора, щоб їх одразу було видно
	Vector3 spawn = mCamera.getSpawnPoint();

	if (ImGui::Selectable("Empty"))
	{
		Entity* entity = new Entity(spawn);
		entity->setName("Empty");
		mSelected = entity;
	}

	if (ImGui::Selectable("Cube"))
	{
		Entity* entity = new Entity(spawn);
		entity->setName("Cube");
		entity->addComponent<MeshRenderer>()->setMesh(
			GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\cube.obj"));
		mSelected = entity;
	}

	if (ImGui::Selectable("Sphere"))
	{
		Entity* entity = new Entity(spawn);
		entity->setName("Sphere");
		entity->addComponent<MeshRenderer>()->setMesh(
			GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\sphere.obj"));
		mSelected = entity;
	}

	if (ImGui::Selectable("Plane"))
	{
		Entity* entity = new Entity(spawn);
		entity->setName("Plane");
		entity->addComponent<MeshRenderer>()->setMesh(
			GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\plane.obj"));
		mSelected = entity;
	}

	ImGui::Separator();

	if (ImGui::Selectable("Camera"))
	{
		Entity* entity = new Entity(spawn);
		entity->setName("Camera");
		entity->addComponent<Camera>();
		mSelected = entity;
	}

	if (ImGui::Selectable("Directional Light"))
	{
		Entity* entity = new Entity(spawn);
		entity->setName("Directional Light");
		entity->getTransform()->setForward(Vector3(-0.55f, -1.0f, -0.35f));
		entity->addComponent<DirectionalLight>();
		mSelected = entity;
	}

	ImGui::EndPopup();
}

// Малює інспектор вибраного об'єкта
void Editor::drawInspector()
{
	placeWindow(-380.0f, 10.0f, 370.0f, -20.0f);

	ImGui::Begin("Inspector");
	keepWindowOnScreen();

	if (mSelected == nullptr)
	{
		ImGui::TextUnformatted("Nothing selected");
		ImGui::End();
		return;
	}

	// Перевірка, що вибраний об'єкт ще існує у сцені
	bool alive = false;

	for (Entity* entity : EntityManager::get()->getEntities())
	{
		if (entity == mSelected) { alive = true; break; }
	}

	if (!alive)
	{
		mSelected = nullptr;
		ImGui::TextUnformatted("Nothing selected");
		ImGui::End();
		return;
	}

	// Поле імені заповнюється лише тоді, коли воно не редагується
	if (!ImGui::IsAnyItemActive())
	{
		strncpy_s(mNameBuffer, sizeof(mNameBuffer), mSelected->getName().c_str(), _TRUNCATE);
	}

	inspectorLabel("Name");
	if (ImGui::InputText("##name", mNameBuffer, sizeof(mNameBuffer)))
	{
		mSelected->setName(mNameBuffer);
	}

	inspectorLabel("Active");
	ImGui::Checkbox("##active", &mSelected->isActiveSelf);

	ImGui::Separator();

	drawTransform(mSelected);

	// Копія списку компонентів, бо видалення змінює його під час обходу
	std::vector<Component*> components(mSelected->getComponentList().begin(), mSelected->getComponentList().end());

	for (Component* component : components)
	{
		ImGui::PushID(component);
		ImGui::Separator();

		// Правий край рядка беремо до заголовка: після нього курсор уже на наступному рядку
		float rightEdge = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
		float buttonWidth = ImGui::CalcTextSize("Remove").x + ImGui::GetStyle().FramePadding.x * 2.0f;

		// Заголовок займає весь рядок з AllowOverlap для кнопки видалення
		bool open = ImGui::CollapsingHeader(component->getTypeName(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);

		ImGui::SameLine(rightEdge - buttonWidth);

		if (ImGui::SmallButton("Remove"))
		{
			mSelected->removeComponent(component);
			ImGui::PopID();
			continue;
		}

		if (open)
		{
			InspectorVisitor inspector;
			component->visitProperties(inspector);

			if (Renderer* renderer = dynamic_cast<Renderer*>(component))
			{
				drawRendererAssets(renderer);
			}
		}

		ImGui::PopID();
	}

	ImGui::Separator();

	drawAddComponentMenu(mSelected);

	ImGui::End();
}

// Малює поля трансформації об'єкта
void Editor::drawTransform(Entity* entity)
{
	Transform* transform = entity->getTransform();

	// Локальні координати мають зміст лише для дочірніх об'єктів. У кореневих вони лишаються
	// нульовими, бо setPosition задає світові, тому інспектор показує та змінює саме світові
	bool hasParent = entity->getParent() != nullptr;

	ImGui::TextUnformatted(hasParent ? "Transform (local)" : "Transform (world)");

	Vector3 position = hasParent ? transform->getLocalPosition() : transform->getPosition();
	Vector3 rotation = hasParent ? transform->getLocalRotation() : transform->getRotation();
	Vector3 scale = hasParent ? transform->getLocalScale() : transform->getScale();

	inspectorLabel("Position");
	if (ImGui::DragFloat3("##position", &position.x, 0.05f))
	{
		if (hasParent) transform->setLocalPosition(position);
		else transform->setPosition(position);
	}

	inspectorLabel("Rotation");
	if (ImGui::DragFloat3("##rotation", &rotation.x, 0.01f))
	{
		if (hasParent) transform->setLocalRotation(rotation);
		else transform->setRotation(rotation);
	}

	inspectorLabel("Scale");
	if (ImGui::DragFloat3("##scale", &scale.x, 0.02f))
	{
		if (hasParent) transform->setLocalScale(scale);
		else transform->setScale(scale);
	}
}

// Малює меню додавання компонента до вибраного об'єкта
void Editor::drawAddComponentMenu(Entity* entity)
{
	if (ImGui::Button("Add Component", ImVec2(-1.0f, 0.0f)))
	{
		ImGui::OpenPopup("AddComponent");
	}

	if (ImGui::BeginPopup("AddComponent"))
	{
		for (const std::string& name : ComponentRegistry::getTypeNames())
		{
			if (ImGui::Selectable(name.c_str()))
			{
				ComponentRegistry::create(name, entity);
			}
		}

		ImGui::EndPopup();
	}
}

// Малює вибір меша та матеріалів для рендер-компонента
void Editor::drawRendererAssets(Renderer* renderer)
{
	Mesh* mesh = renderer->getMesh();

	std::string current = "none";

	if (mesh)
	{
		std::wstring path = mesh->getFullPath();
		current = std::string(path.begin(), path.end());

		size_t slash = current.find_last_of("\\/");
		if (slash != std::string::npos) current = current.substr(slash + 1);
	}

	inspectorLabel("Mesh");
	if (ImGui::BeginCombo("##mesh", current.c_str()))
	{
		for (size_t i = 0; i < mMeshPaths.size(); i++)
		{
			std::string name = mMeshNames[i];

			size_t slash = name.find_last_of("\\/");
			if (slash != std::string::npos) name = name.substr(slash + 1);

			if (ImGui::Selectable(name.c_str()))
			{
				renderer->setMesh(GraphicsEngine::get()->getMeshManager()->createMeshFromFile(mMeshPaths[i].c_str()));
			}
		}

		ImGui::EndCombo();
	}

	inspectorLabel("Cast Shadows");
	ImGui::Checkbox("##castShadows", &renderer->castShadows);

	// Кожен слот матеріалу редагується окремо
	unsigned int slots = renderer->getMaterialCount();

	for (unsigned int slot = 0; slot < slots; slot++)
	{
		drawMaterial(renderer->getMaterial(slot), (int)slot);
	}
}

// Малює поля матеріалу
void Editor::drawMaterial(Material* material, int slot)
{
	if (material == nullptr) return;

	ImGui::PushID(slot);

	char label[64] = {};
	sprintf_s(label, sizeof(label), "Material %d", slot);

	if (ImGui::TreeNode(label))
	{
		inspectorLabel("Color");
		ImGui::ColorEdit4("##color", material->color);
		inspectorLabel("Ambient");
		ImGui::DragFloat("##ambient", &material->ambient, 0.01f, 0.0f, 2.0f);
		inspectorLabel("Smoothness");
		ImGui::DragFloat("##smoothness", &material->smoothness, 0.01f, 0.0f, 1.0f);
		inspectorLabel("Shininess");
		ImGui::DragFloat("##shininess", &material->shininess, 0.5f, 1.0f, 256.0f);
		inspectorLabel("Texture Scale");
		ImGui::DragFloat("##textureScale", &material->textureScale, 0.1f, 0.01f, 200.0f);
		inspectorLabel("Cull Back");
		ImGui::Checkbox("##cullBack", &material->cullBack);
		ImGui::SameLine();
		inspectorLabel("Clamp");
		ImGui::Checkbox("##clamp", &material->clampTexture);

		std::wstring texture = material->getTexturePath();
		std::string current = texture.empty() ? "none" : std::string(texture.begin(), texture.end());

		size_t slash = current.find_last_of("\\/");
		if (slash != std::string::npos) current = current.substr(slash + 1);

		inspectorLabel("Texture");
		if (ImGui::BeginCombo("##texture", current.c_str()))
		{
			for (size_t i = 0; i < mTexturePaths.size(); i++)
			{
				std::string name = mTextureNames[i];

				size_t nameSlash = name.find_last_of("\\/");
				if (nameSlash != std::string::npos) name = name.substr(nameSlash + 1);

				if (ImGui::Selectable(name.c_str()))
				{
					// Матеріал показує лише першу текстуру, тому стару треба прибрати
					while (material->getTextureCount() > 0) material->removeTexture(0);

					material->addTexture(GraphicsEngine::get()->getTextureManager()->createTextureFromFile(mTexturePaths[i].c_str()));
				}
			}

			ImGui::EndCombo();
		}

		ImGui::TreePop();
	}

	ImGui::PopID();
}

// Малює список ресурсів проєкту
void Editor::drawAssets()
{
	placeWindow(10.0f, -210.0f, 300.0f, 200.0f);

	ImGui::Begin("Assets");
	keepWindowOnScreen();

	if (ImGui::CollapsingHeader("Meshes", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (size_t i = 0; i < mMeshNames.size(); i++)
		{
			std::string name = mMeshNames[i];

			size_t slash = name.find_last_of("\\/");
			if (slash != std::string::npos) name = name.substr(slash + 1);

			// Подвійний клік створює у сцені об'єкт з цим мешем
			if (ImGui::Selectable(name.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)
				&& ImGui::IsMouseDoubleClicked(0))
			{
				Entity* entity = new Entity(mCamera.getPosition());
				entity->setName(name);

				MeshRenderer* renderer = entity->addComponent<MeshRenderer>();
				renderer->setMesh(GraphicsEngine::get()->getMeshManager()->createMeshFromFile(mMeshPaths[i].c_str()));

				mSelected = entity;
			}
		}
	}

	if (ImGui::CollapsingHeader("Textures"))
	{
		for (const std::string& path : mTextureNames)
		{
			std::string name = path;

			size_t slash = name.find_last_of("\\/");
			if (slash != std::string::npos) name = name.substr(slash + 1);

			ImGui::TextUnformatted(name.c_str());
		}
	}

	ImGui::End();
}

// Переходить у режим гри, зберігши стан сцени
void Editor::play()
{
	// Запам'ятовуються лише ті дані, які змінює гра. Перебудовувати сцену з файлу не можна:
	// текстовий формат не зберігає посилань між об'єктами, і після відновлення вони були б порожні
	mSnapshot.clear();

	for (Entity* entity : EntityManager::get()->getEntities())
	{
		EntitySnapshot snapshot;

		snapshot.entity = entity;
		snapshot.active = entity->isActiveSelf;
		snapshot.hasParent = entity->getParent() != nullptr;

		Transform* transform = entity->getTransform();

		if (snapshot.hasParent)
		{
			snapshot.position = transform->getLocalPosition();
			snapshot.rotation = transform->getLocalRotation();
			snapshot.scale = transform->getLocalScale();
		}
		else
		{
			snapshot.position = transform->getPosition();
			snapshot.rotation = transform->getRotation();
			snapshot.scale = transform->getScale();
		}

		mSnapshot.push_back(snapshot);
	}

	mPlaying = true;
}

// Повертається до редагування, відновивши збережений стан сцени
void Editor::stop()
{
	mPlaying = false;

	// Об'єкти, що існували до запуску гри
	std::vector<Entity*> known;

	for (const EntitySnapshot& snapshot : mSnapshot)
	{
		known.push_back(snapshot.entity);
	}

	auto isKnown = [&known](Entity* entity)
	{
		for (Entity* candidate : known)
		{
			if (candidate == entity) return true;
		}

		return false;
	};

	// Усе, що гра створила, прибирається. Знищення забирає і нащадків, тому беремо лише
	// верхні з нових об'єктів, інакше дочірні були б видалені двічі
	std::vector<Entity*> spawned;

	for (Entity* entity : EntityManager::get()->getEntities())
	{
		if (isKnown(entity)) continue;

		Entity* parent = entity->getParent();

		if (parent == nullptr || isKnown(parent)) spawned.push_back(entity);
	}

	for (Entity* entity : spawned)
	{
		if (entity == mSelected) mSelected = nullptr;

		entity->destroy();
	}

	// Об'єктам, що пережили гру, повертається стан, який вони мали до її запуску
	for (const EntitySnapshot& snapshot : mSnapshot)
	{
		bool alive = false;

		for (Entity* entity : EntityManager::get()->getEntities())
		{
			if (entity == snapshot.entity) { alive = true; break; }
		}

		if (!alive) continue;

		snapshot.entity->isActiveSelf = snapshot.active;

		Transform* transform = snapshot.entity->getTransform();

		if (snapshot.hasParent)
		{
			transform->setLocalScale(snapshot.scale);
			transform->setLocalRotation(snapshot.rotation);
			transform->setLocalPosition(snapshot.position);
		}
		else
		{
			transform->setScale(snapshot.scale);
			transform->setRotation(snapshot.rotation);
			transform->setPosition(snapshot.position);
		}
	}

	// Компоненти, що пережили гру, ще пам'ятають знищені об'єкти, тому мають скинути свій стан
	EntityManager::get()->notifyEditorStop();

	mSnapshot.clear();

	Input::hideCursor(false);
}

// Обробляє вибір об'єкта мишею та малює маніпулятор
void Editor::updateSelection()
{
	// У режимі гри сценою керує сама гра, тому маніпулятор не показуємо
	if (mPlaying) return;

	ImGuiIO& io = ImGui::GetIO();

	// Поки камера обертається, вибір і маніпулятор лише заважали б
	if (Input::getMouseButton(MB_Right)) return;

	bool overGizmo = mGizmo.update(mSelected);

	// Клік по панелі редактора не має міняти вибір у сцені
	if (io.WantCaptureMouse) return;

	if (overGizmo) return;

	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		mSelected = Gizmo::pick(io.MousePos.x, io.MousePos.y);
	}
}

// Наводить камеру редактора на вибраний об'єкт
void Editor::focusSelected()
{
	if (mSelected == nullptr) return;

	float radius = 1.0f;

	if (Renderer* renderer = mSelected->getComponent<Renderer>())
	{
		if (renderer->getMesh())
		{
			Vector3 scale = mSelected->getTransform()->getScale();

			float maxScale = fabsf(scale.x);
			if (fabsf(scale.y) > maxScale) maxScale = fabsf(scale.y);
			if (fabsf(scale.z) > maxScale) maxScale = fabsf(scale.z);

			radius = renderer->getMesh()->getBoundsRadius() * maxScale;
		}
	}

	mCamera.focusOn(mSelected->getTransform()->getPosition(), radius);
}

// Знищує вибраний об'єкт
void Editor::deleteSelected()
{
	if (mSelected == nullptr) return;

	mSelected->destroy();
	mSelected = nullptr;
}
