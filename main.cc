#include <chrono>
#include <iomanip>
#include "rtweekend.h"

#include "camera.h"
#include "hittable_list.h"
#include "material.h"
#include "quad.h"
#include "sphere.h"
#include "obj_loader.h"
#include "obj_loader.cpp"
#include "triangle.h"
#include "external/rgb2spec.h"
RGB2Spec *g_rgb2spec_model = nullptr;

int main(int argc, char** argv) {
    g_rgb2spec_model = rgb2spec_load("external/jakob-and-hanika-2019-srgb.coeff");
    if (!g_rgb2spec_model) {
        std::cerr << "Failed to load rgb2spec model!\n";
        return -1;
    }
    hittable_list world;

    auto red   = make_shared<lambertian>(color(.65, .05, .05));
    auto white = make_shared<lambertian>(color(.73, .73, .73));
    auto dark = make_shared<lambertian>(color(.03, .03, .03));
    auto blue = make_shared<lambertian>(color(.05, .05, .65));
    auto yellow = make_shared<lambertian>(color(.70, .70, .05));
    auto green = make_shared<lambertian>(color(.12, .45, .15));
    auto light = make_shared<diffuse_light>(color(60.0, 60.0, 60.0));
    shared_ptr<material> aluminum = make_shared<metal>(color(0.8, 0.85, 0.88), 0.0);
    auto glass = make_shared<dielectric>(1.7, 0.015);
    auto checker_tex = make_shared<checker_texture>(40.0,color(.85, .85, .85),color(.10, .10, .10));
    auto checker_mat = make_shared<lambertian>(checker_tex);
    auto wave_tex = make_shared<wave_texture>(25.0,color(.85, .85, .85),color(.10, .10, .10));
    auto wave_mat = make_shared<lambertian>(wave_tex);

    hittable_list lights;
    camera cam;

    std::string obj_path = "";
    double obj_scale = 100.0;
    vec3 obj_offset = vec3(278, 0, 278);

    if (argc >= 2) {
        obj_path = argv[1];
    }

    if (argc >= 3) {
        obj_scale = std::atof(argv[2]);
    }

    if (argc >= 6) {
        obj_offset = vec3(
            std::atof(argv[3]),
            std::atof(argv[4]),
            std::atof(argv[5])
        );
    }

    switch (4) {
    case 1: { // Cornell box + glass ball
      // Cornell box sides
      auto glass = make_shared<dielectric>(1.7, 0.15);
      auto glass2 = make_shared<dielectric>(1.5, 0.15);
      world.add(make_shared<quad>(point3(555,0,0), vec3(0,0,555), vec3(0,555,0), white)); //left
      world.add(make_shared<quad>(point3(0,0,555), vec3(0,0,-555), vec3(0,555,0), white)); //right
      world.add(make_shared<quad>(point3(0,555,0), vec3(555,0,0), vec3(0,0,555), white)); //up
      world.add(make_shared<quad>(point3(0,0,555), vec3(555,0,0), vec3(0,0,-555), white)); //buttom
      world.add(make_shared<quad>(point3(555,0,555), vec3(-555,0,0), vec3(0,555,0), wave_mat)); //back

      // Big Light
      auto light = make_shared<diffuse_light>(color(10.0, 10.0, 10.0));
      world.add(make_shared<quad>(point3(148,554,174), vec3(260,0,0), vec3(0,0,210), light));
      lights.add(make_shared<quad>(point3(343,554,332), vec3(-130,0,0), vec3(0,0,-105), light));

      // Box 1
      shared_ptr<hittable> box1 = box(point3(0,0,0), point3(165,330,165), white);
      box1 = make_shared<rotate_y>(box1, 15);
      box1 = make_shared<translate>(box1, vec3(265,0,295));
      world.add(box1);

      // Glass Sphere
      world.add(make_shared<sphere>(point3(190,130,190), 100, glass2));
      // world.add(make_shared<sphere>(point3(265,60,295), 50, glass));

      // Box 2
      // shared_ptr<hittable> box2 = box(point3(0,0,0), point3(165,165,165), glass);
      // box2 = make_shared<rotate_y>(box2, -18);
      // box2 = make_shared<translate>(box2, vec3(130,10,65));
      // world.add(box2);

      cam.vfov     = 40;
      cam.lookfrom = point3(278, 278, -800);
      cam.lookat   = point3(278, 278, 0);
      cam.vup      = vec3(0, 1, 0);
      break;
    }
    case 2: { // Cornell box + glass prism to look back dispersion
      auto glass = make_shared<dielectric>(1.7, 0.015);
      // Cornell box sides
      world.add(make_shared<quad>(point3(555,0,0), vec3(0,0,555), vec3(0,555,0), white)); //left
      world.add(make_shared<quad>(point3(0,0,555), vec3(0,0,-555), vec3(0,555,0), white)); //right
      world.add(make_shared<quad>(point3(0,555,0), vec3(555,0,0), vec3(0,0,555), white)); //up
      world.add(make_shared<quad>(point3(0,0,555), vec3(555,0,0), vec3(0,0,-555), white)); //buttom
      world.add(make_shared<quad>(point3(555,0,555), vec3(-555,0,0), vec3(0,555,0), wave_mat)); //back

      // Light
      auto light = make_shared<diffuse_light>(color(10.0, 10.0, 10.0));
      world.add(make_shared<quad>(point3(148,554,174), vec3(260,0,0), vec3(0,0,210), light));
      lights.add(make_shared<quad>(point3(343,554,332), vec3(-130,0,0), vec3(0,0,-105), light));

      // prism
      shared_ptr<hittable> prism1 = prism(point3(0,0,0), point3(-150,0,180), point3(150,0,180), 400.0, glass);
      prism1 = make_shared<translate>(prism1, vec3(275,10,75));
      world.add(prism1);

      cam.vfov     = 40;
      cam.lookfrom = point3(278, 278, -800);
      cam.lookat   = point3(278, 278, 0);
      cam.vup      = vec3(0, 1, 0);
      break;
    }
    case 3: { // dispersion prism to ground
      auto glass2 = make_shared<dielectric>(1.4, 0.5);
      world.add(make_shared<quad>(point3(555,0,0), vec3(0,0,555), vec3(0,555,0), green));
      world.add(make_shared<quad>(point3(0,0,555), vec3(0,0,-555), vec3(0,555,0), red));
      world.add(make_shared<quad>(point3(0,555,0), vec3(555,0,0), vec3(0,0,555), white)); //up
      world.add(make_shared<quad>(point3(0,0,555), vec3(555,0,0), vec3(0,0,-555), white)); //buttom
      world.add(make_shared<quad>(point3(555,0,555), vec3(-555,0,0), vec3(0,555,0), white)); //back
      // world.add(make_shared<quad>(point3(0,300,270), vec3(555,0,0), vec3(0,0,-275), white)); //front-blocker
      // world.add(make_shared<quad>(point3(0,300,560), vec3(555,0,0), vec3(0,0,-283), white)); //back-blocker

      auto light = make_shared<diffuse_light>(color(60.0, 60.0, 60.0));
      world.add(make_shared<quad>(point3(0,554,280), vec3(0,0,-15), vec3(555,0,0), light));
      lights.add(make_shared<quad>(point3(0,554,280), vec3(0,0,-15), vec3(555,0,0), light));

      shared_ptr<hittable> prism1 = prism(point3(0,0,0), point3(0,65,100), point3(0,-65,100), 400.0, glass2);
      prism1 = make_shared<rotate_x>(prism1, -60);
      prism1 = make_shared<translate>(prism1, vec3(75,25,250));
      world.add(prism1);

      cam.vfov     = 40;
      cam.lookfrom = point3(278, 278, -800);
      cam.lookat   = point3(278, 278, 0);
      cam.vup      = vec3(0, 1, 0);
      break;
    }
    case 4: { // input OBJ
      world.add(make_shared<quad>(point3(555,0,0), vec3(0,0,555), vec3(0,555,0), white)); //left
      world.add(make_shared<quad>(point3(0,0,555), vec3(0,0,-555), vec3(0,555,0), white)); //right
      world.add(make_shared<quad>(point3(0,555,0), vec3(555,0,0), vec3(0,0,555), white)); //up
      world.add(make_shared<quad>(point3(0,0,555), vec3(555,0,0), vec3(0,0,-555), white)); //buttom
      world.add(make_shared<quad>(point3(555,0,555), vec3(-555,0,0), vec3(0,555,0), wave_mat)); //back

      // Small Light 
      auto light = make_shared<diffuse_light>(color(1000.0, 1000.0, 1000.0));
      world.add(make_shared<quad>(point3(265, 554, 268.5), vec3(26, 0, 0), vec3(0, 0, 21), light));
      lights.add(make_shared<quad>(point3(265, 554, 268.5), vec3(26, 0, 0), vec3(0, 0, 21), light));

      // input OBJ
      // obj_options.default_material = make_shared<dielectric>(1.5, 0.15);

      cam.vfov     = 40;
      cam.lookfrom = point3(278, 555, -400);
      cam.lookat   = point3(278, 278, 0);
      cam.vup      = vec3(0, 1, 0);
      break;
    }
    case 5: { // input final scene
      world.add(make_shared<quad>(point3(-555,0,2555), vec3(1110,0,0), vec3(0,0,-3110), white)); //buttom

      auto light = make_shared<diffuse_light>(color(50.0, 50.0, 50.0));
      // world.add(make_shared<quad>(point3(300,600,0), vec3(26, 0, 0), vec3(0, 0, 26), light));
      // lights.add(make_shared<quad>(point3(300,600,0), vec3(26, 0, 0), vec3(0, 0, 26), light));
      // emitters.add_quad(point3(300,600,0), vec3(26, 0, 0), vec3(0, 0, 26), color(2000.0, 2000.0, 2000.0));
      world.add(make_shared<quad>(point3(500,500,200), vec3(100, -120, 0), vec3(-50, 0, 50), light));
      lights.add(make_shared<quad>(point3(500,500,200), vec3(100, -120, 0), vec3(-50, 0, 50), light));
      // input OBJ
      // obj_options.default_material = make_shared<dielectric>(1.5, 0.15);

      cam.vfov     = 20;
      cam.lookfrom = point3(0, 800, -1200);
      cam.lookat   = point3(0, 0, 0);
      cam.vup      = vec3(0, 1, 0);
      break;
    }
    default:
      break;
    }

    if (!obj_path.empty()) {
        obj_load_options obj_options;
        obj_options.scale = obj_scale;
        obj_options.offset = obj_offset;
        obj_options.use_mtl_materials = true;
        obj_options.default_material = white;
        obj_options.use_bvh = true;
        obj_options.use_vertex_normals = true;

        obj_load_result obj_result = load_obj_model(obj_path, obj_options);

        if (!obj_result.error.empty()) {
            std::cerr << "Failed to load OBJ: " << obj_result.error << "\n";

            if (!obj_result.warning.empty())
                std::cerr << "OBJ warning:\n" << obj_result.warning << "\n";

            rgb2spec_free(g_rgb2spec_model);
            return -1;
        }

        world.add(obj_result.object);

        std::clog << "OBJ added to world.\n";
    }

    cam.aspect_ratio      = 1.0;
    cam.image_width       = 600;
    cam.samples_per_pixel = 5000;
    cam.max_depth         = 50;
    cam.background        = color(0,0,0);

    cam.defocus_angle = 0;

    auto start = std::chrono::high_resolution_clock::now();

    cam.render(world, lights);

    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = end - start;

    std::clog << "\nRender time: "
              << std::fixed << std::setprecision(3)
              << elapsed.count()
              << " seconds\n";

    rgb2spec_free(g_rgb2spec_model);
    return 0;
}