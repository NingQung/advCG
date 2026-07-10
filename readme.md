# 光譜焦散算圖研究與實作
## 簡介
### 進階圖學的上課內容 
NTNU advance computer graphic 的上課內容與作業進度。
基本上就是 Raytracing in Oneweekend Series 為主的內容。

### 大四下的畢業專題
以 Ray Tracing: The Rest of Your Life (也就是最後的 Path tracer) 為基礎，實作一套支援光譜算圖與焦散效果的算圖/渲染系統。

## 新增與更動
`spectral.h` : 光譜運算與轉換相關、與 `rgb2spec` 函式庫有關。
`photon_map.h` : photon 的計算與顏色估計。
`obj_loader.h` : 載入模型與 normal 運算。

## 分支
- `master (this)` : `raytracing_final` + 能與 `spectral` 的比較的 scene 更動。  
- `hw1`, `hw2`, `hw3` : 進階圖學的作業相關  
- `dev` : 圖學的讀書成果，現已廢棄。  

- `spectral` : 光譜算圖目前最新的進度，release 也在這裡發。  
- `exp/real-hero-wavelength` : hero-wl 的第一次實作，由於這裡被教授噹了一下才發現問題所在。  
- `exp/spec-photon-map` : photon mapping 的實作，不過這裡還沒修正重複累積的問題。  
- `exp/obj_loader` : 載入 obj 的實作。  
- `exp/photon-rework` : 修正了許多 photon 相關的小問題，還有最終的場景調整。  

## 命名規範
### commit
sv[大版本].[中版本].[hotfix]

### image
`260610-1847_sv42_10000_5mp_1.0r+1.0k_max2.0_200pg_nfc_3526s_th16_pc3`  
日期_版本_spp_photonCount_radiusStart_radiusAdd_radiusMax_gatherCount_noFullCount_time_threadCount_PC  
> 註: pc1 為Chiyin-Laptop, pc2 是借來的筆電(湆洇測邊1), pc3 是實驗室桌機
