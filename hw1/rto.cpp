#include "rtweekend.h"
#include "hittable.h"
#include "hittable_list.h"
#include "sphere.h"
#include "triangle.h"
#include "imageIO.h"

color ray_color(const ray& r, const hittable& world) {
    hit_record rec;
    if (world.hit(r, interval(0, infinity), rec)) {
        auto map01 = [](double v)->double {
            double t = (v + 1.0) * 0.5;
            if (t < 0.0) return 0.0;
            if (t > 1.0) return 1.0;
            return t;
        };
        double r = map01(rec.p.x());
        double g = map01(rec.p.y());
        double b = map01(rec.p.z());
        return color(r, g, b);
    }
    return color(0,0,0);
}


int main(int argc, char** argv) {
    if (argc < 1) {
        std::cerr << "Usage: " << argv[0] << " scene_file.txt > out.ppm\n";
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
                return 1;
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
                return 1;
            }
        } else if (token == "R") {
            int w,h;
            if (iss >> w >> h) {
                image_width = w;
                image_height = h;
                have_res = true;
            } else {
                std::cerr << "Invalid R line: " << line << "\n";
                return 1;
            }
        } else if (token == "S") {
            double ox,oy,oz,r;
            if (iss >> ox >> oy >> oz >> r) {
                world.add(make_shared<sphere>(point3(ox,oy,oz), r));
            } else {
                std::cerr << "Invalid S line: " << line << "\n";
                return 1;
            }
        } else if (token == "T") {
            double x1,y1,z1,x2,y2,z2,x3,y3,z3;
            if (iss >> x1>>y1>>z1>>x2>>y2>>z2>>x3>>y3>>z3) {
                world.add(make_shared<triangle>(point3(x1,y1,z1), point3(x2,y2,z2), point3(x3,y3,z3)));
            } else {
                std::cerr << "Invalid T line: " << line << "\n";
                return 1;
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

    // Image

    // auto aspect_ratio = 16.0 / 9.0;
    // int image_width = 400;

    // Calculate the image height, and ensure that it's at least 1.
    // int image_height = int(image_width / aspect_ratio);
    // image_height = (image_height < 1) ? 1 : image_height;

    // World

    // hittable_list world;

    // world.add(make_shared<sphere>(point3(0,0,-1), 0.5));
    // world.add(make_shared<sphere>(point3(0,-100.5,-1), 100));

    // Camera

    auto focal_length = 1.0;
    auto viewport_height = 2.0;
    auto viewport_width = viewport_height * (double(image_width)/image_height);
    auto camera_center = Eye;

    // Calculate the vectors across the horizontal and down the vertical viewport edges.
    auto viewport_u = vec3(viewport_width, 0, 0);
    auto viewport_v = vec3(0, -viewport_height, 0);

    // Calculate the horizontal and vertical delta vectors from pixel to pixel.
    // auto pixel_delta_u = viewport_u / image_width;
    // auto pixel_delta_v = viewport_v / image_height;
    vec3 pixel_delta_u = (UR - UL) / double(image_width);
    vec3 pixel_delta_v = (LL - UL) / double(image_height);


    // Calculate the location of the upper left pixel.
    // auto viewport_upper_left = camera_center
    //                          - vec3(0, 0, focal_length) - viewport_u/2 - viewport_v/2;
    // auto pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);
    vec3 pixel00_loc = UL + 0.5 * (pixel_delta_u + pixel_delta_v);

    // Render

    // std::cout << "P3\n" << image_width << " " << image_height << "\n255\n";
    ColorImage image;
    image.init(image_width, image_width);
    for (int j = 0; j < image_height; j++) {
        // std::clog << "\rScanlines remaining: " << (image_height - j) << ' ' << std::flush;
        for (int i = 0; i < image_width; i++) {
            auto pixel_center = pixel00_loc + (i * pixel_delta_u) + (j * pixel_delta_v);
            auto ray_direction = pixel_center - camera_center;
            ray r(camera_center, ray_direction);

            color pixel_color = ray_color(r, world);
            image.writePixel(i, j, pixel_color);
        }
    }
    image.outputPPM("image.ppm");
    std::clog << "\rDone.                 \n";
}