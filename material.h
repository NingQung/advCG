#ifndef MATERIAL_H
#define MATERIAL_H

#include "hittable.h"
#include "pdf.h"
#include "texture.h"

class scatter_record {
  public:
    color attenuation;
    shared_ptr<pdf> pdf_ptr;
    bool skip_pdf;
    ray skip_pdf_ray;
};

class material {
  public:
    virtual ~material() = default;

    virtual SpectralEnergy emitted(double u, double v, const point3& p) const {
        return SpectralEnergy(0.0, 0.0, 0.0, 0.0);
    }
    virtual color emitted(
        const ray& r_in, const hit_record& rec, double u, double v, const point3& p
    ) const {
        return color(0,0,0);
    }

    virtual bool scatter(
        const ray& r_in, const hit_record& rec, SpectralEnergy& attenuation, scatter_record& srec
    ) const {
        return false;
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

    bool scatter(
        const ray& r_in, const hit_record& rec, SpectralEnergy& attenuation, scatter_record& srec
    ) const override {
        onb uvw(rec.normal);
        auto scatter_direction = uvw.transform(random_cosine_direction());

        // Create scattered ray carrying the same wavelengths
        //scattered = ray(rec.p, unit_vector(scatter_direction), r_in.time(), r_in.wavelengths());
        
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
            attenuation.energy[i] = rgb2spec_eval_fast(coeffs, r_in.wavelengths().lambda[i]);
        }

        srec.attenuation = tex->value(rec.u, rec.v, rec.p);
        srec.pdf_ptr = make_shared<cosine_pdf>(rec.normal);
        srec.skip_pdf = false;
        return true;
    }

    double scattering_pdf(const ray& r_in, const hit_record& rec, const ray& scattered) const override {
        auto cos_theta = dot(rec.normal, unit_vector(scattered.direction()));
        return cos_theta < 0 ? 0 : cos_theta/pi;
    }
  
  private:
    color albedo;
    shared_ptr<texture> tex;
};

class metal : public material {
  public:
    metal(const color& albedo, double fuzz) : albedo(albedo), fuzz(fuzz < 1 ? fuzz : 1) {}

    bool scatter(const ray& r_in, const hit_record& rec, SpectralEnergy& attenuation, scatter_record& srec) const override {
        vec3 reflected = reflect(r_in.direction(), rec.normal);
        reflected = unit_vector(reflected) + (fuzz * random_unit_vector());
        srec.attenuation = albedo;
        srec.pdf_ptr = nullptr;
        srec.skip_pdf = true;
        srec.skip_pdf_ray = ray(rec.p, reflected, r_in.time());

        return true;
    }
  private:
    shared_ptr<texture> tex;
    color albedo;
    double fuzz;
};


class diffuse_light : public material {
  public:
    diffuse_light(const SpectralEnergy& emit) : emit_color(emit) {}
    diffuse_light(const color& emit) : emit_color(emit.x(), emit.y(), emit.z(), emit.x()) {}
    
    SpectralEnergy emitted(double u, double v, const point3& p) const override {
        return emit_color;
    }
    
    bool scatter(const ray& r_in, const hit_record& rec, SpectralEnergy& attenuation, scatter_record& srec) const override {
        return false;
    }

  private:
    SpectralEnergy emit_color;
};

class dielectric : public material {
  public:
    dielectric(double refraction_index) : refraction_index(refraction_index) {}

    bool scatter(const ray& r_in, const hit_record& rec, SpectralEnergy& attenuation, scatter_record& srec) const override {
        srec.attenuation = color(1.0, 1.0, 1.0);
        srec.pdf_ptr = nullptr;
        srec.skip_pdf = true;
        double ri = rec.front_face ? (1.0/refraction_index) : refraction_index;

        vec3 unit_direction = unit_vector(r_in.direction());
        double cos_theta = std::fmin(dot(-unit_direction, rec.normal), 1.0);
        double sin_theta = std::sqrt(1.0 - cos_theta*cos_theta);

        bool cannot_refract = ri * sin_theta > 1.0;
        vec3 direction;

        if (cannot_refract || reflectance(cos_theta, ri) > random_double())
            direction = reflect(unit_direction, rec.normal);
        else
            direction = refract(unit_direction, rec.normal, ri);

        srec.skip_pdf_ray = ray(rec.p, direction, r_in.time());
        return true;
    }

  private:
    // Refractive index in vacuum or air, or the ratio of the material's refractive index over
    // the refractive index of the enclosing media
    double refraction_index;

    static double reflectance(double cosine, double refraction_index) {
        // Use Schlick's approximation for reflectance.
        auto r0 = (1 - refraction_index) / (1 + refraction_index);
        r0 = r0*r0;
        return r0 + (1-r0)*std::pow((1 - cosine),5);
    }
};
  
class isotropic : public material {
  public:
    isotropic(const color& albedo) : tex(make_shared<solid_color>(albedo)) {}
    isotropic(shared_ptr<texture> tex) : tex(tex) {}

    bool scatter(const ray& r_in, const hit_record& rec, scatter_record& srec) const override {
        srec.attenuation = tex->value(rec.u, rec.v, rec.p);
        srec.pdf_ptr = make_shared<sphere_pdf>();
        srec.skip_pdf = false;
        return true;
    }
    double scattering_pdf(const ray& r_in, const hit_record& rec, const ray& scattered)
    const override {
        return 1 / (4 * pi);
    }

  private:
    shared_ptr<texture> tex;
};


#endif