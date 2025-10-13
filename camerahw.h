#ifndef CAMERAHW_H
#define CAMERAHW_H

#include "hittable.h"
#include "material.h"
#include "imageIO.h"

class camera {
  public:
    double aspect_ratio = 1.0;  // Ratio of image width over height
    int    image_width  = 100;  // Rendered image width in pixel count
    int    image_height = 100;  // Rendered image height
    int    samples_per_pixel = 5;   // Count of random samples for each pixel
    int    max_depth         = 10;   // Maximum number of ray bounces into scene

    double vfov = 90;  // Vertical view angle (field of view)
    point3 lookfrom = point3(0,0,0);   // Point camera is looking from
    point3 lookat   = point3(0,0,-1);  // Point camera is looking at
    vec3   vup      = vec3(0,1,0);     // Camera-relative "up" direction
    point3 light_pos = point3(0,0,0);  // hw2 light_pos

    void render(const hittable& world) {
        initialize();

        ColorImage image;
        image.init(image_width, image_width);
        for (int j = 0; j < image_height; j++) {
            std::clog << "\rScanlines remaining: " << (image_height - j) << ' ' << std::flush;
            for (int i = 0; i < image_width; i++) {
                color pixel_color(0,0,0);
                for (int sample = 0; sample < samples_per_pixel; sample++) {
                    ray r = get_ray(i, j);
                    pixel_color += ray_color(r, max_depth, world);
                }
                image.writePixel(i, j, pixel_samples_scale * pixel_color);
            }
        }
        image.outputPPM("output.ppm");
        std::clog << "\rDone.                 \n";
    }

  private:
    double pixel_samples_scale;  // Color scale factor for a sum of pixel samples
    point3 center;         // Camera center
    point3 pixel00_loc;    // Location of pixel 0, 0
    vec3   pixel_delta_u;  // Offset to pixel to the right
    vec3   pixel_delta_v;  // Offset to pixel below
    vec3   u, v, w;              // Camera frame basis vectors

    void initialize() {
        image_height = int(image_width / aspect_ratio);
        image_height = (image_height < 1) ? 1 : image_height;

        center = lookfrom;

        pixel_samples_scale = 1.0 / samples_per_pixel;

        // Determine viewport dimensions.
        auto focal_length = (lookfrom - lookat).length();
        auto theta = degrees_to_radians(vfov);
        auto h = std::tan(theta/2);
        auto viewport_height = 2 * h * focal_length;
        auto viewport_width = viewport_height * (double(image_width)/image_height);

        // Calculate the u,v,w unit basis vectors for the camera coordinate frame.
        w = unit_vector(lookfrom - lookat);
        u = unit_vector(cross(vup, w));
        v = cross(w, u);

        // Calculate the vectors across the horizontal and down the vertical viewport edges.
        vec3 viewport_u = viewport_width * u;    // Vector across viewport horizontal edge
        vec3 viewport_v = viewport_height * -v;  // Vector down viewport vertical edge

        // Calculate the horizontal and vertical delta vectors from pixel to pixel.
        pixel_delta_u = viewport_u / image_width;
        pixel_delta_v = viewport_v / image_height;

        // Calculate the location of the upper left pixel.
        auto viewport_upper_left = center - (focal_length * w) - viewport_u/2 - viewport_v/2;
        pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);
    }

    ray get_ray(int i, int j) const {
        // Construct a camera ray originating from the origin and directed at randomly sampled
        // point around the pixel location i, j.

        auto offset = sample_square();
        auto pixel_sample = pixel00_loc
                          + ((i + offset.x()) * pixel_delta_u)
                          + ((j + offset.y()) * pixel_delta_v);

        auto ray_origin = center;
        auto ray_direction = pixel_sample - ray_origin;

        return ray(ray_origin, ray_direction);
    }

    vec3 sample_square() const {
        // Returns the vector to a random point in the [-.5,-.5]-[+.5,+.5] unit square.
        return vec3(random_double() - 0.5, random_double() - 0.5, 0);
    }

    color ray_color(const ray& r, int depth, const hittable& world) const {
        // If we've exceeded the ray bounce limit, no more light is gathered.
        if (depth <= 0)
            return color(0,0,0);

        hit_record rec;

        if (world.hit(r, interval(0.001, infinity), rec)) { //hw2: remake ray calc after hit
            const shared_ptr<phong> phong_mat = std::dynamic_pointer_cast<phong>(rec.mat);
            point3 light_pos = this->light_pos;
            vec3 light_dir = unit_vector(light_pos - rec.p);
            vec3 view_dir = r.direction();

            // calc phong color
            color ambient_color = phong_mat->ambient(); 
            color diffuse_color = phong_mat->diffuse(light_dir, rec.normal);
            color specular_color = phong_mat->specular(light_dir, rec.normal, view_dir);

            color local_phong_color;

            // shadow
            double dist_to_light = (light_pos - rec.p).length();
            ray ray_shadow = ray(rec.p, light_dir);
            hit_record shadow_rec;

            if (world.hit(ray_shadow, interval(0.001, dist_to_light), shadow_rec)) { // is in shadow
                local_phong_color = ambient_color;
            } else {
                local_phong_color = ambient_color + diffuse_color + specular_color;
            }

            double reflect_ratio = phong_mat->get_MR();

            if (reflect_ratio > 0.0) {
              vec3 reflected_direction = reflect(unit_vector(view_dir), rec.normal); 
              ray scattered = ray(rec.p, reflected_direction); 

              color reflection_color = ray_color(scattered, depth-1, world);

              return (1.0 - reflect_ratio) * local_phong_color + reflect_ratio * reflection_color;
          }

            return local_phong_color;
        }
        return color(0,0,0);
        vec3 unit_direction = unit_vector(r.direction());
        auto a = 0.5*(unit_direction.y() + 1.0);
        return (1.0-a)*color(1.0, 1.0, 1.0) + a*color(0.5, 0.7, 1.0); //hw2: adjust bg light
    }
};

#endif