# Root Cause Analysis: Floor Surface Not Textured

**Date:** 2026-02-22
**Severity:** Feature gap
**Status:** Analysis complete -- changes required across 2 files

---

## Problem Statement

The floor surface (`marble_floor`) on which balls roll is rendered as a flat metallic color instead of displaying a texture. The user wants it textured using an image file (`~/Downloads/neon.jpg`) applied as a procedural-style equirectangular texture via triplanar mapping.

**Scope:** Scene definition (`nwave_bowling.yaml`), material system, and shader texture sampling for non-sphere shapes.

---

## WHY 1: Why is the floor not textured?

**Symptom:** The floor renders as a uniform metallic color (RGB 0.75, 0.72, 0.68) with fuzz 0.02.

**Evidence:** In `scenes/nwave_bowling.yaml` lines 68-72:
```yaml
  - name: floor_marble
    type: metal
    albedo: [0.75, 0.72, 0.68]
    fuzz: 0.02
```
And the floor object at line 100:
```yaml
  - { name: marble_floor, type: box, min: [-15.0, -0.05, -10.0], max: [10.0, 0.0, 10.0], material: floor_marble, physics: { body_type: static } }
```

**Finding:** The `floor_marble` material is type `metal` with a plain albedo. It has no `texture` field, no `texture_scale`, and no `texture_mapping`. The material system treats it as a simple flat-color metal.

---

## WHY 2: Why does the material have no texture?

**Evidence:** The YAML loader (`src/infrastructure/yaml_scene_loader.cpp` lines 68-72) shows that `type: metal` creates a `Metal` object with only `albedo` and `fuzz`:
```cpp
std::shared_ptr<Material> create_metal(const YAML::Node& node) {
    auto albedo = parse_vec3(node["albedo"]);
    double fuzz = node["fuzz"] ? node["fuzz"].as<double>() : 0.0;
    return std::make_shared<Metal>(albedo, fuzz);
}
```

The `Metal` class (`src/domain/materials/metal.h`) stores only albedo and fuzziness -- it has no texture support. Texturing is only supported through:
- `ImageTexture` (type: `image_texture`) -- loads a JPEG/PNG file
- `ProceduralTexture` (type: `procedural_texture`) -- generates an equirectangular texture from a pattern name

**Finding:** The `metal` material type is fundamentally incapable of carrying texture data. The floor was defined as metal for its reflective properties, but this material type has no texture pathway.

---

## WHY 3: What material types support textures, and what is the full texture pipeline?

### Branch A: Image Texture Pipeline (fully functional)

**Evidence:** `yaml_scene_loader.cpp` lines 76-89:
```cpp
} else if (type == "image_texture") {
    std::string path = mat_node["texture"].as<std::string>();
    material = ImageTexture::load_from_file(path);
    if (mat_node["texture_scale"]) {
        auto* img = dynamic_cast<ImageTexture*>(material.get());
        if (img) img->set_texture_scale(mat_node["texture_scale"].as<float>());
    }
    if (mat_node["texture_mapping"]) {
        std::string mapping = mat_node["texture_mapping"].as<std::string>();
        if (mapping == "cube_map") {
            auto* img = dynamic_cast<ImageTexture*>(material.get());
            if (img) img->set_texture_scale(-1.0f);
        }
    }
}
```

`ImageTexture::load_from_file()` (`image_texture.cpp` line 45-56) uses stb_image to load JPEG/PNG into RGBA pixel buffer. It stores: `pixels_` (RGBA bytes), `width_`, `height_`, `texture_scale_`.

The scene flattener (`scene_flattener.cpp` lines 65-76) recognizes `ImageTexture` via `dynamic_cast` and copies its pixel data into the GPU texture buffer:
```cpp
if (auto* img = dynamic_cast<const ImageTexture*>(mat)) {
    gpu_mat.material_type = static_cast<uint32_t>(GPUMaterialType::LAMBERTIAN);
    gpu_mat.albedo[0] = 1.0f; gpu_mat.albedo[1] = 1.0f; gpu_mat.albedo[2] = 1.0f;
    gpu_mat.texture_offset = static_cast<int32_t>(texture_data.size());
    gpu_mat.texture_width = static_cast<float>(img->width());
    gpu_mat.texture_height = static_cast<float>(img->height());
    gpu_mat.texture_scale = img->texture_scale();
    texture_data.insert(texture_data.end(), img->pixels().begin(), img->pixels().end());
}
```

**Critical observation:** `ImageTexture` is flattened as `GPUMaterialType::LAMBERTIAN` on the GPU. This means image-textured objects will scatter light diffusely (Lambertian), not reflectively (Metal). This is a design constraint of the current material system -- there is no "textured metal" GPU material type.

### Branch B: Procedural Texture Pipeline (fully functional)

**Evidence:** `ProceduralTexture` extends `ImageTexture` (`procedural_texture.h` line 35):
```cpp
class ProceduralTexture : public ImageTexture { ... };
```

It generates equirectangular pixel data at construction time, then behaves identically to `ImageTexture` through the flattener and shader. The `generate()` function leaves `texture_scale` at `0.0f` (default).

### Branch C: GPU Shader Texture Sampling for Non-Sphere Shapes

**Evidence:** `ray_trace.metal` lines 975-1023 show the texture sampling logic in the shader. For non-sphere shapes (the floor is a BOX), the final `else` branch at line 1018 is used:
```metal
} else {
    // Non-sphere shapes: triplanar mapping
    albedo = sample_texture_triplanar(texture_data, mat.texture_offset,
                                      mat.texture_width, mat.texture_height,
                                      rec.point, rec.normal, 2.0f);
}
```

**Finding:** The shader already supports texture sampling on boxes via triplanar mapping. If a BOX shape references a material with `texture_offset >= 0`, the shader will apply the texture using triplanar projection with a tile scale of 2.0.

---

## WHY 4: Why can't the floor simply use an `image_texture` material referencing `neon.jpg`?

**Finding:** It CAN. All the necessary infrastructure exists.

**Evidence chain:**

1. **File exists and is valid:** `~/Downloads/neon.jpg` is a 1024x512 JPEG (confirmed via `file` command). The 2:1 aspect ratio makes it an equirectangular map, matching the project's procedural texture conventions.

2. **YAML loader supports `image_texture`:** The parser at lines 76-89 handles `type: image_texture` with a `texture` field pointing to a file path.

3. **Scene flattener handles `ImageTexture`:** The `resolve_material_index` function at line 65 detects `ImageTexture` via dynamic_cast and copies pixel data to the GPU buffer with proper offset/dimensions.

4. **Shader handles textured boxes:** The triplanar sampling path at line 1018 applies to any non-sphere shape (including BOX) when `texture_offset >= 0`.

**However, there is one design limitation:**

The scene flattener maps `ImageTexture` to `GPUMaterialType::LAMBERTIAN`. The current floor material is `metal` (reflective). Changing to `image_texture` will change the floor's lighting behavior from metallic reflection to Lambertian diffuse scattering. There is no "textured metal" material type in the GPU pipeline.

This is acceptable for the user's request since they want a visible neon texture on the floor -- diffuse scattering will display the texture clearly, whereas metallic reflection would blend the texture with environment reflections.

---

## WHY 5: What is the root cause and what is the complete chain of changes?

### Root Cause

The floor is not textured because it was defined with `type: metal` (a flat-color material type) when it should use `type: image_texture` to reference the neon.jpg image file. All infrastructure to support image textures on box geometry already exists in the codebase.

### Required Changes (2 files)

**Change 1: `scenes/nwave_bowling.yaml` -- Replace the `floor_marble` material definition**

Current (lines 68-72):
```yaml
  - name: floor_marble
    type: metal
    albedo: [0.75, 0.72, 0.68]
    fuzz: 0.02
```

Required:
```yaml
  - name: floor_marble
    type: image_texture
    texture: "/Users/andrealaforgia/Downloads/neon.jpg"
```

No changes are needed to the floor object definition at line 100. It already references `floor_marble` by name.

**Change 2: None required in code**

The full pipeline is already implemented:
- `YamlSceneLoader::parse_materials()` handles `type: image_texture` with `texture:` path
- `ImageTexture::load_from_file()` loads JPEG via stb_image into RGBA pixels
- `SceneFlattener::resolve_material_index()` detects `ImageTexture`, copies pixels to GPU buffer
- `ray_trace.metal` samples the texture via triplanar mapping for BOX shapes

### Optional Enhancements

1. **Tile scale control:** The triplanar shader uses a hardcoded tile scale of `2.0f` (line 1022 of `ray_trace.metal`). This means the neon texture will tile every 0.5 world units. For the 25x20 unit floor, this produces ~50x40 tiles. If the tiling density is undesirable, the `texture_scale` field could be repurposed to control tile density, but this would require a shader change.

2. **Texture mapping mode:** Adding `texture_mapping: cube_map` in YAML would set `texture_scale = -1.0f`, which would route through the cube_map path for spheres. For boxes, the shader ignores this flag and always uses triplanar mapping. No change needed.

---

## Validation: Forward Chain

If the `floor_marble` material is changed from `type: metal` to `type: image_texture` with `texture: "/Users/andrealaforgia/Downloads/neon.jpg"`:

1. YAML loader parses it as `image_texture` -> calls `ImageTexture::load_from_file("/Users/andrealaforgia/Downloads/neon.jpg")` -> loads 1024x512 JPEG into RGBA pixel buffer (1024 * 512 * 4 = 2,097,152 bytes).
2. Scene flattener detects `ImageTexture` via dynamic_cast -> sets `gpu_mat.material_type = LAMBERTIAN`, `gpu_mat.texture_offset` to current buffer position, copies 2MB of pixel data into texture buffer.
3. The floor BOX shape references this material index.
4. During ray tracing, when a ray hits the floor box, the shader fetches the material, finds `texture_offset >= 0`, falls through to the triplanar mapping branch (since `shape_type == SHAPE_BOX`), and samples the neon texture using world-space coordinates with mirror-repeat tiling.
5. The floor renders with the neon.jpg texture applied via triplanar projection.

**Validated:** The chain is complete and each step has existing, tested code.

---

## Summary

| Level | Finding | Evidence |
|-------|---------|----------|
| WHY 1 | Floor uses flat-color metal material | YAML: `type: metal, albedo: [0.75, 0.72, 0.68]` |
| WHY 2 | `Metal` class has no texture support | `metal.h`: only stores albedo + fuzziness |
| WHY 3 | `ImageTexture` and `ProceduralTexture` types carry texture data through the full GPU pipeline | `scene_flattener.cpp` line 65, `ray_trace.metal` line 1018 |
| WHY 4 | All infrastructure for textured boxes exists; only the YAML material definition needs changing | Triplanar sampling in shader handles BOX shapes with textures |
| WHY 5 | Root cause: YAML material definition uses wrong type | Single YAML change needed: `type: image_texture` with `texture:` path |

### Immediate Fix

Change `floor_marble` material in `scenes/nwave_bowling.yaml` from `type: metal` to `type: image_texture` with `texture: "/Users/andrealaforgia/Downloads/neon.jpg"`.

### Behavioral Change to Note

The floor will change from metallic (specular reflection) to Lambertian (diffuse scattering). The neon texture will be clearly visible but the floor will lose its mirror-like reflectivity. This is an inherent limitation of the current GPU material system, which does not support textured metals.
