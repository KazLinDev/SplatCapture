# SplatCapture — Installation Guide

A Unreal Engine 5 plugin that automatically generates synthetic training datasets for 3D Gaussian Splatting.

---

## Requirements

- Unreal Engine 5.3 currently supported

---

## Installation

**1. Download the plugin**

Clone or download this repository and unzip if needed.

**2. Copy into your project**

Copy the `SplatCapture` folder into your project's `Plugins` folder.
If the `Plugins` folder doesn't exist, create it next to your `.uproject` file.

```
YourProject/
├── Content/
├── Plugins/
│   └── SplatCapture/       ← place it here
│       ├── Content/
│       ├── Resources/
│       ├── Source/
│       └── SplatCapture.uplugin
├── YourProject.uproject
```

**3. Regenerate project files**

Right-click your `.uproject` file → **Generate Visual Studio project files**.

**4. Enable the plugin**

Open your project in Unreal Engine, then go to:

`Edit → Plugins → search "SplatCapture" → Enable → Restart Editor`

---

## Usage

1. Place the **SplatCapture Volume Box** actor into your scene and scale it to cover the area you want to capture
2. Open the plugin UI from the **toolbar** next to the Play button
3. Configure grid spacing, jitter, collision radius, and render quality
4. Click **Render** — the plugin will place cameras, check collisions, and render all frames automatically
5. Import the exported folder into **Postshot** and train your Gaussian Splat

### Exported files

| File | Description |
|---|---|
| `cameras.txt` | Camera intrinsics (focal length, resolution) |
| `images.txt` | Camera extrinsics (position + rotation per frame) |
| `points3D.ply` | Point cloud (binary) |
| `points3D.txt` | Point cloud (plain text for Postshot) |
| `frame.####.png` | Rendered images |

---

## Important Notes

- All meshes in the capture volume must have **collision enabled** for the occlusion checks to work correctly
- The plugin module type is `UncookedOnly` — it is an editor-only tool and will not be included in packaged builds

---

## License

Copyright (c) 2026 Linus Käpplinger. All rights reserved.
