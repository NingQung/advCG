#ifndef CAMERA_H
#define CAMERA_H

#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

#include "hittable.h"
#include "pdf.h"
#include "material.h"
#include "photon_map.h"

class camera {
  public:
    double aspect_ratio = 1.0;  // Ratio of image width over height
    int    image_width  = 100;  // Rendered image width in pixel count
    int    samples_per_pixel = 5;   // Count of random samples for each pixel
    int    max_depth         = 10;   // Maximum number of ray bounces into scene
    color  background;               // Scene background color

    double vfov = 90;  // Vertical view angle (field of view)
    point3 lookfrom = point3(0,0,0);   // Point camera is looking from
    point3 lookat   = point3(0,0,-1);  // Point camera is looking at
    vec3   vup      = vec3(0,1,0);     // Camera-relative "up" direction

    double defocus_angle = 0;  // Variation angle of rays through each pixel
    double focus_dist = 10;    // Distance from camera lookfrom point to plane of perfect focus

    bool debug_only_photon_render = false;  // debug mode 
    bool use_photon_rgb_caustic = false;
    bool use_parallel_render = true;
    // <= 0 means use std::thread::hardware_concurrency().
    int thread_count = 0;

    void render(const hittable& world, const hittable& lights) {
        photon_map empty_caustic_map;
        render(world, lights, empty_caustic_map);
    }

    void render(const hittable& world, const hittable& lights, const photon_map& caustic_map) {
        initialize();

        std::vector<color> framebuffer(image_width * image_height, color(0, 0, 0));

        int worker_count = 1;

        if (use_parallel_render) {
            unsigned int hardware_threads = std::thread::hardware_concurrency();

            if (thread_count > 0) {
                worker_count = thread_count;
            } else if (hardware_threads > 0) {
                worker_count = int(hardware_threads);
            } else {
                worker_count = 1;
            }
        }

        worker_count = std::max(1, worker_count);

        std::clog << "Render threads: " << worker_count << "\n";

        std::atomic<int> next_row(0);
        std::atomic<int> completed_rows(0);
        std::mutex log_mutex;

        auto render_worker = [&]() {
            while (true) {
                int j = next_row.fetch_add(1, std::memory_order_relaxed);

                if (j >= image_height)
                    break;

                for (int i = 0; i < image_width; ++i) {
                    framebuffer[j * image_width + i] =
                        render_pixel(i, j, world, lights, caustic_map);
                }

                int done = completed_rows.fetch_add(1, std::memory_order_relaxed) + 1;

                if (done == image_height || done % 8 == 0) {
                    std::lock_guard<std::mutex> lock(log_mutex);
                    std::clog << "\rScanlines remaining: "
                              << (image_height - done) << ' ' << std::flush;
                }
            }
        };

        if (worker_count == 1) {
            render_worker();
        } else {
            std::vector<std::thread> workers;
            workers.reserve(worker_count);

            for (int t = 0; t < worker_count; ++t) {
                workers.emplace_back(render_worker);
            }

            for (auto& worker : workers) {
                worker.join();
            }
        }

        std::clog << "\rWriting image.                 \n";

        std::cout << "P3\n" << image_width << ' ' << image_height << "\n255\n";

        for (int j = 0; j < image_height; ++j) {
            for (int i = 0; i < image_width; ++i) {
                write_color(std::cout, framebuffer[j * image_width + i]);
            }
        }

        std::clog << "\rDone.                 \n";
    }

  private:
    int    image_height;   // Rendered image height
    double pixel_samples_scale;  // Color scale factor for a sum of pixel samples
    int    sqrt_spp;             // Square root of number of samples per pixel
    double recip_sqrt_spp;       // 1 / sqrt_spp
    point3 center;         // Camera center
    point3 pixel00_loc;    // Location of pixel 0, 0
    vec3   pixel_delta_u;  // Offset to pixel to the right
    vec3   pixel_delta_v;  // Offset to pixel below
    vec3   u, v, w;              // Camera frame basis vectors
    vec3   defocus_disk_u;       // Defocus disk horizontal radius
    vec3   defocus_disk_v;       // Defocus disk vertical radius

    void initialize() {
        image_height = int(image_width / aspect_ratio);
        image_height = (image_height < 1) ? 1 : image_height;

        sqrt_spp = int(std::sqrt(samples_per_pixel));
        sqrt_spp = (sqrt_spp < 1) ? 1 : sqrt_spp;

        pixel_samples_scale = 1.0 / (sqrt_spp * sqrt_spp);
        recip_sqrt_spp = 1.0 / sqrt_spp;

        center = lookfrom;

        pixel_samples_scale = 1.0 / samples_per_pixel;

        // Determine viewport dimensions.
        auto theta = degrees_to_radians(vfov);
        auto h = std::tan(theta/2);
        auto viewport_height = 2 * h * focus_dist;
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
        auto viewport_upper_left = center - (focus_dist * w) - viewport_u/2 - viewport_v/2;
        pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

        // Calculate the camera defocus disk basis vectors.
        auto defocus_radius = focus_dist * std::tan(degrees_to_radians(defocus_angle / 2));
        defocus_disk_u = u * defocus_radius;
        defocus_disk_v = v * defocus_radius;
    }

    ray get_ray(int i, int j, int s_i, int s_j) const {
        // Construct a camera ray originating from the defocus disk and directed at a randomly
        // sampled point around the pixel location i, j for stratified sample square s_i, s_j.

        auto offset = sample_square_stratified(s_i, s_j);
        auto pixel_sample = pixel00_loc
                          + ((i + offset.x()) * pixel_delta_u)
                          + ((j + offset.y()) * pixel_delta_v);

        auto ray_origin = (defocus_angle <= 0) ? center : defocus_disk_sample();
        auto ray_direction = pixel_sample - ray_origin;
        auto ray_time = random_double();

        auto wl = Wavelengths::sample();

        return ray(ray_origin, ray_direction, ray_time, wl);
    }

    vec3 sample_square_stratified(int s_i, int s_j) const {
        // Returns the vector to a random point in the square sub-pixel specified by grid
        // indices s_i and s_j, for an idealized unit square pixel [-.5,-.5] to [+.5,+.5].

        auto px = ((s_i + random_double()) * recip_sqrt_spp) - 0.5;
        auto py = ((s_j + random_double()) * recip_sqrt_spp) - 0.5;

        return vec3(px, py, 0);
    }

    vec3 sample_square() const {
        // Returns the vector to a random point in the [-.5,-.5]-[+.5,+.5] unit square.
        return vec3(random_double() - 0.5, random_double() - 0.5, 0);
    }

    point3 defocus_disk_sample() const {
        // Returns a random point in the camera defocus disk.
        auto p = random_in_unit_disk();
        return center + (p[0] * defocus_disk_u) + (p[1] * defocus_disk_v);
    }

    double average_active_energy(
        const SpectralEnergy& energy,
        const Wavelengths& wavelengths
    ) const {
        double sum = 0.0;
        int count = 0;

        for (int i = 0; i < WL_PER_RAY; ++i) {
            if (wavelengths.hero_only && i != 0)
                continue;

            sum += std::fmax(0.0, energy.energy[i]);
            count++;
        }

        if (count <= 0)
            return 0.0;

        return sum / count;
    }

    struct sample_result {
        SpectralEnergy spectral;
        color photon_rgb_caustic;

        sample_result()
            : spectral(0.0), photon_rgb_caustic(0, 0, 0) {}
    };

    color render_pixel(
        int i,
        int j,
        const hittable& world,
        const hittable& lights,
        const photon_map& caustic_map
    ) const {
        color pixel_color(0, 0, 0);

        for (int s_j = 0; s_j < sqrt_spp; ++s_j) {
            for (int s_i = 0; s_i < sqrt_spp; ++s_i) {
                ray r = get_ray(i, j, s_i, s_j);

                if (use_photon_rgb_caustic) {
                    sample_result sample =
                        ray_color_with_rgb_caustic(r, max_depth, world, lights, caustic_map);

                    vec3 sample_rgb = spectral_to_rgb(sample.spectral, r.wavelengths());
                    pixel_color += sample_rgb + sample.photon_rgb_caustic;
                } else {
                    SpectralEnergy sample_energy =
                        ray_color(r, max_depth, world, lights, caustic_map);

                    vec3 sample_rgb = spectral_to_rgb(sample_energy, r.wavelengths());
                    pixel_color += sample_rgb;
                }
            }
        }

        return pixel_samples_scale * pixel_color;
    }

    sample_result ray_color_with_rgb_caustic(
        const ray& r,
        int depth,
        const hittable& world,
        const hittable& lights,
        const photon_map& caustic_map,
        bool allow_caustic_gather = true
    ) const {
        sample_result result;

        if (depth <= 0)
            return result;

        hit_record rec;

        if (!world.hit(r, interval(0.001, infinity), rec))
            return result;

        scatter_record srec;
        SpectralEnergy color_from_emission = rec.mat->emitted(r, rec, rec.u, rec.v, rec.p);

        if (!rec.mat->scatter(r, rec, srec)) {
            result.spectral = color_from_emission;
            return result;
        }

        if (srec.skip_pdf) {
            sample_result child =
                ray_color_with_rgb_caustic(
                    srec.skip_pdf_ray,
                    depth - 1,
                    world,
                    lights,
                    caustic_map,
                    allow_caustic_gather
                );

            result.spectral = srec.attenuation * child.spectral;

            // Convert specular attenuation on this camera path to RGB and apply it
            // to the RGB caustic side-channel. This keeps camera-through-glass
            // caustics from ignoring the glass path.
            double attenuation_scalar = average_active_energy(srec.attenuation, r.wavelengths());
            result.photon_rgb_caustic = attenuation_scalar * child.photon_rgb_caustic;

            return result;
        }

        color color_from_caustic =
            allow_caustic_gather
                ? caustic_map.estimate_caustic_rgb(rec, r)
                : color(0, 0, 0);

        if (debug_only_photon_render) {
            result.photon_rgb_caustic = color_from_caustic;
            return result;
        }

        auto light_ptr = make_shared<spectral_light_pdf>(lights, rec.p);
        mixture_pdf p(light_ptr, srec.pdf_ptr);

        ray scattered = ray(rec.p, p.generate(r.wavelengths()), r.time(), r.wavelengths());
        auto pdf_value = p.value_joint(scattered.direction(), r.wavelengths());

        if (pdf_value <= 0.0) {
            result.spectral = color_from_emission;
            result.photon_rgb_caustic = color_from_caustic;
            return result;
        }

        SpectralEnergy scattering_pdf;

        for (int k = 0; k < WL_PER_RAY; ++k) {
            if (r.wavelengths().hero_only && k != 0) {
                scattering_pdf.energy[k] = 0.0;
                continue;
            }

            scattering_pdf.energy[k] =
                rec.mat->scattering_pdf(r, rec, scattered, r.wavelengths().lambda[k]);
        }

        sample_result child =
            ray_color_with_rgb_caustic(
                scattered,
                depth - 1,
                world,
                lights,
                caustic_map,
                false
            );

        result.spectral =
            color_from_emission +
            (srec.attenuation * scattering_pdf * child.spectral) / pdf_value;

        result.photon_rgb_caustic = color_from_caustic;

        return result;
    }

    SpectralEnergy ray_color(const ray& r, int depth, const hittable& world, const hittable& lights, const photon_map& caustic_map, bool allow_caustic_gather = true) const {
        if (depth <= 0)
            return SpectralEnergy(0.0);

        hit_record rec;

        if (!world.hit(r, interval(0.001, infinity), rec))
            return SpectralEnergy(0.0);

        scatter_record srec;
        SpectralEnergy color_from_emission = rec.mat->emitted(r, rec, rec.u, rec.v, rec.p);

        if (!rec.mat->scatter(r, rec, srec))
            return color_from_emission;

        if (srec.skip_pdf) {
            return srec.attenuation * ray_color(srec.skip_pdf_ray, depth - 1, world, lights, caustic_map, allow_caustic_gather);
        }

        SpectralEnergy color_from_caustic = allow_caustic_gather ? 
          caustic_map.estimate_caustic(rec, r) : SpectralEnergy(0.0);

        if (debug_only_photon_render) {
            return color_from_caustic;
        }

        auto light_ptr = make_shared<spectral_light_pdf>(lights, rec.p);
        mixture_pdf p(light_ptr, srec.pdf_ptr);

        ray scattered = ray(rec.p, p.generate(r.wavelengths()), r.time(), r.wavelengths());
        auto pdf_value = p.value_joint(scattered.direction(), r.wavelengths());

        if (pdf_value <= 0.0)
            return color_from_emission + color_from_caustic;

        SpectralEnergy scattering_pdf;

        for (int k = 0; k < WL_PER_RAY; ++k) {
            if (r.wavelengths().hero_only && k != 0) {
                scattering_pdf.energy[k] = 0.0;
                continue;
            }

            scattering_pdf.energy[k] =
                rec.mat->scattering_pdf(r, rec, scattered, r.wavelengths().lambda[k]);
        }

        SpectralEnergy sample_color = ray_color(scattered, depth - 1, world, lights, caustic_map, false);
        SpectralEnergy color_from_scatter = (srec.attenuation * scattering_pdf * sample_color) / pdf_value;

        return color_from_emission + color_from_caustic + color_from_scatter;

    }
};

#endif