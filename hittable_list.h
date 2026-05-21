#ifndef HITTABLE_LIST_H
#define HITTABLE_LIST_H

#include "aabb.h"
#include "hittable.h"

#include <vector>

class hittable_list : public hittable {
  public:
    std::vector<shared_ptr<hittable>> objects;

    hittable_list() {}
    hittable_list(shared_ptr<hittable> object) { add(object); }

    void clear() { objects.clear(); }

    void add(shared_ptr<hittable> object) {
        objects.push_back(object);
        bbox = aabb(bbox, object->bounding_box());
    }

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        hit_record temp_rec;
        bool hit_anything = false;
        auto closest_so_far = ray_t.max;

        for (const auto& object : objects) {
            if (object->hit(r, interval(ray_t.min, closest_so_far), temp_rec)) {
                hit_anything = true;
                closest_so_far = temp_rec.t;
                rec = temp_rec;
            }
        }

        return hit_anything;
    }
    
    aabb bounding_box() const override { return bbox; }

    double pdf_value(const point3& origin, const vec3& direction, double lambda) const override {
        auto weight = 1.0 / objects.size();
        auto sum = 0.0;

        for (const auto& object : objects)
            sum += weight * object->pdf_value(origin, direction, lambda);

        return sum;
    }

    vec3 random(const point3& origin, double lambda) const override {
        auto int_size = int(objects.size());
        return objects[random_int(0, int_size-1)]->random(origin, lambda);
    }

    double spectral_pdf_weight(double lambda) const override {
        double sum = 0.0;

        for (const auto& object : objects)
            sum += object->spectral_pdf_weight(lambda);

        return sum;
    }

    double spectral_pdf_value(const point3& origin, const vec3& direction, double lambda) const override {
        double total_weight = spectral_pdf_weight(lambda);

        if (total_weight <= 0.0)
            return pdf_value(origin, direction, lambda);

        double sum = 0.0;

        for (const auto& object : objects) {
            double w = object->spectral_pdf_weight(lambda);

            if (w <= 0.0)
                continue;

            sum += (w / total_weight) * object->spectral_pdf_value(origin, direction, lambda);
        }

        return sum;
    }

    vec3 spectral_random(const point3& origin, double lambda) const override {
        double total_weight = spectral_pdf_weight(lambda);

        if (total_weight <= 0.0)
            return random(origin, lambda);

        double target = random_double() * total_weight;
        double accum = 0.0;

        for (const auto& object : objects) {
            double w = object->spectral_pdf_weight(lambda);

            if (w <= 0.0)
                continue;

            accum += w;

            if (target <= accum)
                return object->spectral_random(origin, lambda);
        }

        return objects.back()->spectral_random(origin, lambda);
    }

  private:
    aabb bbox;
};

#endif