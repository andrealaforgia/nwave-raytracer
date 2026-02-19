#include "infrastructure/gpu/scene_flattener.h"
#include "domain/scene.h"
#include "domain/shapes/sphere.h"
#include "domain/shapes/plane.h"
#include "domain/shapes/box.h"
#include "domain/shapes/cylinder.h"
#include "domain/shapes/triangle.h"
#include "domain/shapes/transformed_shape.h"
#include "domain/shapes/triangle_mesh.h"
#include "domain/materials/lambertian.h"
#include "domain/materials/metal.h"
#include "domain/materials/dielectric.h"
#include "domain/materials/emissive.h"
#include "domain/lights/point_light.h"
#include "domain/lights/directional_light.h"
#include <unordered_map>
#include <iostream>
#include <cstring>

namespace nwave {

namespace {

uint32_t ensure_default_material(
    std::unordered_map<const Material*, uint32_t>& material_map,
    std::vector<GPUMaterial>& materials)
{
    auto it = material_map.find(nullptr);
    if (it != material_map.end()) {
        return it->second;
    }
    GPUMaterial default_mat{};
    default_mat.material_type = static_cast<uint32_t>(GPUMaterialType::LAMBERTIAN);
    default_mat.albedo[0] = 0.5f;
    default_mat.albedo[1] = 0.5f;
    default_mat.albedo[2] = 0.5f;
    uint32_t index = static_cast<uint32_t>(materials.size());
    materials.push_back(default_mat);
    material_map[nullptr] = index;
    return index;
}

uint32_t resolve_material_index(
    const Material* mat,
    std::unordered_map<const Material*, uint32_t>& material_map,
    std::vector<GPUMaterial>& materials)
{
    if (!mat) {
        std::cerr << "SceneFlattener: shape has null material, using default\n";
        return ensure_default_material(material_map, materials);
    }

    auto it = material_map.find(mat);
    if (it != material_map.end()) {
        return it->second;
    }

    GPUMaterial gpu_mat{};
    if (auto* lamb = dynamic_cast<const Lambertian*>(mat)) {
        gpu_mat.material_type = static_cast<uint32_t>(GPUMaterialType::LAMBERTIAN);
        gpu_mat.albedo[0] = static_cast<float>(lamb->albedo().r());
        gpu_mat.albedo[1] = static_cast<float>(lamb->albedo().g());
        gpu_mat.albedo[2] = static_cast<float>(lamb->albedo().b());
    } else if (auto* metal = dynamic_cast<const Metal*>(mat)) {
        gpu_mat.material_type = static_cast<uint32_t>(GPUMaterialType::METAL);
        gpu_mat.albedo[0] = static_cast<float>(metal->albedo().r());
        gpu_mat.albedo[1] = static_cast<float>(metal->albedo().g());
        gpu_mat.albedo[2] = static_cast<float>(metal->albedo().b());
        gpu_mat.param1 = static_cast<float>(metal->fuzziness());
    } else if (auto* diel = dynamic_cast<const Dielectric*>(mat)) {
        gpu_mat.material_type = static_cast<uint32_t>(GPUMaterialType::DIELECTRIC);
        gpu_mat.param1 = static_cast<float>(diel->ior());
        gpu_mat.tint[0] = static_cast<float>(diel->tint().r());
        gpu_mat.tint[1] = static_cast<float>(diel->tint().g());
        gpu_mat.tint[2] = static_cast<float>(diel->tint().b());
    } else if (auto* emis = dynamic_cast<const Emissive*>(mat)) {
        gpu_mat.material_type = static_cast<uint32_t>(GPUMaterialType::EMISSIVE);
        gpu_mat.albedo[0] = static_cast<float>(emis->color().r());
        gpu_mat.albedo[1] = static_cast<float>(emis->color().g());
        gpu_mat.albedo[2] = static_cast<float>(emis->color().b());
        gpu_mat.param1 = static_cast<float>(emis->intensity());
    }

    uint32_t index = static_cast<uint32_t>(materials.size());
    materials.push_back(gpu_mat);
    material_map[mat] = index;
    return index;
}

bool flatten_shape(
    const Shape* shape,
    GPUShape& gpu_shape,
    std::unordered_map<const Material*, uint32_t>& material_map,
    std::vector<GPUMaterial>& materials)
{
    std::memset(&gpu_shape, 0, sizeof(GPUShape));

    if (auto* sphere = dynamic_cast<const Sphere*>(shape)) {
        gpu_shape.shape_type = static_cast<uint32_t>(GPUShapeType::SPHERE);
        gpu_shape.params[0] = static_cast<float>(sphere->center().x());
        gpu_shape.params[1] = static_cast<float>(sphere->center().y());
        gpu_shape.params[2] = static_cast<float>(sphere->center().z());
        gpu_shape.params[3] = static_cast<float>(sphere->radius());
        gpu_shape.material_index = resolve_material_index(sphere->material(), material_map, materials);
    } else if (auto* plane = dynamic_cast<const Plane*>(shape)) {
        gpu_shape.shape_type = static_cast<uint32_t>(GPUShapeType::PLANE);
        gpu_shape.params[0] = static_cast<float>(plane->point().x());
        gpu_shape.params[1] = static_cast<float>(plane->point().y());
        gpu_shape.params[2] = static_cast<float>(plane->point().z());
        gpu_shape.params[4] = static_cast<float>(plane->normal().x());
        gpu_shape.params[5] = static_cast<float>(plane->normal().y());
        gpu_shape.params[6] = static_cast<float>(plane->normal().z());
        gpu_shape.material_index = resolve_material_index(plane->material(), material_map, materials);
    } else if (auto* box = dynamic_cast<const Box*>(shape)) {
        gpu_shape.shape_type = static_cast<uint32_t>(GPUShapeType::BOX);
        gpu_shape.params[0] = static_cast<float>(box->box_min().x());
        gpu_shape.params[1] = static_cast<float>(box->box_min().y());
        gpu_shape.params[2] = static_cast<float>(box->box_min().z());
        gpu_shape.params[4] = static_cast<float>(box->box_max().x());
        gpu_shape.params[5] = static_cast<float>(box->box_max().y());
        gpu_shape.params[6] = static_cast<float>(box->box_max().z());
        gpu_shape.material_index = resolve_material_index(box->material(), material_map, materials);
    } else if (auto* cyl = dynamic_cast<const Cylinder*>(shape)) {
        gpu_shape.shape_type = static_cast<uint32_t>(GPUShapeType::CYLINDER);
        gpu_shape.params[0] = static_cast<float>(cyl->center().x());
        gpu_shape.params[1] = static_cast<float>(cyl->center().y());
        gpu_shape.params[2] = static_cast<float>(cyl->center().z());
        gpu_shape.params[3] = static_cast<float>(cyl->radius());
        gpu_shape.params[4] = 0.0f;  // axis x
        gpu_shape.params[5] = 1.0f;  // axis y (Y-axis aligned)
        gpu_shape.params[6] = 0.0f;  // axis z
        gpu_shape.params[7] = static_cast<float>(cyl->height() / 2.0);  // half_height
        gpu_shape.material_index = resolve_material_index(cyl->material(), material_map, materials);
    } else if (auto* tri = dynamic_cast<const Triangle*>(shape)) {
        gpu_shape.shape_type = static_cast<uint32_t>(GPUShapeType::TRIANGLE);
        gpu_shape.params[0] = static_cast<float>(tri->v0().x());
        gpu_shape.params[1] = static_cast<float>(tri->v0().y());
        gpu_shape.params[2] = static_cast<float>(tri->v0().z());
        gpu_shape.params[4] = static_cast<float>(tri->v1().x());
        gpu_shape.params[5] = static_cast<float>(tri->v1().y());
        gpu_shape.params[6] = static_cast<float>(tri->v1().z());
        gpu_shape.params[8] = static_cast<float>(tri->v2().x());
        gpu_shape.params[9] = static_cast<float>(tri->v2().y());
        gpu_shape.params[10] = static_cast<float>(tri->v2().z());
        gpu_shape.material_index = resolve_material_index(tri->material(), material_map, materials);
    } else if (dynamic_cast<const TriangleMesh*>(shape)) {
        std::cerr << "SceneFlattener: TriangleMesh skipped (not yet supported for GPU)\n";
        return false;
    } else {
        std::cerr << "SceneFlattener: unknown shape type skipped\n";
        return false;
    }

    return true;
}

} // anonymous namespace

FlatScene SceneFlattener::flatten(const Scene& scene) const {
    FlatScene result;
    std::unordered_map<const Material*, uint32_t> material_map;

    for (const auto& shape_ptr : scene.shapes()) {
        const Shape* shape = shape_ptr.get();

        // Handle TransformedShape: unwrap to inner shape, record transform
        const TransformedShape* transformed = dynamic_cast<const TransformedShape*>(shape);

        GPUShape gpu_shape;
        const Shape* inner_shape = transformed ? transformed->inner().get() : shape;

        if (!flatten_shape(inner_shape, gpu_shape, material_map, result.materials)) {
            continue;
        }

        if (transformed) {
            gpu_shape.has_transform = 1;
            const Matrix4x4& inv = transformed->inverse_matrix();
            // Store in column-major order to match Metal's float4x4 layout
            for (int r = 0; r < 4; ++r) {
                for (int c = 0; c < 4; ++c) {
                    gpu_shape.inverse_transform[c * 4 + r] = static_cast<float>(inv.m[r][c]);
                }
            }
        }

        result.shapes.push_back(gpu_shape);
    }

    for (const auto& light_ptr : scene.lights()) {
        GPULight gpu_light{};

        if (auto* point = dynamic_cast<const PointLight*>(light_ptr.get())) {
            gpu_light.light_type = static_cast<uint32_t>(GPULightType::POINT);
            gpu_light.position[0] = static_cast<float>(point->position().x());
            gpu_light.position[1] = static_cast<float>(point->position().y());
            gpu_light.position[2] = static_cast<float>(point->position().z());
            gpu_light.color[0] = static_cast<float>(point->color().r());
            gpu_light.color[1] = static_cast<float>(point->color().g());
            gpu_light.color[2] = static_cast<float>(point->color().b());
            gpu_light.intensity = static_cast<float>(point->intensity());
        } else if (auto* dir = dynamic_cast<const DirectionalLight*>(light_ptr.get())) {
            gpu_light.light_type = static_cast<uint32_t>(GPULightType::DIRECTIONAL);
            gpu_light.position[0] = static_cast<float>(dir->direction().x());
            gpu_light.position[1] = static_cast<float>(dir->direction().y());
            gpu_light.position[2] = static_cast<float>(dir->direction().z());
            gpu_light.color[0] = static_cast<float>(dir->color().r());
            gpu_light.color[1] = static_cast<float>(dir->color().g());
            gpu_light.color[2] = static_cast<float>(dir->color().b());
            gpu_light.intensity = static_cast<float>(dir->intensity());
        } else {
            std::cerr << "SceneFlattener: unknown light type skipped\n";
            continue;
        }

        result.lights.push_back(gpu_light);
    }

    return result;
}

} // namespace nwave
