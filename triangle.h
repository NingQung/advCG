#ifndef TRIANGLE_H
#define TRIANGLE_H

#include "rtweekend.h"
#include "hittable.h"
#include "hittable_list.h"
#include "quad.h"

class triangle : public hittable {
  public:
    triangle(const point3& _Q, const vec3& _u, const vec3& _v, shared_ptr<material> m)
      : Q(_Q), u(_u), v(_v), mat(m), has_vertex_normals(false)
    {
        initialize();
    }

    triangle(
        const point3& _Q,
        const vec3& _u,
        const vec3& _v,
        const vec3& _n0,
        const vec3& _n1,
        const vec3& _n2,
        shared_ptr<material> m
    )
      : Q(_Q),
        u(_u),
        v(_v),
        mat(m),
        n0(unit_vector(_n0)),
        n1(unit_vector(_n1)),
        n2(unit_vector(_n2)),
        has_vertex_normals(true)
    {
        initialize();
    }

    void initialize() {
        auto n = cross(u, v);
        geometric_normal = unit_vector(n);
        D = dot(geometric_normal, Q);
        w = n / dot(n, n);

        area = n.length() / 2.0;

        set_bounding_box();
    }

    virtual void set_bounding_box() {
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
        auto denom = dot(geometric_normal, r.direction());

        if (std::fabs(denom) < 1e-8)
            return false;

        auto t = (D - dot(geometric_normal, r.origin())) / denom;

        if (!ray_t.contains(t))
            return false;

        auto intersection = r.at(t);
        vec3 planar_hitpt_vector = intersection - Q;

        auto alpha = dot(w, cross(planar_hitpt_vector, v));
        auto beta  = dot(w, cross(u, planar_hitpt_vector));

        if (!is_interior(alpha, beta, rec))
            return false;

        rec.t = t;
        rec.p = intersection;
        rec.mat = mat;

        vec3 outward_normal = interpolated_normal(alpha, beta);

        // Keep the shading normal in the same hemisphere as the geometric normal.
        // This avoids flipped OBJ normals breaking front/back behavior too badly.
        if (dot(outward_normal, geometric_normal) < 0.0)
            outward_normal = -outward_normal;

        // front_face is still decided using geometric normal.
        // rec.normal uses the interpolated shading normal, oriented against the ray.
        rec.front_face = dot(r.direction(), geometric_normal) < 0.0;
        rec.normal = rec.front_face ? outward_normal : -outward_normal;

        return true;
    }

    virtual bool is_interior(double a, double b, hit_record& rec) const {
        if ((a < 0) || (b < 0) || (a + b > 1.0))
            return false;

        rec.u = a;
        rec.v = b;
        return true;
    }

    double pdf_value(const point3& origin, const vec3& direction, double lambda) const override {
        hit_record rec;

        if (!this->hit(ray(origin, direction), interval(0.001, infinity), rec))
            return 0;

        auto distance_squared = rec.t * rec.t * direction.length_squared();

        // Use geometric normal for area pdf, not shading normal.
        auto cosine = std::fabs(dot(direction, geometric_normal) / direction.length());

        return distance_squared / (cosine * area);
    }

    vec3 random(const point3& origin, double lambda) const override {
        auto r1 = random_double();
        auto r2 = random_double();

        if (r1 + r2 > 1.0) {
            r1 = 1.0 - r1;
            r2 = 1.0 - r2;
        }

        auto p = Q + (r1 * u) + (r2 * v);
        return p - origin;
    }

  private:
    vec3 interpolated_normal(double alpha, double beta) const {
        if (!has_vertex_normals)
            return geometric_normal;

        double gamma = 1.0 - alpha - beta;

        vec3 n = gamma * n0 + alpha * n1 + beta * n2;

        if (n.near_zero())
            return geometric_normal;

        return unit_vector(n);
    }

    point3 Q;
    vec3 u, v;
    shared_ptr<material> mat;
    aabb bbox;

    vec3 geometric_normal;
    vec3 n0, n1, n2;
    bool has_vertex_normals = false;

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