# -*- coding: utf-8 -*-
"""nDisplay konfiguratsiyalarini yaratadi - 360/180 egri ekran va 2-3 tekis ekran.

Ikkalasi ham ATAYLAB BITTA NODE: bitta jarayon, bitta oyna, ichida N ta viewport.
Sababi muhim - bitta jarayonda bitta pawn va bitta vidjet bo'ladi, ya'ni
operator panelining hozirgi Remote* API si hech qanday o'zgarishsiz ishlayveradi.
Har monitorga alohida node qo'ysak, har biri o'z nusxasini yuritadi va buyruqlar
nDisplay Cluster Event orqali tarqalishi kerak bo'lardi - butunlay boshqa ish.

2-3 ekran holatida monitorlar Windows da UZLUKSIZ ish stoli bo'lishi kerak
(NVIDIA Surround / AMD Eyefinity), shunda bitta oyna hammasini qoplaydi.

Format: .ndisplay JSON. 5.8 da ham 4.27 sxemasi o'qiladi - dvijokda faqat
JSON426 va JSON427 parserlari bor (JSON500 yo'q), Version_500 esa asset formati.
Kalitlar FJsonObjectConverter qoidasi bo'yicha camelCase.

5.8 TUZOG'I: 'simple' proyeksiya siyosati olib tashlangan. U jimgina 'mesh' ga
aylantiriladi va 'screen' parametri 'Component' bo'lib qoladi
(DisplayClusterConfigurationUtils::RedirectSimpleToMeshPolicy). Shuning uchun
quyida 'simple' yozilgan bo'lsa ham, dvijok ichida mesh bo'lib ishlaydi.
"""
import io
import json
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))


def screen(yaw_deg, radius_m, width_m, height_m):
    """Ko'rish nuqtasidan radius_m uzoqlikda, yaw_deg burchakda turgan ekran.

    UE o'qlari: X oldinga, Y o'ngga, Z tepaga. Birlik - METR (piksel emas).
    """
    r = math.radians(yaw_deg)
    return {
        "parentId": "",
        "location": {"x": round(radius_m * math.cos(r), 6),
                     "y": round(radius_m * math.sin(r), 6),
                     "z": 0.0},
        "rotation": {"pitch": 0.0, "yaw": float(yaw_deg), "roll": 0.0},
        "size": {"width": round(width_m, 6), "height": round(height_m, 6)},
    }


def build(description, screens, window_w, window_h, viewport_w):
    """screens: [(id, yaw)] chapdan o'ngga. Viewportlar oynada yonma-yon tiziladi."""
    scene_screens = {}
    viewports = {}
    for i, (sid, spec) in enumerate(screens):
        scene_screens[sid] = spec
        viewports["vp_%d" % i] = {
            "camera": "",                     # bo'sh = standart ko'rish nuqtasi
            "bufferRatio": 1.0,
            "gpuIndex": 0,
            "allowCrossGPUTransfer": False,
            "isShared": False,
            "region": {"x": i * viewport_w, "y": 0, "w": viewport_w, "h": window_h},
            "projectionPolicy": {"type": "simple", "parameters": {"screen": sid}},
        }

    return {
        "nDisplay": {
            "description": description,
            "version": "4.27",
            "misc": {
                # Kamera o'yinning o'z pawn iga ergashadi - operator paneli
                # pawn ni boshqaradi, ya'ni panel butun ekranni boshqaradi.
                "bFollowLocalPlayerCamera": True,
                # Esc bilan yopilsin: ko'rgazmada klaviatura yonida bo'ladi.
                "bExitOnEsc": True,
            },
            "scene": {"xforms": {}, "cameras": {}, "screens": scene_screens},
            "cluster": {
                "masterNode": {
                    "id": "node_main",
                    "ports": {"ClusterSync": 41001,
                              "ClusterEventsJson": 41003,
                              "ClusterEventsBinary": 41004},
                },
                "sync": {
                    # Bitta node uchun sinxronlash ahamiyatsiz, lekin maydon
                    # bo'sh qolsa parser standart qiymatni qo'yadi.
                    "renderSyncPolicy": {"type": "ethernet", "parameters": {}},
                    "inputSyncPolicy": {"type": "replicatePrimary", "parameters": {}},
                },
                "network": {},
                "nodes": {
                    "node_main": {
                        "host": "127.0.0.1",
                        "sound": True,
                        "fullScreen": False,
                        "window": {"x": 0, "y": 0, "w": window_w, "h": window_h},
                        "postprocess": {},
                        "viewports": viewports,
                    }
                },
            },
            "customParameters": {},
            "diagnostics": {"simulateLag": False, "minLagTime": 0.01, "maxLagTime": 0.3},
        }
    }


def write(name, data):
    p = os.path.join(HERE, name)
    io.open(p, "w", encoding="utf-8").write(json.dumps(data, indent=2, ensure_ascii=False))
    n = len(data["nDisplay"]["cluster"]["nodes"]["node_main"]["viewports"])
    w = data["nDisplay"]["cluster"]["nodes"]["node_main"]["window"]
    print("  %-34s %d viewport   oyna %dx%d" % (name, n, w["w"], w["h"]))


# ---- 1) 360 gradus: 6 ta viewport, har biri 60 gradus -----------------------
# Nega 6: bitta warp mesh uchun 90 gradus amaliy chegara (179 - qattiq chegara),
# 60 ga bo'lish esa chekkalardagi cho'zilishni sezilarli kamaytiradi.
R = 1.0
SEG360 = 6
SEG_DEG = 360.0 / SEG360
W360 = 2 * R * math.tan(math.radians(SEG_DEG / 2))      # 60 gradusni qoplaydigan kenglik
H360 = W360 * 9.0 / 16.0
VP = 1280                                                # har bir viewport eni
write("ANIMA_360.ndisplay", build(
    "ANIMA 360 - bitta chiqish, 6 viewport",
    [("screen_%d" % i, screen(i * SEG_DEG, R, W360, H360)) for i in range(SEG360)],
    window_w=VP * SEG360, window_h=720, viewport_w=VP))

# ---- 2) 180 gradus: 3 ta viewport, har biri 60 gradus ----------------------
write("ANIMA_180.ndisplay", build(
    "ANIMA 180 - bitta chiqish, 3 viewport",
    [("screen_%d" % i, screen(-60.0 + i * 60.0, R, W360, H360)) for i in range(3)],
    window_w=VP * 3, window_h=720, viewport_w=VP))

# ---- 3) 3 ta tekis ekran ----------------------------------------------------
# Uchta odatiy 1920x1080 monitor yonma-yon (jami 5760x1080), o'rtadagisi
# to'g'riga, yonlari 45 gradusga burilgan - stolda shunday qo'yiladi.
WFLAT, HFLAT = 0.6, 0.6 * 9.0 / 16.0
write("ANIMA_3ekran.ndisplay", build(
    "ANIMA 3 tekis ekran - Surround/Eyefinity bilan bitta ish stoli",
    [("screen_left",   screen(-45.0, R, WFLAT, HFLAT)),
     ("screen_center", screen(0.0,   R, WFLAT, HFLAT)),
     ("screen_right",  screen(45.0,  R, WFLAT, HFLAT))],
    window_w=1920 * 3, window_h=1080, viewport_w=1920))

# ---- 4) Sinov uchun: bitta oynada 4 viewport, 1280x720 ---------------------
# Lumen chokini shu kompyuterda, temirsiz tekshirish uchun. Oyna kichik,
# lekin viewport chegaralari haqiqiy - chok bo'lsa shu yerda ko'rinadi.
write("ANIMA_SINOV_4vp.ndisplay", build(
    "SINOV - 4 viewport, Lumen chokini tekshirish uchun",
    [("screen_%d" % i, screen(-67.5 + i * 45.0, R, 2 * R * math.tan(math.radians(22.5)),
                              2 * R * math.tan(math.radians(22.5)) * 9.0 / 16.0))
     for i in range(4)],
    window_w=640 * 4, window_h=360, viewport_w=640))

# ---- 5) Har xil o'lchamli 2 ekran, HAQIQIY fizik o'lcham bilan --------------
# O'lchangan stol: 34" ultrawide 21:9 (79.4 x 33.7 sm, EDID da o'lcham yo'q -
# diagonaldan hisoblangan) va 55" TV (120 x 68 sm, EDID dan). Boshqa ekranlar
# uchun SCREENS ni o'zgartiring: WmiMonitorBasicDisplayParams sm da beradi.
#
# Burchak FIZIK kenglikdan kelib chiqadi, pikseldan emas. Ikkalasiga bir xil
# 45 gradus berilganda televizorda hamma narsa yirikroq ko'rinardi.
# Viewport region lari bir xil balandlikda bo'lishi shart emas.
D = 1.20                                  # tomoshabingacha masofa, metr
SCREENS = [
    # (id, fizik_kenglik_m, fizik_balandlik_m, viewport x, y, w, h)
    ("screen_uw", 0.794, 0.337,    0, 0, 3440, 1440),
    ("screen_tv", 1.200, 0.680, 3440, 0, 2194, 1234),
]
angles, edge = [], 0.0
for sid, w_m, h_m, *_ in SCREENS:
    half = math.degrees(math.atan((w_m / 2) / D))
    angles.append(edge + half)
    edge += 2 * half
angles = [a - edge / 2 for a in angles]   # guruhni markazga siljitish

cfg = build("ANIMA 2 ekran - haqiqiy fizik o'lchamlar bilan", [], 5634, 1440, 1)
nd = cfg["nDisplay"]
for (sid, w_m, h_m, x, y, w_px, h_px), yaw in zip(SCREENS, angles):
    nd["scene"]["screens"][sid] = screen(yaw, D, w_m, h_m)
    nd["cluster"]["nodes"]["node_main"]["viewports"][sid] = {
        "camera": "", "bufferRatio": 1.0, "gpuIndex": 0,
        "allowCrossGPUTransfer": False, "isShared": False,
        "region": {"x": x, "y": y, "w": w_px, "h": h_px},
        "projectionPolicy": {"type": "simple", "parameters": {"screen": sid}}}
nd["cluster"]["nodes"]["node_main"]["window"] = {"x": 0, "y": 0, "w": 5634, "h": 1440}
write("ANIMA_2MONITOR.ndisplay", cfg)

print()
print("Ishga tushirish:")
print('  <Loyiha>.exe -dc_cluster -dc_cfg="<yo\'l>\\ANIMA_360.ndisplay" -dc_node=node_main -RCWebControlEnable')
