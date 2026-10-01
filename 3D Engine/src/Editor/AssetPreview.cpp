#include "AssetPreview.h"
#include "MaterialLibrary.h"
#include "PrefabLibrary.h"
#include "GraphicsEngine.h"
#include "DeviceContext.h"
#include "GlobalResources.h"
#include "MeshManager.h"
#include "Mesh.h"
#include "TextureManager.h"
#include "Texture.h"
#include "Material.h"
#include "IndexBuffer.h"
#include "Prefab.h"
#include "Entity.h"
#include "Renderer.h"
#include "Matrix.h"

#include "imgui.h"
#include <DirectXTex.h>

#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>
#include <algorithm>
#include <cmath>

namespace filesystem = std::experimental::filesystem;

// Тека кешу мініатюр; як і інші похідні файли, вона не потрібна в репозиторії
static const char* CACHE_FOLDER = "Library\\Thumbnails";
// Номер вигляду мініатюр: його зміна перемальовує всі збережені
static const char* PREVIEW_VERSION = "2";
// Скільки кадрів мініатюра не перевіряє, чи змінився її ресурс
static const int CHECK_INTERVAL = 30;

// Одна сітка, яку треба намалювати в мініатюрі: меш, його розташування і матеріал кожного слота
struct PreviewItem
{
	Mesh* mesh = nullptr;
	Matrix matrix;
	std::vector<Material*> materials;
};

// Повертає час останньої зміни файлу числом, або 0, якщо файлу немає
static long long modifiedTime(const std::string& path)
{
	std::error_code error;

	if (!filesystem::exists(path, error)) return 0;

	return (long long)filesystem::last_write_time(path, error).time_since_epoch().count();
}

// Переносить точку з простору меша у світ рядковою матрицею рушія
static Vector3 transformPoint(const Matrix& m, const Vector3& v)
{
	return Vector3(
		v.x * m.mat[0][0] + v.y * m.mat[1][0] + v.z * m.mat[2][0] + m.mat[3][0],
		v.x * m.mat[0][1] + v.y * m.mat[1][1] + v.z * m.mat[2][1] + m.mat[3][1],
		v.x * m.mat[0][2] + v.y * m.mat[1][2] + v.z * m.mat[2][2] + m.mat[3][2]);
}

// Збирає меші образу префаба разом з матрицями й матеріалами, обходячи всі його частини
static void collectPrefab(Entity* entity, bool isRoot, std::vector<PreviewItem>& items)
{
	// Вимкнена частина не видна і в екземплярі; корінь образу вимкнений завжди, тож його не перевіряємо
	if (!isRoot && !entity->isActiveSelf) return;

	for (Component* component : entity->getComponentList())
	{
		Renderer* renderer = dynamic_cast<Renderer*>(component);

		if (renderer == nullptr || renderer->getMesh() == nullptr) continue;

		PreviewItem item;
		item.mesh = renderer->getMesh();
		item.matrix = *entity->getTransform()->getMatrix();

		for (unsigned int slot = 0; slot < renderer->getMaterialCount(); slot++)
		{
			item.materials.push_back(renderer->getSharedMaterial(slot));
		}

		items.push_back(item);
	}

	for (Entity* child : *entity->getChildren())
	{
		collectPrefab(child, false, items);
	}
}

// Створює буфер глибини, шейдер копіювання текстур і семплер
bool AssetPreview::init()
{
	ID3D11Device* device = GraphicsEngine::get()->mD3dDevice;

	D3D11_TEXTURE2D_DESC depthDesc = {};
	depthDesc.Width = SIZE;
	depthDesc.Height = SIZE;
	depthDesc.MipLevels = 1;
	depthDesc.ArraySize = 1;
	depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.Usage = D3D11_USAGE_DEFAULT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	if (FAILED(device->CreateTexture2D(&depthDesc, nullptr, &mDepth))) return false;
	if (FAILED(device->CreateDepthStencilView(mDepth, nullptr, &mDepthView))) return false;

	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	if (FAILED(device->CreateSamplerState(&samplerDesc, &mSampler))) return false;

	mFullscreenShader = GraphicsEngine::get()->getVertexShader(L"src\\Shaders\\PostProcessVertexShader.hlsl", "main");
	mBlitShader = GraphicsEngine::get()->getPixelShader(L"src\\Shaders\\PreviewBlitPixelShader.hlsl", "main");

	std::error_code error;
	filesystem::create_directories(CACHE_FOLDER, error);

	return mFullscreenShader != nullptr && mBlitShader != nullptr;
}

// Звільняє всі мініатюри та ресурси відеокарти
void AssetPreview::release()
{
	for (auto& entry : mThumbnails)
	{
		replace(entry.second, nullptr, nullptr);
	}

	mThumbnails.clear();
	mQueue.clear();

	if (mDepthView) mDepthView->Release();
	if (mDepth) mDepth->Release();
	if (mSampler) mSampler->Release();

	mDepthView = nullptr;
	mDepth = nullptr;
	mSampler = nullptr;
}

// Повертає позначку стану ресурсу: час зміни його файлів, тож змінений ресурс отримує нову мініатюру
std::string AssetPreview::stampOf(Kind kind, const std::string& path) const
{
	std::string stamp = PREVIEW_VERSION;

	// Вбудований матеріал не має файлу і не змінюється
	if (kind == Kind::Material && path == MaterialLibrary::DEFAULT_NAME) return stamp + "-builtin";

	stamp += "-" + std::to_string(modifiedTime(path));

	// Заміни матеріалів моделі лежать у її файлі налаштувань імпорту
	if (kind == Kind::Model) stamp += "-" + std::to_string(modifiedTime(path + ".import"));

	return stamp;
}

// Повертає шлях файлу кешу мініатюри для ключа та позначки стану
std::string AssetPreview::cacheFile(const std::string& key, const std::string& stamp) const
{
	return cachePrefix(key) + stamp + ".png";
}

// Повертає початок шляху файлів кешу мініатюри ресурсу, спільний для всіх його станів
std::string AssetPreview::cachePrefix(const std::string& key) const
{
	std::string name = key;

	for (char& symbol : name)
	{
		if (std::string("\\/:*?\"<>| ").find(symbol) != std::string::npos) symbol = '_';
	}

	return std::string(CACHE_FOLDER) + "\\" + name + "_";
}

// Повертає мініатюру ресурсу або nullptr, поки її ще не намальовано; застарілу мініатюру ставить у чергу на перемальовування
ID3D11ShaderResourceView* AssetPreview::get(Kind kind, const std::string& path)
{
	std::string key = std::to_string((int)kind) + ":" + path;
	Thumbnail& thumbnail = mThumbnails[key];

	int frame = ImGui::GetFrameCount();

	// Файли ресурсів перевіряються не щокадру, а раз на кілька кадрів
	if (frame - thumbnail.checkedFrame < CHECK_INTERVAL) return thumbnail.view;

	thumbnail.checkedFrame = frame;

	std::string stamp = stampOf(kind, path);

	if (stamp == thumbnail.stamp || thumbnail.queued) return thumbnail.view;

	// Мініатюра того самого стану ресурсу вже могла бути збережена раніше
	std::string file = cacheFile(key, stamp);

	if (loadCached(file, thumbnail))
	{
		thumbnail.stamp = stamp;
		thumbnail.file = file;
	}
	else
	{
		thumbnail.queued = true;
		mQueue.push_back(std::make_pair(kind, path));
	}

	return thumbnail.view;
}

// Читає мініатюру з файлу кешу; повертає false, якщо файлу немає
bool AssetPreview::loadCached(const std::string& file, Thumbnail& thumbnail)
{
	std::error_code error;

	if (!filesystem::exists(file, error)) return false;

	std::wstring wide(file.begin(), file.end());

	// Збережені значення вже закодовані для показу, тож їх не треба вважати кольорами sRGB
	DirectX::ScratchImage image;

	if (FAILED(DirectX::LoadFromWICFile(wide.c_str(), DirectX::WIC_FLAGS_IGNORE_SRGB, nullptr, image))) return false;

	ID3D11Resource* resource = nullptr;
	ID3D11ShaderResourceView* view = nullptr;

	ID3D11Device* device = GraphicsEngine::get()->mD3dDevice;

	if (FAILED(DirectX::CreateTexture(device, image.GetImages(), image.GetImageCount(), image.GetMetadata(), &resource))) return false;

	if (FAILED(device->CreateShaderResourceView(resource, nullptr, &view)))
	{
		resource->Release();
		return false;
	}

	replace(thumbnail, resource, view);

	return true;
}

// Замінює зображення мініатюри новим, звільняючи старе
void AssetPreview::replace(Thumbnail& thumbnail, ID3D11Resource* resource, ID3D11ShaderResourceView* view)
{
	if (thumbnail.view) thumbnail.view->Release();
	if (thumbnail.resource) thumbnail.resource->Release();

	thumbnail.resource = resource;
	thumbnail.view = view;
}

// Малює одну мініатюру з черги; викликається раз на кадр до рендеру сцени
void AssetPreview::renderPending()
{
	if (mQueue.empty() || mDepthView == nullptr) return;

	Kind kind = mQueue.front().first;
	std::string path = mQueue.front().second;
	mQueue.erase(mQueue.begin());

	std::string key = std::to_string((int)kind) + ":" + path;
	Thumbnail& thumbnail = mThumbnails[key];

	thumbnail.queued = false;

	std::string stamp = stampOf(kind, path);

	ID3D11Texture2D* texture = nullptr;

	// Ресурс, який не вдалося прочитати, лишається з заглушкою, доки його файл не зміниться
	if (!render(kind, path, texture))
	{
		thumbnail.stamp = stamp;
		return;
	}

	ID3D11ShaderResourceView* view = nullptr;

	D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
	viewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	viewDesc.Texture2D.MipLevels = 1;

	if (FAILED(GraphicsEngine::get()->mD3dDevice->CreateShaderResourceView(texture, &viewDesc, &view)))
	{
		texture->Release();
		thumbnail.stamp = stamp;
		return;
	}

	// Новий файл кешу заміняє мініатюру попереднього стану ресурсу
	std::string file = cacheFile(key, stamp);

	save(texture, file);

	// Файли попередніх станів того самого ресурсу більше не знадобляться
	std::string prefix = cachePrefix(key);
	std::error_code error;

	for (const auto& entry : filesystem::directory_iterator(CACHE_FOLDER, error))
	{
		std::string other = std::string(CACHE_FOLDER) + "\\" + entry.path().filename().string();

		if (other != file && other.compare(0, prefix.size(), prefix) == 0) filesystem::remove(entry.path(), error);
	}

	replace(thumbnail, texture, view);

	thumbnail.stamp = stamp;
	thumbnail.file = file;
}

// Малює мініатюру ресурсу в нове зображення; повертає false, якщо ресурс не вдалося прочитати
bool AssetPreview::render(Kind kind, const std::string& path, ID3D11Texture2D*& texture)
{
	GraphicsEngine* graphics = GraphicsEngine::get();
	ID3D11Device* device = graphics->mD3dDevice;
	DeviceContext* context = graphics->getImmDeviceContext();
	ID3D11DeviceContext* raw = context->mDeviceContext;

	std::wstring widePath(path.begin(), path.end());

	// Що саме малювати: сітки з матеріалами або текстуру
	std::vector<PreviewItem> items;
	Texture* source = nullptr;

	if (kind == Kind::Texture)
	{
		source = graphics->getTextureManager()->createTextureFromFile(widePath.c_str());

		if (source == nullptr) return false;
	}
	else if (kind == Kind::Model)
	{
		Mesh* mesh = graphics->getMeshManager()->createMeshFromFile(widePath.c_str());

		if (mesh == nullptr) return false;

		PreviewItem item;
		item.mesh = mesh;
		item.matrix.setIdentity();

		// Модель показується зі своїми матеріалами, як її поставить у сцену перетягування
		for (unsigned int slot = 0; slot < (mesh->getMaterialCount() > 0 ? mesh->getMaterialCount() : 1); slot++)
		{
			Material* material = MaterialLibrary::get()->modelMaterial(mesh, slot);
			item.materials.push_back(material ? material : graphics->getGlobalResources()->getDefaultMaterial());
		}

		items.push_back(item);
	}
	else if (kind == Kind::Prefab)
	{
		Prefab* prefab = PrefabLibrary::get()->getTemplate(path);

		if (prefab == nullptr) return false;

		collectPrefab(prefab, true, items);

		if (items.empty()) return false;
	}
	else
	{
		Material* material = MaterialLibrary::get()->find(path);
		Mesh* sphere = graphics->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\sphere.obj");

		if (material == nullptr || sphere == nullptr) return false;

		PreviewItem item;
		item.mesh = sphere;
		item.matrix.setIdentity();
		item.materials.assign(sphere->getMaterialCount() > 0 ? sphere->getMaterialCount() : 1, material);

		items.push_back(item);
	}

	// Зображення без типу дозволяє писати в нього з кодуванням sRGB, а читати закодовані значення як є
	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = SIZE;
	desc.Height = SIZE;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	if (FAILED(device->CreateTexture2D(&desc, nullptr, &texture))) return false;

	// Текстура копіюється без перекодування, а освітлені сітки пишуться з переведенням з лінійного простору
	D3D11_RENDER_TARGET_VIEW_DESC targetDesc = {};
	targetDesc.Format = kind == Kind::Texture ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	targetDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

	ID3D11RenderTargetView* target = nullptr;

	if (FAILED(device->CreateRenderTargetView(texture, &targetDesc, &target)))
	{
		texture->Release();
		texture = nullptr;
		return false;
	}

	// Прозоре тло дає мініатюрі лягти на колір панелі
	const float clear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	raw->ClearRenderTargetView(target, clear);

	if (kind == Kind::Texture)
	{
		// Текстура вписується в квадрат зі збереженням пропорцій
		unsigned int width = 1;
		unsigned int height = 1;
		source->getSize(width, height);

		float scale = (float)SIZE / (float)(width > height ? width : height);

		D3D11_VIEWPORT viewport = {};
		viewport.Width = width * scale;
		viewport.Height = height * scale;
		viewport.TopLeftX = (SIZE - viewport.Width) * 0.5f;
		viewport.TopLeftY = (SIZE - viewport.Height) * 0.5f;
		viewport.MaxDepth = 1.0f;

		raw->OMSetRenderTargets(1, &target, nullptr);
		raw->RSSetViewports(1, &viewport);
		raw->RSSetState(nullptr);

		context->setBlendState(nullptr);
		context->setVertexShader(mFullscreenShader);
		context->setPixelShader(mBlitShader);
		context->setTexture(source, 0);
		context->setSamplerState(mSampler, 3);
		context->setFullscreenTriangle();
		context->drawTriangleList(3, 0);
	}
	else
	{
		raw->ClearDepthStencilView(mDepthView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		raw->OMSetRenderTargets(1, &target, mDepthView);
		context->setViewportSize(SIZE, SIZE);

		// Справжні межі вершин задають, звідки на сітки дивиться камера: так ресурс заповнює мініатюру
		Vector3 low(1e30f, 1e30f, 1e30f);
		Vector3 high(-1e30f, -1e30f, -1e30f);

		for (const PreviewItem& item : items)
		{
			for (const Vector3& position : item.mesh->getPositions())
			{
				Vector3 world = transformPoint(item.matrix, position);

				low = Vector3(std::min(low.x, world.x), std::min(low.y, world.y), std::min(low.z, world.z));
				high = Vector3(std::max(high.x, world.x), std::max(high.y, world.y), std::max(high.z, world.z));
			}
		}

		if (low.x > high.x)
		{
			low = Vector3(-1.0f, -1.0f, -1.0f);
			high = Vector3(1.0f, 1.0f, 1.0f);
		}

		Vector3 center = (low + high) * 0.5f;

		// Куля матеріалу вписується майже впритул, а сітка - за сферою навколо своїх меж, трохи щільніше, бо кути меж рідко видно
		float radius = kind == Kind::Material ? 1.05f : std::max((high - low).length() * 0.5f * 0.8f, 0.01f);

		// Камера дивиться згори спереду збоку, матеріал - майже прямо спереду
		Vector3 direction = kind == Kind::Material ? Vector3(0.0f, 0.25f, 1.0f).normalized() : Vector3(0.9f, 0.65f, 1.3f).normalized();

		const float fov = 0.5f;
		float distance = radius / sinf(fov * 0.5f);

		Vector3 eye = center + direction * distance;
		Vector3 forward = (center - eye).normalized();

		Matrix camera;
		camera.setIdentity();
		camera.setRotation(Vector3(asinf(-forward.y), atan2f(forward.x, forward.z), 0.0f));
		camera.setTranslation(eye);

		Matrix view = camera;
		view.inverse();

		Matrix projection;
		projection.setPerspectivePM(fov, 1.0f, std::max(distance - radius * 2.0f, 0.01f), distance + radius * 2.0f);

		// Кадр сцени вже зібрав свої сталі, тож мініатюра працює з копією і повертає їх після себе
		constant* constants = graphics->getGlobalResources()->getConstantData();
		constant saved = *constants;

		constants->view = view;
		constants->projection = projection;
		constants->cameraPos[0] = eye.x;
		constants->cameraPos[1] = eye.y;
		constants->cameraPos[2] = eye.z;

		// Світло падає згори зліва від глядача, без тіней сцени
		Vector3 light = Vector3(-0.5f, -0.75f, -0.45f).normalized();

		constants->lightDir[0] = light.x;
		constants->lightDir[1] = light.y;
		constants->lightDir[2] = light.z;
		constants->lightPos[0] = center.x - light.x * 1000.0f;
		constants->lightPos[1] = center.y - light.y * 1000.0f;
		constants->lightPos[2] = center.z - light.z * 1000.0f;
		constants->lightColor[0] = 1.2f;
		constants->lightColor[1] = 1.16f;
		constants->lightColor[2] = 1.1f;
		constants->shadowParams[3] = 0.0f;

		graphics->bindEnvironment();

		// Спершу непрозорі частини, потім прозорі, як у сцені
		for (RenderPass pass : { RenderPass::Opaque, RenderPass::Transparent })
		{
			graphics->setRenderPass(pass);

			for (const PreviewItem& item : items)
			{
				constants->model = item.matrix;

				Matrix inverseTranspose = item.matrix;
				inverseTranspose.inverse();
				inverseTranspose.transpose();
				constants->invTransModel = inverseTranspose;

				graphics->getGlobalResources()->updateConstantBuffer();

				Mesh* mesh = item.mesh;

				if (mesh->getIndexBuffer() == nullptr) continue;

				context->setVertexBuffer(mesh->getVertexBuffer());
				context->setIndexBuffer(mesh->getIndexBuffer());

				auto materialOf = [&](unsigned int slot) {
					Material* material = slot < item.materials.size() ? item.materials[slot] : nullptr;
					return material ? material : graphics->getGlobalResources()->getDefaultMaterial();
				};

				const std::vector<SubMesh>& subMeshes = mesh->getSubMeshes();

				if (subMeshes.empty())
				{
					Material* material = materialOf(0);

					if (material->isTransparent() != (pass == RenderPass::Transparent)) continue;

					graphics->setMaterial(material);
					context->drawIndexedTriangleList(mesh->getIndexBuffer()->getVertexListSize(), 0, 0);
					continue;
				}

				for (const SubMesh& subMesh : subMeshes)
				{
					Material* material = materialOf(subMesh.materialSlot);

					if (material->isTransparent() != (pass == RenderPass::Transparent)) continue;

					graphics->setMaterial(material);
					context->drawIndexedTriangleList(subMesh.indexCount, subMesh.indexStart, 0);
				}
			}
		}

		graphics->resetRenderStates();
		graphics->setRenderPass(RenderPass::Opaque);

		*constants = saved;
		graphics->getGlobalResources()->updateConstantBuffer();
	}

	// Мініатюра більше не ціль рендеру, тож її можна читати в панелі
	raw->OMSetRenderTargets(0, nullptr, nullptr);
	target->Release();

	return true;
}

// Записує намальовану мініатюру у файл кешу
void AssetPreview::save(ID3D11Texture2D* texture, const std::string& file)
{
	ID3D11Device* device = GraphicsEngine::get()->mD3dDevice;
	ID3D11DeviceContext* raw = GraphicsEngine::get()->getImmDeviceContext()->mDeviceContext;

	D3D11_TEXTURE2D_DESC desc = {};
	texture->GetDesc(&desc);

	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

	ID3D11Texture2D* staging = nullptr;

	if (FAILED(device->CreateTexture2D(&desc, nullptr, &staging))) return;

	raw->CopyResource(staging, texture);

	D3D11_MAPPED_SUBRESOURCE mapped = {};

	if (SUCCEEDED(raw->Map(staging, 0, D3D11_MAP_READ, 0, &mapped)))
	{
		DirectX::Image image = {};
		image.width = desc.Width;
		image.height = desc.Height;
		image.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		image.rowPitch = mapped.RowPitch;
		image.slicePitch = (size_t)mapped.RowPitch * desc.Height;
		image.pixels = (uint8_t*)mapped.pData;

		std::wstring wide(file.begin(), file.end());

		// Значення вже закодовані для показу, тож файл записується без перетворення кольору
		DirectX::SaveToWICFile(image, DirectX::WIC_FLAGS_NONE, DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), wide.c_str());

		raw->Unmap(staging, 0);
	}

	staging->Release();
}
