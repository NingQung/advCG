#ifndef PHOTON_MAP_H
#define PHOTON_MAP_H

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

#include "rtweekend.h"
#include "hittable.h"
#include "material.h"
#include "onb.h"

struct spectral_photon {
    point3 position;
    vec3 normal;
    vec3 incident_direction;
    SpectralEnergy power;
    Wavelengths wavelengths;
};

inline bool has_positive_spectral_power(const SpectralEnergy& power) {
    for (int i = 0; i < WL_PER_RAY; ++i) {
        if (power.energy[i] > 0.0)
            return true;
    }

    return false;
}

class area_light_emitter {
  public:
    area_light_emitter() = default;

    area_light_emitter(const point3& Q, const vec3& u, const vec3& v, const color& emit_rgb)
      : Q(Q), u(u), v(v), emit_rgb(emit_rgb)
    {
        vec3 n = cross(u, v);
        area = n.length();
        normal = unit_vector(n);
    }

    ray sample_ray(const Wavelengths& wl) const {
        point3 origin = Q + random_double() * u + random_double() * v;

        onb uvw(normal);
        vec3 direction = unit_vector(uvw.transform(random_cosine_direction()));

        // Move the origin slightly toward the emitting hemisphere, so the photon
        // does not immediately intersect the light surface that emitted it.
        return ray(origin + 0.001 * normal, direction, random_double(), wl);
    }

    SpectralEnergy initial_power(const Wavelengths& wl, int photon_count, int emitter_count) const {
        if (photon_count <= 0)
            return SpectralEnergy(0.0);

        // A diffuse area emitter with radiance Le has total flux roughly:
        //
        //     flux = Le * area * pi
        //
        // Emitters are selected uniformly, so multiply by emitter_count to compensate
        // for the emitter-selection probability.
        SpectralEnergy emitted = rgb_emission_to_spectral_energy(emit_rgb, wl);
        return emitted * (area * pi * emitter_count / double(photon_count));
    }

  private:
    point3 Q;
    vec3 u;
    vec3 v;
    color emit_rgb;
    vec3 normal;
    double area = 0.0;
};

class light_emitter_list {
  public:
    void add_quad(const point3& Q, const vec3& u, const vec3& v, const color& emit_rgb) {
        emitters.push_back(area_light_emitter(Q, u, v, emit_rgb));
    }

    bool empty() const { return emitters.empty(); }
    int size() const { return int(emitters.size()); }

    const area_light_emitter& random_emitter() const {
        return emitters[random_int(0, int(emitters.size()) - 1)];
    }

  private:
    std::vector<area_light_emitter> emitters;
};

class photon_map {
  public:
    int photon_count = 200000;
    int max_depth = 20;

    // Cornell box size is roughly 555, so 8~20 is a reasonable first test range.
    double gather_radius = 12.0;

    // Since photons and camera rays sample different wavelengths, this acts as a
    // simple spectral reconstruction radius in nanometers.
    double spectral_radius_nm = 25.0;

    // Practical brightness knob. Photon mapping brightness often needs calibration
    // in a first implementation.
    double caustic_strength = 1.0;

    void clear() {
        photons.clear();
    }

    void reserve(int count) {
        photons.reserve(count);
    }

    void store(
        const point3& position,
        const vec3& normal,
        const vec3& incident_direction,
        const SpectralEnergy& power,
        const Wavelengths& wavelengths
    ) {
        photons.push_back({
            position,
            unit_vector(normal),
            unit_vector(incident_direction),
            power,
            wavelengths
        });
    }

    int size() const {
        return int(photons.size());
    }

    SpectralEnergy estimate_caustic(const hit_record& rec, const ray& r_in) const {
        if (photons.empty() || gather_radius <= 0.0 || spectral_radius_nm <= 0.0)
            return SpectralEnergy(0.0);

        if (!rec.mat->supports_photon_gather())
            return SpectralEnergy(0.0);

        const Wavelengths& query_wl = r_in.wavelengths();

        SpectralEnergy flux(0.0);
        const double radius2 = gather_radius * gather_radius;

        for (const auto& photon : photons) {
            vec3 delta = photon.position - rec.p;

            if (delta.length_squared() > radius2)
                continue;

            // Avoid mixing photons from the opposite side of thin surfaces.
            if (dot(photon.normal, rec.normal) <= 0.1)
                continue;

            for (int q = 0; q < WL_PER_RAY; ++q) {
                if (query_wl.hero_only && q != 0)
                    continue;

                double accum_weight = 0.0;
                double accum_power = 0.0;

                for (int p = 0; p < WL_PER_RAY; ++p) {
                    if (photon.wavelengths.hero_only && p != 0)
                        continue;

                    if (photon.power.energy[p] <= 0.0)
                        continue;

                    double distance_nm = std::fabs(query_wl.lambda[q] - photon.wavelengths.lambda[p]);

                    if (distance_nm >= spectral_radius_nm)
                        continue;

                    // Triangular spectral kernel.
                    double weight = 1.0 - distance_nm / spectral_radius_nm;

                    accum_weight += weight;
                    accum_power += weight * photon.power.energy[p];
                }

                if (accum_weight > 0.0)
                    flux.energy[q] += accum_power / accum_weight;
            }
        }

        SpectralEnergy brdf = rec.mat->photon_gather_brdf(r_in, rec, query_wl);
        const double area = pi * gather_radius * gather_radius;

        return caustic_strength * (brdf * (flux / area));
    }

  private:
    std::vector<spectral_photon> photons;
};

inline void build_caustic_photon_map(
    const hittable& world,
    const light_emitter_list& emitters,
    photon_map& map
) {
    map.clear();

    if (emitters.empty() || map.photon_count <= 0)
        return;

    map.reserve(map.photon_count);

    for (int i = 0; i < map.photon_count; ++i) {
        if ((i + 1) % 10000 == 0) {
            std::clog << "\rPhoton pass: " << (i + 1) << " / " << map.photon_count << ' ' << std::flush;
        }

        Wavelengths wavelengths = Wavelengths::sample();
        const area_light_emitter& emitter = emitters.random_emitter();

        ray photon_ray = emitter.sample_ray(wavelengths);
        SpectralEnergy power = emitter.initial_power(wavelengths, map.photon_count, emitters.size());

        bool has_delta_bounce = false;

        for (int depth = 0; depth < map.max_depth; ++depth) {
            if (!has_positive_spectral_power(power))
                break;

            hit_record rec;

            if (!world.hit(photon_ray, interval(0.001, infinity), rec))
                break;

            scatter_record srec;

            if (!rec.mat->scatter(photon_ray, rec, srec))
                break;

            if (srec.skip_pdf) {
                has_delta_bounce = true;
                power = power * srec.attenuation;
                photon_ray = srec.skip_pdf_ray;
                continue;
            }

            // This first version is a caustic photon map:
            //
            //     light -> specular / dielectric -> diffuse
            //
            // Store only photons that passed through at least one delta interaction,
            // then terminate them. This avoids double-counting ordinary diffuse
            // indirect light, which the current path tracer already estimates.
            if (has_delta_bounce && rec.mat->supports_photon_gather()) {
                map.store(rec.p, rec.normal, photon_ray.direction(), power, photon_ray.wavelengths());
            }

            break;
        }
    }

    std::clog << "\rPhoton pass: done, stored " << map.size() << " caustic photons.          \n";
}

#endif