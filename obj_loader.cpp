#define TINYOBJLOADER_IMPLEMENTATION
#include "external/tiny_obj_loader.h"

#include "obj_loader.h"

#include <cmath>
#include <iostream>
#include <vector>

#include "bvh.h"
#include "hittable_list.h"
#include "triangle.h"
#include "texture.h"

namespace {

std::string dirname_of(const std::string& filename) {
    auto pos = filename.find_last_of("/\\");
    if (pos == std::string::npos)
        return "";

    return filename.substr(0, pos + 1);
}

std::string join_path(const std::string& base, const std::string& name) {
    if (name.empty())
        return name;

    if (name[0] == '/' || name[0] == '\\')
        return name;

    if (base.empty())
        return name;

    char last = base[base.size() - 1];

    if (last == '/' || last == '\\')
        return base + name;

    return base + "/" + name;
}

double color_length_squared(double r, double g, double b) {
    return r * r + g * g + b * b;
}

point3 read_vertex(
    const tinyobj::attrib_t& attrib,
    int vertex_index,
    const obj_load_options& options
) {
    const int base = 3 * vertex_index;

    double x = static_cast<double>(attrib.vertices[base + 0]);
    double y = static_cast<double>(attrib.vertices[base + 1]);
    double z = static_cast<double>(attrib.vertices[base + 2]);

    return point3(
        options.scale * x + options.offset.x(),
        options.scale * y + options.offset.y(),
        options.scale * z + options.offset.z()
    );
}

shared_ptr<material> make_material_from_tinyobj(
    const tinyobj::material_t& src,
    const std::string& texture_base_path,
    shared_ptr<material> fallback_material
) {
    double er = static_cast<double>(src.emission[0]);
    double eg = static_cast<double>(src.emission[1]);
    double eb = static_cast<double>(src.emission[2]);

    if (color_length_squared(er, eg, eb) > 1e-12) {
        return make_shared<diffuse_light>(color(er, eg, eb));
    }

    if (!src.diffuse_texname.empty()) {
        std::string texture_path = join_path(texture_base_path, src.diffuse_texname);
        return make_shared<lambertian>(make_shared<image_texture>(texture_path.c_str()));
    }

    double dr = static_cast<double>(src.diffuse[0]);
    double dg = static_cast<double>(src.diffuse[1]);
    double db = static_cast<double>(src.diffuse[2]);

    if (color_length_squared(dr, dg, db) <= 1e-12) {
        if (fallback_material)
            return fallback_material;

        return make_shared<lambertian>(color(0.73, 0.73, 0.73));
    }

    return make_shared<lambertian>(color(dr, dg, db));
}

std::vector<shared_ptr<material>> build_material_table(
    const std::vector<tinyobj::material_t>& tiny_materials,
    const std::string& texture_base_path,
    shared_ptr<material> fallback_material
) {
    std::vector<shared_ptr<material>> materials;
    materials.reserve(tiny_materials.size());

    for (const auto& tiny_mat : tiny_materials) {
        materials.push_back(
            make_material_from_tinyobj(tiny_mat, texture_base_path, fallback_material)
        );
    }

    return materials;
}

shared_ptr<material> choose_face_material(
    int material_id,
    const std::vector<shared_ptr<material>>& materials,
    shared_ptr<material> fallback_material
) {
    if (material_id >= 0 && material_id < int(materials.size()))
        return materials[material_id];

    if (fallback_material)
        return fallback_material;

    return make_shared<lambertian>(color(0.73, 0.73, 0.73));
}

bool is_degenerate_triangle(const point3& p0, const point3& p1, const point3& p2) {
    vec3 u = p1 - p0;
    vec3 v = p2 - p0;
    vec3 n = cross(u, v);

    return n.length_squared() <= 1e-20;
}

} // namespace

obj_load_result load_obj_model(
    const std::string& filename,
    const obj_load_options& options
) {
    obj_load_result result;

    tinyobj::ObjReaderConfig config;
    config.triangulate = true;

    std::string obj_dir = dirname_of(filename);

    if (options.material_search_path.empty()) {
        config.mtl_search_path = obj_dir;
    } else {
        config.mtl_search_path = options.material_search_path;
    }

    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(filename, config)) {
        result.warning = reader.Warning();
        result.error = reader.Error();

        if (result.error.empty())
            result.error = "Failed to parse OBJ file: " + filename;

        return result;
    }

    result.warning = reader.Warning();

    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();
    const auto& tiny_materials = reader.GetMaterials();

    shared_ptr<material> fallback_material = options.default_material;

    if (!fallback_material)
        fallback_material = make_shared<lambertian>(color(0.73, 0.73, 0.73));

    std::vector<shared_ptr<material>> materials;

    if (options.use_mtl_materials) {
        materials = build_material_table(
            tiny_materials,
            config.mtl_search_path,
            fallback_material
        );
    }

    auto triangles = make_shared<hittable_list>();

    for (const auto& shape : shapes) {
        size_t index_offset = 0;

        for (size_t face_index = 0;
             face_index < shape.mesh.num_face_vertices.size();
             ++face_index) {
            int face_vertex_count = int(shape.mesh.num_face_vertices[face_index]);

            if (face_vertex_count != 3) {
                index_offset += face_vertex_count;
                continue;
            }

            const tinyobj::index_t& i0 = shape.mesh.indices[index_offset + 0];
            const tinyobj::index_t& i1 = shape.mesh.indices[index_offset + 1];
            const tinyobj::index_t& i2 = shape.mesh.indices[index_offset + 2];

            index_offset += face_vertex_count;

            if (i0.vertex_index < 0 || i1.vertex_index < 0 || i2.vertex_index < 0)
                continue;

            point3 p0 = read_vertex(attrib, i0.vertex_index, options);
            point3 p1 = read_vertex(attrib, i1.vertex_index, options);
            point3 p2 = read_vertex(attrib, i2.vertex_index, options);

            if (is_degenerate_triangle(p0, p1, p2))
                continue;

            int material_id = -1;

            if (face_index < shape.mesh.material_ids.size())
                material_id = shape.mesh.material_ids[face_index];

            shared_ptr<material> mat = choose_face_material(
                material_id,
                materials,
                fallback_material
            );

            triangles->add(make_shared<triangle>(p0, p1 - p0, p2 - p0, mat));
            result.triangle_count++;
        }
    }

    if (result.triangle_count <= 0) {
        result.error = "OBJ file loaded, but no valid triangles were created: " + filename;
        return result;
    }

    if (options.use_bvh) {
        result.object = make_shared<bvh_node>(*triangles);
    } else {
        result.object = triangles;
    }

    std::clog << "Loaded OBJ: " << filename
              << ", triangles = " << result.triangle_count
              << ", materials = " << tiny_materials.size()
              << "\n";

    if (!result.warning.empty()) {
        std::clog << "OBJ loader warning:\n" << result.warning << "\n";
    }

    return result;
}