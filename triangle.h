#ifndef TRIANGLE_H
#define TRIANGLE_H

#include "rtweekend.h"
#include "hittable.h"

class triangle : public hittable {
  public:
    triangle(const point3& _Q, const vec3& _u, const vec3& _v, shared_ptr<material> m)
      : Q(_Q), u(_u), v(_v), mat(m)
    {
        auto n = cross(u, v);
        normal = unit_vector(n);
        D = dot(normal, Q);
        w = n / dot(n,n);
        
        area = n.length() / 2.0;

        set_bounding_box();
    }

    virtual void set_bounding_box() {
        // Compute the bounding box of the three vertices: Q, Q+u, Q+v
        point3 p0 = Q;
        point3 p1 = Q + u;
        point3 p2 = Q + v;

        double min_x = std::fmin(p0.x(), std::fmin(p1.x(), p2.x()));
        double min_y = std::fmin(p0.y(), std::fmin(p1.y(), p2.y()));
        double min_z = std::fmin(p0.z(), std::fmin(p1.z(), p2.z()));

        double max_x = std::fmax(p0.x(), std::fmax(p1.x(), p2.x()));
        double max_y = std::fmax(p0.y(), std::fmax(p1.y(), p2.y()));
        double max_z = std::fmax(p0.z(), std::fmax(p1.z(), p2.z()));

        bbox = aabb(point3(min_x, min_y, min_z), point3(max_x, max_y, max_z));
    }

    aabb bounding_box() const override { return bbox; }

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        auto denom = dot(normal, r.direction());

        // No hit if the ray is parallel to the plane.
        if (std::fabs(denom) < 1e-8)
            return false;

        // Return false if the hit point parameter t is outside the ray interval.
        auto t = (D - dot(normal, r.origin())) / denom;
        if (!ray_t.contains(t))
            return false;

        // Determine the hit point lies within the planar shape using its plane coordinates.
        auto intersection = r.at(t);
        vec3 planar_hitpt_vector = intersection - Q;
        auto alpha = dot(w, cross(planar_hitpt_vector, v));
        auto beta = dot(w, cross(u, planar_hitpt_vector));

        if (!is_interior(alpha, beta, rec))
            return false;

        // Ray hits the 2D shape; set the rest of the hit record and return true.
        rec.t = t;
        rec.p = intersection;
        rec.mat = mat;
        rec.set_face_normal(r, normal);

        return true;
    }

    virtual bool is_interior(double a, double b, hit_record& rec) const {
        // Given the hit point in plane coordinates, return false if it is outside the
        // primitive, otherwise set the hit record UV coordinates and return true.
        // For a triangle, we check if it fulfills the barycentric coordinate constraints.
        if ((a < 0) || (b < 0) || (a + b > 1.0))
            return false;

        rec.u = a;
        rec.v = b;
        return true;
    }

    double pdf_value(const point3& origin, const vec3& direction) const override {
        hit_record rec;
        if (!this->hit(ray(origin, direction), interval(0.001, infinity), rec))
            return 0;

        auto distance_squared = rec.t * rec.t * direction.length_squared();
        auto cosine = std::fabs(dot(direction, rec.normal) / direction.length());

        return distance_squared / (cosine * area);
    }

    vec3 random(const point3& origin) const override {
        auto r1 = random_double();
        auto r2 = random_double();
        
        // Uniform sampling on a triangle surface
        if (r1 + r2 > 1.0) {
            r1 = 1.0 - r1;
            r2 = 1.0 - r2;
        }

        auto p = Q + (r1 * u) + (r2 * v);
        return p - origin;
    }

  private:
    point3 Q;
    vec3 u, v;
    shared_ptr<material> mat;
    aabb bbox;
    vec3 normal;
    double D;
    vec3 w;
    double area;
};

inline shared_ptr<hittable_list> prism(const point3& p0, const point3& p1, const point3& p2, double height, shared_ptr<material> mat)
{
    // Returns a 3D triangular prism using 2 triangles and 3 quads.
    
    auto sides = make_shared<hittable_list>();

    vec3 u = p1 - p0;
    vec3 v = p2 - p0;

    // Calculate the outward normal of the base triangle to determine the extrusion direction.
    vec3 n = unit_vector(cross(u, v));
    vec3 h_vec = n * height;

    // Bottom face. We swap u and v to ensure the normal points outward (downward).
    sides->add(make_shared<triangle>(p0, v, u, mat));

    // Top face. The normal naturally points outward (upward).
    sides->add(make_shared<triangle>(p0 + h_vec, u, v, mat));

    // Three side faces constructed using quads.
    // The cross product of the edge vectors and the height vector will point outward.
    sides->add(make_shared<quad>(p0, u, h_vec, mat));
    sides->add(make_shared<quad>(p1, p2 - p1, h_vec, mat));
    sides->add(make_shared<quad>(p2, p0 - p2, h_vec, mat));

    return sides;
}

#endif