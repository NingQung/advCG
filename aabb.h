#ifndef AABB_H
#define AABB_H

#include "ray.h"
#include "interval.h"
#include "vec3.h"

// this is sth like empty: do some calc or anchor positioning but not visible
class aabb {
  public:
    interval x, y, z;

    aabb() {}

    aabb(const interval& ix, const interval& iy, const interval& iz)
        : x(ix), y(iy), z(iz) {}

    aabb(const point3& p0, const point3& p1) {
        // get right dir
        x = interval(std::fmin(p0.x(), p1.x()), std::fmax(p0.x(), p1.x()));
        y = interval(std::fmin(p0.y(), p1.y()), std::fmax(p0.y(), p1.y()));
        z = interval(std::fmin(p0.z(), p1.z()), std::fmax(p0.z(), p1.z()));
    }
    
    bool hit(const ray& r, interval ray_t) const {
        // for x[0], y[1], z[2]
        for (int a = 0; a < 3; a++) {
            auto inv_D = 1.0 / r.direction()[a];
            auto orig_a = r.origin()[a];

            auto t0 = (min_side(a).min - orig_a) * inv_D;
            auto t1 = (min_side(a).max - orig_a) * inv_D;

            if (inv_D < 0.0) std::swap(t0, t1);

            ray_t.min = std::fmax(ray_t.min, t0);
            ray_t.max = std::fmin(ray_t.max, t1);

            if (ray_t.max <= ray_t.min)
                return false;
        }
        return true;
    }

    const interval& min_side(int axis) const {
        if (axis == 0) return x;
        if (axis == 1) return y;
        return z;
    }
    
};

#endif