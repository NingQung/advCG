#ifndef OBJ_LOADER_H
#define OBJ_LOADER_H

#include <string>

#include "rtweekend.h"
#include "hittable.h"
#include "material.h"

struct obj_load_options {
    double scale = 1.0;
    vec3 offset = vec3(0, 0, 0);

    // If empty, the loader uses the OBJ file's directory as the MTL search path.
    std::string material_search_path = "";

    // Use Kd / Ke from .mtl when available.
    bool use_mtl_materials = true;

    // Wrap the triangle list in a BVH.
    bool use_bvh = true;

    // Used when the OBJ has no material or material loading fails.
    shared_ptr<material> default_material = nullptr;
};

struct obj_load_result {
    shared_ptr<hittable> object = nullptr;
    int triangle_count = 0;
    std::string warning = "";
    std::string error = "";

    bool ok() const {
        return object != nullptr && triangle_count > 0 && error.empty();
    }
};

obj_load_result load_obj_model(
    const std::string& filename,
    const obj_load_options& options = obj_load_options()
);

#endif