// aabb.h (新增檔案)

#ifndef AABB_H
#define AABB_H

#include "ray.h"
#include "interval.h"
#include "vec3.h"

// 假設 interval 類別已經包含在您的 rtweekend.h 或 interval.h 中

class aabb {
  public:
    // AABB 由三個軸上的區間 (interval) 定義
    interval x, y, z;

    aabb() {} // 預設建構子

    aabb(const interval& ix, const interval& iy, const interval& iz)
        : x(ix), y(iy), z(iz) {}

    // 使用兩個點建立 AABB (通常是兩個對角點)
    aabb(const point3& p0, const point3& p1) {
        // 確保 min/max 順序正確
        x = interval(std::fmin(p0.x(), p1.x()), std::fmax(p0.x(), p1.x()));
        y = interval(std::fmin(p0.y(), p1.y()), std::fmax(p0.y(), p1.y()));
        z = interval(std::fmin(p0.z(), p1.z()), std::fmax(p0.z(), p1.z()));
    }
    
    // *** 核心加速函式：AABB 的 hit 測試 ***
    // 判斷光線是否擊中這個邊界盒
    bool hit(const ray& r, interval ray_t) const {
        // 使用優化後的 SLAB 方法進行 AABB 求交
        for (int a = 0; a < 3; a++) {
            auto inv_D = 1.0 / r.direction()[a];
            auto orig_a = r.origin()[a];

            auto t0 = (min_side(a).min - orig_a) * inv_D;
            auto t1 = (min_side(a).max - orig_a) * inv_D;

            if (inv_D < 0.0) std::swap(t0, t1); // 如果光線方向是負值，交換 t0, t1

            // 更新光線 t 值的有效區間 (ray_t)
            ray_t.min = std::fmax(ray_t.min, t0);
            ray_t.max = std::fmin(ray_t.max, t1);

            if (ray_t.max <= ray_t.min)
                return false;
        }
        return true;
    }

    // 取得 x, y, z 區間的輔助函式
    const interval& min_side(int axis) const {
        if (axis == 0) return x;
        if (axis == 1) return y;
        return z;
    }
};

// 輔助函式：合併兩個 AABB 邊界盒，得到一個更大的新 AABB
inline aabb surrounding_box(const aabb& box0, const aabb& box1) {
    return aabb(
        interval(std::fmin(box0.x.min, box1.x.min), std::fmax(box0.x.max, box1.x.max)),
        interval(std::fmin(box0.y.min, box1.y.min), std::fmax(box0.y.max, box1.y.max)),
        interval(std::fmin(box0.z.min, box1.z.min), std::fmax(box0.z.max, box1.z.max))
    );
}

#endif