#ifndef MATERIAL_H
#define MATERIAL_H

#include "hittable.h"
#include "pdf.h"
#include "texture.h"

class scatter_record {
  public:
    SpectralEnergy attenuation;
    shared_ptr<pdf> pdf_ptr;
    bool skip_pdf;
    ray skip_pdf_ray;
};

class material {
  public:
    virtual ~material() = default;

    virtual SpectralEnergy emitted(const ray& r_in, const hit_record& rec, double u, double v, const point3& p) const {
        return SpectralEnergy(0.0);
    }

    virtual double scattering_pdf(const ray& r_in, const hit_record& rec, const ray& scattered, double lambda) const {
        return 0;
    }

    virtual bool scatter(const ray& r_in, const hit_record& rec, scatter_record& srec) const {
        return false;
    }
};

class lambertian : public material {
  public:
    lambertian(const color& albedo) : tex(make_shared<solid_color>(albedo)) {}
    lambertian(shared_ptr<texture> tex) : tex(tex) {}

    bool scatter(const ray& r_in, const hit_record& rec, scatter_record& srec) const override {
        color albedo_rgb = tex->value(rec.u, rec.v, rec.p);

        srec.attenuation = rgb_reflectance_to_spectral_energy(albedo_rgb, r_in.wavelengths());
        srec.pdf_ptr = make_shared<cosine_pdf>(rec.normal);
        srec.skip_pdf = false;

        return true;
    }

    double scattering_pdf(const ray& r_in, const hit_record& rec, const ray& scattered, double lambda) const override {
        auto cos_theta = dot(rec.normal, unit_vector(scattered.direction()));
        return cos_theta < 0 ? 0 : cos_theta/pi;
    }

  private:
    shared_ptr<texture> tex;
};

class metal : public material {
  public:
    metal(const color& albedo, double fuzz) : albedo(albedo), fuzz(fuzz < 1 ? fuzz : 1) {}

    bool scatter(const ray& r_in, const hit_record& rec, scatter_record& srec) const override {
        vec3 reflected = reflect(r_in.direction(), rec.normal);
        reflected = unit_vector(reflected) + (fuzz * random_unit_vector());

        srec.attenuation = rgb_reflectance_to_spectral_energy(albedo, r_in.wavelengths());
        srec.pdf_ptr = nullptr;
        srec.skip_pdf = true;
        srec.skip_pdf_ray = ray(rec.p, reflected, r_in.time(), r_in.wavelengths());

        return true;
    }

  private:
    color albedo;
    double fuzz;
};

class dielectric : public material {
  public:
    dielectric(double base_ior, double dispersion_strength)
      : A(base_ior), B(dispersion_strength) {}

    bool scatter(const ray& r_in, const hit_record& rec, scatter_record& srec) const override {
        srec.pdf_ptr = nullptr;
        srec.skip_pdf = true;

        const Wavelengths& wl = r_in.wavelengths();
        const double hero_lambda = wl.lambda[0];

        vec3 unit_direction = unit_vector(r_in.direction());
        double cos_theta = std::fmin(dot(-unit_direction, rec.normal), 1.0);
        double sin_theta = std::sqrt(1.0 - cos_theta * cos_theta);

        double hero_ior = ior_at(hero_lambda);
        double hero_eta = rec.front_face ? (1.0 / hero_ior) : hero_ior;

        bool hero_cannot_refract = hero_eta * sin_theta > 1.0;
        double hero_reflect_pdf = hero_cannot_refract
                                ? 1.0
                                : reflectance(cos_theta, hero_eta);

        bool choose_reflect = hero_cannot_refract || hero_reflect_pdf > random_double();

        if (choose_reflect) {
            vec3 direction = reflect(unit_direction, rec.normal);

            // Reflection direction is aligned for all wavelengths.
            // Use spectral MIS over wavelength-dependent Fresnel probabilities.
            double pdfs[WL_PER_RAY];
            double sum_pdf = 0.0;

            for (int i = 0; i < WL_PER_RAY; ++i) {
                double ior = ior_at(wl.lambda[i]);
                double eta = rec.front_face ? (1.0 / ior) : ior;

                bool cannot_refract = eta * sin_theta > 1.0;
                pdfs[i] = cannot_refract ? 1.0 : reflectance(cos_theta, eta);

                sum_pdf += pdfs[i];
            }

            for (int i = 0; i < WL_PER_RAY; ++i) {
                srec.attenuation.energy[i] =
                    (sum_pdf > 0.0) ? (WL_PER_RAY * pdfs[i] / sum_pdf) : 0.0;
            }

            srec.skip_pdf_ray = ray(rec.p, direction, r_in.time(), wl);
            return true;
        }

        // Refraction is wavelength-dependent when B != 0.
        // The hero wavelength chooses the physical refracted direction.
        vec3 direction = refract(unit_direction, rec.normal, hero_eta);
        srec.skip_pdf_ray = ray(rec.p, direction, r_in.time(), wl);

        if (std::fabs(B) > 1e-12) {
            Wavelengths next_wl = collapse_to_hero_only(wl);

            srec.attenuation = hero_only_attenuation(wl);
            srec.skip_pdf_ray = ray(rec.p, direction, r_in.time(), next_wl);

            return true;
        }

        // Non-dispersive refraction:
        // direction is aligned, so all channels may be retained.
        double pdfs[WL_PER_RAY];
        double sum_pdf = 0.0;

        for (int i = 0; i < WL_PER_RAY; ++i) {
            double ior = ior_at(wl.lambda[i]);
            double eta = rec.front_face ? (1.0 / ior) : ior;

            bool cannot_refract = eta * sin_theta > 1.0;
            pdfs[i] = cannot_refract ? 0.0 : (1.0 - reflectance(cos_theta, eta));

            sum_pdf += pdfs[i];
        }

        for (int i = 0; i < WL_PER_RAY; ++i) {
            srec.attenuation.energy[i] =
                (sum_pdf > 0.0) ? (WL_PER_RAY * pdfs[i] / sum_pdf) : 0.0;
        }

        return true;
    }

  private:
    double A;
    double B;

    double ior_at(double lambda_nm) const {
        double lambda_um = lambda_nm * 0.001;
        return A + (B / (lambda_um * lambda_um));
    }

    static double reflectance(double cosine, double refraction_ratio) {
        auto r0 = (1 - refraction_ratio) / (1 + refraction_ratio);
        r0 = r0 * r0;
        return r0 + (1 - r0) * std::pow((1 - cosine), 5);
    }
};

class diffuse_light : public material {
  public:
    diffuse_light(const SpectralEnergy& emit)
      : emit_color(emit), emit_rgb(0,0,0), use_rgb(false) {}

    diffuse_light(const color& emit)
      : emit_color(0.0), emit_rgb(emit), use_rgb(true) {}

    diffuse_light(const double& emit)
      : emit_color(emit), emit_rgb(0,0,0), use_rgb(false) {}

    SpectralEnergy emitted(const ray& r_in, const hit_record& rec, double u, double v, const point3& p) const override {
        if (!rec.front_face)
            return SpectralEnergy(0.0);

        if (use_rgb)
            return rgb_emission_to_spectral_energy(emit_rgb, r_in.wavelengths());

        return emit_color;
    }

  private:
    SpectralEnergy emit_color;
    color emit_rgb;
    bool use_rgb;
};

// class isotropic : public material {
//   public:
//     isotropic(const color& albedo) : tex(make_shared<solid_color>(albedo)) {}
//     isotropic(shared_ptr<texture> tex) : tex(tex) {}

//     bool scatter(
//         const ray& r_in, const hit_record& rec, color& attenuation, ray& scattered, double& pdf
//     ) const override {
//         scattered = ray(rec.p, random_unit_vector(), r_in.time(), r_in.wavelengths());
//         attenuation = tex->value(rec.u, rec.v, rec.p);
//         pdf = 1 / (4 * pi);
//         return true;
//     }
//     double scattering_pdf(const ray& r_in, const hit_record& rec, const ray& scattered)
//     const override {
//         return 1 / (4 * pi);
//     }

//   private:
//     shared_ptr<texture> tex;
// };

#endif