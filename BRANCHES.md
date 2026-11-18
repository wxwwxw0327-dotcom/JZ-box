# 分支索引

`main` = **真正的初始 demo**（Windows 桌面 `mdfz-robot-code-修改版.zip`），未经任何提速改动。

之后每个版本一个分支，全部从 `main` 切出，内容为完整可编译快照。

## 策略层迭代（平台下发的修改版）

| 分支 | 内容 |
|---|---|
| `main` | 初始 demo：P0 正确性修复 + 进球导向改动 + 11 条内置规则 |
| `v02-defense-push` | 防线整体前移实验（4 场 1胜1平2负，已回滚，留作对照） |
| `v03-situation-coach` | 自动局势教练：protect/balanced/chase 三态 + 进攻留人（rest defense）+ 最后 90 秒 endgame |
| `v04-decision-opt` | 四项决策优化：传球安全门 0.7 / 防压死闭环 pinScore / 射门范围动态化 / 失球后 8 秒盾 |
| `v05-cover-cap` | 留人线上限 +2m(平分领先)/+3.5m(落后) / 贴脸射 3.5m / 主射门 5.5m / 动态 6.5m / 保护期 5s·3s |

## 走行引擎提速（本次新增）

| 分支 | 速度 | 内容 |
|---|---|---|
| `v06-2.3ms-steady` | 2.3 m/s | 步幅 1.16 / 加减速 1.35 / 步频 1.08，normal 档贴满 fast，真机可用 |
| `v07-2.5ms-sim` | 2.5 | 1.20 / 1.45 / 1.10，放宽转弯降速曲线 |
| `v08-3.0ms-max` | 3.0 | 1.34 / 1.60 / 1.18，放开频率偏移与关节动作 clip |
| `v09-dual-speed` | 2.3 ↔ 3.0 | 按球可见性、方向、抢位、带球、对抗自动切换，平滑过渡 + 预测刹车 |
| `v10-lateral-boost` | 同上 + 侧向 ×1.20/×1.45 | 放宽"直跑压制侧向"曲线 500/1500 → 1200/2600 |
| `v11-precision` | 同上 | 目标 700mm / 球 1000mm 内回巡航档，刹车减速度 2400 → 3200 |
| `v12-close-stance` | 同上 | 拦截 420mm 内收窄侧步到 55mm，防穿裆 |
| `v13-close-stance-gated` | 同上 | 并腿加闸门：只有球横向偏差 < 90mm 才并腿（最新） |

## 差异范围

- `v02`～`v05` 只改 `WhiteFa62/Strategy/` 下 2 个文件
- `v06`～`v13` 只改 `WhiteFa62/Providers/WhiteFleetWalkingEngine.cpp`（v09 起加 `.h`）
- `reference/` 任何分支都没有改动

## 调参位置

`WhiteFa62/Providers/WhiteFleetWalkingEngine.cpp` 的 `applyProfile()`：

```cpp
const float s  = 1.16f + (1.34f - 1.16f) * blend;   // 步幅与速度上限
const float a  = 1.35f + (1.60f - 1.35f) * blend;   // 每步加减速
const float f  = 1.08f + (1.18f - 1.08f) * blend;   // 步频
const float t  = 1.10f + (1.25f - 1.10f) * blend;   // 转弯降速曲线
const float ac = 1.00f + (1.15f - 1.00f) * blend;   // 关节动作 clip
const float fc = 1.00f + (1.15f - 1.00f) * blend;   // 频率偏移 clip
const float sy = 1.20f + (1.45f - 1.20f) * blend;   // 侧向额外增益
```

`blend = 0` 巡航档，`blend = 1` 冲刺档；切换条件在 `decideSprint()`。

## 基座换版（平台 2026-11 下发新基座）

| 分支 | 基座 | 内容 |
|---|---|---|
| `v27-base-018481c2` | `018481c2`（`codex/class-base-v21-v1`） | 把 `v26` 的四处改动整套搬到新基座；补回新基座丢掉的 6 处上游修复 + 3 处误伤；新增 `MDFZ_SPEED` 启动日志 |

`main` 及 `v02`～`v26` 全部建立在旧基座 `3d11ae03` 上，**不能再用**。

新基座与旧基座的差异只有 15 个文件，见 `BASE.json` 与本节对应提交说明。v27 相对新基座只改 11 个文件、452 增 / 63 删（约 515 行，平台上限 3000）。

### v27 改了哪 11 个文件

| 文件 | 改动 |
|---|---|
| `Providers/WhiteFleetWalkingEngine.cpp` | v26 冲刺档整套（1.20/1.45/1.10 + 转弯曲线 ×1.15 + normal 贴 fast 99%）+ `MDFZ_SPEED` 日志 |
| `Strategy/WhiteFleetStrategyBehaviorControl.cpp` | 恢复内置 12 条规则与 `parseMdfzRuleLine`；恢复"无 cfg 时默认开编队"；情境教练 / 回位防守 / 丢球盾 / 动态射程 / 传球闸门 / 开球后退 / 角球站位；传球对象改回最前队友 |
| `Strategy/WhiteFleetStrategyBehaviorControl.h` | 上述成员与声明；`followBall` 默认 0.4 |
| `Providers/WhiteFleetFieldBallProvider.cpp` | 球速只用旋转不用位姿平移；队球速度不再取未赋值成员 |
| `Providers/WhiteFleetFieldRatingProvider.cpp` | 本方底线 / 对方门线评分把 `y()` 改回 `x()` |
| `Providers/WhiteFleetGetUpEngine.cpp` | 起身速度因子夹到 1.5~2 |
| `Providers/WhiteFleetLibLookActiveProvider.cpp` | 扫描索引恢复为"到位就推进" |
| `SkillBehaviorControl/Skills/Ball/Zweikampf.cpp` | 对手角区间取中点；滚动球按终点插值；每回合清空踢球距离表 |
| `SkillBehaviorControl/Skills/Output/HeadMotionRequest/HeadControl.cpp` | 球速阈值回到 50 mm/s |
| `Platform/WhiteFleetK1HotChase.h` | 热追球开关回到真实标志位 |
| `Strategy/ActiveRoles/WhiteFleetRolePlayBall.cpp` | 恢复 2 m 内 / 门前 4.5 m 的近射加成 |

### 本地验证

- 编译：`BUILD_OK`（`libSimulatedRobot.so` 链接通过）
- 3v3 跑通：`MDFZ_SPEED fast=2400/1800 normal=2376/1782`、`MDFZ_RULES_DEFAULTS`、`MDFZ_RULE`、`MDFZ_RESTDEF`、`MDFZ_SITUATION`、`MDFZ_KICKOFF_LINE`、`MDFZ_PASSGATE` 全部出现
