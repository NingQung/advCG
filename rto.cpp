#include "rtweekend.h"

#include "camerahw.h"
#include "hittable.h"
#include "hittable_list.h"
#include "material.h"
#include "sphere.h"
#include "triangle.h"

#include <chrono>

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
    bool have_eye = false, have_res = false, have_view = false, have_fov = false;
    point3 Eye;
    vec3 view_dir, view_up;
    double fov = 90;
    int image_width = 0, image_height = 0;
    double aspect_ratio = 0;
    

    hittable_list world;
    camera cam;
    cam.samples_per_pixel = 100;
    cam.max_depth         = 50;

    auto material_static = make_shared<phong>(color(0.8, 0.8, 0.8), 0.2, 0.7, 1.0, 10.0, 0.5);

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

        if (token == "E") { // Eye pos
            double x,y,z;
            if (iss >> x >> y >> z) {
                Eye = point3(x,y,z);
                have_eye = true;
            } else {
                std::cerr << "Invalid E line: " << line << "\n";
                // return 1;
            }
        } else if (token == "V") { //View
          double dx,dy,dz,ux,uy,uz;
          if (iss >> dx>>dy>>dz>>ux>>uy>>uz) {
            view_dir = vec3(dx,dy,dz);
            view_up  = vec3(ux,uy,uz);
            have_view = true;
          } else {
            std::cerr << "Invalid V line: " << line << "\n";
            // return 1;
          }
        } else if (token == "F") { //Fov
          double fangle;
          if(iss >> fangle){
            fov = fangle;
            have_fov = true;
          } else {
            std::cerr << "Invalid F line: " << line << "\n";
            // return 1;
          }
        } else if (token == "R") { //Resolution
            int w,h;
            if (iss >> w >> h) {
                image_width = w;
                image_height = h;
                aspect_ratio = (double)image_width / (double)image_height;
                have_res = true;
            } else {
                std::cerr << "Invalid R line: " << line << "\n";
                // return 1;
            }
        } else if (token == "S") { //Sphere
            double ox,oy,oz,r;
            if (iss >> ox >> oy >> oz >> r) {
                world.add(make_shared<sphere>(point3(ox,oy,oz), r, material_static));
            } else {
                std::cerr << "Invalid S line: " << line << "\n";
                // return 1;
            }
        } else if (token == "T") { //triangle
            double x1,y1,z1,x2,y2,z2,x3,y3,z3;
            if (iss >> x1>>y1>>z1>>x2>>y2>>z2>>x3>>y3>>z3) {
                world.add(make_shared<triangle>(point3(x1,y1,z1), point3(x2,y2,z2), point3(x3,y3,z3), material_static));
            } else {
                std::cerr << "Invalid T line: " << line << "\n";
                // return 1;
            }
        } else if (token == "L") { //Light pos
          double lx,ly,lz;
          if (iss >> lx>>ly>>lz) {
              point3 light_pos(lx, ly, lz);
              cam.light_pos = light_pos; 
          } else {
              std::cerr << "Invalid L line: " << line << "\n";
              // return 1;
          }
        } else if (token == "M") { //Material (Phong here)
          double mr,mg,mb,Ka,Kd,Ks,exps,MR;
          if (iss >> mr>>mg>>mb>>Ka>>Kd>>Ks>>exps>>MR) {
              material_static = make_shared<phong>(color(mr, mg, mb), Ka, Kd, Ks, exps, MR);
          } else {
              std::cerr << "Invalid M line: " << line << "\n";
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
    if (!have_res) {
        std::cerr << "Input file missing Resolution (R) line.\n";
        return 1;
    }

    // E & V & F & R
    cam.aspect_ratio      = aspect_ratio;
    cam.image_width       = image_width;
    cam.image_height      = image_height;
    cam.lookfrom          = Eye;
    cam.lookat            = Eye + view_dir;
    cam.vup               = view_up;
    cam.vfov              = fov;

    auto start_time = std::chrono::high_resolution_clock::now();

    cam.render(world);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::clog << "Total Rendering Time: " << duration.count() << " milliseconds\n";
}