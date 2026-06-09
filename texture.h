#ifndef TEXTURE_H
#define TEXTURE_H

#include "perlin.h"
#include "rtw_stb_image.h"

class texture {
  public:
    virtual ~texture() = default;

    virtual color value(double u, double v, const point3& p) const = 0;
};

class solid_color : public texture {
  public:
    solid_color(const color& albedo) : albedo(albedo) {}

    solid_color(double red, double green, double blue) : solid_color(color(red,green,blue)) {}

    color value(double u, double v, const point3& p) const override {
        return albedo;
    }

  private:
    color albedo;
};

class checker_texture : public texture {
  public:
    checker_texture(double scale, shared_ptr<texture> even, shared_ptr<texture> odd)
      : inv_scale(1.0 / scale), even(even), odd(odd) {}

    checker_texture(double scale, const color& c1, const color& c2)
      : checker_texture(scale, make_shared<solid_color>(c1), make_shared<solid_color>(c2)) {}

    color value(double u, double v, const point3& p) const override {
        auto xInteger = int(std::floor(inv_scale * p.x()));
        auto yInteger = int(std::floor(inv_scale * p.y()));
        auto zInteger = int(std::floor(inv_scale * p.z()));

        bool isEven = (xInteger + yInteger + zInteger) % 2 == 0;

        return isEven ? even->value(u, v, p) : odd->value(u, v, p);
    }

  private:
    double inv_scale;
    shared_ptr<texture> even;
    shared_ptr<texture> odd;
};

class image_texture : public texture {
  public:
    image_texture(const char* filename) : image(filename) {}

    color value(double u, double v, const point3& p) const override {
        // If we have no texture data, then return solid cyan as a debugging aid.
        if (image.height() <= 0) return color(0,1,1);

        // Clamp input texture coordinates to [0,1] x [1,0]
        u = interval(0,1).clamp(u);
        v = 1.0 - interval(0,1).clamp(v);  // Flip V to image coordinates

        auto i = int(u * image.width());
        auto j = int(v * image.height());
        auto pixel = image.pixel_data(i,j);

        auto color_scale = 1.0 / 255.0;
        return color(color_scale*pixel[0], color_scale*pixel[1], color_scale*pixel[2]);
    }

  private:
    rtw_image image;
};

class noise_texture : public texture {
  public:
    noise_texture(double scale) : scale(scale) {}

    color value(double u, double v, const point3& p) const override {
        return color(.5, .5, .5) * (1 + std::sin(scale * p.z() + 10 * noise.turb(p, 7)));
    }

  private:
    perlin noise;
    double scale;
};
class wave_texture : public texture {
  public:
    wave_texture(double _scale, shared_ptr<texture> _even, shared_ptr<texture> _odd)
      : inv_scale(1.0 / _scale), even(_even), odd(_odd) {}

    wave_texture(double _scale, const color& c1, const color& c2)
      : inv_scale(1.0 / _scale),
        even(make_shared<solid_color>(c1)),
        odd(make_shared<solid_color>(c2)) {}

    color value(double u, double v, const point3& p) const override {
        // Use only the x-coordinate to create stripes (zebra crossing effect).
        // You can change p.x() to p.y() or p.z() to change the stripe orientation.
        auto xInteger = static_cast<int>(std::floor(inv_scale * p.x()));

        bool isEven = xInteger % 2 == 0;

        if (isEven)
            return even->value(u, v, p);
        else
            return odd->value(u, v, p);
    }

  private:
    double inv_scale;
    shared_ptr<texture> even;
    shared_ptr<texture> odd;
};
class rotate_texture : public texture {
  public:
    rotate_texture(shared_ptr<texture> p, double angle_degrees, const vec3& axis = vec3(0, 1, 0))
        : tex(p)
    {
        auto radians = degrees_to_radians(angle_degrees);
        sin_theta = std::sin(radians);
        cos_theta = std::cos(radians);
        unit_axis = unit_vector(axis);
    }

    color value(double u, double v, const point3& p) const override {
        // To rotate the texture visually by theta, we apply an inverse rotation (-theta)
        // to the sampling coordinates.
        double inv_sin = -sin_theta;
        
        // 1. Rotate 3D point using Rodrigues' rotation formula
        vec3 k = unit_axis;
        point3 p_rot = p * cos_theta + cross(k, p) * inv_sin + k * dot(k, p) * (1.0 - cos_theta);

        // 2. Rotate 2D UV coordinates around the center (0.5, 0.5)
        double u_shift = u - 0.5;
        double v_shift = v - 0.5;
        double u_rot = u_shift * cos_theta - v_shift * inv_sin + 0.5;
        double v_rot = u_shift * inv_sin + v_shift * cos_theta + 0.5;

        return tex->value(u_rot, v_rot, p_rot);
    }

  private:
    shared_ptr<texture> tex;
    double sin_theta;
    double cos_theta;
    vec3 unit_axis;
};
#endif