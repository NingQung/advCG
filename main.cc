#include <chrono>
#include <iomanip>
#include "rtweekend.h"

#include "camera.h"
#include "hittable_list.h"
#include "material.h"
#include "quad.h"
#include "sphere.h"
#include "triangle.h"
#include "photon_map.h"
#include "external/rgb2spec.h"
RGB2Spec *g_rgb2spec_model = nullptr;

int main() {
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
    shared_ptr<material> aluminum = make_shared<metal>(color(0.8, 0.85, 0.88), 0.0);
    auto glass = make_shared<dielectric>(1.7, 0.015);
    auto checker_tex = make_shared<checker_texture>(40.0,color(.85, .85, .85),color(.10, .10, .10));
    auto checker_mat = make_shared<lambertian>(checker_tex);
    auto wave_tex = make_shared<wave_texture>(25.0,color(.85, .85, .85),color(.10, .10, .10));
    auto wave_mat = make_shared<lambertian>(wave_tex);

    hittable_list lights;
    camera cam;
    light_emitter_list emitters;

    switch (1) {
    case 1: { // Cornell box + glass ball
      // Cornell box sides
      world.add(make_shared<quad>(point3(555,0,0), vec3(0,0,555), vec3(0,555,0), green));
      world.add(make_shared<quad>(point3(0,0,555), vec3(0,0,-555), vec3(0,555,0), red));
      world.add(make_shared<quad>(point3(0,555,0), vec3(555,0,0), vec3(0,0,555), white)); //up
      world.add(make_shared<quad>(point3(0,0,555), vec3(555,0,0), vec3(0,0,-555), white)); //buttom
      world.add(make_shared<quad>(point3(555,0,555), vec3(-555,0,0), vec3(0,555,0), wave_mat)); //back

      // Light
      auto light = make_shared<diffuse_light>(color(10.0, 10.0, 10.0));
      world.add(make_shared<quad>(point3(148,554,174), vec3(260,0,0), vec3(0,0,210), light));
      lights.add(make_shared<quad>(point3(343,554,332), vec3(-130,0,0), vec3(0,0,-105), light));
      emitters.add_quad(point3(148,554,174), vec3(260,0,0), vec3(0,0,210), color(60.0, 60.0, 60.0));
      // Box 1
      shared_ptr<hittable> box1 = box(point3(0,0,0), point3(165,330,165), white);
      box1 = make_shared<rotate_y>(box1, 15);
      box1 = make_shared<translate>(box1, vec3(265,0,295));
      world.add(box1);

      // Glass Sphere
      world.add(make_shared<sphere>(point3(190,90,190), 90, glass));

      cam.vfov     = 40;
      cam.lookfrom = point3(278, 278, -800);
      cam.lookat   = point3(278, 278, 0);
      cam.vup      = vec3(0, 1, 0);
      break;
    }
    case 2: { // Cornell box + glass prism to look back dispersion
      // Cornell box sides
      world.add(make_shared<quad>(point3(555,0,0), vec3(0,0,555), vec3(0,555,0), green));
      world.add(make_shared<quad>(point3(0,0,555), vec3(0,0,-555), vec3(0,555,0), red));
      world.add(make_shared<quad>(point3(0,555,0), vec3(555,0,0), vec3(0,0,555), white)); //up
      world.add(make_shared<quad>(point3(0,0,555), vec3(555,0,0), vec3(0,0,-555), white)); //buttom
      world.add(make_shared<quad>(point3(555,0,555), vec3(-555,0,0), vec3(0,555,0), wave_mat)); //back

      // Light
      auto light = make_shared<diffuse_light>(color(60.0, 60.0, 60.0));
      world.add(make_shared<quad>(point3(148,554,174), vec3(260,0,0), vec3(0,0,210), light));
      lights.add(make_shared<quad>(point3(343,554,332), vec3(-130,0,0), vec3(0,0,-105), light));
      emitters.add_quad(point3(148,554,174), vec3(260,0,0), vec3(0,0,210), color(60.0, 60.0, 60.0));

      // prism
      shared_ptr<hittable> prism1 = prism(point3(0,0,0), point3(-150,0,180), point3(150,0,180), 400.0, glass);
      prism1 = make_shared<rotate_x>(prism1, 15);
      prism1 = make_shared<translate>(prism1, vec3(275,5,75));
      world.add(prism1);

      cam.vfov     = 40;
      cam.lookfrom = point3(278, 278, -800);
      cam.lookat   = point3(278, 278, 0);
      cam.vup      = vec3(0, 1, 0);
      break;
    }
    case 3: { // dispersion prism to ground
      auto glass2 = make_shared<dielectric>(1.5, 0.015);
      world.add(make_shared<quad>(point3(555,0,0), vec3(0,0,555), vec3(0,555,0), green));
      world.add(make_shared<quad>(point3(0,0,555), vec3(0,0,-555), vec3(0,555,0), red));
      world.add(make_shared<quad>(point3(0,555,0), vec3(555,0,0), vec3(0,0,555), white)); //up
      world.add(make_shared<quad>(point3(0,0,555), vec3(555,0,0), vec3(0,0,-555), white)); //buttom
      world.add(make_shared<quad>(point3(555,0,555), vec3(-555,0,0), vec3(0,555,0), wave_mat)); //back
      world.add(make_shared<quad>(point3(0,278,270), vec3(555,0,0), vec3(0,0,-275), white)); //front-blocker
      world.add(make_shared<quad>(point3(0,278,560), vec3(555,0,0), vec3(0,0,-283), white)); //back-blocker

      auto light = make_shared<diffuse_light>(color(60.0, 60.0, 60.0));
      world.add(make_shared<quad>(point3(0,554,280), vec3(0,0,-15), vec3(555,0,0), light));
      lights.add(make_shared<quad>(point3(0,554,280), vec3(0,0,-15), vec3(555,0,0), light));
      emitters.add_quad(point3(0,554,280), vec3(0,0,-15), vec3(555,0,0), color(60.0, 60.0, 60.0));

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
    default:
      break;
    }

    cam.aspect_ratio      = 1.0;
    cam.image_width       = 600;
    cam.samples_per_pixel = 100;
    cam.max_depth         = 50;
    cam.background        = color(0,0,0);
    cam.defocus_angle = 0;

    cam.debug_only_photon_render = true;

    auto start = std::chrono::high_resolution_clock::now();

    photon_map caustic_map;
    caustic_map.photon_count = 500000;
    caustic_map.max_depth = 20;

    caustic_map.gather_radius = 4.0;
    caustic_map.max_gather_radius = 18.0;
    caustic_map.min_photons_per_gather = 30;
    caustic_map.adaptive_radius_growth = 1.5;

    caustic_map.grid_cell_size = 6.0;
    caustic_map.spectral_radius_nm = 40.0;
    caustic_map.caustic_strength = 2.0;

    caustic_map.use_spatial_grid = true;
    caustic_map.use_adaptive_gather = true;

    caustic_map.use_k_nearest_gather = true;
    caustic_map.k_nearest_photon_count = 50;
    caustic_map.k_nearest_max_radius = 18.0;
    caustic_map.k_nearest_radius_growth = 1.5;
    caustic_map.k_nearest_require_full_count = true;

    caustic_map.debug_write_ply = false;
    caustic_map.debug_ply_filename = "caustic_photons.ply";
    caustic_map.debug_ply_color_scale = 50000.0;

    build_caustic_photon_map(world, emitters, caustic_map);

    cam.render(world, lights, caustic_map);

    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = end - start;

    std::clog << "\nRender time: "
              << std::fixed << std::setprecision(3)
              << elapsed.count()
              << " seconds\n";

    rgb2spec_free(g_rgb2spec_model);
    return 0;
}