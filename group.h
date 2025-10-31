// group.h (新增檔案)

#ifndef GROUP_H
#define GROUP_H

#include "hittable.h"
#include "aabb.h"
#include <vector>

class group : public hittable {
  public:
    aabb bounding_box;
    std::vector<shared_ptr<hittable>> objects; // 儲存 Group 內的幾何體

    group() {} 

    // 將 Group 視為一個列表容器
    void add(shared_ptr<hittable> object) {
        objects.push_back(object);
        
        // 每次新增物體時，更新 Group 的 AABB
        if (objects.size() == 1) {
            // 需要物體提供 AABB 的方法，假設 hittable 介面擴展或幾何體直接提供
            // 由於您沒有提供幾何體的 AABB 資訊，這裡使用 Placeholder
            // *** TODO: 您的幾何體類別（sphere/triangle）需要實現 get_bounding_box() ***
            // bounding_box = object->get_bounding_box(); 
        } else {
            // bounding_box = surrounding_box(bounding_box, object->get_bounding_box());
        }
    }
    
    // 這是加速的關鍵：先測試 AABB，再測試內部圖元
    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        
        // 1. AABB 碰撞測試 (邊界體加速)
        if (!bounding_box.hit(r, ray_t)) {
            // 如果光線沒有擊中 Group 的邊界盒，則直接跳過 Group 內部的所有物件
            return false;
        }

        // 2. 內部物件碰撞測試 (如果 Group 被擊中)
        hit_record temp_rec;
        bool hit_anything = false;
        auto closest_so_far = ray_t.max;

        for (const auto& object : objects) {
            // 僅測試在當前最近擊中點前的物件
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