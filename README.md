# Spectral Caustic Renderer

這個 repository 是「光譜焦散算圖研究與實作」專題的程式實作紀錄。

如果幾年後回來看，這份 README 的目的不是把每個公式重新推導一次，而是讓我能快速理解：

- 這個 project 當初在做什麼
- 目前的 renderer 能做哪些事
- 程式架構大概長在哪裡
- 哪些地方是刻意的工程折衷
- 要怎麼 build、run、調參與繼續改

簡單一句話：

> 這是一個從 Ray Tracing in One Weekend 系列延伸出來的 CPU offline renderer，目標是用 spectral path tracing 加上 caustic photon mapping，重建透明材質造成的彩虹焦散。

---

## 1. Project Goal

本專題的研究目標是實作透明材質造成的彩虹焦散效果。

傳統 RGB path tracing 通常只在 RGB 三通道中傳遞能量，camera ray 本身沒有 wavelength，因此很難自然描述折射率隨波長改變所造成的 dispersion。即使改成 spectral path tracing，從 camera 端反向取樣到 caustic path 的機率仍然很低。

典型焦散路徑如下：

```text
Light -> Glass / Specular -> Diffuse -> Camera
```

但 camera path tracing 需要剛好走到：

```text
Camera -> Diffuse -> Glass / Specular -> Light
```

這種路徑取樣機率很小，所以焦散收斂很慢。

因此這個 renderer 採用兩個主要方向：

1. 使用 spectral path tracing 讓 rays 帶有 wavelength samples，支援 wavelength-dependent refraction。
2. 使用 caustic photon mapping 從 light 端捕捉 `Light -> Glass / Specular -> Diffuse` 的焦散光路，再在 rendering pass 中補強焦散。

---

## 2. What This Project Is and Is Not

### This project is

- 一個教學與專題規模的 spectral renderer。
- 一個以 CPU 執行的 offline path tracer。
- 一個把 RTW-style renderer 擴充成 spectral path tracing 的實驗。
- 一個專注於 caustic photon map 的實作。
- 一個用來研究透明材質、dispersion、photon gather 與彩虹焦散的實驗場。

### This project is not

- 不是 production renderer。
- 不是完整 PBRT / Mitsuba 等級的渲染系統。
- 不是 unbiased photon mapping 解法。Photon mapping 本身在這裡是 biased estimator。
- 不是完整 global photon map，只專注在 caustic photon map。
- 不是 GPU renderer。
- 沒有直接實作 `Spectral Primary Decomposition for Rendering with sRGB Reflectance` 的三基底最佳化方法。

`Spectral Primary Decomposition` 主要作為理解 sRGB reflectance 與 spectral rendering 問題的理論背景。實作上，這個 project 使用 `rgb2spec` 與 Jakob-Hanika 2019 的 coefficient model 進行 RGB-to-spectrum conversion。

---

## 3. Main Features

### 3.1 Spectral Path Tracing

每條 ray 都攜帶 wavelength samples，並使用 `SpectralEnergy` 儲存各 wavelength channel 的能量。

目前的 wavelength range：

```cpp
WL_MIN = 390.0 nm
WL_MAX = 830.0 nm
WL_PER_RAY = 4
```

### 3.2 Hero Wavelength Sampling

每條 ray 先 sample 一個 hero wavelength，再依固定間隔產生其他 wavelength channels。

這比單一 wavelength per ray 更穩定，也比完整 dense spectrum 更適合這個小型 renderer。

### 3.3 RGB-to-Spectrum Conversion

場景與材質大多仍然用 RGB 指定，例如 Lambertian albedo、light color 等。

在 spectral rendering 中，RGB 會被轉成可在指定 wavelength 上 evaluate 的 spectral value。

使用的外部資料與程式：

```text
external/rgb2spec.h
external/rgb2spec.cpp
external/jakob-and-hanika-2019-srgb.coeff
```

### 3.4 Wavelength-dependent Dielectric Dispersion

`dielectric` material 使用簡化的 wavelength-dependent IOR：

```cpp
ior(lambda) = A + B / lambda_um^2
```

其中 `A` 是 base IOR，`B` 是 dispersion strength。

當 `B != 0` 時，不同 wavelength 會產生不同折射方向，進而形成彩虹色散與彩色焦散。

### 3.5 Caustic Photon Map

這個 project 的 photon map 是 caustic photon map，而不是 global photon map。

Photon pass 中，photons 從 light source 出發，經過 glass / specular interaction 之後，如果打到 diffuse receiver，就儲存到 photon map。

儲存的 photon 資訊包含：

```text
position
normal
incident_direction
spectral power
wavelengths
```

### 3.6 Spatial Grid Acceleration

Photon map 建完後會建立 spatial grid，用來加速附近 photon 查詢。

這不是最精準或最高效的資料結構，但對目前專題規模足夠直觀，也比每次 brute-force 掃所有 photons 實用很多。

### 3.7 Adaptive / k-nearest Photon Gather

Renderer 支援不同的 gather 策略：

- fixed radius gather
- adaptive gather
- k-nearest gather
- grid accelerated gather
- brute-force fallback

實際調參時，k-nearest gather 對穩定焦散比較有用。

### 3.8 Photon Caustic Color Revision

這是後期重要修正。

早期方法嘗試讓 camera ray wavelengths 和 photon wavelengths 對齊，只有 wavelength 差距在某個 range 內的 photons 才參與估計。

問題是 camera rays 和 photons 都是隨機取樣 wavelengths，兩者很難剛好對上，導致有效 photons 太少，也讓計算量浪費。

新版做法：

- camera ray 只負責找到 diffuse hit point。
- photon map 表示該表面上已形成的可見 caustic contribution。
- 每個 photon 使用自己儲存的 wavelengths 與 spectral power 計算 contribution。
- contribution 依空間距離加權後轉成 RGB caustic color。

這個做法是工程折衷，但比 wavelength matching 更穩定，也更適合目前的結果展示。

### 3.9 Caustic Replacement / Duplicate Contribution Suppression

Spectral path tracing 理論上也能估計 caustics，只是收斂很慢。

Photon map 也估計同一類 `Light -> Glass / Specular -> Diffuse` 路徑。

如果兩者直接相加，可能會 double count caustic energy。

目前採用的策略是：

- 一般光照由 SPT 處理。
- 焦散由 photon map 補強。
- 當 hit point 有可見 photon caustic 時，抑制後續可能重複的 SPT caustic contribution。

這不是完整嚴格的 physically correct formulation，但可以避免最明顯的能量重複問題。

---

## 4. Repository Structure

主要檔案大致如下：

```text
main.cc                 Main program, scene setup, render settings
camera.h                Camera, spectral path tracing, photon caustic integration
photon_map.h            Caustic photon map, photon emission, gather, grid
spectral.h              Wavelength sampling, SpectralEnergy, RGB/spectrum conversion
material.h              Lambertian, metal, dielectric, diffuse light
hittable.h              Hittable interface and transform wrappers
hittable_list.h         Scene object list
sphere.h                Sphere primitive
quad.h                  Quad primitive and box helper
triangle.h              Triangle and prism-related geometry
texture.h               Solid, checker, wave textures
pdf.h                   PDFs for path tracing and light sampling
obj_loader.h/.cpp       OBJ loading support
external/               rgb2spec and tinyobjloader related files
assets/                 Test OBJ/MTL assets
makefile                Build and run shortcuts
todo.txt                Old experiment notes and render command notes
```

---

## 5. Render Pipeline

The high-level pipeline is:

```text
Load scene / OBJ
        ↓
Create materials, lights, camera, world
        ↓
Convert RGB materials to spectral representation
        ↓
Photon pass: emit spectral photons from lights
        ↓
Trace photons through glass / specular paths
        ↓
Store caustic photons on diffuse receivers
        ↓
Build spatial grid for photon lookup
        ↓
Rendering pass: trace spectral camera rays
        ↓
At diffuse hit point, gather nearby photons
        ↓
Estimate photon caustic color from photons' own wavelengths and power
        ↓
Combine SPT lighting and photon caustic contribution
        ↓
Suppress duplicated SPT caustic paths when needed
        ↓
Convert spectral energy to RGB
        ↓
Gamma correction and PPM output
```

---

## 6. Build

The main build target is currently:

```bash
make spr
```

Equivalent command:

```bash
g++ -std=c++17 main.cc external/rgb2spec.cpp -O3 -pthread -o main
```

If the build fails, check that these files exist:

```text
external/rgb2spec.cpp
external/rgb2spec.h
external/jakob-and-hanika-2019-srgb.coeff
external/tiny_obj_loader.h
```

---

## 7. Run

### Default scene

```bash
./main > image.ppm
```

### OBJ scene

```bash
./main assets/test4.obj 150 278 150 278 > image.ppm
```

Argument meaning:

```text
./main <obj_path> <obj_scale> <offset_x> <offset_y> <offset_z>
```

Example:

```bash
./main assets/test4.obj 150 278 150 278 > image3.ppm
```

Convert PPM to PNG if needed:

```bash
magick image.ppm image.png
```

or:

```bash
convert image.ppm image.png
```

---

## 8. Important Parameters

Most experiment parameters are currently set in `main.cc`.

### Camera / path tracing

```cpp
cam.aspect_ratio
cam.image_width
cam.samples_per_pixel
cam.max_depth
cam.use_parallel_render
cam.use_russian_roulette
cam.replace_spt_caustics_with_photon_map
cam.spt_caustic_weight
```

### Photon map

```cpp
caustic_map.photon_count
caustic_map.max_depth
caustic_map.gather_radius
caustic_map.max_gather_radius
caustic_map.min_photons_per_gather
caustic_map.grid_cell_size
caustic_map.caustic_strength
caustic_map.rgb_caustic_strength
caustic_map.use_spatial_grid
caustic_map.use_adaptive_gather
caustic_map.use_k_nearest_gather
caustic_map.k_nearest_photon_count
caustic_map.k_nearest_max_radius
caustic_map.k_nearest_require_full_count
```

### Debug output

```cpp
caustic_map.debug_write_ply
caustic_map.debug_ply_filename
caustic_map.debug_ply_color_scale
```

When `debug_write_ply` is true, the renderer can output a photon point cloud for debugging photon distribution.

---

## 9. Parameter Notes

### photon_count

More photons usually produce smoother and more stable caustics, but increase photon pass time and memory usage.

### gather_radius / k_nearest_max_radius

Smaller radius gives sharper caustics but can create speckles.

Larger radius gives smoother caustics but can blur the result.

### k_nearest_photon_count

Higher count stabilizes the estimate, but can blur small caustic details.

### rgb_caustic_strength

Practical brightness knob for RGB photon caustic side-channel.

Photon mapping brightness often needs calibration in this implementation.

---

## 10. Current Limitations

- Photon mapping is biased.
- Caustic brightness depends heavily on photon count and gather parameters.
- Photon caustic color is currently an engineering approximation using photon-side wavelengths.
- No full global photon map.
- No BVH / KD-tree photon lookup yet. Spatial grid is used instead.
- No GPU acceleration.
- OBJ / MTL support is basic.
- Scene setup is hard-coded in `main.cc`.
- Some render settings are experiment-specific and not exposed through config files.
- Spectral material model is simplified.
- Wavelength-dependent IOR uses a simple Cauchy-like approximation.

---

## 11. Troubleshooting

### The render is black

Check:

- Is the light added to both `world` and `lights`?
- Is the emitter added to `light_emitter_list`?
- Is `external/jakob-and-hanika-2019-srgb.coeff` loaded correctly?
- Is the camera looking at the scene?
- Is the object scale or offset wrong?

### No visible caustic

Try:

- Increase `photon_count`.
- Increase light intensity.
- Check whether photons actually pass through dielectric objects.
- Enable photon PLY output and inspect photon distribution.
- Increase `rgb_caustic_strength`.
- Adjust `k_nearest_max_radius` or `gather_radius`.

### Caustic is too blurry

Try:

- Decrease `gather_radius`.
- Decrease `k_nearest_max_radius`.
- Reduce `k_nearest_photon_count`.
- Increase `photon_count` to compensate.

### Caustic is too noisy or speckled

Try:

- Increase `photon_count`.
- Increase `k_nearest_photon_count`.
- Increase `gather_radius`.
- Enable adaptive gather.

### Render takes too long

Try:

- Reduce `image_width`.
- Reduce `samples_per_pixel`.
- Reduce `photon_count`.
- Use fewer OBJ triangles.
- Keep `use_parallel_render = true`.

---

## 12. Future Work

Possible future directions:

- Replace spatial grid with KD-tree or BVH-style photon lookup.
- Implement progressive photon mapping or stochastic progressive photon mapping.
- Improve spectral photon density estimation.
- Make photon caustic color calculation more physically rigorous.
- Add better scene configuration instead of hard-coded `main.cc` scenes.
- Improve OBJ / MTL / texture support.
- Add BVH acceleration for scene geometry.
- Add SIMD or GPU acceleration.
- Add more realistic wavelength-dependent material data.
- Add rough dielectric and microfacet BSDF.
- Add proper experiment scripts for comparing PT, SPT, and SPT + photon.

---

## 13. References

- Peter Shirley, Trevor David Black, Steve Hollasch, Ray Tracing in One Weekend Series.
- Peter Shirley, Trevor David Black, Steve Hollasch, Ray Tracing: The Rest of Your Life.
- Henrik Wann Jensen, Realistic Image Synthesis Using Photon Mapping.
- Ian Mallett and Cem Yuksel, Spectral Primary Decomposition for Rendering with sRGB Reflectance, 2019.
- Wenzel Jakob and Johannes Hanika, A Low-Dimensional Function Space for Efficient Spectral Upsampling, 2019.
- Wilkie et al., Hero Wavelength Spectral Sampling, 2014.
- Shadertoy WfjGWG, used as a visual reference for caustic-style rendering.

---

## 14. Historical Note

This project was originally developed as a university advanced computer graphics project.

The coursework/report part is considered finished. From this point forward, the renderer should be treated as a personal graphics experiment rather than a class assignment.

If I return to this repository years later, the most important thing to remember is:

> The goal was never to build the most correct renderer. The goal was to understand why spectral caustics are hard, then build enough of the pipeline to see them appear on screen.
