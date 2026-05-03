#ifndef RAY_H
#define RAY_H

#include "vec3.h"
#include "spectral.h"

class ray {
  public:
    ray() {}

    ray(const point3& origin, const vec3& direction, double time)
      : orig(origin), dir(direction), tm(time) {}

    ray(const point3& origin, const vec3& direction)
      : ray(origin, direction, 0) {}

    ray(const point3& origin, const vec3& direction, double time, const Wavelengths& wavelengths)
      : orig(origin), dir(direction), tm(time), wl(wavelengths) {}

    const point3& origin() const  { return orig; }
    const vec3& direction() const { return dir; }

    double time() const { return tm; }
    const Wavelengths& wavelengths() const { return wl; }

    point3 at(double t) const {
        return orig + t*dir;
    }

    bool is_decoupled = false;

  private:
    point3 orig;
    vec3 dir;
    double tm;
    Wavelengths wl;
};

#endif