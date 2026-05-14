#ifndef PDF_H
#define PDF_H

#include "hittable_list.h"
#include "onb.h"

class pdf {
  public:
    virtual ~pdf() {}

    virtual double value(const vec3& direction, double lambda) const = 0;
    virtual vec3 generate(double lambda) const = 0;
};

class sphere_pdf : public pdf {
  public:
    sphere_pdf() {}

    double value(const vec3& direction, double lambda) const override {
        return 1/ (4 * pi);
    }

    vec3 generate(double lambda) const override {
        return random_unit_vector();
    }
};

class cosine_pdf : public pdf {
  public:
    cosine_pdf(const vec3& w) : uvw(w) {}

    double value(const vec3& direction, double lambda) const override {
        auto cosine_theta = dot(unit_vector(direction), uvw.w());
        return std::fmax(0, cosine_theta/pi);
    }

    vec3 generate(double lambda) const override {
        return uvw.transform(random_cosine_direction());
    }

  private:
    onb uvw;
};

class hittable_pdf : public pdf {
  public:
    hittable_pdf(const hittable& objects, const point3& origin)
      : objects(objects), origin(origin)
    {}

    double value(const vec3& direction, double lambda) const override {
        return objects.pdf_value(origin, direction, lambda);
    }

    vec3 generate(double lambda) const override {
        return objects.random(origin, lambda);
    }

  private:
    const hittable& objects;
    point3 origin;
};

class spectral_light_pdf : public pdf {
  public:
    spectral_light_pdf(const hittable& objects, const point3& origin)
      : objects(objects), origin(origin)
    {}

    double value(const vec3& direction, double lambda) const override {
        return objects.spectral_pdf_value(origin, direction, lambda);
    }

    vec3 generate(double lambda) const override {
        return objects.spectral_random(origin, lambda);
    }

  private:
    const hittable& objects;
    point3 origin;
};

class mixture_pdf : public pdf {
  public:
    mixture_pdf(shared_ptr<pdf> p0, shared_ptr<pdf> p1) {
        p[0] = p0;
        p[1] = p1;
    }

    double value(const vec3& direction, double lambda) const override {
        return 0.5 * p[0]->value(direction, lambda) + 0.5 * p[1]->value(direction, lambda);
    }

    vec3 generate(double lambda) const override {
        if (random_double() < 0.5)
            return p[0]->generate(lambda);
        else
            return p[1]->generate(lambda);
    }
    vec3 generate(const Wavelengths& wl) const {
        return generate(wl.lambda[0]);
    }

    double value_joint(const vec3& direction, const Wavelengths& wl) const {
        double sum = 0.0;
        int active_channels = 0;

        for (int k = 0; k < WL_PER_RAY; ++k) {
            if (wl.hero_only && k != 0)
                continue;

            sum += value(direction, wl.lambda[k]);
            active_channels++;
        }

        if (active_channels == 0)
            return 0.0;

        return sum / active_channels;
    }

  private:
    shared_ptr<pdf> p[2];
};

#endif