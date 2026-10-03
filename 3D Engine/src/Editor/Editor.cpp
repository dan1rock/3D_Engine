#include "Editor.h"
#include "ComponentRegistry.h"
#include "SceneSerializer.h"
#include "InspectorVisitor.h"
#include "PrefabLibrary.h"
#include "MaterialLibrary.h"
#include "SceneManager.h"

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
#include "Texture.h"
#include "GraphicsEngine.h"
#include "GlobalResources.h"
#include "ShadowMap.h"
#include "PostProcessing.h"
#include "Input.h"
#include "Properties.h"
#include <unordered_set>

#include "imgui.h"

#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>
#include <iostream>
#include <algorithm>

namespace filesystem = std::experimental::filesystem;

// Колір префабів у вигляді, зручному для ImGui
static ImVec4 prefabColor(float alpha = 1.0f)
{
	return ImVec4(PREFAB_COLOR[0], PREFAB_COLOR[1], PREFAB_COLOR[2], alpha);
}

// Перевіряє, чи об'єкт ще існує, не розіменовуючи вказівник
static bool isAlive(Entity* entity)
{
	for (Entity* candidate : EntityManager::get()->getEntities())
	{
		if (candidate == entity) return true;
	}

	return false;
}

// Дописує об'єкт і всіх його нащадків у порядку дерева
static void collectSubtree(Entity* entity, std::vector<Entity*>& out)
{
	out.push_back(entity);

	for (Entity* child : *entity->getChildren())
	{
		collectSubtree(child, out);
	}
}

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

	mGrid.init();
	mOutline.init();
	mPreview.init();

	refreshAssetLists();
}

// Перечитує перелік мешів і текстур у теках ресурсів
void Editor::refreshAssetLists()
{
	mMeshPaths.clear();
	mMeshNames.clear();
	mTexturePaths.clear();
	mTextureNames.clear();

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
			std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);

			// До списку потрапляють лише ті типи файлів, які рушій уміє завантажити
			bool isMesh = extension == L".obj" || extension == L".fbx" || extension == L".dae" || extension == L".gltf" || extension == L".glb";
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
	// Сховані в редакторі об'єкти не малюються, поки сценою керує редактор: під час редагування й на паузі
	EntityManager::get()->hideEditorHidden = mEnabled && (!mPlaying || mPaused);

	// У режимі префаба сховати редактор не можна: тоді сцена почала б грати, а в ній лише префаб
	if (Input::getKeyDown(VK_F1) && !isPrefabMode()) mEnabled = !mEnabled;

	// Сцену можуть замінити й тоді, коли редактор сховано, тож це відстежується завжди
	trackSceneLoads();

	if (!mEnabled) return;

	syncSelection();

	// Вибір перед будь-якою зміною цього кадру: його поверне скасування цієї зміни
	if (!mPlaying && mUndoCheckFrames == 0 && !ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsAnyItemActive())
	{
		captureSelection(mUndoSelection, mUndoActive);
	}

	// Поза режимом гри та на паузі сценою керує камера редактора
	if (!mPlaying || mPaused)
	{
		if (!Input::getMouseButton(MB_Right) && Input::isCursorHidden())
		{
			Input::hideCursor(false);
		}

		mCamera.update();
	}

	// Одна мініатюра за кадр малюється до інтерфейсу, тож вона з'являється вже в цьому кадрі
	mPreview.renderPending();

	drawToolbar();
	drawHierarchy();
	drawInspector();
	drawAssets();
	drawPrefabModeBar();

	syncSelection();
	updateSelection();
	dropPrefabIntoScene();

	// Гарячі клавіші працюють лише тоді, коли ввід не перехоплює поле тексту
	ImGuiIO& io = ImGui::GetIO();

	handleShortcuts();

	if (!io.WantCaptureKeyboard && !io.KeyCtrl)
	{
		if (Input::getKeyDown('F')) focusSelected();
		if (Input::getKeyDown(VK_DELETE)) deleteSelected();

		// W, E, R перемикають режим маніпулятора. Поки тримають праву кнопку,
		// ці ж клавіші ведуть камеру, тому режим тоді не міняється
		if (!Input::getMouseButton(MB_Right))
		{
			if (Input::getKeyDown('W')) mGizmo.mode = GizmoMode::Translate;
			if (Input::getKeyDown('E')) mGizmo.mode = GizmoMode::Rotate;
			if (Input::getKeyDown('R')) mGizmo.mode = GizmoMode::Scale;
			if (Input::getKeyDown('X')) mGizmo.local = !mGizmo.local;
		}
	}

	applyPrefabModeRequests();
	applySceneRequests();

	// Правку завершено, коли відпустили кнопку миші або поле перестало бути активним
	bool itemActive = ImGui::IsAnyItemActive();

	if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) || (mWasItemActive && !itemActive)) mUndoCheckFrames = 2;

	mWasItemActive = itemActive;

	// Сцена перевіряється, лише коли нічого не тягнуть і не редагують, тож перетягування стає одним кроком
	if (mUndoCheckFrames > 0 && !ImGui::IsMouseDown(ImGuiMouseButton_Left) && !itemActive && !mGizmo.isDragging())
	{
		recordUndo();
		mUndoCheckFrames--;

		// Завершена правка матеріалу одразу потрапляє в його файл
		MaterialLibrary::get()->saveChanged();
	}
}

// Помічає, що сцену замінили, і забуває вибір, якщо вибраний об'єкт знищено
void Editor::trackSceneLoads()
{
	unsigned int loadCount = SceneManager::get()->getLoadCount();

	if (loadCount != mSeenLoadCount)
	{
		mSeenLoadCount = loadCount;

		// Під час гри сцени перемикають компоненти, а зупинка однаково поверне знімок, тож
		// збереженим стає лише те, що відкрили для редагування
		if (!mPlaying) markSceneSaved();

		// Історія скасування належить попередній сцені
		if (!mPlaying) resetUndo();
	}

	// Гра чи перемикання сцени могли знищити вибраний об'єкт
	if (mSelected && !isAlive(mSelected)) mSelected = nullptr;
}

// Просить відкрити сцену з файлу або, з createNew, створити нову
void Editor::requestScene(const std::string& path, bool createNew)
{
	mSceneRequest = path;
	mNewSceneRequest = createNew;

	// Без змін питати нема про що, і перехід відбувається одразу
	if (isSceneDirty())
	{
		mSceneDecision = 0;
		mShowScenePrompt = true;
	}
	else
	{
		mSceneDecision = 1;
	}
}

// Виконує відкладене відкриття чи створення сцени, коли всі панелі вже намальовано
void Editor::applySceneRequests()
{
	if (mSceneRequest.empty() && !mNewSceneRequest) return;

	// Рішення про незбережені зміни ще не прийняте
	if (mSceneDecision == 0) return;

	std::string path = mSceneRequest;
	bool createNew = mNewSceneRequest;
	int decision = mSceneDecision;

	mSceneRequest.clear();
	mNewSceneRequest = false;
	mSceneDecision = 0;

	// Сцену не міняють під час гри та в режимі префаба, навіть якщо запит лишився з раніше
	if (decision < 0 || mPlaying || isPrefabMode()) return;

	if (decision == 2) saveScene();

	// Старі об'єкти зараз зникнуть разом з вибором
	mSelected = nullptr;

	if (createNew) SceneManager::get()->createScene("New Scene");
	else SceneManager::get()->openScene(path);

	// Навіть якщо файл не прочитався, відкритою лишається попередня сцена, тож рахунок узгоджуємо
	mSeenLoadCount = SceneManager::get()->getLoadCount();

	markSceneSaved();
	resetUndo();
}

// Малює питання про збереження сцени перед переходом до іншої
void Editor::drawScenePrompt()
{
	if (mShowScenePrompt)
	{
		ImGui::OpenPopup("Save Scene");
		mShowScenePrompt = false;
	}

	if (ImGui::BeginPopupModal("Save Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Scene '%s' has unsaved changes.", SceneManager::get()->getActiveSceneName().c_str());

		if (ImGui::Button("Save")) { mSceneDecision = 2; ImGui::CloseCurrentPopup(); }

		ImGui::SameLine();

		if (ImGui::Button("Don't Save")) { mSceneDecision = 1; ImGui::CloseCurrentPopup(); }

		ImGui::SameLine();

		if (ImGui::Button("Cancel")) { mSceneDecision = -1; ImGui::CloseCurrentPopup(); }

		ImGui::EndPopup();
	}
}

// Перевіряє, чи сцена змінилася від останнього відкриття чи збереження
bool Editor::isSceneDirty()
{
	// Під час гри та в режимі префаба у світі не та сцена, що у файлі, тож лишається остання оцінка
	if (mPlaying || isPrefabMode()) return mSceneDirty;

	mSceneDirty = SceneSerializer::serialize(true) != mSavedScene;

	return mSceneDirty;
}

// Запам'ятовує поточний стан сцени як збережений
void Editor::markSceneSaved()
{
	mSavedScene = SceneSerializer::serialize(true);
	mSceneDirty = false;
	mDirtyCheckFrame = 0;
}

// Зберігає відкриту сцену у її файл
void Editor::saveScene()
{
	if (SceneManager::get()->saveScene())
	{
		std::cout << "Scene saved to " << SceneManager::get()->getActiveScenePath() << std::endl;
		markSceneSaved();
	}
	else
	{
		std::cout << "Failed to save scene" << std::endl;
	}
}

// Перевіряє, чи сцена зараз програється, а не редагується
bool Editor::isPlaying() const
{
	return mPlaying;
}

// Перевіряє, чи гру поставлено на паузу
bool Editor::isPaused() const
{
	return mPaused;
}

// Перевіряє, чи цього кадру сцена оновлюється: гра йде без паузи, або редактор сховано
bool Editor::isSimulating() const
{
	// Пауза тримає гру і тоді, коли редактор сховано: інакше F1 непомітно зняв би її
	if (mPlaying) return !mPaused;

	return !mEnabled;
}

// Ставить гру на паузу або знімає з неї; на паузі сцену знову можна редагувати
void Editor::setPaused(bool paused)
{
	if (!mPlaying || paused == mPaused) return;

	mPaused = paused;

	if (mPaused)
	{
		// Камера редактора стає туди, звідки дивилася гра, щоб зображення не стрибнуло
		constant* constantData = GraphicsEngine::get()->getGlobalResources()->getConstantData();

		mCamera.setFromViewMatrix(constantData->view);

		// Гра могла сховати курсор, а на паузі ним вибирають і тягнуть об'єкти
		Input::hideCursor(false);
	}
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
		// Гра запускає сцену, а в режимі префаба сцени немає — лише сам префаб
		ImGui::BeginDisabled(isPrefabMode());
		if (ImGui::Button("Play", ImVec2(70, 0))) play();
		ImGui::EndDisabled();
	}

	ImGui::SameLine();

	// Пауза має зміст лише під час гри, тож поза нею кнопка сіра. Натиснута пауза підсвічена,
	// як кнопка-перемикач
	ImGui::BeginDisabled(!mPlaying);

	if (mPaused) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

	bool togglePause = ImGui::Button(mPaused ? "Resume" : "Pause", ImVec2(70, 0));

	if (mPaused) ImGui::PopStyleColor();

	if (togglePause) setPaused(!mPaused);

	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::TextUnformatted(mPlaying ? (mPaused ? "Paused" : "Playing") : isPrefabMode() ? "Prefab Mode" : "Editing");

	ImGui::Separator();

	// Зірочка позначає незбережені зміни сцени
	if (!mPlaying && !isPrefabMode() && ++mDirtyCheckFrame >= 15)
	{
		mDirtyCheckFrame = 0;
		isSceneDirty();
	}

	ImGui::Text("Scene: %s%s", SceneManager::get()->getActiveSceneName().c_str(), mSceneDirty ? " *" : "");

	// У режимі префаба зберегти можна лише сам префаб: інакше у файл сцени потрапив би він один.
	// Під час гри теж: у файл потрапив би стан гри, а не відредагована сцена
	ImGui::BeginDisabled(isPrefabMode() || mPlaying);

	if (ImGui::Button("Save Scene")) saveScene();

	ImGui::SameLine();

	if (ImGui::Button("New Scene")) requestScene(std::string(), true);

	ImGui::EndDisabled();

	drawScenePrompt();

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

		// Сила навколишнього світла й відбиттів неба
		ImGui::SliderFloat("Environment Intensity", &GraphicsEngine::get()->environmentIntensity, 0.0f, 8.0f);
		ImGui::SliderFloat("Reflection Intensity", &GraphicsEngine::get()->reflectionIntensity, 0.0f, 1.0f);
	}

	ImGui::Separator();
	ImGui::TextUnformatted("F1 hide editor, F focus, Del delete");
	ImGui::TextUnformatted("Click to select, Ctrl/Shift+click to add");
	ImGui::TextUnformatted("W/E/R move/rotate/scale, X local");
	ImGui::TextUnformatted("Ctrl+Z/Y undo/redo, Ctrl+S save");
	ImGui::TextUnformatted("Ctrl+D duplicate, Ctrl+C/V copy/paste");
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

	mHierarchyRowsBuilding.clear();

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

	// Порожнє місце під деревом приймає об'єкт як кореневий, у кінець списку
	ImVec2 space = ImGui::GetContentRegionAvail();

	ImGui::InvisibleButton("##HierarchyEnd", ImVec2(space.x > 1.0f ? space.x : 1.0f, space.y > 24.0f ? space.y : 24.0f));

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(HIERARCHY_PAYLOAD))
		{
			Entity* dragged = *(Entity* const*)payload->Data;

			mPendingDrop = { dragged, nullptr, DropZone::Root, std::string() };
		}

		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(PREFAB_PAYLOAD))
		{
			mPendingDrop = { nullptr, nullptr, DropZone::Root, std::string((const char*)payload->Data) };
		}

		// У режимі префаба кореневого місця для моделі немає: у префабі лише один корінь
		if (!isPrefabMode())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(MESH_PAYLOAD))
			{
				mPendingDrop = { nullptr, nullptr, DropZone::Root, std::string(), std::wstring((const wchar_t*)payload->Data) };
			}
		}

		ImGui::EndDragDropTarget();
	}

	mHierarchyRows = mHierarchyRowsBuilding;

	// Клік по одному з кількох вибраних без перетягування лишає вибраним лише його
	if (mPendingSingleSelect && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		ImGuiIO& io = ImGui::GetIO();

		if (io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] <= io.MouseDragThreshold * io.MouseDragThreshold) selectOnly(mPendingSingleSelect);

		mPendingSingleSelect = nullptr;
	}

	applyDrop();

	ImGui::End();
}

// Малює один вузол дерева разом з його дочірніми об'єктами
void Editor::drawEntityNode(Entity* entity)
{
	// Перекриття дозволяє замку в кінці рядка отримувати кліки замість самого рядка
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap;

	if (isSelected(entity)) flags |= ImGuiTreeNodeFlags_Selected;

	mHierarchyRowsBuilding.push_back(entity);
	if (entity->getChildren()->empty()) flags |= ImGuiTreeNodeFlags_Leaf;

	// Неактивні об'єкти показуються приглушеним кольором, а частини префабів — блакитним
	bool active = entity->isActiveSelf;
	bool inPrefab = PrefabLibrary::findInstanceRoot(entity) != nullptr;
	bool colored = !active || inPrefab;

	if (inPrefab) ImGui::PushStyleColor(ImGuiCol_Text, prefabColor(active ? 1.0f : 0.5f));
	else if (!active) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.55f, 1.0f));

	// Батько, у який щойно поклали об'єкт, розгортається, щоб цей об'єкт було видно
	if (entity == mExpandEntity)
	{
		ImGui::SetNextItemOpen(true);
		mExpandEntity = nullptr;
	}

	bool open = ImGui::TreeNodeEx((void*)entity, flags, "%s", entity->getName().c_str());

	if (colored) ImGui::PopStyleColor();

	// Клік по оку чи замку в кінці рядка лише перемикає їх і не вибирає сам рядок
	ImVec2 rowMin = ImGui::GetItemRectMin();
	ImVec2 rowMax = ImGui::GetItemRectMax();
	bool rowHovered = ImGui::IsItemHovered();
	float rowHeight = rowMax.y - rowMin.y;
	bool overToggles = ImGui::GetIO().MousePos.x >= rowMax.x - rowHeight * 2.0f - 4.0f;

	if (ImGui::IsItemClicked() && !overToggles) clickHierarchyRow(entity);

	// Рядок можна тягнути мишею, щоб змінити місце об'єкта в дереві; вибраний тягне за собою весь вибір
	if (ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload(HIERARCHY_PAYLOAD, &entity, sizeof(Entity*));

		if (isSelected(entity) && mSelection.size() > 1) ImGui::Text("%d objects", (int)topLevelSelection().size());
		else ImGui::TextUnformatted(entity->getName().c_str());

		ImGui::EndDragDropSource();
	}

	drawDropTarget(entity);

	drawVisibilityToggle(entity, rowMin, rowMax, rowHovered);
	drawLockToggle(entity, rowMin, rowMax, rowHovered);

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

// Малює око перед замком у рядку дерева: видно, коли об'єкт схований або рядок під курсором, клік ховає чи показує об'єкт у вікні сцени
void Editor::drawVisibilityToggle(Entity* entity, const ImVec2& rowMin, const ImVec2& rowMax, bool rowHovered)
{
	float size = rowMax.y - rowMin.y;
	float x = rowMax.x - size * 2.0f - 4.0f;

	ImGui::SameLine();
	ImGui::SetCursorScreenPos(ImVec2(x, rowMin.y));

	ImGui::PushID("visibility");
	bool clicked = ImGui::InvisibleButton("##visibility", ImVec2(size, size));
	bool hovered = ImGui::IsItemHovered();
	ImGui::PopID();

	if (clicked) entity->hiddenInEditor = !entity->hiddenInEditor;

	if (hovered) ImGui::SetTooltip(entity->hiddenInEditor ? "Hidden in the Scene view (still visible in play mode)" : "Hide in the Scene view");

	// Схований через батька показується блідіше, а видимий - лише коли рядок під курсором
	bool inherited = !entity->hiddenInEditor && entity->isHiddenInEditor();
	bool hidden = entity->hiddenInEditor || inherited;

	if (!hidden && !rowHovered && !hovered) return;

	ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Text);

	if (inherited) color.w = 0.45f;
	else if (!hidden) color.w = hovered ? 0.8f : 0.35f;

	ImU32 packed = ImGui::GetColorU32(color);
	ImDrawList* draw = ImGui::GetWindowDrawList();

	// Око - мигдалеподібний контур із двох кривих і зіниця; у схованого зіниці немає, а око перекреслене
	float centerX = x + size * 0.5f;
	float centerY = rowMin.y + size * 0.52f;
	float halfWidth = size * 0.36f;
	float halfHeight = size * 0.36f;

	draw->AddBezierQuadratic(ImVec2(centerX - halfWidth, centerY), ImVec2(centerX, centerY - halfHeight), ImVec2(centerX + halfWidth, centerY), packed, 1.4f);
	draw->AddBezierQuadratic(ImVec2(centerX - halfWidth, centerY), ImVec2(centerX, centerY + halfHeight), ImVec2(centerX + halfWidth, centerY), packed, 1.4f);

	if (hidden) draw->AddLine(ImVec2(centerX - halfWidth * 0.8f, centerY + halfHeight * 0.7f), ImVec2(centerX + halfWidth * 0.8f, centerY - halfHeight * 0.7f), packed, 1.6f);
	else draw->AddCircleFilled(ImVec2(centerX, centerY), size * 0.11f, packed);
}

// Малює замок у кінці рядка дерева: видно, коли об'єкт замкнений або рядок під курсором, клік перемикає замок
void Editor::drawLockToggle(Entity* entity, const ImVec2& rowMin, const ImVec2& rowMax, bool rowHovered)
{
	float size = rowMax.y - rowMin.y;

	ImGui::SameLine();
	ImGui::SetCursorScreenPos(ImVec2(rowMax.x - size - 2.0f, rowMin.y));

	ImGui::PushID("lock");
	bool clicked = ImGui::InvisibleButton("##lock", ImVec2(size, size));
	bool hovered = ImGui::IsItemHovered();
	ImGui::PopID();

	if (clicked) entity->lockedInEditor = !entity->lockedInEditor;

	if (hovered) ImGui::SetTooltip(entity->lockedInEditor ? "Locked: can't be picked in the Scene view" : "Lock picking in the Scene view");

	// Замкнений через батька показується блідіше, а відімкнений - лише коли рядок під курсором
	bool inherited = !entity->lockedInEditor && entity->isLockedInEditor();
	bool locked = entity->lockedInEditor || inherited;

	if (!locked && !rowHovered && !hovered) return;

	ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Text);

	if (inherited) color.w = 0.45f;
	else if (!locked) color.w = hovered ? 0.8f : 0.35f;

	ImU32 packed = ImGui::GetColorU32(color);
	ImDrawList* draw = ImGui::GetWindowDrawList();

	// Корпус замка - прямокутник, дужка - півколо над ним; у відімкненого дужка піднята й відкрита справа
	float x = rowMax.x - size - 2.0f;
	float y = rowMin.y;
	float centerX = x + size * 0.5f;
	float bodyTop = y + size * 0.48f;
	float radius = size * 0.18f;
	float lift = locked ? 0.0f : size * 0.12f;

	draw->AddRectFilled(ImVec2(centerX - size * 0.26f, bodyTop), ImVec2(centerX + size * 0.26f, y + size * 0.86f), packed, 1.5f);

	draw->PathArcTo(ImVec2(centerX, bodyTop - lift), radius, 3.14159265f, locked ? 6.28318531f : 5.4f, 12);
	draw->PathStroke(packed, 0, 1.6f);

	// Піднята дужка лівою ніжкою досі тримається за корпус
	if (!locked) draw->AddLine(ImVec2(centerX - radius, bodyTop - lift), ImVec2(centerX - radius, bodyTop), packed, 1.6f);
}

// Приймає перетягнутий об'єкт на рядок дерева: над ним, під ним чи всередину
void Editor::drawDropTarget(Entity* target)
{
	if (!ImGui::BeginDragDropTarget()) return;

	ImVec2 min = ImGui::GetItemRectMin();
	ImVec2 max = ImGui::GetItemRectMax();

	float height = max.y - min.y;
	float mouseY = ImGui::GetIO().MousePos.y;

	// Верхня й нижня чверті рядка ставлять об'єкт поруч, а середина — всередину
	DropZone zone = DropZone::Inside;

	if (mouseY < min.y + height * 0.25f) zone = DropZone::Before;
	else if (mouseY > max.y - height * 0.25f) zone = DropZone::After;

	// Вміст приймається ще до того, як кнопку відпустили: так можна намалювати підказку,
	// куди саме ляже об'єкт, а стандартну рамку ImGui замінюємо власною лінією
	const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(HIERARCHY_PAYLOAD,
		ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);

	if (payload)
	{
		Entity* dragged = *(Entity* const*)payload->Data;
		Entity* newParent = zone == DropZone::Inside ? target : target->getParent();

		if (dragged != target && canDrop(dragged, newParent))
		{
			ImDrawList* draw = ImGui::GetWindowDrawList();
			ImU32 color = ImGui::GetColorU32(ImGuiCol_DragDropTarget);

			if (zone == DropZone::Inside)
			{
				draw->AddRect(min, max, color, 0.0f, 0, 2.0f);
			}
			else
			{
				float y = zone == DropZone::Before ? min.y : max.y;

				draw->AddLine(ImVec2(min.x, y), ImVec2(max.x, y), color, 2.0f);
			}

			if (payload->IsDelivery()) mPendingDrop = { dragged, target, zone, std::string() };
		}
	}

	// Префаб з панелі ресурсів кладеться тими самими зонами рядка, що й звичайний об'єкт
	const ImGuiPayload* prefabPayload = ImGui::AcceptDragDropPayload(PREFAB_PAYLOAD,
		ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);

	if (prefabPayload)
	{
		Entity* newParent = zone == DropZone::Inside ? target : target->getParent();

		if (canDropPrefab(newParent))
		{
			ImDrawList* draw = ImGui::GetWindowDrawList();
			ImU32 color = ImGui::GetColorU32(ImGuiCol_DragDropTarget);

			if (zone == DropZone::Inside) draw->AddRect(min, max, color, 0.0f, 0, 2.0f);
			else draw->AddLine(ImVec2(min.x, zone == DropZone::Before ? min.y : max.y), ImVec2(max.x, zone == DropZone::Before ? min.y : max.y), color, 2.0f);

			if (prefabPayload->IsDelivery()) mPendingDrop = { nullptr, target, zone, std::string((const char*)prefabPayload->Data) };
		}
	}

	// Модель з панелі ресурсів кладеться так само, а в режимі префаба - лише всередину його частин
	const ImGuiPayload* modelPayload = ImGui::AcceptDragDropPayload(MESH_PAYLOAD,
		ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);

	if (modelPayload)
	{
		Entity* newParent = zone == DropZone::Inside ? target : target->getParent();

		if (!isPrefabMode() || newParent != nullptr)
		{
			ImDrawList* draw = ImGui::GetWindowDrawList();
			ImU32 color = ImGui::GetColorU32(ImGuiCol_DragDropTarget);

			if (zone == DropZone::Inside) draw->AddRect(min, max, color, 0.0f, 0, 2.0f);
			else draw->AddLine(ImVec2(min.x, zone == DropZone::Before ? min.y : max.y), ImVec2(max.x, zone == DropZone::Before ? min.y : max.y), color, 2.0f);

			if (modelPayload->IsDelivery()) mPendingDrop = { nullptr, target, zone, std::string(), std::wstring((const wchar_t*)modelPayload->Data) };
		}
	}

	ImGui::EndDragDropTarget();
}

// Перевіряє, чи можна зробити об'єкт дочірнім для вказаного батька (nullptr - корінь)
bool Editor::canDrop(Entity* dragged, Entity* newParent) const
{
	// У префабі корінь один: він не переїжджає, а все інше лишається під ним
	if (isPrefabMode() && (dragged == mPrefabRoot || newParent == nullptr)) return false;

	// Об'єкт, що переживає зміну сцени, мусить лишатися кореневим: інакше його знищив би батько
	if (dragged->dontDestroyOnLoad && newParent != nullptr) return false;

	for (Entity* ancestor = newParent; ancestor; ancestor = ancestor->getParent())
	{
		// Власний нащадок не може стати батьком: вийшов би цикл
		if (ancestor == dragged) return false;

		// Такий об'єкт не потрапляє у файл сцени, тож і покладені в нього діти зникли б із файлу
		if (ancestor->dontDestroyOnLoad) return false;
	}

	return true;
}

// Перевіряє, чи можна покласти новий екземпляр префаба під вказаного батька
bool Editor::canDropPrefab(Entity* newParent) const
{
	// Вкладених префабів немає, тож у режимі префаба інші префаби не кладуться
	if (isPrefabMode()) return false;

	// Такий об'єкт не потрапляє у файл сцени, тож і покладені в нього діти зникли б із файлу
	for (Entity* ancestor = newParent; ancestor; ancestor = ancestor->getParent())
	{
		if (ancestor->dontDestroyOnLoad) return false;
	}

	return true;
}

// Складає матрицю з положення, повороту й масштабу так само, як трансформація об'єкта
static Matrix composeTransform(const Vector3& position, const Vector3& rotation, const Vector3& scale)
{
	Matrix matrix;
	matrix.setIdentity();
	matrix.setScale(scale);
	matrix.setRotation(rotation);
	matrix.setTranslation(position);

	return matrix;
}

// Створює в сцені об'єкти за вузлами моделі: кожен вузол стає об'єктом, а його сітки - рендер-компонентом з матеріалами моделі
Entity* Editor::instantiateModel(const std::wstring& path, Entity* parent)
{
	Mesh* model = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(path.c_str());

	if (model == nullptr) return nullptr;

	std::string name = filesystem::path(path).stem().string();
	const std::vector<ModelNode>& nodes = model->getModelNodes();
	MaterialLibrary* library = MaterialLibrary::get();

	// Модель без вузлів стає одним об'єктом з цілим мешем
	if (nodes.empty())
	{
		Entity* entity = new Entity();
		entity->setName(name);

		if (parent) entity->setParent(parent);

		MeshRenderer* renderer = entity->addComponent<MeshRenderer>();
		renderer->setMesh(model);
		library->applyModelMaterials(renderer);

		return entity;
	}

	// Корінь без сіток з єдиною дитиною зливається з нею, щоб модель з одного об'єкта не мала зайвого рівня
	int rootChildren = 0;

	for (const ModelNode& node : nodes)
	{
		if (node.parent == 0) rootChildren++;
	}

	bool merge = !nodes[0].hasMeshes && rootChildren == 1;

	std::vector<Entity*> created(nodes.size(), nullptr);
	Entity* root = nullptr;

	for (size_t i = 0; i < nodes.size(); i++)
	{
		const ModelNode& node = nodes[i];

		if (merge && i == 0) continue;

		bool isRoot = i == 0 || (merge && node.parent == 0);

		Vector3 position = node.position;
		Vector3 rotation = node.rotation;
		Vector3 scale = node.scale;

		// Злитий корінь несе й перетворення кореня моделі, тож частина стоїть так само, як у файлі
		if (isRoot && merge)
		{
			Matrix combined = composeTransform(node.position, node.rotation, node.scale);
			combined *= composeTransform(nodes[0].position, nodes[0].rotation, nodes[0].scale);

			rotation = combined.getRotation();
			scale = combined.getScale();
		}

		// Корінь стає туди, куди поклали модель, тож зсув кореня з файлу не береться
		if (isRoot) position = Vector3(0.0f, 0.0f, 0.0f);

		Entity* entity = new Entity();
		entity->setName(isRoot ? name : node.name);

		Entity* entityParent = isRoot ? parent : created[node.parent];

		if (entityParent) entity->setParent(entityParent);

		// Дочірній зберігає розташування відносно батька, корінь без батька - у світі
		Transform* transform = entity->getTransform();

		if (entityParent)
		{
			transform->setLocalScale(scale);
			transform->setLocalRotation(rotation);
			transform->setLocalPosition(position);
		}
		else
		{
			transform->setScale(scale);
			transform->setRotation(rotation);
			transform->setPosition(position);
		}

		// Сітки вузла - окремий меш модель::вузол з матеріалами моделі в його слотах
		if (node.hasMeshes)
		{
			MeshRenderer* renderer = entity->addComponent<MeshRenderer>();
			renderer->setMesh(GraphicsEngine::get()->getMeshManager()->createMeshFromFile((path + L"::" + std::wstring(node.key.begin(), node.key.end())).c_str()));
			library->applyModelMaterials(renderer);
		}

		created[i] = entity;

		if (isRoot) root = entity;
	}

	// Усі вузли вже прочитано, тож збережена для них модель більше не потрібна
	Mesh::releaseImportCache();

	EntityManager::get()->sortByHierarchy();

	return root;
}

// Виконує відкладене перетягування, коли дерево вже намальоване
void Editor::applyDrop()
{
	PendingDrop drop = mPendingDrop;
	mPendingDrop = PendingDrop();

	bool isPrefab = !drop.prefab.empty();
	bool isModel = !drop.model.empty();

	if (drop.dragged == nullptr && !isPrefab && !isModel) return;

	// Під час перетягування гра могла знищити будь-який з цих об'єктів
	if (drop.dragged && !isAlive(drop.dragged)) return;
	if (drop.target && !isAlive(drop.target)) return;

	// Вибраний рядок переносить увесь вибір, окрім самої цілі
	std::vector<Entity*> items;

	if (drop.dragged && isSelected(drop.dragged) && mSelection.size() > 1) items = topLevelSelection();
	else if (drop.dragged) items.push_back(drop.dragged);

	items.erase(std::remove(items.begin(), items.end(), drop.target), items.end());

	if (!isPrefab && !isModel && items.empty()) return;

	Entity* newParent = nullptr;
	Entity* before = nullptr;

	if (drop.zone == DropZone::Inside)
	{
		newParent = drop.target;
	}
	else if (drop.zone != DropZone::Root)
	{
		newParent = drop.target->getParent();

		// Сусіди за деревом: діти батька або кореневі об'єкти, вже без самого перетягнутого
		std::vector<Entity*> siblings;

		if (newParent)
		{
			siblings.assign(newParent->getChildren()->begin(), newParent->getChildren()->end());
		}
		else
		{
			for (Entity* entity : EntityManager::get()->getEntities())
			{
				if (entity->getParent() == nullptr) siblings.push_back(entity);
			}
		}

		for (Entity* item : items)
		{
			siblings.erase(std::remove(siblings.begin(), siblings.end(), item), siblings.end());
		}

		auto position = std::find(siblings.begin(), siblings.end(), drop.target);

		if (drop.zone == DropZone::Before) before = drop.target;
		else if (position != siblings.end() && position + 1 != siblings.end()) before = *(position + 1);
	}

	if (isPrefab)
	{
		if (!canDropPrefab(newParent)) return;

		// Новий екземпляр стає на місце перетягнутого об'єкта в дереві
		drop.dragged = PrefabLibrary::get()->instantiate(drop.prefab, newParent);

		if (drop.dragged == nullptr) return;

		// Дочірній екземпляр стоїть у початку координат батька, а кореневий — перед камерою
		if (newParent == nullptr) drop.dragged->getTransform()->setPosition(mCamera.getSpawnPoint());

		mSelected = drop.dragged;
		items.assign(1, drop.dragged);
	}

	if (isModel)
	{
		if (isPrefabMode() && newParent == nullptr) return;

		// Модель стає на місце в дереві так само, як новий екземпляр префаба
		drop.dragged = instantiateModel(drop.model, newParent);

		if (drop.dragged == nullptr) return;

		if (newParent == nullptr) drop.dragged->getTransform()->setPosition(mCamera.getSpawnPoint());

		mSelected = drop.dragged;
		items.assign(1, drop.dragged);
	}

	// Кожен стає перед тим самим сусідом, тож порядок перенесених зберігається
	for (Entity* item : items)
	{
		if (!canDrop(item, newParent)) continue;

		// Об'єкт лишається там, де був у світі, змінюється лише його батько
		item->setParent(newParent, true);

		if (newParent) newParent->moveChildBefore(item, before);
		else EntityManager::get()->moveRootBefore(item, before);
	}

	EntityManager::get()->sortByHierarchy();

	if (drop.zone == DropZone::Inside) mExpandEntity = newParent;
}

// Повертає назву зміненої властивості для списку змін: об'єкт, компонент і поле
static std::string describeOverride(const std::string& key, Entity* instanceRoot)
{
	size_t bar = key.find('|');

	std::string path = key.substr(0, bar);
	std::string field = bar == std::string::npos ? key : key.substr(bar + 1);

	// Шлях — номери дочірніх від кореня, тож за ним можна знайти сам об'єкт і показати його ім'я
	Entity* entity = instanceRoot;
	size_t position = 0;

	while (entity && position < path.size())
	{
		size_t next = path.find('/', position + 1);
		int ordinal = atoi(path.substr(position + 1, next == std::string::npos ? std::string::npos : next - position - 1).c_str());

		Entity* child = nullptr;
		int index = 0;

		for (Entity* candidate : *entity->getChildren())
		{
			if (index++ == ordinal) { child = candidate; break; }
		}

		entity = child;
		position = next == std::string::npos ? path.size() : next;
	}

	// Номер компонента показуємо, лише коли однотипних кілька
	size_t hash = field.find("#0.");

	if (hash != std::string::npos) field.erase(hash, 2);

	std::string owner = entity ? entity->getName() : std::string("(removed object)");

	return path.empty() ? field : owner + " / " + field;
}

// Малює панель префаба у вибраного екземпляра: застосувати, скасувати зміни, розірвати зв'язок
void Editor::drawPrefabBar()
{
	if (mInstanceRoot == nullptr) return;

	PrefabLibrary* library = PrefabLibrary::get();

	std::string name = PrefabLibrary::getName(mInstanceRoot->prefabAsset);

	ImGui::Separator();

	// Керувати префабом можна лише з кореня екземпляра
	if (mInstanceRoot != mSelected)
	{
		ImGui::TextColored(prefabColor(), "Part of prefab %s", name.c_str());
		ImGui::TextDisabled("Select '%s' to apply or revert", mInstanceRoot->getName().c_str());
		return;
	}

	// Файл префаба зник: з'єднати нічого, але об'єкт однаково відновився зі збережених даних
	if (library->getData(mInstanceRoot->prefabAsset) == nullptr)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.45f, 1.0f), "Missing prefab: %s", mInstanceRoot->prefabAsset.c_str());

		if (ImGui::Button("Unpack")) library->unpack(mInstanceRoot);

		return;
	}

	ImGui::TextColored(prefabColor(), "Prefab: %s", name.c_str());
	ImGui::SameLine();

	if (mOverrides.empty()) ImGui::TextDisabled("(no overrides)");
	else ImGui::TextDisabled("(%d overrides)", (int)mOverrides.size());

	// Під час гри файл префаба не змінюється: після зупинки сцена повернеться, а файл лишився б новим
	ImGui::BeginDisabled(mPlaying || mOverrides.empty());
	bool apply = ImGui::Button("Apply");
	ImGui::EndDisabled();

	ImGui::SameLine();

	ImGui::BeginDisabled(mOverrides.empty());
	bool revert = ImGui::Button("Revert");
	ImGui::EndDisabled();

	ImGui::SameLine();

	bool unpack = ImGui::Button("Unpack");

	ImGui::SameLine();

	// Відкриває сам префаб для редагування
	ImGui::BeginDisabled(mPlaying);
	if (ImGui::Button("Open")) mOpenPrefabRequest = mInstanceRoot->prefabAsset;
	ImGui::EndDisabled();

	// Перелік змінених полів екземпляра
	if (!mOverrides.empty() && ImGui::TreeNode("Overrides"))
	{
		for (const std::string& key : mOverrides)
		{
			ImGui::BulletText("%s", describeOverride(key, mInstanceRoot).c_str());
		}

		ImGui::TreePop();
	}

	// Дії виконуються після малювання: вони змінюють сам об'єкт, поля якого щойно показано
	if (apply) library->apply(mInstanceRoot);
	else if (revert) library->revert(mInstanceRoot);
	else if (unpack) library->unpack(mInstanceRoot);
}

// Створює екземпляр префаба, відпущеного над самою сценою, у точці під курсором
void Editor::dropPrefabIntoScene()
{
	const ImGuiPayload* payload = ImGui::GetDragDropPayload();

	if (payload == nullptr) return;

	bool isModel = payload->IsDataType(MESH_PAYLOAD);

	if (!isModel && !payload->IsDataType(PREFAB_PAYLOAD)) return;

	// Вкладених префабів немає, тож у режимі префаба інші префаби не кладуться, а модель стає частиною префаба
	if (!isModel && isPrefabMode()) return;

	// Кнопку відпустили саме зараз і не над панеллю редактора: інакше це не скидання на сцену
	if (!ImGui::IsMouseReleased(ImGuiMouseButton_Left)) return;
	if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)) return;

	ImVec2 mouse = ImGui::GetIO().MousePos;

	// Екземпляр стає туди, куди вказує курсор, а якщо там порожньо — перед камерою
	Vector3 position = mCamera.getSpawnPoint();

	float distance = 0.0f;

	// Об'єкт ставиться й на замкнену поверхню: замок забороняє лише вибір кліком
	if (Gizmo::pick(mouse.x, mouse.y, &distance, true))
	{
		Ray ray = Gizmo::screenPointToRay(mouse.x, mouse.y);

		position = ray.origin + ray.direction * distance;
	}

	Entity* instance = isModel ? instantiateModel((const wchar_t*)payload->Data, nullptr) : PrefabLibrary::get()->instantiate((const char*)payload->Data, nullptr);

	if (instance == nullptr) return;

	instance->getTransform()->setPosition(position);

	if (isModel) adoptIntoPrefab(instance);

	mSelected = instance;
}

// Перевіряє, чи поле вибраного об'єкта змінене відносно префаба
bool Editor::isOverridden(const std::string& key) const
{
	return mInstanceRoot != nullptr && mOverrides.count(key) > 0;
}

// Перевіряє, чи редактор зараз у режимі редагування префаба
bool Editor::isPrefabMode() const
{
	return !mPrefabModePath.empty();
}

// У режимі префаба робить новий кореневий об'єкт дочірнім для кореня префаба
void Editor::adoptIntoPrefab(Entity* entity)
{
	if (!isPrefabMode() || entity == nullptr || entity == mPrefabRoot || entity->getParent() != nullptr) return;

	entity->setParent(mPrefabRoot, true);
	EntityManager::get()->sortByHierarchy();

	mExpandEntity = mPrefabRoot;
}

// Відкриває префаб в ізольованій сцені, де є лише він сам
void Editor::openPrefab(const std::string& path)
{
	if (mPlaying) return;

	// Інший префаб відкривається лише після того, як закрито поточний, спитавши про збереження
	if (isPrefabMode())
	{
		if (path == mPrefabModePath) return;

		mAfterClose = path;
		requestClosePrefab();

		return;
	}

	const JsonValue* data = PrefabLibrary::get()->getData(path);

	if (data == nullptr) return;

	// Сцена зберігається так само, як перед грою, і після виходу відновлюється з цього знімка.
	// Екземпляри в ньому пам'ятають свої зміни, тож правки префаба дістануться їм при відновленні
	mPrefabModeScene = SceneSerializer::serialize(true);

	const std::list<Entity*>& entities = EntityManager::get()->getEntities();

	mPrefabModeSelected = -1;

	int index = 0;

	for (Entity* entity : entities)
	{
		if (entity == mSelected) mPrefabModeSelected = index;
		index++;
	}

	mPrefabModeView = mCamera.getView();

	// Об'єкти сцени знищуються напряму, а не завантаженням порожньої сцени: так її меші, текстури
	// та приготовані фізичні форми лишаються в пам'яті, і повернення не перечитує їх заново
	std::vector<Entity*> roots;

	for (Entity* entity : entities)
	{
		if (entity->getParent() == nullptr) roots.push_back(entity);
	}

	for (Entity* root : roots)
	{
		root->destroy();
	}

	std::vector<Entity*> built = SceneSerializer::buildSubtree(*data, nullptr, false);

	mPrefabRoot = built.empty() ? nullptr : built[0];
	mPrefabModePath = path;

	mSelected = mPrefabRoot;
	mExpandEntity = mPrefabRoot;

	focusSelected();

	// Префаб має власну історію скасування
	resetUndo();
}

// Повертає сцену, з якої відкривали префаб; save спершу записує зміни у файл префаба
void Editor::closePrefab(bool save)
{
	if (!isPrefabMode()) return;

	if (save && mPrefabRoot) PrefabLibrary::get()->saveAsset(mPrefabModePath, mPrefabRoot);

	mPrefabRoot = nullptr;
	mPrefabModePath.clear();
	mSelected = nullptr;

	// Відновлення знищує об'єкти префаба й будує сцену назад, а її екземпляри при цьому
	// наздоганяють файл префаба, зберігаючи власні зміни
	std::vector<Entity*> restored;

	if (!SceneSerializer::deserialize(mPrefabModeScene, &restored, true))
	{
		std::cout << "Failed to restore the scene after prefab mode" << std::endl;
	}

	if (mPrefabModeSelected >= 0 && mPrefabModeSelected < (int)restored.size())
	{
		mSelected = restored[mPrefabModeSelected];
	}

	mCamera.setView(mPrefabModeView);

	mPrefabModeScene.clear();

	resetUndo();

	// Об'єкти сцени, створені заново, можуть сховати курсор, як SceneChanger при прокиданні
	Input::hideCursor(false);
}

// Просить закрити префаб: із незбереженими змінами спершу питає, чи їх зберегти
void Editor::requestClosePrefab()
{
	if (isPrefabDirty()) mShowSavePrompt = true;
	else mClosePrefabRequest = 1;
}

// Перевіряє, чи вміст префаба в редакторі відрізняється від файлу
bool Editor::isPrefabDirty()
{
	if (!isPrefabMode() || mPrefabRoot == nullptr) return false;

	const JsonValue* data = PrefabLibrary::get()->getData(mPrefabModePath);

	if (data == nullptr) return true;

	return SceneSerializer::serializeSubtree(mPrefabRoot).toString() != data->toString();
}

// Малює смугу режиму префаба зверху та вікно з питанням про збереження
void Editor::drawPrefabModeBar()
{
	if (!isPrefabMode()) return;

	ImVec2 display = ImGui::GetIO().DisplaySize;

	// Смуга стоїть угорі посередині сцени
	ImGui::SetNextWindowPos(ImVec2(display.x * 0.5f, 10.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
	ImGui::Begin("Prefab Mode", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize
		| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse);

	bool dirty = isPrefabDirty();

	std::string name = PrefabLibrary::getName(mPrefabModePath);

	if (ImGui::Button("< Scene")) requestClosePrefab();

	ImGui::SameLine();
	ImGui::TextColored(prefabColor(), "Prefab Mode: %s%s", name.c_str(), dirty ? " *" : "");
	ImGui::SameLine();

	ImGui::BeginDisabled(!dirty);
	if (ImGui::Button("Save")) PrefabLibrary::get()->saveAsset(mPrefabModePath, mPrefabRoot);
	ImGui::EndDisabled();

	// Питання про збереження, коли з префаба виходять із незбереженими змінами
	if (mShowSavePrompt)
	{
		ImGui::OpenPopup("Save Prefab");
		mShowSavePrompt = false;
	}

	if (ImGui::BeginPopupModal("Save Prefab", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Prefab '%s' has unsaved changes.", name.c_str());

		if (ImGui::Button("Save")) { mClosePrefabRequest = 2; ImGui::CloseCurrentPopup(); }

		ImGui::SameLine();

		if (ImGui::Button("Don't Save")) { mClosePrefabRequest = 1; ImGui::CloseCurrentPopup(); }

		ImGui::SameLine();

		if (ImGui::Button("Cancel")) { mAfterClose.clear(); ImGui::CloseCurrentPopup(); }

		ImGui::EndPopup();
	}

	ImGui::End();
}

// Виконує відкладені відкриття та закриття префаба, коли всі панелі вже намальовано
void Editor::applyPrefabModeRequests()
{
	// Подвійний клік відкриває префаб, лише коли кнопку відпустили без перетягування. Перетягування
	// ловимо, поки кнопку ще тримають: на кадрі відпускання вміст уже прийнято й очищено
	if (!mPendingPrefabOpen.empty())
	{
		ImGuiIO& io = ImGui::GetIO();

		bool dragged = ImGui::GetDragDropPayload() != nullptr
			|| io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] > io.MouseDragThreshold * io.MouseDragThreshold;

		if (dragged)
		{
			mPendingPrefabOpen.clear();
		}
		else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
		{
			mOpenPrefabRequest = mPendingPrefabOpen;
			mPendingPrefabOpen.clear();
		}
	}

	if (mClosePrefabRequest != 0)
	{
		bool save = mClosePrefabRequest == 2;

		mClosePrefabRequest = 0;
		closePrefab(save);

		// Якщо закривали, щоб відкрити інший префаб, відкриваємо його тепер
		if (!mAfterClose.empty())
		{
			mOpenPrefabRequest = mAfterClose;
			mAfterClose.clear();
		}
	}

	if (!mOpenPrefabRequest.empty())
	{
		std::string path = mOpenPrefabRequest;

		mOpenPrefabRequest.clear();
		openPrefab(path);
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

	Entity* previous = mSelected;

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

	// Щойно створений об'єкт стає вибраним, тож за цим видно, що меню щось створило
	if (mSelected != previous) adoptIntoPrefab(mSelected);

	ImGui::EndPopup();
}

// Малює інспектор вибраного об'єкта
void Editor::drawInspector()
{
	placeWindow(-380.0f, 10.0f, 370.0f, -20.0f);

	ImGui::Begin("Inspector");
	keepWindowOnScreen();

	// Перехід до налаштувань моделі з редактора матеріалу виконується тут, до того як інспектор почне малювати вибраний об'єкт
	if (!mOpenModelRequest.empty())
	{
		selectOnly(nullptr);
		mInspectedMaterial.clear();
		mInspectedModel = mOpenModelRequest;
		mOpenModelRequest.clear();
	}

	if (mSelected == nullptr)
	{
		// Без вибраного об'єкта інспектор показує вибрані в панелі ресурсів модель чи матеріал
		if (!mInspectedModel.empty()) drawModelAsset();
		else if (!mInspectedMaterial.empty()) drawMaterialAsset();
		else ImGui::TextUnformatted("Nothing selected");

		ImGui::End();
		return;
	}

	// Вибраний об'єкт заміняє в інспекторі вибрані раніше модель чи матеріал
	mInspectedMaterial.clear();
	mInspectedModel.clear();

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

	if (mSelection.size() > 1) ImGui::TextDisabled("%d objects selected, editing the active one", (int)mSelection.size());

	// Зв'язок вибраного об'єкта з префабом і змінені поля рахуються раз на кадр
	mInstanceRoot = PrefabLibrary::findInstanceRoot(mSelected);
	mInstancePath.clear();
	mOverrides.clear();

	if (mInstanceRoot)
	{
		mInstancePath = PrefabLibrary::pathInInstance(mSelected, mInstanceRoot);
		mOverrides = PrefabLibrary::get()->computeOverrides(mInstanceRoot);
	}

	// Поле імені заповнюється лише тоді, коли воно не редагується
	if (!ImGui::IsAnyItemActive())
	{
		strncpy_s(mNameBuffer, sizeof(mNameBuffer), mSelected->getName().c_str(), _TRUNCATE);
	}

	inspectorLabel("Name", isOverridden(PrefabLibrary::entityKey(mInstancePath, "name")));
	if (ImGui::InputText("##name", mNameBuffer, sizeof(mNameBuffer)))
	{
		mSelected->setName(mNameBuffer);
	}

	inspectorLabel("Active", isOverridden(PrefabLibrary::entityKey(mInstancePath, "active")));
	ImGui::Checkbox("##active", &mSelected->isActiveSelf);

	drawPrefabBar();

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
			// Поля компонента мають ключі, за якими видно, чи змінені вони відносно префаба
			mComponentKey = PrefabLibrary::componentKey(mInstancePath, component->getTypeName(), PrefabLibrary::componentOccurrence(component), "");

			InspectorVisitor inspector;

			if (mInstanceRoot) inspector.setOverrides(&mOverrides, mComponentKey);

			component->visitProperties(inspector);

			if (Renderer* renderer = dynamic_cast<Renderer*>(component))
			{
				drawRendererAssets(renderer);
				drawRendererMaterials(renderer);
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

	inspectorLabel("Position", isOverridden(PrefabLibrary::entityKey(mInstancePath, "position")));
	if (ImGui::DragFloat3("##position", &position.x, 0.05f))
	{
		if (hasParent) transform->setLocalPosition(position);
		else transform->setPosition(position);
	}

	inspectorLabel("Rotation", isOverridden(PrefabLibrary::entityKey(mInstancePath, "rotation")));
	if (ImGui::DragFloat3("##rotation", &rotation.x, 0.01f))
	{
		if (hasParent) transform->setLocalRotation(rotation);
		else transform->setRotation(rotation);
	}

	inspectorLabel("Scale", isOverridden(PrefabLibrary::entityKey(mInstancePath, "scale")));
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

	inspectorLabel("Mesh", isOverridden(mComponentKey + "mesh"));
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
				MaterialLibrary::get()->applyModelMaterials(renderer);
			}
		}

		ImGui::EndCombo();
	}
	else if (ImGui::BeginDragDropTarget())
	{
		// Меш з панелі ресурсів можна просто покласти на поле
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(MESH_PAYLOAD))
		{
			renderer->setMesh(GraphicsEngine::get()->getMeshManager()->createMeshFromFile((const wchar_t*)payload->Data));
			MaterialLibrary::get()->applyModelMaterials(renderer);
		}

		ImGui::EndDragDropTarget();
	}

	inspectorLabel("Cast Shadows", isOverridden(mComponentKey + "castShadows"));
	ImGui::Checkbox("##castShadows", &renderer->castShadows);

	ImGui::SeparatorText("Materials");

	// Кожен слот меша має власне поле матеріалу
	unsigned int slots = renderer->getMaterialCount();

	for (unsigned int slot = 0; slot < slots; slot++)
	{
		drawMaterialSlot(renderer, slot);
	}
}

// Повертає ім'я матеріалу, яке показує редактор
static std::string materialName(Material* material)
{
	if (material == nullptr) return "None";

	return material->name.empty() ? std::string("Material") : material->name;
}

// Малює поле матеріалу слота: вибір файлу матеріалу зі списку або перетягнутого з панелі ресурсів
void Editor::drawMaterialSlot(Renderer* renderer, unsigned int slot)
{
	// Підпис - ім'я частини з файлу моделі, а без нього номер елемента
	Mesh* mesh = renderer->getMesh();
	std::string label = "Element " + std::to_string(slot);

	if (mesh && slot < mesh->getMaterialCount() && !mesh->getMaterialName(slot).empty()) label = mesh->getMaterialName(slot);

	Material* shared = renderer->getSharedMaterial(slot);

	// У режимі гри слот показує власну копію матеріалу цього об'єкта
	Material* shown = mPlaying ? renderer->getMaterial(slot) : shared;

	ImGui::PushID((int)slot);

	inspectorLabel(label.c_str(), isOverridden(mComponentKey + "materials"));

	MaterialLibrary* library = MaterialLibrary::get();

	if (ImGui::BeginCombo("##material", materialName(shown).c_str()))
	{
		if (ImGui::Selectable(MaterialLibrary::DEFAULT_NAME, MaterialLibrary::isDefault(shared)))
		{
			renderer->setMaterial(slot, GraphicsEngine::get()->getGlobalResources()->getDefaultMaterial());
		}

		for (const std::string& path : library->getPaths())
		{
			if (ImGui::Selectable(MaterialLibrary::getName(path).c_str(), shared->assetPath == path))
			{
				if (Material* material = library->load(path)) renderer->setMaterial(slot, material);
			}
		}

		// Матеріали, вбудовані у модель цього меша, підписано іменем моделі
		std::string modelPath = MaterialLibrary::modelPathOf(mesh);
		std::string modelName = MaterialLibrary::getName(modelPath);

		for (const std::string& name : library->modelMaterialNames(mesh))
		{
			if (name.empty()) continue;

			Material* embedded = library->embeddedMaterial(modelPath, name);

			if (ImGui::Selectable((name + " (" + modelName + ")").c_str(), shared == embedded)) renderer->setMaterial(slot, embedded);
		}

		ImGui::EndCombo();
	}

	// Матеріал з панелі ресурсів можна просто покласти на поле
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(MATERIAL_PAYLOAD))
		{
			renderer->setMaterial(slot, library->find((const char*)payload->Data));
		}

		ImGui::EndDragDropTarget();
	}

	ImGui::PopID();
}

// Малює редактори матеріалів рендер-компонента вкладеними вузлами під ним самим
void Editor::drawRendererMaterials(Renderer* renderer)
{
	std::vector<Material*> shown;

	for (unsigned int slot = 0; slot < renderer->getMaterialCount(); slot++)
	{
		// У режимі гри правляться власні копії, тож файли матеріалів лишаються як були
		Material* material = mPlaying ? renderer->getMaterial(slot) : renderer->getSharedMaterial(slot);

		if (std::find(shown.begin(), shown.end(), material) == shown.end()) shown.push_back(material);
	}

	// Невеликий відступ зліва показує, що матеріали належать саме цьому рендер-компоненту
	const float indent = 12.0f;

	ImGui::Indent(indent);

	// Кожен матеріал - згорнутий вкладений вузол; розгортають лише той, який хочуть правити
	for (Material* material : shown)
	{
		ImGui::PushID(material);

		std::string title = materialName(material) + " (Material)###material";

		if (ImGui::TreeNodeEx(title.c_str(), ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth))
		{
			drawMaterialEditor(material, mPlaying);
			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	ImGui::Unindent(indent);
}

// Малює налаштування імпорту моделі, вибраної в панелі ресурсів: її матеріали, заміни та витягування у файли
void Editor::drawModelAsset()
{
	std::wstring wide(mInspectedModel.begin(), mInspectedModel.end());
	Mesh* mesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(wide.c_str());

	ImGui::Text("%s (Model)", filesystem::path(mInspectedModel).filename().string().c_str());
	ImGui::TextDisabled("%s", mInspectedModel.c_str());

	if (mesh == nullptr)
	{
		ImGui::TextDisabled("The model can't be loaded.");
		return;
	}

	ImGui::TextDisabled("%d vertices, %d triangles, %u material slots", (int)mesh->getPositions().size(), (int)mesh->getIndices().size() / 3, mesh->getMaterialCount());

	MaterialLibrary* library = MaterialLibrary::get();
	std::vector<std::string> names = library->modelMaterialNames(mesh);

	names.erase(std::remove(names.begin(), names.end(), std::string()), names.end());

	ImGui::SeparatorText("Materials");

	if (names.empty())
	{
		ImGui::TextDisabled("The model file has no materials of its own.");
		return;
	}

	inspectorLabel("Location");
	ImGui::TextUnformatted("Use Embedded Materials");

	// Кожен вбудований матеріал можна замінити файлом матеріалу; без заміни діє вбудований
	ImGui::SeparatorText("Remapped Materials");

	bool anyEmbedded = false;

	for (const std::string& name : names)
	{
		ImGui::PushID(name.c_str());

		std::string remap = library->getRemap(mInspectedModel, name);

		if (remap.empty()) anyEmbedded = true;

		inspectorLabel(name.c_str());

		if (ImGui::BeginCombo("##remap", remap.empty() ? "None (embedded)" : MaterialLibrary::getName(remap).c_str()))
		{
			if (ImGui::Selectable("None (embedded)", remap.empty())) library->setRemap(mInspectedModel, name, std::string());

			for (const std::string& path : library->getPaths())
			{
				if (ImGui::Selectable(MaterialLibrary::getName(path).c_str(), path == remap)) library->setRemap(mInspectedModel, name, path);
			}

			ImGui::EndCombo();
		}
		else if (ImGui::BeginDragDropTarget())
		{
			// Замінити можна лише файлом матеріалу, а не вбудованим
			const ImGuiPayload* payload = ImGui::GetDragDropPayload();
			std::string dragged = payload && payload->IsDataType(MATERIAL_PAYLOAD) ? (const char*)payload->Data : "";

			if (!dragged.empty() && dragged != MaterialLibrary::DEFAULT_NAME && ImGui::AcceptDragDropPayload(MATERIAL_PAYLOAD)) library->setRemap(mInspectedModel, name, dragged);

			ImGui::EndDragDropTarget();
		}

		ImGui::PopID();
	}

	// Витягування записує кожен вбудований матеріал у файл і одразу ставить його заміною
	ImGui::BeginDisabled(!anyEmbedded);

	if (ImGui::Button("Extract Materials", ImVec2(-1.0f, 0.0f)))
	{
		int count = library->extractMaterials(mInspectedModel);

		refreshAssetLists();

		mModelStatus = "Extracted " + std::to_string(count) + " material(s) to " + MaterialLibrary::FOLDER;
	}

	ImGui::EndDisabled();

	if (!mModelStatus.empty()) ImGui::TextDisabled("%s", mModelStatus.c_str());

	// Самі вбудовані матеріали видно, але не редагуються
	ImGui::SeparatorText("Embedded Materials");

	for (const std::string& name : names)
	{
		Material* material = library->embeddedMaterial(mInspectedModel, name);

		if (material == nullptr) continue;

		ImGui::PushID(material);

		if (ImGui::CollapsingHeader((name + "###embedded").c_str())) drawMaterialEditor(material, false);

		ImGui::PopID();
	}
}

// Малює вибраний у панелі ресурсів файл матеріалу
void Editor::drawMaterialAsset()
{
	MaterialLibrary* library = MaterialLibrary::get();

	Material* material = mInspectedMaterial == MaterialLibrary::DEFAULT_NAME ? GraphicsEngine::get()->getGlobalResources()->getDefaultMaterial() : library->load(mInspectedMaterial);

	if (material == nullptr)
	{
		ImGui::TextDisabled("Material file is missing");
		return;
	}

	ImGui::Text("%s (Material)", materialName(material).c_str());

	if (!material->assetPath.empty())
	{
		// Ім'я поля заповнюється лише тоді, коли його не редагують
		if (!ImGui::IsAnyItemActive()) strncpy_s(mMaterialNameBuffer, sizeof(mMaterialNameBuffer), material->name.c_str(), _TRUNCATE);

		inspectorLabel("Name");

		// Файл перейменовується, коли ім'я підтвердили клавішею Enter
		if (ImGui::InputText("##materialName", mMaterialNameBuffer, sizeof(mMaterialNameBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
		{
			std::string oldReference = MaterialLibrary::quotedReference(material->assetPath);
			std::string newPath = library->rename(material, mMaterialNameBuffer);

			if (!newPath.empty())
			{
				mInspectedMaterial = newPath;

				// Файл відкритої сцени вже виправлено, тож і збережений стан для порівняння має посилатися на новий шлях
				std::string newReference = MaterialLibrary::quotedReference(newPath);

				for (size_t at = mSavedScene.find(oldReference); at != std::string::npos; at = mSavedScene.find(oldReference, at + newReference.size()))
				{
					mSavedScene.replace(at, oldReference.size(), newReference);
				}
			}
		}

		ImGui::TextDisabled("%s", material->assetPath.c_str());
	}

	ImGui::Separator();

	drawMaterialEditor(material, false);
}

// Шейдери, які можна вибрати для матеріалу, як Universal Render Pipeline/Lit, Unlit і прототипна сітка
static const wchar_t* SHADER_PATHS[] = { L"src\\Shaders\\PixelShader.hlsl", L"src\\Shaders\\UnlitPixelShader.hlsl", L"src\\Shaders\\PrototypePixelShader.hlsl" };

// Повертає номер шейдера матеріалу серед тих, що пропонує інспектор
static int shaderIndex(Material* material)
{
	std::wstring path = material->getPixelShaderPath();

	if (path.find(L"Unlit") != std::wstring::npos) return 1;
	if (path.find(L"Prototype") != std::wstring::npos) return 2;

	return 0;
}

// Малює налаштування матеріалу й змінює їх у самому матеріалі; повертає, чи щось змінилося
bool Editor::drawMaterialEditor(Material* material, bool instance)
{
	bool embedded = MaterialLibrary::isEmbedded(material);
	bool editable = !MaterialLibrary::isDefault(material) && !embedded;

	// Матеріал з файлу моделі змінюється лише разом з моделлю, тож його спершу витягують у файл матеріалу
	if (embedded)
	{
		std::string model = MaterialLibrary::embeddedModel(material);

		ImGui::TextDisabled("Embedded in %s, can't be edited.", filesystem::path(model).filename().string().c_str());
		ImGui::TextDisabled("Extract materials in the model import settings.");

		if (ImGui::Button("Open Model Import Settings")) mOpenModelRequest = model;

		ImGui::BeginDisabled();
	}
	// Вбудований матеріал спільний для всіх об'єктів без власного, тож його не редагують
	else if (!editable)
	{
		ImGui::TextDisabled("Built-in material, can't be edited.");
		ImGui::TextDisabled("Create one in Assets > Materials and assign it.");
		ImGui::BeginDisabled();
	}
	else if (instance)
	{
		ImGui::TextDisabled("Instance: changes are discarded on Stop.");
	}
	else if (material->assetPath.empty())
	{
		// Матеріал без файлу, створений кодом, можна перетворити на файл матеріалу
		ImGui::TextDisabled("Saved inside the scene.");

		if (ImGui::Button("Save as Material Asset"))
		{
			std::string path = MaterialLibrary::get()->createAsset(material->name.empty() ? "New Material" : material->name, material);

			if (Material* asset = MaterialLibrary::get()->load(path))
			{
				for (Renderer* renderer : EntityManager::get()->getRenderers())
				{
					for (unsigned int slot = 0; slot < renderer->getMaterialCount(); slot++)
					{
						if (renderer->getSharedMaterial(slot) == material) renderer->setMaterial(slot, asset);
					}
				}
			}
		}
	}

	// Правки йдуть у копію налаштувань і переносяться в матеріал, лише коли щось справді змінили
	MaterialProperties p = material->properties;

	Texture* maps[(int)MaterialMap::Count] = {};

	for (int i = 0; i < (int)MaterialMap::Count; i++)
	{
		maps[i] = material->getMap((MaterialMap)i);
	}

	int shader = shaderIndex(material);
	int originalShader = shader;

	bool changed = false;

	// Перелік режиму малюється як випадний список над цілим числом
	auto combo = [&](const char* name, const char* id, int& value, const char* items) {
		inspectorLabel(name);
		changed |= ImGui::Combo(id, &value, items);
	};

	combo("Shader", "##shader", shader, "Lit\0Unlit\0Prototype\0");

	bool lit = shader != 1;

	int workflow = (int)p.workflow;
	int surface = (int)p.surface;
	int blend = (int)p.blend;
	int face = (int)p.renderFace;
	int source = (int)p.smoothnessSource;

	ImGui::SeparatorText("Surface Options");

	if (lit) combo("Workflow Mode", "##workflow", workflow, "Metallic\0Specular\0");

	combo("Surface Type", "##surface", surface, "Opaque\0Transparent\0");

	if (surface == (int)SurfaceType::Transparent) combo("Blending Mode", "##blend", blend, "Alpha\0Premultiply\0Additive\0Multiply\0");

	combo("Render Face", "##face", face, "Front\0Back\0Both\0");

	inspectorLabel("Alpha Clipping");
	changed |= ImGui::Checkbox("##alphaClipping", &p.alphaClipping);

	if (p.alphaClipping)
	{
		inspectorLabel("Threshold");
		changed |= ImGui::SliderFloat("##cutoff", &p.alphaCutoff, 0.0f, 1.0f);
	}

	if (lit)
	{
		inspectorLabel("Receive Shadows");
		changed |= ImGui::Checkbox("##receiveShadows", &p.receiveShadows);
	}

	ImGui::SeparatorText("Surface Inputs");

	changed |= drawMapField("Base Map", "##baseMap", maps[(int)MaterialMap::Base]);
	inspectorLabel("Base Color");
	changed |= ImGui::ColorEdit4("##baseColor", p.baseColor);

	if (lit)
	{
		// Повзунок металевості чи колір відблиску ховаються, коли їх замінює карта
		if (workflow == (int)MaterialWorkflow::Metallic)
		{
			changed |= drawMapField("Metallic Map", "##metallicMap", maps[(int)MaterialMap::Metallic]);

			if (maps[(int)MaterialMap::Metallic] == nullptr)
			{
				inspectorLabel("Metallic");
				changed |= ImGui::SliderFloat("##metallic", &p.metallic, 0.0f, 1.0f);
			}
		}
		else
		{
			changed |= drawMapField("Specular Map", "##specularMap", maps[(int)MaterialMap::Specular]);

			if (maps[(int)MaterialMap::Specular] == nullptr)
			{
				inspectorLabel("Specular");
				changed |= ImGui::ColorEdit3("##specular", p.specularColor);
			}
		}

		inspectorLabel("Smoothness");
		changed |= ImGui::SliderFloat("##smoothness", &p.smoothness, 0.0f, 1.0f);

		combo("Source", "##source", source, workflow == (int)MaterialWorkflow::Metallic ? "Metallic Alpha\0Albedo Alpha\0" : "Specular Alpha\0Albedo Alpha\0");

		changed |= drawMapField("Normal Map", "##normalMap", maps[(int)MaterialMap::Normal]);
		inspectorLabel("Scale");
		changed |= ImGui::DragFloat("##normalScale", &p.normalScale, 0.01f);

		changed |= drawMapField("Height Map", "##heightMap", maps[(int)MaterialMap::Height]);
		inspectorLabel("Scale");
		changed |= ImGui::SliderFloat("##heightScale", &p.heightScale, 0.005f, 0.08f);

		changed |= drawMapField("Occlusion Map", "##occlusionMap", maps[(int)MaterialMap::Occlusion]);
		inspectorLabel("Strength");
		changed |= ImGui::SliderFloat("##occlusion", &p.occlusionStrength, 0.0f, 1.0f);

		inspectorLabel("Emission");
		changed |= ImGui::Checkbox("##emission", &p.emission);

		if (p.emission)
		{
			changed |= drawMapField("Emission Map", "##emissionMap", maps[(int)MaterialMap::Emission]);
			inspectorLabel("Color");
			changed |= ImGui::ColorEdit3("##emissionColor", p.emissionColor, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
		}
	}

	inspectorLabel("Tiling");
	changed |= ImGui::DragFloat2("##tiling", p.tiling, 0.05f);
	inspectorLabel("Offset");
	changed |= ImGui::DragFloat2("##offset", p.offset, 0.01f);

	if (lit)
	{
		ImGui::SeparatorText("Detail Inputs");

		changed |= drawMapField("Mask", "##detailMask", maps[(int)MaterialMap::DetailMask]);
		changed |= drawMapField("Base Map", "##detailAlbedo", maps[(int)MaterialMap::DetailAlbedo]);
		inspectorLabel("Scale");
		changed |= ImGui::SliderFloat("##detailAlbedoScale", &p.detailAlbedoScale, 0.0f, 2.0f);
		changed |= drawMapField("Normal Map", "##detailNormal", maps[(int)MaterialMap::DetailNormal]);
		inspectorLabel("Scale");
		changed |= ImGui::DragFloat("##detailNormalScale", &p.detailNormalScale, 0.01f);
		inspectorLabel("Tiling");
		changed |= ImGui::DragFloat2("##detailTiling", p.detailTiling, 0.05f);
		inspectorLabel("Offset");
		changed |= ImGui::DragFloat2("##detailOffset", p.detailOffset, 0.01f);
	}

	ImGui::SeparatorText("Advanced Options");

	if (lit)
	{
		inspectorLabel("Spec. Highlights");
		changed |= ImGui::Checkbox("##specularHighlights", &p.specularHighlights);
		inspectorLabel("Env. Reflections");
		changed |= ImGui::Checkbox("##environmentReflections", &p.environmentReflections);
	}

	inspectorLabel("Sorting Priority");
	changed |= ImGui::SliderInt("##sortingPriority", &p.sortingPriority, -50, 50);

	if (changed && editable)
	{
		p.workflow = (MaterialWorkflow)workflow;
		p.surface = (SurfaceType)surface;
		p.blend = (BlendMode)blend;
		p.renderFace = (RenderFace)face;
		p.smoothnessSource = (SmoothnessSource)source;

		material->properties = p;

		for (int i = 0; i < (int)MaterialMap::Count; i++)
		{
			material->setMap((MaterialMap)i, maps[i]);
		}

		if (shader != originalShader) material->setPixelShader(GraphicsEngine::get()->getPixelShader(SHADER_PATHS[shader], "main"));
	}

	if (!editable) ImGui::EndDisabled();

	return changed && editable;
}

// Малює вибір текстури для карти матеріалу; повертає true, якщо вибрано іншу
bool Editor::drawMapField(const char* label, const char* id, Texture*& map)
{
	std::string current = "None";

	if (map)
	{
		std::wstring path = map->getFullPath();
		current = std::string(path.begin(), path.end());

		size_t slash = current.find_last_of("\\/");
		if (slash != std::string::npos) current = current.substr(slash + 1);
	}

	inspectorLabel(label);

	bool changed = false;

	if (ImGui::BeginCombo(id, current.c_str()))
	{
		if (ImGui::Selectable("None", map == nullptr))
		{
			map = nullptr;
			changed = true;
		}

		for (size_t i = 0; i < mTexturePaths.size(); i++)
		{
			std::string name = mTextureNames[i];

			size_t slash = name.find_last_of("\\/");
			if (slash != std::string::npos) name = name.substr(slash + 1);

			if (ImGui::Selectable(name.c_str()))
			{
				map = GraphicsEngine::get()->getTextureManager()->createTextureFromFile(mTexturePaths[i].c_str());
				changed = true;
			}
		}

		ImGui::EndCombo();
	}
	else if (ImGui::BeginDragDropTarget())
	{
		// Текстуру з панелі ресурсів можна просто покласти на поле карти
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(TEXTURE_PAYLOAD))
		{
			map = GraphicsEngine::get()->getTextureManager()->createTextureFromFile((const wchar_t*)payload->Data);
			changed = true;
		}

		ImGui::EndDragDropTarget();
	}

	return changed;
}

// Малює список ресурсів проєкту
void Editor::drawAssets()
{
	placeWindow(10.0f, -210.0f, 300.0f, 200.0f);

	ImGui::Begin("Assets");
	keepWindowOnScreen();

	// Ресурси показуються плитками з мініатюрами або списком; розмір плиток можна змінити
	if (ImGui::RadioButton("Grid", mAssetGrid)) mAssetGrid = true;
	ImGui::SameLine();
	if (ImGui::RadioButton("List", !mAssetGrid)) mAssetGrid = false;

	if (mAssetGrid)
	{
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::SliderFloat("##tileSize", &mAssetTileSize, 48.0f, 128.0f, "%.0f px");
	}

	if (ImGui::CollapsingHeader("Scenes", ImGuiTreeNodeFlags_DefaultOpen))
	{
		SceneManager* scenes = SceneManager::get();

		// Під час гри сцени перемикає сама гра, а в режимі префаба сцени немає взагалі
		ImGui::BeginDisabled(mPlaying || isPrefabMode());

		for (const std::string& path : scenes->getScenePaths())
		{
			ImGui::PushID(path.c_str());

			// Подвійний клік відкриває сцену; відкрита сцена підсвічена
			if (ImGui::Selectable(SceneManager::getSceneName(path).c_str(), path == scenes->getActiveScenePath(), ImGuiSelectableFlags_AllowDoubleClick)
				&& ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				requestScene(path, false);
			}

			ImGui::PopID();
		}

		if (scenes->getScenePaths().empty()) ImGui::TextDisabled("No scenes yet");

		ImGui::EndDisabled();
	}

	ImVec4 textColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);

	// Перетягуваний ресурс показує свою мініатюру поруч з іменем
	auto dragLabel = [](ID3D11ShaderResourceView* preview, const std::string& name) {
		if (preview) ImGui::Image((ImTextureID)(intptr_t)preview, ImVec2(48.0f, 48.0f));
		ImGui::TextUnformatted(name.c_str());
	};

	// Кожен розділ панелі має власний простір імен, бо заголовки розділів його не створюють і однакові номери плиток збігалися б
	ImGui::PushID("Prefabs");

	if (ImGui::CollapsingHeader("Prefabs", ImGuiTreeNodeFlags_DefaultOpen))
	{
		PrefabLibrary* library = PrefabLibrary::get();

		mAssetGridCount = 0;

		for (const std::string& path : library->getPaths())
		{
			std::string name = PrefabLibrary::getName(path);
			ID3D11ShaderResourceView* preview = mPreview.get(AssetPreview::Kind::Prefab, path);

			ImGui::PushID(path.c_str());

			// Екземпляр створюється перетягуванням, тож клік нічого не вибирає
			drawAssetItem(name, preview, false, prefabColor());

			// Подвійний клік відкриває префаб для редагування; сам перехід чекає, поки кнопку відпустять, щоб не спрацювати на початку перетягування
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) mPendingPrefabOpen = path;

			// Префаб перетягують у дерево сцени або просто на сцену
			if (ImGui::BeginDragDropSource())
			{
				ImGui::SetDragDropPayload(PREFAB_PAYLOAD, path.c_str(), path.size() + 1);
				dragLabel(preview, name);
				ImGui::EndDragDropSource();
			}

			ImGui::PopID();
		}

		if (library->getPaths().empty()) ImGui::TextDisabled("No prefabs yet");

		// Об'єкт із дерева сцени, покладений сюди, стає новим префабом; під час гри й у режимі префаба нові префаби не створюються
		bool canCreate = !mPlaying && !isPrefabMode();

		ImGui::BeginDisabled(!canCreate);
		ImGui::Button(mPlaying ? "Prefabs can't be created in play mode" : isPrefabMode() ? "Not available in Prefab Mode" : "Drop an object here to create a prefab", ImVec2(-1.0f, 32.0f));
		ImGui::EndDisabled();

		if (canCreate && ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(HIERARCHY_PAYLOAD))
			{
				Entity* entity = *(Entity* const*)payload->Data;

				if (isAlive(entity)) library->createAsset(entity);
			}

			ImGui::EndDragDropTarget();
		}
	}

	ImGui::PopID();
	ImGui::PushID("Materials");

	if (ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen))
	{
		MaterialLibrary* library = MaterialLibrary::get();

		// Вбудований матеріал видно разом з файлами, але він лише для перегляду
		std::vector<std::string> entries(1, MaterialLibrary::DEFAULT_NAME);
		entries.insert(entries.end(), library->getPaths().begin(), library->getPaths().end());

		mAssetGridCount = 0;

		for (const std::string& entry : entries)
		{
			bool builtIn = entry == MaterialLibrary::DEFAULT_NAME;
			std::string name = builtIn ? entry : MaterialLibrary::getName(entry);
			ID3D11ShaderResourceView* preview = mPreview.get(AssetPreview::Kind::Material, entry);

			ImGui::PushID(entry.c_str());

			// Клік показує матеріал в інспекторі замість вибраного об'єкта
			if (drawAssetItem(name, preview, mInspectedMaterial == entry && mSelected == nullptr, builtIn ? ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled) : textColor))
			{
				selectOnly(nullptr);
				mInspectedModel.clear();
				mInspectedMaterial = entry;
			}

			// Матеріал перетягують на поле слота в інспекторі
			if (ImGui::BeginDragDropSource())
			{
				ImGui::SetDragDropPayload(MATERIAL_PAYLOAD, entry.c_str(), entry.size() + 1);
				dragLabel(preview, name);
				ImGui::EndDragDropSource();
			}

			ImGui::PopID();
		}

		if (ImGui::Button("Create Material", ImVec2(-1.0f, 0.0f)))
		{
			strcpy_s(mNewMaterialName, sizeof(mNewMaterialName), "New Material");
			ImGui::OpenPopup("Create Material");
		}

		// Новий матеріал отримує типові значення і одразу відкривається в інспекторі
		if (ImGui::BeginPopup("Create Material"))
		{
			if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();

			bool create = ImGui::InputText("##newMaterial", mNewMaterialName, sizeof(mNewMaterialName), ImGuiInputTextFlags_EnterReturnsTrue);

			ImGui::SameLine();
			create |= ImGui::Button("Create");

			if (create)
			{
				std::string path = library->createAsset(mNewMaterialName);

				if (!path.empty())
				{
					selectOnly(nullptr);
					mInspectedModel.clear();
					mInspectedMaterial = path;
				}

				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	ImGui::PopID();
	ImGui::PushID("Meshes");

	if (ImGui::CollapsingHeader("Meshes", ImGuiTreeNodeFlags_DefaultOpen))
	{
		mAssetGridCount = 0;

		for (size_t i = 0; i < mMeshNames.size(); i++)
		{
			std::string name = mMeshNames[i];

			size_t slash = name.find_last_of("\\/");
			if (slash != std::string::npos) name = name.substr(slash + 1);

			std::string modelPath(mMeshPaths[i].begin(), mMeshPaths[i].end());

			// Шлях файлу однозначно визначає плитку навіть після зміни переліку
			ImGui::PushID(modelPath.c_str());
			ID3D11ShaderResourceView* preview = mPreview.get(AssetPreview::Kind::Model, modelPath);

			// Клік показує налаштування імпорту моделі, подвійний створює у сцені об'єкт з нею
			bool clicked = drawAssetItem(name, preview, mInspectedModel == modelPath && mSelected == nullptr, textColor);

			if (clicked)
			{
				selectOnly(nullptr);
				mInspectedMaterial.clear();
				mInspectedModel = modelPath;
			}

			if (clicked && ImGui::IsMouseDoubleClicked(0))
			{
				if (Entity* entity = instantiateModel(mMeshPaths[i], nullptr))
				{
					entity->getTransform()->setPosition(mCamera.getSpawnPoint());

					adoptIntoPrefab(entity);

					mSelected = entity;
				}
			}

			// Меш перетягують на поле меша в інспекторі або на сцену
			if (ImGui::BeginDragDropSource())
			{
				ImGui::SetDragDropPayload(MESH_PAYLOAD, mMeshPaths[i].c_str(), (mMeshPaths[i].size() + 1) * sizeof(wchar_t));
				dragLabel(preview, name);
				ImGui::EndDragDropSource();
			}

			ImGui::PopID();
		}
	}

	ImGui::PopID();
	ImGui::PushID("Textures");

	if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen))
	{
		mAssetGridCount = 0;

		for (size_t i = 0; i < mTextureNames.size(); i++)
		{
			std::string name = mTextureNames[i];

			size_t slash = name.find_last_of("\\/");
			if (slash != std::string::npos) name = name.substr(slash + 1);

			std::string texturePath(mTexturePaths[i].begin(), mTexturePaths[i].end());
			ID3D11ShaderResourceView* preview = mPreview.get(AssetPreview::Kind::Texture, texturePath);

			ImGui::PushID(texturePath.c_str());
			drawAssetItem(name, preview, false, textColor);

			// Текстуру перетягують на поле карти матеріалу
			if (ImGui::BeginDragDropSource())
			{
				ImGui::SetDragDropPayload(TEXTURE_PAYLOAD, mTexturePaths[i].c_str(), (mTexturePaths[i].size() + 1) * sizeof(wchar_t));
				dragLabel(preview, name);
				ImGui::EndDragDropSource();
			}

			ImGui::PopID();
		}
	}

	ImGui::PopID();

	ImGui::End();
}

// Малює ресурс панелі плиткою з мініатюрою або рядком списку; повертає, чи по ньому клікнули
bool Editor::drawAssetItem(const std::string& name, ID3D11ShaderResourceView* preview, bool selected, const ImVec4& color)
{
	if (!mAssetGrid)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, color);
		bool clicked = ImGui::Selectable(name.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick);
		ImGui::PopStyleColor();

		return clicked;
	}

	float tile = mAssetTileSize;
	float textHeight = ImGui::GetTextLineHeight();

	// Плитки стають в один рядок, поки вміщаються в ширину панелі
	float right = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

	if (mAssetGridCount > 0 && ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + tile <= right) ImGui::SameLine();

	mAssetGridCount++;

	ImVec2 start = ImGui::GetCursorScreenPos();
	bool clicked = ImGui::Selectable("##asset", selected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(tile, tile + textHeight));

	// Мініатюра займає квадрат плитки, а поки її малюють - сіра заглушка
	ImDrawList* draw = ImGui::GetWindowDrawList();
	const float padding = 4.0f;
	ImVec2 imageMin(start.x + padding, start.y + padding);
	ImVec2 imageMax(start.x + tile - padding, start.y + tile - padding);

	if (preview) draw->AddImage((ImTextureID)(intptr_t)preview, imageMin, imageMax);
	else draw->AddRectFilled(imageMin, imageMax, ImGui::GetColorU32(ImGuiCol_FrameBg), 4.0f);

	// Довге ім'я вкорочується до ширини плитки, а повне видно в підказці
	std::string label = name;

	while (label.size() > 1 && ImGui::CalcTextSize((label + "...").c_str()).x > tile)
	{
		label.pop_back();

		while (!label.empty() && ((unsigned char)label.back() & 0xC0) == 0x80) label.pop_back();
	}

	if (label != name) label += "...";

	float width = ImGui::CalcTextSize(label.c_str()).x;
	draw->AddText(ImVec2(start.x + (tile - width) * 0.5f, start.y + tile), ImGui::GetColorU32(color), label.c_str());

	if (label != name && ImGui::IsItemHovered() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) ImGui::SetTooltip("%s", name.c_str());

	return clicked;
}

// Переходить у режим гри, зберігши стан сцени
void Editor::play()
{
	// Гра запускає сцену, а в режимі префаба сцени немає — лише сам префаб
	if (isPrefabMode()) return;

	// Уся сцена записується тим самим серіалізатором, що й файл, і після зупинки відновлюється
	// з цього знімка. Так назад повертається все, що гра могла змінити: поля компонентів, матеріали,
	// імена, додані й прибрані компоненти, знищені об'єкти, а не лише трансформації.
	// Об'єкти, що переживають зміну сцени, теж потрапляють у знімок: у редакторі вони частина сцени
	mPlayScene = SceneSerializer::serialize(true);
	mPlayScenePath = SceneManager::get()->getActiveScenePath();

	const std::list<Entity*>& entities = EntityManager::get()->getEntities();

	mPlayOrder.assign(entities.begin(), entities.end());

	mPlaySelectedIndex = -1;

	for (size_t i = 0; i < mPlayOrder.size(); i++)
	{
		if (mPlayOrder[i] == mSelected) { mPlaySelectedIndex = (int)i; break; }
	}

	mPlayView = mCamera.getView();

	// У режимі гри кожен рендер-компонент отримує власні копії матеріалів
	Renderer::setInstancing(true);

	mPlaying = true;
	mPaused = false;
}

// Повертається до редагування, відновивши збережений стан сцени
void Editor::stop()
{
	mPlaying = false;
	mPaused = false;

	// Копії матеріалів зникають разом з об'єктами гри, а відновлена сцена знову малюється спільними
	Renderer::setInstancing(false);

	// На паузі камеру редактора могли відвести; редагування продовжується з того місця, де його лишили
	mCamera.setView(mPlayView);

	// Гра могла знищити вибраний об'єкт, тому його вказівник лише порівнюємо, не розіменовуючи
	int selectedIndex = -1;

	for (size_t i = 0; i < mPlayOrder.size(); i++)
	{
		if (mPlayOrder[i] == mSelected) { selectedIndex = (int)i; break; }
	}

	// Вибраний під час гри об'єкт існував і до неї — лишаємо його. Інакше повертаємо вибір,
	// що був у момент запуску: після зупинки той об'єкт знову існує
	if (selectedIndex < 0) selectedIndex = mPlaySelectedIndex;

	// Після відновлення всі об'єкти будуть новими екземплярами, тож старий вказівник недійсний
	mSelected = nullptr;

	std::vector<Entity*> restored;

	// Знімок записав цей самий рушій, тож прочитатися він має завжди; якщо ні, краще лишити
	// сцену як є, ніж знищити її
	if (!SceneSerializer::deserialize(mPlayScene, &restored, true))
	{
		std::cout << "Failed to restore the scene after play" << std::endl;
	}

	// Об'єкти створюються у порядку знімка, тож вибраний знаходиться за тим самим номером.
	// Створене грою об'єктів у знімку немає, і вибір тоді просто знімається
	if (selectedIndex >= 0 && selectedIndex < (int)restored.size())
	{
		mSelected = restored[selectedIndex];
	}

	// Гра могла перемкнути сцену, а відновлено ту, що була відкрита до неї. Власного завантаження
	// тут немає, тож рахунок узгоджується, щоб незбережені до гри зміни не вважались збереженими
	SceneManager::get()->setActiveScenePath(mPlayScenePath);
	mSeenLoadCount = SceneManager::get()->getLoadCount();

	mPlayScene.clear();
	mPlayScenePath.clear();
	mPlayOrder.clear();

	// Відновлення після гри створило всі об'єкти й матеріали заново, тож старі кроки на них не вказують
	resetUndo();

	Input::hideCursor(false);
}

// Обробляє вибір об'єкта мишею та малює маніпулятор
void Editor::updateSelection()
{
	// Поки гра йде, сценою керує вона сама, тому маніпулятор не показуємо. На паузі - показуємо
	if (mPlaying && !mPaused) return;

	ImGuiIO& io = ImGui::GetIO();

	// Поки камера обертається, вибір і маніпулятор лише заважали б
	if (Input::getMouseButton(MB_Right)) return;

	bool overGizmo = mGizmo.update(mSelected, topLevelSelection());

	// Клік по панелі редактора не має міняти вибір у сцені
	if (io.WantCaptureMouse) return;

	if (overGizmo) return;

	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		Entity* picked = Gizmo::pick(io.MousePos.x, io.MousePos.y);

		// Ctrl чи Shift додають об'єкт до вибору або прибирають з нього
		if (io.KeyCtrl || io.KeyShift)
		{
			if (picked) toggleSelected(picked);
		}
		else
		{
			selectOnly(picked);
		}
	}
}

// Малює допоміжну геометрію редактора, як-от сітку й обведення вибраного, поверх готового кадру,
// але під інтерфейсом; width і height - розмір вікна
void Editor::renderOverlay(SwapChain* swapChain, unsigned int width, unsigned int height)
{
	// Поки гра йде, допоміжна геометрія заважала б; на паузі редагують, тож вона потрібна
	if (!mEnabled || (mPlaying && !mPaused)) return;

	// Префаб показується без сцени навколо, тож сітка дає відчуття землі та масштабу
	if (isPrefabMode()) mGrid.render(swapChain);

	syncSelection();

	// Обведення малюється після сітки, щоб лежати поверх неї
	mOutline.render(swapChain, width, height, mSelection);
}

// Повертає радіус, у якому вміщується меш об'єкта
static float boundsRadius(Entity* entity)
{
	float radius = 1.0f;

	if (Renderer* renderer = entity->getComponent<Renderer>())
	{
		if (renderer->getMesh())
		{
			Vector3 scale = entity->getTransform()->getScale();

			float maxScale = fabsf(scale.x);
			if (fabsf(scale.y) > maxScale) maxScale = fabsf(scale.y);
			if (fabsf(scale.z) > maxScale) maxScale = fabsf(scale.z);

			radius = renderer->getMesh()->getBoundsRadius() * maxScale;
		}
	}

	return radius;
}

// Наводить камеру редактора на вибрані об'єкти
void Editor::focusSelected()
{
	syncSelection();

	if (mSelection.empty()) return;

	Vector3 center;

	for (Entity* entity : mSelection)
	{
		center = center + entity->getTransform()->getPosition();
	}

	center = center * (1.0f / (float)mSelection.size());

	// Радіус охоплює всі вибрані разом з їхніми мешами
	float radius = 0.0f;

	for (Entity* entity : mSelection)
	{
		float reach = (entity->getTransform()->getPosition() - center).length() + boundsRadius(entity);

		if (reach > radius) radius = reach;
	}

	mCamera.focusOn(center, radius);
}

// Знищує вибрані об'єкти
void Editor::deleteSelected()
{
	syncSelection();

	// Нащадки зникають разом з предками, тож знищуються лише верхні з вибраних
	for (Entity* entity : topLevelSelection())
	{
		// Без кореня префаба не лишилося б чого зберігати
		if (isPrefabMode() && entity == mPrefabRoot) continue;

		entity->destroy();
	}

	syncSelection();

	mUndoCheckFrames = 2;
}

// Прибирає з вибору знищені об'єкти і узгоджує перелік вибраних з активним об'єктом
void Editor::syncSelection()
{
	// Живі об'єкти збираються один раз, а не шукаються для кожного вибраного
	const std::list<Entity*>& entities = EntityManager::get()->getEntities();
	std::unordered_set<Entity*> alive(entities.begin(), entities.end());

	mSelection.erase(std::remove_if(mSelection.begin(), mSelection.end(), [&alive](Entity* entity) { return alive.count(entity) == 0; }), mSelection.end());

	if (mSelected && !alive.count(mSelected)) mSelected = mSelection.empty() ? nullptr : mSelection.back();

	// Код, що просто ставить mSelected, вибирає лише цей об'єкт
	if (mSelected == nullptr) mSelection.clear();
	else if (std::find(mSelection.begin(), mSelection.end(), mSelected) == mSelection.end()) mSelection.assign(1, mSelected);

	if (mSelectionAnchor && !alive.count(mSelectionAnchor)) mSelectionAnchor = nullptr;
	if (mPendingSingleSelect && !alive.count(mPendingSingleSelect)) mPendingSingleSelect = nullptr;

	updateSelectionSet();
}

// Оновлює множину вибраних після зміни переліку
void Editor::updateSelectionSet()
{
	mSelectionSet = std::unordered_set<Entity*>(mSelection.begin(), mSelection.end());
}

// Перевіряє, чи об'єкт вибрано
bool Editor::isSelected(Entity* entity) const
{
	return mSelectionSet.count(entity) > 0;
}

// Лишає вибраним лише вказаний об'єкт; nullptr знімає вибір
void Editor::selectOnly(Entity* entity)
{
	mSelection.clear();

	if (entity) mSelection.push_back(entity);

	mSelected = entity;
	mSelectionAnchor = entity;

	updateSelectionSet();
}

// Додає об'єкт до вибору або прибирає з нього
void Editor::toggleSelected(Entity* entity)
{
	auto it = std::find(mSelection.begin(), mSelection.end(), entity);

	if (it != mSelection.end())
	{
		mSelection.erase(it);

		// Активним стає останній з тих, що лишилися
		if (mSelected == entity) mSelected = mSelection.empty() ? nullptr : mSelection.back();
	}
	else
	{
		mSelection.push_back(entity);
		mSelected = entity;
	}

	mSelectionAnchor = entity;

	updateSelectionSet();
}

// Вибирає рядки дерева від опорного до вказаного, як при затиснутому Shift
void Editor::selectRange(Entity* entity)
{
	auto from = std::find(mHierarchyRows.begin(), mHierarchyRows.end(), mSelectionAnchor);
	auto to = std::find(mHierarchyRows.begin(), mHierarchyRows.end(), entity);

	// Без опорного рядка діапазону немає
	if (from == mHierarchyRows.end() || to == mHierarchyRows.end())
	{
		selectOnly(entity);
		return;
	}

	if (from > to) std::swap(from, to);

	mSelection.assign(from, to + 1);
	mSelected = entity;

	updateSelectionSet();
}

// Обробляє клік по рядку дерева з урахуванням Ctrl та Shift
void Editor::clickHierarchyRow(Entity* entity)
{
	ImGuiIO& io = ImGui::GetIO();

	if (io.KeyCtrl)
	{
		toggleSelected(entity);
	}
	else if (io.KeyShift && mSelectionAnchor)
	{
		selectRange(entity);
	}
	else if (isSelected(entity) && mSelection.size() > 1)
	{
		// Вибір лишається, поки не ясно, чи це клік, чи початок перетягування всієї групи
		mSelected = entity;
		mPendingSingleSelect = entity;
	}
	else if (!isSelected(entity))
	{
		// Вибір чекає, поки кнопку відпустять без перетягування: так об'єкт можна тягнути на поле в інспекторі, не змінюючи вибраного
		mPendingSingleSelect = entity;
	}
	else
	{
		selectOnly(entity);
	}
}

// Повертає вибрані об'єкти без тих, чий предок теж вибраний, у порядку дерева
std::vector<Entity*> Editor::topLevelSelection() const
{
	std::vector<Entity*> result;

	for (Entity* entity : EntityManager::get()->getEntities())
	{
		if (!isSelected(entity)) continue;

		bool ancestorSelected = false;

		for (Entity* parent = entity->getParent(); parent; parent = parent->getParent())
		{
			if (isSelected(parent)) { ancestorSelected = true; break; }
		}

		if (!ancestorSelected) result.push_back(entity);
	}

	return result;
}

// Вибирає всі об'єкти сцени
void Editor::selectAll()
{
	const std::list<Entity*>& entities = EntityManager::get()->getEntities();

	mSelection.assign(entities.begin(), entities.end());

	updateSelectionSet();

	if (!mSelection.empty() && !isSelected(mSelected)) mSelected = mSelection.back();
	if (mSelection.empty()) mSelected = nullptr;
}

// Обробляє поєднання клавіш з Ctrl
void Editor::handleShortcuts()
{
	ImGuiIO& io = ImGui::GetIO();

	// Поки друкують у полі, Ctrl+Z і решта належать самому полю
	if (io.WantTextInput || !io.KeyCtrl) return;

	bool shift = io.KeyShift;

	// Збереження: у режимі префаба зберігається префаб; під час гри у файл потрапив би стан гри
	if (ImGui::IsKeyPressed(ImGuiKey_S, false) && !mPlaying)
	{
		if (isPrefabMode()) PrefabLibrary::get()->saveAsset(mPrefabModePath, mPrefabRoot);
		else saveScene();
	}

	if (ImGui::IsKeyPressed(ImGuiKey_Z, true))
	{
		if (shift) redo();
		else undo();
	}

	if (ImGui::IsKeyPressed(ImGuiKey_Y, true)) redo();

	if (ImGui::IsKeyPressed(ImGuiKey_D, false)) duplicateSelection();
	if (ImGui::IsKeyPressed(ImGuiKey_C, false)) copySelection(mClipboard);
	if (ImGui::IsKeyPressed(ImGuiKey_V, false)) pasteClipboard(mClipboard);
	if (ImGui::IsKeyPressed(ImGuiKey_A, false)) selectAll();

	// Ctrl+P запускає й зупиняє гру, Ctrl+Shift+P ставить на паузу; у режимі префаба гри немає
	if (ImGui::IsKeyPressed(ImGuiKey_P, false) && !isPrefabMode())
	{
		if (shift) setPaused(!mPaused);
		else if (mPlaying) stop();
		else play();
	}
}

// Записує поточний вибір як незмінні номери об'єктів
void Editor::captureSelection(std::vector<int>& selection, int& active) const
{
	selection.clear();

	for (Entity* entity : mSelection)
	{
		selection.push_back((int)entity->getId());
	}

	active = mSelected ? (int)mSelected->getId() : -1;
}

// Вибирає об'єкти за їхніми номерами
void Editor::restoreSelection(const std::vector<int>& selection, int active)
{
	// Кроки могли знищити об'єкти, на які вказували відкладені дії
	mPendingSingleSelect = nullptr;
	mPendingDrop = PendingDrop();
	mExpandEntity = nullptr;

	mSelection.clear();

	for (int id : selection)
	{
		if (Entity* entity = EntityManager::get()->findById((unsigned int)id)) mSelection.push_back(entity);
	}

	mSelected = active >= 0 ? EntityManager::get()->findById((unsigned int)active) : nullptr;

	if (mSelected == nullptr && !mSelection.empty()) mSelected = mSelection.back();

	mSelectionAnchor = mSelected;

	updateSelectionSet();
}

// Записує крок скасування, якщо сцена змінилася від попереднього запису
void Editor::recordUndo()
{
	if (mPlaying) return;

	std::vector<int> selection;
	int active = -1;
	captureSelection(selection, active);

	mHistory.record(mUndoSelection, mUndoActive, selection, active);

	mUndoSelection = selection;
	mUndoActive = active;
}

// Починає історію від поточного стану сцени
void Editor::resetUndo()
{
	mSceneGeneration++;
	mUndoCheckFrames = 0;

	mHistory.reset();

	captureSelection(mUndoSelection, mUndoActive);
}

// Скасовує останню зміну
void Editor::undo()
{
	// Під час гри сцену однаково поверне зупинка, а посеред перетягування крок ще не завершено
	if (mPlaying || mGizmo.isDragging()) return;

	// Зміна, яку ще не записали, теж має скасуватися
	recordUndo();

	std::vector<int> selection;
	int active = -1;

	if (!mHistory.undo(selection, active)) return;

	// Скасована правка матеріалу теж записується в його файл
	MaterialLibrary::get()->saveChanged();

	// Знищені й відновлені об'єкти мають нові адреси, тож старі джерела копіювання вже не ті
	mSceneGeneration++;

	restoreSelection(selection, active);

	captureSelection(mUndoSelection, mUndoActive);
}

// Повертає скасовану зміну
void Editor::redo()
{
	if (mPlaying || mGizmo.isDragging()) return;

	std::vector<int> selection;
	int active = -1;

	if (!mHistory.redo(selection, active)) return;

	MaterialLibrary::get()->saveChanged();

	mSceneGeneration++;

	restoreSelection(selection, active);

	captureSelection(mUndoSelection, mUndoActive);
}

// Збирає посилання компонента в порядку, у якому він їх перелічує
class ReferenceReader : public ReferenceVisitor
{
public:
	std::vector<Entity*> values;

	void reference(const char* name, Entity*& value, ReferenceKind kind) override { values.push_back(value); }
};

// Повертає копії посилання на об'єкти поза скопійованим набором, які запис не зберіг
class ReferenceRestorer : public ReferenceVisitor
{
public:
	ReferenceRestorer(const std::vector<Entity*>& values, const std::vector<Entity*>& copied) : mValues(values), mCopied(copied) {}

	void reference(const char* name, Entity*& value, ReferenceKind kind) override
	{
		Entity* original = mIndex < mValues.size() ? mValues[mIndex] : nullptr;
		mIndex++;

		// Посилання всередині набору запис уже переніс на копії, тож повертаються лише зовнішні
		if (value == nullptr && original && std::find(mCopied.begin(), mCopied.end(), original) == mCopied.end()) value = original;
	}

private:
	const std::vector<Entity*>& mValues;
	const std::vector<Entity*>& mCopied;
	size_t mIndex = 0;
};

// Записує вибрані об'єкти в буфер
void Editor::copySelection(Clipboard& clipboard) const
{
	std::vector<Entity*> roots = topLevelSelection();

	// Корінь префаба в режимі префаба лишається єдиним, тож його не копіюють
	if (isPrefabMode()) roots.erase(std::remove(roots.begin(), roots.end(), mPrefabRoot), roots.end());

	if (roots.empty()) return;

	clipboard.data = SceneSerializer::serializeEntities(roots);
	clipboard.sources = roots;
	clipboard.subtreeSizes.clear();
	clipboard.generation = mSceneGeneration;

	for (Entity* root : roots)
	{
		std::vector<Entity*> subtree;
		collectSubtree(root, subtree);

		clipboard.subtreeSizes.push_back((int)subtree.size());
	}
}

// Створює копії об'єктів з буфера; поруч із джерелами, якщо вони ще в цій самій сцені
void Editor::pasteClipboard(const Clipboard& clipboard)
{
	if (clipboard.data.empty()) return;

	std::string error;
	JsonValue data = JsonValue::parse(clipboard.data, &error);

	if (!error.empty()) return;

	std::vector<Entity*> created = SceneSerializer::buildSubtree(data, nullptr, false);

	// Джерела ще ті самі об'єкти, лише поки сцену не перебудовували
	bool sourcesValid = clipboard.generation == mSceneGeneration;

	std::vector<Entity*> copiedSources;

	if (sourcesValid)
	{
		for (Entity* source : clipboard.sources)
		{
			if (isAlive(source)) collectSubtree(source, copiedSources);
		}
	}

	std::vector<Entity*> pasted;

	size_t offset = 0;

	for (size_t root = 0; root < clipboard.subtreeSizes.size(); root++)
	{
		size_t size = (size_t)clipboard.subtreeSizes[root];

		Entity* copy = offset < created.size() ? created[offset] : nullptr;
		Entity* source = sourcesValid && isAlive(clipboard.sources[root]) ? clipboard.sources[root] : nullptr;

		// Копію, яку знищив власний компонент при прокиданні, пропускаємо
		if (copy)
		{
			std::vector<Entity*> sourceTree;
			if (source) collectSubtree(source, sourceTree);

			// Посилання на об'єкти поза копією запис губить, тож їх беремо з живого джерела
			if (sourceTree.size() == size)
			{
				for (size_t i = 0; i < size; i++)
				{
					Entity* copyEntity = created[offset + i];

					if (copyEntity == nullptr) continue;

					const std::list<Component*>& sourceComponents = sourceTree[i]->getComponentList();
					const std::list<Component*>& copyComponents = copyEntity->getComponentList();

					if (sourceComponents.size() != copyComponents.size()) continue;

					auto copyIt = copyComponents.begin();

					for (Component* sourceComponent : sourceComponents)
					{
						Component* copyComponent = *copyIt++;

						if (std::string(sourceComponent->getTypeName()) != copyComponent->getTypeName()) continue;

						ReferenceReader reader;
						sourceComponent->visitProperties(reader);

						ReferenceRestorer restorer(reader.values, copiedSources);
						copyComponent->visitProperties(restorer);
					}
				}
			}

			// Копія стає одразу після джерела під тим самим батьком, інакше в кінець сцени
			Entity* parent = source ? source->getParent() : nullptr;

			if (parent)
			{
				copy->setParent(parent, true);

				auto& siblings = *parent->getChildren();
				auto next = std::find(siblings.begin(), siblings.end(), source);

				if (next != siblings.end()) ++next;
				while (next != siblings.end() && std::find(pasted.begin(), pasted.end(), *next) != pasted.end()) ++next;

				parent->moveChildBefore(copy, next != siblings.end() ? *next : nullptr);
			}
			else if (source)
			{
				const std::list<Entity*>& entities = EntityManager::get()->getEntities();

				Entity* before = nullptr;
				bool passed = false;

				for (Entity* entity : entities)
				{
					if (entity == source) { passed = true; continue; }

					if (passed && entity->getParent() == nullptr && entity != copy && std::find(pasted.begin(), pasted.end(), entity) == pasted.end()) { before = entity; break; }
				}

				EntityManager::get()->moveRootBefore(copy, before);
			}

			// У префабі корінь один, тож копія лягає під нього
			if (isPrefabMode() && copy->getParent() == nullptr) adoptIntoPrefab(copy);

			pasted.push_back(copy);
		}

		offset += size;
	}

	EntityManager::get()->sortByHierarchy();

	if (pasted.empty()) return;

	mSelection = pasted;
	mSelected = pasted.back();
	mSelectionAnchor = mSelected;

	updateSelectionSet();

	mUndoCheckFrames = 2;
}

// Створює копії вибраних об'єктів поруч з ними
void Editor::duplicateSelection()
{
	syncSelection();

	// Буфер копіювання при цьому не змінюється
	Clipboard clipboard;
	copySelection(clipboard);

	pasteClipboard(clipboard);
}
