#ifndef PHOTON_MAP_H
#define PHOTON_MAP_H

#include <algorithm>
#include <cmath>
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <utility>
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

struct photon_grid_key {
    int x;
    int y;
    int z;

    bool operator==(const photon_grid_key& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct photon_grid_key_hash {
    std::size_t operator()(const photon_grid_key& key) const {
        // Hash three signed integers. The constants are common large primes.
        std::size_t h1 = std::hash<int>{}(key.x);
        std::size_t h2 = std::hash<int>{}(key.y);
        std::size_t h3 = std::hash<int>{}(key.z);

        return h1 ^ (h2 * 73856093u) ^ (h3 * 19349663u);
    }
};

struct photon_candidate {
    int photon_index;
    double dist2;
};

struct photon_emission_sample {
    ray photon_ray;
    SpectralEnergy power;
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

    photon_emission_sample sample_photon(
        const Wavelengths& wl,
        int photon_count,
        int emitter_count,
        bool use_target,
        const point3& target_center,
        double target_radius
    ) const {
        point3 origin = Q + random_double() * u + random_double() * v;

        if (use_target && target_radius > 0.0) {
            vec3 to_target = target_center - origin;
            double distance_to_target = to_target.length();

            if (distance_to_target > target_radius) {
                double sin_theta_max = target_radius / distance_to_target;

                if (sin_theta_max > 0.999999)
                    sin_theta_max = 0.999999;

                double cos_theta_max = std::sqrt(
                    std::fmax(0.0, 1.0 - sin_theta_max * sin_theta_max)
                );

                double u1 = random_double();
                double u2 = random_double();

                double cos_theta = 1.0 - u1 * (1.0 - cos_theta_max);
                double sin_theta = std::sqrt(std::fmax(0.0, 1.0 - cos_theta * cos_theta));
                double phi = 2.0 * pi * u2;

                onb target_basis(unit_vector(to_target));
                vec3 local_direction(
                    std::cos(phi) * sin_theta,
                    std::sin(phi) * sin_theta,
                    cos_theta
                );

                vec3 direction = unit_vector(target_basis.transform(local_direction));
                double cos_on_light = dot(direction, normal);

                if (cos_on_light > 0.0) {
                    double solid_angle = 2.0 * pi * (1.0 - cos_theta_max);

                    SpectralEnergy emitted = rgb_emission_to_spectral_energy(emit_rgb, wl);
                    SpectralEnergy power =
                        emitted *
                        (area * solid_angle * cos_on_light * emitter_count / double(photon_count));

                    return {
                        ray(origin + 0.001 * normal, direction, random_double(), wl),
                        power
                    };
                }
            }
        }

        ray emitted_ray = sample_ray(wl);
        SpectralEnergy power = initial_power(wl, photon_count, emitter_count);

        return { emitted_ray, power };
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

    void set_target_sphere(const point3& center, double radius) {
        target_enabled = radius > 0.0;
        target_center_value = center;
        target_radius_value = radius;
    }

    void clear_target_sphere() {
        target_enabled = false;
        target_radius_value = 0.0;
    }

    bool has_target_sphere() const {
        return target_enabled && target_radius_value > 0.0;
    }

    const point3& target_center() const {
        return target_center_value;
    }

    double target_radius() const {
        return target_radius_value;
    }

  private:
    std::vector<area_light_emitter> emitters;

    bool target_enabled = false;
    point3 target_center_value = point3(0, 0, 0);
    double target_radius_value = 0.0;
};

class photon_map {
  public:
    int photon_count = 200000;
    int max_depth = 20;

    // Cornell box size is roughly 555, so 8~20 is a reasonable first test range.
    double gather_radius = 12.0;

    // If <= 0, build_grid() uses gather_radius as the cell size.
    // Usually cell_size = gather_radius is a good first choice.
    double grid_cell_size = 0.0;

    // Since photons and camera rays sample different wavelengths, this acts as a
    // simple spectral reconstruction radius in nanometers.
    double spectral_radius_nm = 25.0;

    // Practical brightness knob. Photon mapping brightness often needs calibration
    // in a first implementation.
    double caustic_strength = 1.0;
    double rgb_caustic_strength = 1.0;

    // Turn this off to compare against brute-force search.
    bool use_spatial_grid = true;

    // debug for photon map to ply file.
    bool debug_write_ply = false;
    std::string debug_ply_filename = "caustic_photons.ply";
    double debug_ply_color_scale = 50000.0;

    // Adaptive gather tries to avoid isolated photon speckles.
    // gather_radius becomes the initial radius.
    // max_gather_radius is the largest radius it may expand to.
    bool use_adaptive_gather = true;
    int min_photons_per_gather = 30;
    double max_gather_radius = 18.0;
    double adaptive_radius_growth = 1.5;

    // K-nearest gather uses the current spatial grid to collect nearby candidates.
    // It replaces radius accumulation with a fixed number of nearest photons.
    bool use_k_nearest_gather = false;
    int k_nearest_photon_count = 50;
    double k_nearest_max_radius = 18.0;
    double k_nearest_radius_growth = 1.5;
    bool k_nearest_require_full_count = true;

    void clear() {
        photons.clear();
        grid.clear();
        grid_built = false;
        effective_cell_size = 0.0;
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

        grid_built = false;
    }

    int size() const {
        return int(photons.size());
    }

    void build_grid() {
        grid.clear();

        if (photons.empty()) {
            grid_built = true;
            return;
        }

        effective_cell_size = (grid_cell_size > 0.0) ? grid_cell_size : gather_radius;

        if (effective_cell_size <= 0.0) {
            grid_built = false;
            return;
        }

        for (int i = 0; i < int(photons.size()); ++i) {
            photon_grid_key key = point_to_key(photons[i].position);
            grid[key].push_back(i);
        }

        grid_built = true;

        std::clog << "Built photon grid: "
                  << photons.size() << " photons, "
                  << grid.size() << " occupied cells, "
                  << "cell size = " << effective_cell_size << "\n";
    }

    SpectralEnergy estimate_caustic(const hit_record& rec, const ray& r_in) const {
        if (photons.empty() || gather_radius <= 0.0 || spectral_radius_nm <= 0.0)
            return SpectralEnergy(0.0);

        if (!rec.mat->supports_photon_gather())
            return SpectralEnergy(0.0);

        double radius = gather_radius;
        SpectralEnergy flux(0.0);

        if (use_k_nearest_gather) {
            if (!gather_k_nearest(rec, r_in, flux, radius))
                return SpectralEnergy(0.0);
        } else {
            if (use_adaptive_gather) {
                if (!choose_adaptive_radius(rec, radius))
                    return SpectralEnergy(0.0);
            }

            if (has_valid_grid()) {
                accumulate_caustic_grid(rec, r_in, radius, flux);
            } else {
                accumulate_caustic_bruteforce(rec, r_in, radius, flux);
            }
        }

        SpectralEnergy brdf = rec.mat->photon_gather_brdf(r_in, rec, r_in.wavelengths());

        // Integral of kernel w(r)=1-r/R over a disk is πR²/3.
        const double kernel_area = pi * radius * radius / 3.0;

        return caustic_strength * (brdf * (flux / kernel_area));
    }

    color estimate_caustic_rgb(const hit_record& rec, const ray& r_in) const {
        if (photons.empty() || gather_radius <= 0.0)
            return color(0, 0, 0);

        if (!rec.mat->supports_photon_gather())
            return color(0, 0, 0);

        double radius = gather_radius;
        std::vector<photon_candidate> candidates;

        if (use_k_nearest_gather) {
            if (!select_k_nearest_candidates(rec, candidates, radius))
                return color(0, 0, 0);
        } else {
            if (use_adaptive_gather) {
                if (!choose_adaptive_radius(rec, radius))
                    return color(0, 0, 0);
            }

            collect_photon_candidates(rec, radius, candidates);

            if (candidates.empty())
                return color(0, 0, 0);
        }

        color rgb_flux(0, 0, 0);

        for (const auto& candidate : candidates) {
            accumulate_photon_rgb(candidate, rec, r_in, radius, rgb_flux);
        }

        // Integral of kernel w(r)=1-r/R over a disk is πR²/3.
        const double kernel_area = pi * radius * radius / 3.0;
        color result = rgb_caustic_strength * (rgb_flux / kernel_area);

        return nonnegative_color(result);
    }

    void write_ply(const std::string& filename, double color_scale = 1.0) const {
        std::ofstream out(filename);

        if (!out) {
            std::clog << "Failed to write photon PLY: " << filename << "\n";
            return;
        }

        out << "ply\n";
        out << "format ascii 1.0\n";
        out << "element vertex " << photons.size() << "\n";
        out << "property float x\n";
        out << "property float y\n";
        out << "property float z\n";
        out << "property uchar red\n";
        out << "property uchar green\n";
        out << "property uchar blue\n";
        out << "end_header\n";

        for (const auto& photon : photons) {
            vec3 rgb = spectral_to_rgb(photon.power * color_scale, photon.wavelengths);

            auto to_byte = [](double x) -> int {
                x = std::fmax(0.0, x);

                // Gamma-like display compression for debug visibility.
                x = std::sqrt(x);

                if (x < 0.0) x = 0.0;
                if (x > 0.999) x = 0.999;

                return int(256.0 * x);
            };

            int r = to_byte(rgb.x());
            int g = to_byte(rgb.y());
            int b = to_byte(rgb.z());

            out << photon.position.x() << ' '
                << photon.position.y() << ' '
                << photon.position.z() << ' '
                << r << ' '
                << g << ' '
                << b << '\n';
        }

        std::clog << "Wrote photon point cloud: " << filename
                  << " (" << photons.size() << " photons)\n";
    }

    void write_debug_outputs() const {
        if (debug_write_ply) {
            write_ply(debug_ply_filename, debug_ply_color_scale);
        }
    }

  private:
    std::vector<spectral_photon> photons;

    std::unordered_map<
        photon_grid_key,
        std::vector<int>,
        photon_grid_key_hash
    > grid;

    bool grid_built = false;
    double effective_cell_size = 0.0;

    photon_grid_key point_to_key(const point3& p) const {
        return photon_grid_key{
            int(std::floor(p.x() / effective_cell_size)),
            int(std::floor(p.y() / effective_cell_size)),
            int(std::floor(p.z() / effective_cell_size))
        };
    }

    bool has_valid_grid() const {
        return use_spatial_grid && grid_built && effective_cell_size > 0.0;
    }

    color nonnegative_color(const color& c) const {
        return color(
            std::fmax(0.0, c.x()),
            std::fmax(0.0, c.y()),
            std::fmax(0.0, c.z())
        );
    }

    bool photon_spatially_valid(
        const spectral_photon& photon,
        const hit_record& rec,
        double radius
    ) const {
        vec3 delta = photon.position - rec.p;
        double dist2 = delta.length_squared();
        double radius2 = radius * radius;

        if (dist2 > radius2)
            return false;

        if (dot(photon.normal, rec.normal) <= 0.1)
            return false;

        return true;
    }

    bool choose_adaptive_radius(const hit_record& rec, double& chosen_radius) const {
        double radius = gather_radius;
        double max_radius = std::max(max_gather_radius, gather_radius);

        while (radius <= max_radius) {
            int count = count_photons_in_radius(rec, radius, min_photons_per_gather);

            if (count >= min_photons_per_gather) {
                chosen_radius = radius;
                return true;
            }

            if (radius >= max_radius)
                break;

            radius = std::min(radius * adaptive_radius_growth, max_radius);
        }

        return false;
    }

    int count_photons_in_radius(const hit_record& rec, double radius, int stop_at) const {
        if (has_valid_grid())
            return count_photons_grid(rec, radius, stop_at);

        return count_photons_bruteforce(rec, radius, stop_at);
    }

    int count_photons_bruteforce(const hit_record& rec, double radius, int stop_at) const {
        int count = 0;

        for (const auto& photon : photons) {
            if (photon_spatially_valid(photon, rec, radius)) {
                count++;

                if (stop_at > 0 && count >= stop_at)
                    return count;
            }
        }

        return count;
    }

    int count_photons_grid(const hit_record& rec, double radius, int stop_at) const {
        int count = 0;

        photon_grid_key center_key = point_to_key(rec.p);
        int cell_radius = int(std::ceil(radius / effective_cell_size));

        for (int dz = -cell_radius; dz <= cell_radius; ++dz) {
            for (int dy = -cell_radius; dy <= cell_radius; ++dy) {
                for (int dx = -cell_radius; dx <= cell_radius; ++dx) {
                    photon_grid_key key{
                        center_key.x + dx,
                        center_key.y + dy,
                        center_key.z + dz
                    };

                    auto it = grid.find(key);

                    if (it == grid.end())
                        continue;

                    for (int photon_index : it->second) {
                        if (photon_spatially_valid(photons[photon_index], rec, radius)) {
                            count++;

                            if (stop_at > 0 && count >= stop_at)
                                return count;
                        }
                    }
                }
            }
        }

        return count;
    }

    void accumulate_photon(
        const spectral_photon& photon,
        const hit_record& rec,
        const ray& r_in,
        double radius,
        SpectralEnergy& flux
    ) const {
        vec3 delta = photon.position - rec.p;
        double dist2 = delta.length_squared();
        double radius2 = radius * radius;

        if (dist2 > radius2)
            return;

        if (dot(photon.normal, rec.normal) <= 0.1)
            return;

        double dist = std::sqrt(dist2);
        double spatial_weight = 1.0 - dist / radius;

        if (spatial_weight <= 0.0)
            return;

        const Wavelengths& query_wl = r_in.wavelengths();

        for (int q = 0; q < WL_PER_RAY; ++q) {
            if (query_wl.hero_only && q != 0)
                continue;

            for (int p = 0; p < WL_PER_RAY; ++p) {
                if (photon.wavelengths.hero_only && p != 0)
                    continue;

                if (photon.power.energy[p] <= 0.0)
                    continue;

                double distance_nm =
                    std::fabs(query_wl.lambda[q] - photon.wavelengths.lambda[p]);

                if (distance_nm >= spectral_radius_nm)
                    continue;

                double spectral_weight = 1.0 - distance_nm / spectral_radius_nm;
                double weight = spatial_weight * spectral_weight;

                flux.energy[q] += weight * photon.power.energy[p];
            }
        }
    }

    void accumulate_photon_rgb(
        const photon_candidate& candidate,
        const hit_record& rec,
        const ray& r_in,
        double radius,
        color& rgb_flux
    ) const {
        const spectral_photon& photon = photons[candidate.photon_index];

        vec3 delta = photon.position - rec.p;
        double dist2 = delta.length_squared();
        double radius2 = radius * radius;

        if (dist2 > radius2)
            return;

        if (dot(photon.normal, rec.normal) <= 0.1)
            return;

        double dist = std::sqrt(dist2);
        double spatial_weight = 1.0 - dist / radius;

        if (spatial_weight <= 0.0)
            return;

        // Use the photon wavelengths, not the camera ray wavelengths.
        // This makes the caustic color come from the photon cloud itself.
        SpectralEnergy brdf = rec.mat->photon_gather_brdf(r_in, rec, photon.wavelengths);
        SpectralEnergy reflected = photon.power * brdf * spatial_weight;

        rgb_flux += spectral_to_rgb(reflected, photon.wavelengths);
    }

    bool gather_k_nearest(
        const hit_record& rec,
        const ray& r_in,
        SpectralEnergy& flux,
        double& estimate_radius
    ) const {
        std::vector<photon_candidate> candidates;

        if (!select_k_nearest_candidates(rec, candidates, estimate_radius))
            return false;

        for (const auto& candidate : candidates) {
            accumulate_photon(photons[candidate.photon_index], rec, r_in, estimate_radius, flux);
        }

        return true;
    }

    bool select_k_nearest_candidates(
        const hit_record& rec,
        std::vector<photon_candidate>& candidates,
        double& estimate_radius
    ) const {
        if (k_nearest_photon_count <= 0)
            return false;

        double radius = gather_radius;
        double max_radius = std::max(k_nearest_max_radius, gather_radius);

        while (radius <= max_radius) {
            candidates.clear();
            collect_photon_candidates(rec, radius, candidates);

            if (int(candidates.size()) >= k_nearest_photon_count)
                break;

            if (radius >= max_radius)
                break;

            radius = std::min(radius * k_nearest_radius_growth, max_radius);
        }

        if (candidates.empty())
            return false;

        if (k_nearest_require_full_count &&
            int(candidates.size()) < k_nearest_photon_count)
            return false;

        int used_count = std::min(k_nearest_photon_count, int(candidates.size()));

        if (used_count <= 0)
            return false;

        auto by_distance = [](const photon_candidate& a, const photon_candidate& b) {
            return a.dist2 < b.dist2;
        };

        if (int(candidates.size()) > used_count) {
            std::nth_element(
                candidates.begin(),
                candidates.begin() + used_count - 1,
                candidates.end(),
                by_distance
            );

            candidates.resize(used_count);
        }

        double max_dist2 = 0.0;

        for (const auto& candidate : candidates) {
            max_dist2 = std::max(max_dist2, candidate.dist2);
        }

        estimate_radius = std::sqrt(std::max(max_dist2, 1e-12));

        // Avoid placing the farthest selected photon exactly on a zero-weight boundary.
        estimate_radius *= 1.0001;

        return true;
    }

    void collect_photon_candidates(
        const hit_record& rec,
        double radius,
        std::vector<photon_candidate>& candidates
    ) const {
        if (has_valid_grid()) {
            collect_photon_candidates_grid(rec, radius, candidates);
        } else {
            collect_photon_candidates_bruteforce(rec, radius, candidates);
        }
    }

    void accumulate_caustic_bruteforce(
        const hit_record& rec,
        const ray& r_in,
        double radius,
        SpectralEnergy& flux
    ) const {
        for (const auto& photon : photons) {
            accumulate_photon(photon, rec, r_in, radius, flux);
        }
    }

    void collect_photon_candidates_bruteforce(
        const hit_record& rec,
        double radius,
        std::vector<photon_candidate>& candidates
    ) const {
        for (int i = 0; i < int(photons.size()); ++i) {
            push_candidate_if_valid(i, rec, radius, candidates);
        }
    }

    void collect_photon_candidates_grid(
        const hit_record& rec,
        double radius,
        std::vector<photon_candidate>& candidates
    ) const {
        photon_grid_key center_key = point_to_key(rec.p);
        int cell_radius = int(std::ceil(radius / effective_cell_size));

        for (int dz = -cell_radius; dz <= cell_radius; ++dz) {
            for (int dy = -cell_radius; dy <= cell_radius; ++dy) {
                for (int dx = -cell_radius; dx <= cell_radius; ++dx) {
                    photon_grid_key key{
                        center_key.x + dx,
                        center_key.y + dy,
                        center_key.z + dz
                    };

                    auto it = grid.find(key);

                    if (it == grid.end())
                        continue;

                    for (int photon_index : it->second) {
                        push_candidate_if_valid(photon_index, rec, radius, candidates);
                    }
                }
            }
        }
    }

    void push_candidate_if_valid(
        int photon_index,
        const hit_record& rec,
        double radius,
        std::vector<photon_candidate>& candidates
    ) const {
        const spectral_photon& photon = photons[photon_index];

        vec3 delta = photon.position - rec.p;
        double dist2 = delta.length_squared();
        double radius2 = radius * radius;

        if (dist2 > radius2)
            return;

        if (dot(photon.normal, rec.normal) <= 0.1)
            return;

        candidates.push_back({photon_index, dist2});
    }

    void accumulate_caustic_grid(
        const hit_record& rec,
        const ray& r_in,
        double radius,
        SpectralEnergy& flux
    ) const {
        photon_grid_key center_key = point_to_key(rec.p);
        int cell_radius = int(std::ceil(radius / effective_cell_size));

        for (int dz = -cell_radius; dz <= cell_radius; ++dz) {
            for (int dy = -cell_radius; dy <= cell_radius; ++dy) {
                for (int dx = -cell_radius; dx <= cell_radius; ++dx) {
                    photon_grid_key key{
                        center_key.x + dx,
                        center_key.y + dy,
                        center_key.z + dz
                    };

                    auto it = grid.find(key);

                    if (it == grid.end())
                        continue;

                    for (int photon_index : it->second) {
                        accumulate_photon(photons[photon_index], rec, r_in, radius, flux);
                    }
                }
            }
        }
    }
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

        photon_emission_sample emission =
            emitter.sample_photon(
                wavelengths,
                map.photon_count,
                emitters.size(),
                emitters.has_target_sphere(),
                emitters.target_center(),
                emitters.target_radius()
            );

        ray photon_ray = emission.photon_ray;
        SpectralEnergy power = emission.power;

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
    map.build_grid();
    map.write_debug_outputs();
}

#endif