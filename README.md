# 3D Engine Prototype

A custom **3D game engine prototype** written in **C++** with **DirectX** and **NVIDIA PhysX**. The goal of this project is to explore real‑time rendering, physics, and engine architecture from the ground up.

---

## ✨ Features

- Custom rendering pipeline (DirectX 11).
- Basic scene management / entity–component system.
- Basic lighting (ambient, diffuse, specular) with a directional light component.
- Real-time shadows (directional shadow map, camera-following, PCF filtered).
- Camera system (FPS + orbital) with smooth input.
- Material system (albedo/normal/roughness/metalness).
- Input handling (keyboard & mouse).
> *Planned:* Visual scene editor and Object manager, Cascaded shadow maps, Post-processing.
---

## 📂 Project Structure

```
3D_Engine/
├── Assets/             # Game assets (models, textures, etc.)
│ ├── Meshes/
│ └── Textures/
│
├── Demo/               # Demo project built on the engine
│ ├── Components/
│ └── Scenes/
│
└── src/                # Source code of the engine
├── Engine/             # Core engine modules
│ ├── Buffer managers/
│ ├── Entity/
│ ├── Main parts/
│ ├── Physics/
│ ├── Resource managers/
│ ├── Scene/
│ └── Shader managers/
│
├── imgui/              # Dear ImGui integration
├── Main/               # Application entry point
├── Math/               # Math utilities (vectors, matrices, etc.)
└── Shaders/            # Shader source code
```

---

## 🚀 Getting Started

### Prerequisites
- **Windows 10/11 (64‑bit)**
- **Visual Studio 2016+** (MSVC toolset)
- **DirectX** (D3D11/D3D12 SDK via Windows SDK)
- **NVIDIA PhysX SDK**
- (Optional) **CMake** if you move to a generator‑based build

### Build
```bash
git clone https://github.com/dan1rock/3D_Engine.git
# Open the solution in Visual Studio and build Debug or Release
```
Run the produced executable from your `bin/` or VS output directory. Ensure the `Assets/` folder is discoverable (current working dir or configured data path).

---

## 🔧 Configuration

- **Window/Renderer:** resolution, vsync.
- **Content paths:** set search paths for `assets/` (models, textures, shaders).

---

## 📸 Demo Screenshots

<img src="https://raw.githubusercontent.com/dan1rock/3D_Engine/main/Images/1.png">
<img src="https://raw.githubusercontent.com/dan1rock/3D_Engine/main/Images/2.png">
<img src="https://raw.githubusercontent.com/dan1rock/3D_Engine/main/Images/3.png">

---

## 🎮 Demo Controls

- **WASD** — Move camera
- **Mouse** — Look around
- **Space / Ctrl** — Up / Down
- **Shift** — Fast move

---

## 📖 References

- [Direct3D documentation](https://learn.microsoft.com/windows/win32/direct3d)
- [NVIDIA PhysX](https://developer.nvidia.com/physx-sdk)

---
