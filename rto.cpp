#include "rtweekend.h"

#include "camera.h"
#include "hittable.h"
#include "hittable_list.h"
#include "material.h"
#include "sphere.h"
#include "triangle.h"
#include "imageIO.h"

int main(int argc, char** argv) {
    if (argc < 1) {
        std::cerr << "Usage: " << argv[0] << " scene_file.txt\n";
        return 1;
    }

    std::ifstream infile(argv[1]);
    if (!infile) {
        std::cerr << "Cannot open file: " << argv[1] << "\n";
        return 1;
    }

    // Variables to be read from file
    bool have_eye = false, have_output = false, have_res = false;
    point3 Eye;
    point3 UL, UR, LL, LR;
    int image_width = 0, image_height = 0;

    hittable_list world;

    std::string line;
    while (std::getline(infile, line)) {
        // skip empty or comment lines (starting with #)
        std::string trimmed = line;
        // trim leading spaces
        size_t pos = trimmed.find_first_not_of(" \t\r\n");
        if (pos == std::string::npos) continue;
        if (trimmed[pos] == '#') continue;

        std::istringstream iss(trimmed);
        std::string token;
        if (!(iss >> token)) continue;

        if (token == "E") {
            double x,y,z;
            if (iss >> x >> y >> z) {
                Eye = point3(x,y,z);
                have_eye = true;
            } else {
                std::cerr << "Invalid E line: " << line << "\n";
                // return 1;
            }
        } else if (token == "O") {
            double ulx,uly,ulz, urx,ury,urz, llx,lly,llz, lrx,lry,lrz;
            if (iss >> ulx >> uly >> ulz >> urx >> ury >> urz >> llx >> lly >> llz >> lrx >> lry >> lrz) {
                UL = point3(ulx,uly,ulz);
                UR = point3(urx,ury,urz);
                LL = point3(llx,lly,llz);
                LR = point3(lrx,lry,lrz);
                have_output = true;
            } else {
                std::cerr << "Invalid O line: " << line << "\n";
                // return 1;
            }
        } else if (token == "R") {
            int w,h;
            if (iss >> w >> h) {
                image_width = w;
                image_height = h;
                have_res = true;
            } else {
                std::cerr << "Invalid R line: " << line << "\n";
                // return 1;
            }
        } else if (token == "S") {
            double ox,oy,oz,r;
            if (iss >> ox >> oy >> oz >> r) {
                world.add(make_shared<sphere>(point3(ox,oy,oz), r));
            } else {
                std::cerr << "Invalid S line: " << line << "\n";
                // return 1;
            }
        } else if (token == "T") {
            double x1,y1,z1,x2,y2,z2,x3,y3,z3;
            if (iss >> x1>>y1>>z1>>x2>>y2>>z2>>x3>>y3>>z3) {
                world.add(make_shared<triangle>(point3(x1,y1,z1), point3(x2,y2,z2), point3(x3,y3,z3)));
            } else {
                std::cerr << "Invalid T line: " << line << "\n";
                // return 1;
            }
        } else {
            // unknown token: ignore or warn
            std::clog << "Warning: unknown token '" << token << "' in line: " << line << "\n";
        }
    }

    if (!have_eye) {
        std::cerr << "Input file missing Eye (E) line.\n";
        return 1;
    }
    if (!have_output) {
        std::cerr << "Input file missing Output Image (O) line.\n";
        return 1;
    }
    if (!have_res) {
        std::cerr << "Input file missing Resolution (R) line.\n";
        return 1;
    }

    hittable_list world;

    auto material_ground = make_shared<lambertian>(color(0.8, 0.8, 0.0));
    auto material_center = make_shared<lambertian>(color(0.1, 0.2, 0.5));
    auto material_left   = make_shared<dielectric>(1.50);
    auto material_bubble = make_shared<dielectric>(1.00 / 1.50);
    auto material_right  = make_shared<metal>(color(0.8, 0.6, 0.2), 1.0);

    world.add(make_shared<sphere>(point3( 0.0, -100.5, -1.0), 100.0, material_ground));
    world.add(make_shared<sphere>(point3( 0.0,    0.0, -1.2),   0.5, material_center));
    world.add(make_shared<sphere>(point3(-1.0,    0.0, -1.0),   0.5, material_left));
    world.add(make_shared<sphere>(point3(-1.0,    0.0, -1.0),   0.4, material_bubble));
    world.add(make_shared<sphere>(point3( 1.0,    0.0, -1.0),   0.5, material_right));

    camera cam;

    cam.aspect_ratio      = 16.0 / 9.0;
    cam.image_width       = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth         = 50;


    cam.vfov     = 20;
    cam.lookfrom = point3(-2,2,1);
    cam.lookat   = point3(0,0,-1);
    cam.vup      = vec3(0,1,0);

    cam.render(world);
}