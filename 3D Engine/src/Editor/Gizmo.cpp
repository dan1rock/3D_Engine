#include "Gizmo.h"
#include "Entity.h"
#include "Transform.h"
#include "EntityManager.h"
#include "Renderer.h"
#include "SkySphere.h"
#include "Mesh.h"
#include "GraphicsEngine.h"
#include "GlobalResources.h"

#include "imgui.h"

#include <cmath>
#include <cfloat>

// Довжина осі маніпулятора у частках відстані до камери: так він завжди однакового розміру на екрані
static const float GIZMO_SCREEN_SIZE = 0.16f;
// Наскільки близько до осі має бути курсор, щоб вона вважалася схопленою
static const float GRAB_DISTANCE = 9.0f;

// Кольори осей збігаються зі звичними для редакторів: X червона, Y зелена, Z синя
static const ImU32 AXIS_COLORS[3] = {
	IM_COL32(230, 70, 70, 255),
	IM_COL32(110, 220, 110, 255),
	IM_COL32(90, 140, 240, 255),
};

static const ImU32 HIGHLIGHT_COLOR = IM_COL32(255, 220, 60, 255);

// Повертає глобальні константи кадру, де лежать матриці камери
static constant* frameConstants()
{
	return GraphicsEngine::get()->getGlobalResources()->getConstantData();
}

// Повертає світову матрицю камери, обернену до матриці виду
static Matrix cameraWorld()
{
	Matrix world = frameConstants()->view;
	world.inverse();

	return world;
}

// Будує промінь з камери крізь точку екрана
Ray Gizmo::screenPointToRay(float screenX, float screenY)
{
	ImVec2 display = ImGui::GetIO().DisplaySize;

	constant* constants = frameConstants();

	// Точка екрана переводиться у нормовані координати пристрою
	float ndcX = 2.0f * screenX / display.x - 1.0f;
	float ndcY = 1.0f - 2.0f * screenY / display.y;

	// Масштаби проєкції дають напрямок у просторі камери без оберненої матриці проєкції
	float xScale = constants->projection.mat[0][0];
	float yScale = constants->projection.mat[1][1];

	Vector3 viewDirection(ndcX / xScale, ndcY / yScale, 1.0f);

	Matrix world = cameraWorld();

	Vector3 right = world.getXDirection();
	Vector3 up = world.getYDirection();
	Vector3 forward = world.getZDirection();

	Ray ray;

	ray.origin = world.getTranslation();
	ray.direction = (right * viewDirection.x + up * viewDirection.y + forward * viewDirection.z).normalized();

	return ray;
}

// Переводить точку світу в координати екрана; повертає false, якщо вона позаду камери
bool Gizmo::projectToScreen(const Vector3& world, float& screenX, float& screenY)
{
	constant* constants = frameConstants();

	Matrix viewProjection = constants->view * constants->projection;

	const float(*m)[4] = viewProjection.mat;

	float x = world.x * m[0][0] + world.y * m[1][0] + world.z * m[2][0] + m[3][0];
	float y = world.x * m[0][1] + world.y * m[1][1] + world.z * m[2][1] + m[3][1];
	float w = world.x * m[0][3] + world.y * m[1][3] + world.z * m[2][3] + m[3][3];

	// За камерою точка не має екранного відповідника
	if (w < 0.0001f) return false;

	ImVec2 display = ImGui::GetIO().DisplaySize;

	screenX = (x / w * 0.5f + 0.5f) * display.x;
	screenY = (1.0f - (y / w * 0.5f + 0.5f)) * display.y;

	return true;
}

// Шукає перетин променя зі сферою та повертає відстань до ближчої точки
static bool raySphere(const Ray& ray, const Vector3& center, float radius, float& distance)
{
	Vector3 toCenter = center - ray.origin;

	float projection = toCenter * ray.direction;
	float squaredDistance = (toCenter * toCenter) - projection * projection;
	float squaredRadius = radius * radius;

	if (squaredDistance > squaredRadius) return false;

	float half = sqrtf(squaredRadius - squaredDistance);

	distance = projection - half;

	// Камера всередині сфери: перетин попереду все одно є
	if (distance < 0.0f) distance = projection + half;

	return distance >= 0.0f;
}

// Шукає перетин променя з трикутником за алгоритмом Мьоллера-Трумбора
static bool rayTriangle(const Ray& ray, const Vector3& a, const Vector3& b, const Vector3& c, float& distance)
{
	Vector3 edge1 = b - a;
	Vector3 edge2 = c - a;

	Vector3 h = ray.direction.cross(edge2);

	float determinant = edge1 * h;

	// Промінь паралельний площині трикутника
	if (fabsf(determinant) < 1e-8f) return false;

	float inverse = 1.0f / determinant;

	Vector3 toVertex = ray.origin - a;

	float u = (toVertex * h) * inverse;

	if (u < 0.0f || u > 1.0f) return false;

	Vector3 q = toVertex.cross(edge1);

	float v = (ray.direction * q) * inverse;

	if (v < 0.0f || u + v > 1.0f) return false;

	distance = (edge2 * q) * inverse;

	return distance > 0.0f;
}

// Перетинає промінь з площиною, заданою точкою та нормаллю
static bool rayPlane(const Ray& ray, const Vector3& point, const Vector3& normal, Vector3& hit)
{
	float denominator = normal * ray.direction;

	if (fabsf(denominator) < 1e-6f) return false;

	float distance = (normal * (point - ray.origin)) / denominator;

	if (distance < 0.0f) return false;

	hit = ray.origin + ray.direction * distance;

	return true;
}

// Повертає зсув уздовж осі до точки, на яку зараз дивиться курсор. Промінь перетинається
// з площиною, що містить вісь і якнайкраще повернена до камери
static bool axisOffsetUnderMouse(const Vector3& origin, const Vector3& axis, float& offset)
{
	ImVec2 mouse = ImGui::GetIO().MousePos;

	Ray ray = Gizmo::screenPointToRay(mouse.x, mouse.y);

	Vector3 cameraPosition = cameraWorld().getTranslation();

	Vector3 toCamera = (cameraPosition - origin).normalized();
	Vector3 normal = toCamera - axis * (toCamera * axis);

	// Вісь дивиться точно на камеру: площини для перетягування не існує
	if (normal.length() < 1e-4f) return false;

	normal = normal.normalized();

	Vector3 hit;

	if (!rayPlane(ray, origin, normal, hit)) return false;

	offset = (hit - origin) * axis;

	return true;
}

// Повертає відстань від точки до відрізка на екрані
static float distanceToSegment(float px, float py, float ax, float ay, float bx, float by)
{
	float dx = bx - ax;
	float dy = by - ay;

	float lengthSquared = dx * dx + dy * dy;

	if (lengthSquared < 1e-6f) return sqrtf((px - ax) * (px - ax) + (py - ay) * (py - ay));

	float t = ((px - ax) * dx + (py - ay) * dy) / lengthSquared;

	if (t < 0.0f) t = 0.0f;
	if (t > 1.0f) t = 1.0f;

	float cx = ax + dx * t;
	float cy = ay + dy * t;

	return sqrtf((px - cx) * (px - cx) + (py - cy) * (py - cy));
}

// Шукає найближчий видимий об'єкт під вказаною точкою екрана
Entity* Gizmo::pick(float screenX, float screenY)
{
	Ray ray = screenPointToRay(screenX, screenY);

	Entity* closest = nullptr;
	float closestDistance = FLT_MAX;

	for (Entity* entity : EntityManager::get()->getEntities())
	{
		if (!entity->isActive()) continue;

		Renderer* renderer = entity->getComponent<Renderer>();

		if (renderer == nullptr) continue;

		// Небесна сфера охоплює всю сцену, тому клік по ній нічого не значить
		if (dynamic_cast<SkySphere*>(renderer) != nullptr) continue;

		Mesh* mesh = renderer->getMesh();

		if (mesh == nullptr || mesh->getBoundsRadius() <= 0.0f) continue;

		Matrix model = *entity->getTransform()->getMatrix();

		// Груба перевірка за описаною сферою відкидає більшість об'єктів без обходу трикутників
		Vector3 localCenter = mesh->getBoundsCenter();

		Vector3 center(
			localCenter.x * model.mat[0][0] + localCenter.y * model.mat[1][0] + localCenter.z * model.mat[2][0] + model.mat[3][0],
			localCenter.x * model.mat[0][1] + localCenter.y * model.mat[1][1] + localCenter.z * model.mat[2][1] + model.mat[3][1],
			localCenter.x * model.mat[0][2] + localCenter.y * model.mat[1][2] + localCenter.z * model.mat[2][2] + model.mat[3][2]);

		Vector3 scale = entity->getTransform()->getScale();

		float maxScale = fabsf(scale.x);
		if (fabsf(scale.y) > maxScale) maxScale = fabsf(scale.y);
		if (fabsf(scale.z) > maxScale) maxScale = fabsf(scale.z);

		float sphereDistance = 0.0f;

		if (!raySphere(ray, center, mesh->getBoundsRadius() * maxScale, sphereDistance)) continue;

		// Далі об'єкта, ніж уже знайдений, шукати немає сенсу
		if (sphereDistance > closestDistance) continue;

		// Промінь переводиться у простір об'єкта, щоб не перетворювати кожну вершину
		Matrix inverseModel = model;
		inverseModel.inverse();

		Ray localRay;

		localRay.origin = Vector3(
			ray.origin.x * inverseModel.mat[0][0] + ray.origin.y * inverseModel.mat[1][0] + ray.origin.z * inverseModel.mat[2][0] + inverseModel.mat[3][0],
			ray.origin.x * inverseModel.mat[0][1] + ray.origin.y * inverseModel.mat[1][1] + ray.origin.z * inverseModel.mat[2][1] + inverseModel.mat[3][1],
			ray.origin.x * inverseModel.mat[0][2] + ray.origin.y * inverseModel.mat[1][2] + ray.origin.z * inverseModel.mat[2][2] + inverseModel.mat[3][2]);

		// Напрямок переносу не зазнає, тому останній рядок матриці не береться
		localRay.direction = Vector3(
			ray.direction.x * inverseModel.mat[0][0] + ray.direction.y * inverseModel.mat[1][0] + ray.direction.z * inverseModel.mat[2][0],
			ray.direction.x * inverseModel.mat[0][1] + ray.direction.y * inverseModel.mat[1][1] + ray.direction.z * inverseModel.mat[2][1],
			ray.direction.x * inverseModel.mat[0][2] + ray.direction.y * inverseModel.mat[1][2] + ray.direction.z * inverseModel.mat[2][2]);

		// Масштаб об'єкта міняє довжину напрямку, тому відстані перераховуються назад у світові
		float directionLength = localRay.direction.length();

		if (directionLength < 1e-8f) continue;

		localRay.direction = localRay.direction * (1.0f / directionLength);

		const std::vector<Vector3>& positions = mesh->getPositions();
		const std::vector<unsigned int>& indices = mesh->getIndices();

		for (size_t i = 0; i + 2 < indices.size(); i += 3)
		{
			float distance = 0.0f;

			if (!rayTriangle(localRay, positions[indices[i]], positions[indices[i + 1]], positions[indices[i + 2]], distance)) continue;

			float worldDistance = distance / directionLength;

			if (worldDistance < closestDistance)
			{
				closestDistance = worldDistance;
				closest = entity;
			}
		}
	}

	return closest;
}

// Перевіряє, чи триває перетягування просто зараз
bool Gizmo::isDragging() const
{
	return mDragging;
}

// Малює маніпулятор вибраного об'єкта та обробляє перетягування
bool Gizmo::update(Entity* entity)
{
	if (entity == nullptr)
	{
		mDragging = false;
		mAxis = -1;
		return false;
	}

	Transform* transform = entity->getTransform();

	Vector3 origin = transform->getPosition();

	// Осі беруться або зі світу, або з самого об'єкта
	Matrix model = *transform->getMatrix();

	Vector3 axes[3];

	if (local)
	{
		axes[0] = model.getXDirection().normalized();
		axes[1] = model.getYDirection().normalized();
		axes[2] = model.getZDirection().normalized();
	}
	else
	{
		axes[0] = Vector3(1.0f, 0.0f, 0.0f);
		axes[1] = Vector3(0.0f, 1.0f, 0.0f);
		axes[2] = Vector3(0.0f, 0.0f, 1.0f);
	}

	// Розмір підбирається під відстань до камери, щоб маніпулятор не мінявся на екрані
	Vector3 cameraPosition = cameraWorld().getTranslation();

	float size = (origin - cameraPosition).length() * GIZMO_SCREEN_SIZE;

	if (size < 0.0001f) return false;

	if (mDragging)
	{
		// Кнопку відпустили: перетягування завершено
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			mDragging = false;
			mAxis = -1;
			return true;
		}

		if (mode == GizmoMode::Translate) dragTranslate(entity);
		else if (mode == GizmoMode::Scale) dragScale(entity, size);
		else dragRotate(entity);
	}

	float originX = 0.0f;
	float originY = 0.0f;

	if (!projectToScreen(origin, originX, originY)) return mDragging;

	ImDrawList* draw = ImGui::GetBackgroundDrawList();

	ImVec2 mouse = ImGui::GetIO().MousePos;

	int hovered = -1;
	float hoveredDistance = GRAB_DISTANCE;

	float endX[3] = {};
	float endY[3] = {};
	bool visible[3] = {};

	for (int axis = 0; axis < 3; axis++)
	{
		visible[axis] = projectToScreen(origin + axes[axis] * size, endX[axis], endY[axis]);

		if (!visible[axis]) continue;

		// Обертання хапається за коло, решта режимів — за відрізок осі
		float distance = mode == GizmoMode::Rotate
			? fabsf(distanceToSegment(mouse.x, mouse.y, originX, originY, endX[axis], endY[axis]) - 0.0f)
			: distanceToSegment(mouse.x, mouse.y, originX, originY, endX[axis], endY[axis]);

		if (mode == GizmoMode::Rotate) continue;

		if (distance < hoveredDistance)
		{
			hoveredDistance = distance;
			hovered = axis;
		}
	}

	// Коло обертання перевіряється окремо: курсор має бути біля самого кола, а не всередині
	if (mode == GizmoMode::Rotate)
	{
		for (int axis = 0; axis < 3; axis++)
		{
			float best = FLT_MAX;

			Vector3 first = axes[(axis + 1) % 3];
			Vector3 second = axes[(axis + 2) % 3];

			for (int step = 0; step < 48; step++)
			{
				float angle = (float)step / 48.0f * 6.2831853f;

				Vector3 point = origin + (first * cosf(angle) + second * sinf(angle)) * size;

				float px = 0.0f;
				float py = 0.0f;

				if (!projectToScreen(point, px, py)) continue;

				float distance = sqrtf((mouse.x - px) * (mouse.x - px) + (mouse.y - py) * (mouse.y - py));

				if (distance < best) best = distance;
			}

			if (best < hoveredDistance)
			{
				hoveredDistance = best;
				hovered = axis;
			}
		}
	}

	// Малюємо маніпулятор у поточному режимі
	for (int axis = 0; axis < 3; axis++)
	{
		bool active = mDragging ? mAxis == axis : hovered == axis;

		ImU32 color = active ? HIGHLIGHT_COLOR : AXIS_COLORS[axis];

		if (mode == GizmoMode::Rotate)
		{
			Vector3 first = axes[(axis + 1) % 3];
			Vector3 second = axes[(axis + 2) % 3];

			ImVec2 previous(0.0f, 0.0f);
			bool hasPrevious = false;

			for (int step = 0; step <= 48; step++)
			{
				float angle = (float)step / 48.0f * 6.2831853f;

				Vector3 point = origin + (first * cosf(angle) + second * sinf(angle)) * size;

				float px = 0.0f;
				float py = 0.0f;

				if (!projectToScreen(point, px, py)) { hasPrevious = false; continue; }

				if (hasPrevious) draw->AddLine(previous, ImVec2(px, py), color, active ? 3.0f : 2.0f);

				previous = ImVec2(px, py);
				hasPrevious = true;
			}

			continue;
		}

		if (!visible[axis]) continue;

		ImVec2 from(originX, originY);
		ImVec2 to(endX[axis], endY[axis]);

		draw->AddLine(from, to, color, active ? 4.0f : 2.5f);

		if (mode == GizmoMode::Translate)
		{
			// Вістря стрілки будується у площині екрана, щоб не рахувати конус у просторі
			float dx = to.x - from.x;
			float dy = to.y - from.y;

			float length = sqrtf(dx * dx + dy * dy);

			if (length < 1.0f) continue;

			dx /= length;
			dy /= length;

			float head = active ? 14.0f : 11.0f;

			ImVec2 left(to.x - dx * head - dy * head * 0.45f, to.y - dy * head + dx * head * 0.45f);
			ImVec2 right(to.x - dx * head + dy * head * 0.45f, to.y - dy * head - dx * head * 0.45f);

			draw->AddTriangleFilled(to, left, right, color);
		}
		else
		{
			draw->AddCircleFilled(to, active ? 7.0f : 5.5f, color);
		}
	}

	draw->AddCircleFilled(ImVec2(originX, originY), 4.0f, IM_COL32(240, 240, 240, 255));

	// Починаємо перетягування, якщо натиснули біля осі
	if (!mDragging && hovered >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		mDragging = true;
		mAxis = hovered;

		mDragAxis = axes[mAxis];
		mDragFirst = axes[(mAxis + 1) % 3];
		mDragSecond = axes[(mAxis + 2) % 3];

		mStartPosition = transform->getPosition();
		mStartRotation = transform->getRotation();
		mStartScale = transform->getScale();

		Ray ray = screenPointToRay(mouse.x, mouse.y);

		if (mode == GizmoMode::Rotate)
		{
			Vector3 hit;

			if (rayPlane(ray, origin, axes[mAxis], hit))
			{
				Vector3 offset = hit - origin;

				mStartAngle = atan2f(offset * mDragSecond, offset * mDragFirst);
			}
		}
		else
		{
			mStartOffset = 0.0f;

			axisOffsetUnderMouse(origin, mDragAxis, mStartOffset);
		}
	}

	return mDragging || hovered >= 0;
}

// Переміщує об'єкт уздовж осі за курсором
bool Gizmo::dragTranslate(Entity* entity)
{
	float offset = 0.0f;

	if (!axisOffsetUnderMouse(mStartPosition, mDragAxis, offset)) return false;

	entity->getTransform()->setPosition(mStartPosition + mDragAxis * (offset - mStartOffset));

	return true;
}

// Змінює масштаб об'єкта вздовж осі за курсором
bool Gizmo::dragScale(Entity* entity, float size)
{
	float offset = 0.0f;

	if (!axisOffsetUnderMouse(mStartPosition, mDragAxis, offset)) return false;

	// Зсув на всю довжину осі подвоює масштаб
	float factor = 1.0f + (offset - mStartOffset) / size;

	if (factor < 0.01f) factor = 0.01f;

	Vector3 scale = mStartScale;

	if (mAxis == 0) scale.x = mStartScale.x * factor;
	else if (mAxis == 1) scale.y = mStartScale.y * factor;
	else scale.z = mStartScale.z * factor;

	entity->getTransform()->setScale(scale);

	return true;
}

// Обертає об'єкт навколо осі за курсором
bool Gizmo::dragRotate(Entity* entity)
{
	ImVec2 mouse = ImGui::GetIO().MousePos;

	Ray ray = screenPointToRay(mouse.x, mouse.y);

	Vector3 hit;

	if (!rayPlane(ray, mStartPosition, mDragAxis, hit)) return false;

	// Кут рахується у тій самій парі осей, що й на початку перетягування
	Vector3 offset = hit - mStartPosition;

	float angle = atan2f(offset * mDragSecond, offset * mDragFirst) - mStartAngle;

	// Кути Ейлера самі по собі не дають обертання навколо довільної осі, тому складаємо
	// матриці і лише наприкінці повертаємось до кутів
	Matrix start;
	start.setIdentity();
	start.setRotation(mStartRotation);

	Matrix delta;
	delta.setIdentity();

	if (mAxis == 0) delta.setRotationX(angle);
	else if (mAxis == 1) delta.setRotationY(angle);
	else delta.setRotationZ(angle);

	// У локальному режимі поворот застосовується до осей об'єкта, тобто перед його власним
	Matrix result = local ? delta * start : start * delta;

	entity->getTransform()->setRotation(result.getRotation());

	return true;
}
