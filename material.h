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
        return SpectralEnergy(0.0, 0.0, 0.0, 0.0);
    }

    virtual double scattering_pdf(const ray& r_in, const hit_record& rec, const ray& scattered) const {
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
        // 1. Get RGB color from texture
        color albedo_rgb = tex->value(rec.u, rec.v, rec.p);
        float rgb[3] = { (float)albedo_rgb.x(), (float)albedo_rgb.y(), (float)albedo_rgb.z() };
        
        // 2. Clamp RGB to [0, 1] for safety
        for(int i=0; i<3; i++) {
            if(rgb[i] < 0.0f) rgb[i] = 0.0f;
            if(rgb[i] > 1.0f) rgb[i] = 1.0f;
        }

        // 3. Fetch spectral coefficients using rgb2spec
        float coeffs[3];
        rgb2spec_fetch(g_rgb2spec_model, rgb, coeffs);

        // 4. Evaluate reflectance for each carried wavelength
        for(int i=0; i<4; i++) {
            srec.attenuation.energy[i] = rgb2spec_eval_fast(coeffs, r_in.wavelengths().lambda[i]);
        }

        srec.pdf_ptr = make_shared<cosine_pdf>(rec.normal);
        srec.skip_pdf = false;
        return true;
    }

    double scattering_pdf(const ray& r_in, const hit_record& rec, const ray& scattered) const override {
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
        float rgb[3] = { (float)albedo.x(), (float)albedo.y(), (float)albedo.z() };
        
        // 2. Clamp RGB to [0, 1] for safety
        for(int i=0; i<3; i++) {
            if(rgb[i] < 0.0f) rgb[i] = 0.0f;
            if(rgb[i] > 1.0f) rgb[i] = 1.0f;
        }

        // 3. Fetch spectral coefficients using rgb2spec
        float coeffs[3];
        rgb2spec_fetch(g_rgb2spec_model, rgb, coeffs);

        // 4. Evaluate reflectance for each carried wavelength
        for(int i=0; i<4; i++) {
            srec.attenuation.energy[i] = rgb2spec_eval_fast(coeffs, r_in.wavelengths().lambda[i]);
        }
        srec.pdf_ptr = nullptr;
        srec.skip_pdf = true;
        srec.skip_pdf_ray = ray(rec.p, reflected, r_in.time());
        return true;
    }

  private:
    color albedo;
    double fuzz;
};

class dielectric : public material {
  public:
    // We use Cauchy's equation for simple dispersion:
    // IOR(lambda) = A + B / (lambda^2)
    // A: base IOR (e.g., 1.5 for glass)
    // B: dispersion strength (e.g., 0.005 to 0.02)
    dielectric(double base_ior, double dispersion_strength) 
      : A(base_ior), B(dispersion_strength) {}

    bool scatter(const ray& r_in, const hit_record& rec, scatter_record& srec) const override {
        // Glass absorbs very little energy, so attenuation remains 1.0 for all channels
        srec.attenuation = SpectralEnergy(1.0, 1.0, 1.0, 1.0); 
        srec.pdf_ptr = nullptr;
        srec.skip_pdf = true;

        // 1. Get the Hero Wavelength (assume lambda[0] is our hero)
        double hero_lambda = r_in.wavelengths().lambda[0];
        
        // 2. Calculate dynamic IOR using Cauchy's Equation
        // Convert lambda from nanometers to micrometers to fit typical Cauchy coefficients
        double lambda_um = hero_lambda * 0.001;
        double current_ior = A + (B / (lambda_um * lambda_um));

        double ri = rec.front_face ? (1.0 / current_ior) : current_ior;

        vec3 unit_direction = unit_vector(r_in.direction());
        double cos_theta = std::fmin(dot(-unit_direction, rec.normal), 1.0);
        double sin_theta = std::sqrt(1.0 - cos_theta*cos_theta);

        bool cannot_refract = ri * sin_theta > 1.0;
        vec3 direction;

        if (cannot_refract || reflectance(cos_theta, ri) > random_double()) {
            direction = reflect(unit_direction, rec.normal);
        } else {
            direction = refract(unit_direction, rec.normal, ri);
        }

        // 3. Construct the scattered ray, carrying all original wavelengths along the hero's path
        srec.skip_pdf_ray = ray(rec.p, direction, r_in.time(), r_in.wavelengths());
        return true;
    }

  private:
    double A; // Base index of refraction
    double B; // Dispersion coefficient

    static double reflectance(double cosine, double refraction_index) {
        // Use Schlick's approximation for reflectance.
        auto r0 = (1 - refraction_index) / (1 + refraction_index);
        r0 = r0*r0;
        return r0 + (1-r0)*std::pow((1 - cosine),5);
    }
};

class diffuse_light : public material {
  public:
    diffuse_light(const SpectralEnergy& emit) : emit_color(emit) {}
    diffuse_light(const color& emit) : emit_color(emit.x(), emit.y(), emit.z(), emit.x()) {}
    
    SpectralEnergy emitted(const ray& r_in, const hit_record& rec, double u, double v, const point3& p) const override {
        if (!rec.front_face)
            return SpectralEnergy(0,0,0,0);
        return emit_color;
    }

  private:
    SpectralEnergy emit_color;
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