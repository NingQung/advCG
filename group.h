#ifndef GROUP_H
#define GROUP_H

#include "hittable.h"
#include "aabb.h"
#include <vector>

class group : public hittable {
  public:
    aabb bounding_box;
    std::vector<shared_ptr<hittable>> objects;

    group() {} 

    void add(shared_ptr<hittable> object) {
        objects.push_back(object);
        
        if (objects.size() == 1) {
            // 需要物體提供 AABB 的方法，假設 hittable 介面擴展或幾何體直接提供
            // 由於您沒有提供幾何體的 AABB 資訊，這裡使用 Placeholder
            // *** TODO: 您的幾何體類別（sphere/triangle）需要實現 get_bounding_box() ***
            // bounding_box = object->get_bounding_box(); 
        } else {
            // bounding_box = surrounding_box(bounding_box, object->get_bounding_box());
        }
    }
    
    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        
        if (!bounding_box.hit(r, ray_t)) {
            return false;
        }

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
};

#endif