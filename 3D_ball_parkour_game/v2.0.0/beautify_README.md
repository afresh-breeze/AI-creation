# 美化版 3D 小球跑酷（`index_beautify.html`）

本文件对应仓库内 `v2.0.0/index_beautify.html`：在基础跑酷玩法之上，强化了画面表现、操作维度（跳跃 / 冲刺）、风险收益型积分与存档系统。单文件即可运行（内联样式与脚本，Three.js 通过 CDN 加载）。

与 `v1.0.0` 中的基础版相比，本版本关卡目标分数、前进速度与障碍生成策略均独立配置，并增加了移动端性能分级、桌面端自适应画质等工程向优化。

**可选 URL 参数（调试用）**：

- `?perf=1`：在 `#debug` 中追加一行性能摘要（约每秒刷新），字段与源码 `perfHud.maybeReport` 一致，例如：`帧ms(avg/p95)`、`逻辑ms`、`渲染ms`、`碰撞ms`、`路段ms`、`生成ms`、`规划ms`、`待生成段`、`待创建`、`积压段`、`障碍/积分`、`Draw: calls/triangles/points`、`DPR`、`FX步进`。同时暴露 `window.__runtimePerfSnapshot()`，便于在控制台抓取最近一次文案与 `perfStats`。
- `?trail=low`：尾迹平面几何沿长度方向的分段数折半（`Math.max(8, floor(segments/2))`），降低每帧顶点上传量。
- `?lite=1`：减轻冲刺光环开销（降低透明度权重、部分流光条隔帧/隐藏、纹理滚动降频）。`?lite=2` 为更强一档：直接关闭流光条（`auraFlowStrips`）以砍掉大块透明叠绘。
- `?dpr=`：解析为数字后与机型档位上限取 `min`，用于压测或强制稳定渲染像素比（桌面默认还会再套一层「偏流畅」的全局上限，见下文「性能」）。

---

## 1) 游戏定位与体验概要

- **核心玩法**：小球在持续向前的三维赛道上**左右移动**并**跳跃**，躲避障碍，收集积分；达到当前关目标分数后出现过关界面，进入下一关。
- **关卡结构**：共 **9 个有目标分数的关卡** + **第 10 关无限模式**（无目标分数，可一直挑战）。
- **胜负条件**：
  - 碰到障碍物、或累计前进距离达到本关 `config.maxDistance` 上限 → 触发结束流程（在拥有复活次数时可先进入复活确认，复活后若死因为距离上限则**距离清零**继续）。
  - 收集**红色积分**时按该球标注的**死亡概率**掷骰：成功则当场加分；失败则进入与撞障碍相同的结束流程（复活与计分细节见下文「积分类型」与「复活」）。

---

## 2) 操作说明

### PC（键盘）


| 操作  | 按键                   |
| --- | -------------------- |
| 左移  | `A` 或 `←`            |
| 右移  | `D` 或 `→`            |
| 跳跃  | `W` 或 `↑`（单段跳，离地时无效） |
| 空格  | 见下方「空格键」             |


**空格键（优先级自上而下）**：开始界面 → 开始游戏；复活弹窗 → 继续（消耗复活）；过关界面 → 下一关 / 进入无限模式；游戏结束界面 → **完全重开**（`restartGame`）；暂停菜单 → **继续游戏**（`resumeGame`）；**游玩中** → 打开暂停菜单（`togglePause`）。再次暂停需先关闭暂停菜单回到运行中，再按空格。

与开始界面 `#rules-desktop` 文案一致：开始 / 复活 / 下一关 / 重开均可空格确认；运行中用空格暂停。

### 移动端

- **左右移动**：在屏幕**下方触摸条**（`#touch-area`）内**左右滑动**；游玩中滑动会直接按位移比例移动小球（与桌面「按住方向键」不同，为拖拽式平移）。
- **跳跃**：点击容器 `#mobile-jump-controls` 内左/右 **「跳跃」** 圆形按钮（`#jump-left` / `#jump-right`），内部使用 `tryJump()`，与键盘跳跃规则一致。
- **暂停**：在 `document` 上监听 `click`，当触点位于触摸条**上沿之上**（`clientY < touch-area` 矩形顶部，且目标非一般按钮）时调用 `togglePause()`。开始界面写「点击屏幕上方」即相对底部触摸条而言的上方区域。
- 需要显示触摸控制时，脚本为 `body` 切换 `mobile-controls-active`；触摸条高度含 `env(safe-area-inset-bottom)` 与 CSS 变量 `--viewport-bottom-gap`。后者由 `updateViewportBottomGap()` 根据 `window.visualViewport` 与 `window.innerHeight` 的差值写入，用于消化移动端地址栏/工具条伸缩带来的可视区域偏移，避免跳跃按钮与触摸条被遮挡。
- 窗口尺寸变化时，移动端对「小幅高度漂移」（非横竖屏切换、宽高变化低于阈值）只做画布同步、不刷新像素比，减轻地址栏收起时的清晰度抖动。

---

## 3) 积分类型与风险收益（重要）

场景中积分类型由 `generateObjects` 内的 `rollCollectibleType` 抽取：`blueCollectibleNormalRatio === 20`，故 `blueCollectibleSpawnProbability` 与 `redCollectibleSpawnProbability` 均为 `1/21`，**普通（星光）约为 `19/21`**。红色球的死亡概率从 `redDeathProbabilityOptions` 均匀抽取：**5%、10%、20%、30%、40%、50%**，对应分值 `scoreValue = round(prob * 1000)`（与源码注释一致）。


| 类型         | 作用说明                                                                                                                                                                                                                                                                                                                |
| ---------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **普通（星光）** | 基础收集物，默认分值 **10**。                                                                                                                                                                                                                                                                                                  |
| **蓝色**     | 收集后触发 **冲刺**：在一段距离预算内提高前进速度倍数，接近结束时用 `smoothstep` 缓和回落；横向移动亦有加成。常量包括 `sprintDistanceSeconds`（默认 **12** 秒量级换算的距离预算）、`sprintBoostMultiplier`（**2**）、`sprintMoveSpeedMultiplier`（**1.5**）、`sprintSlowdownTailRatio`（**0.2**）。**冲刺剩余距离大于 0 时**（`invincibleBySprint`），`checkCollisions` 里与复活无敌一起合并为 `ignoreObstacleDamage`，**忽略障碍伤害**；每帧仍按 `frameDistance` 扣减 `sprintDistanceRemaining`，用尽后 `clearSprintState()`。 |
| **红色**     | 拾取后先掷死亡骰：若**存活**，当场加上该球 `scoreValue`。若**死亡**：无复活次数则**不计入该红球分数**并 `endGame()`；若有复活，则把该球分值写入 `pendingRedRewardScore`，弹出复活，**在 `continueAfterRevive()` 中再结算加分**，同时可用 `reviveRespawnState` 恢复位置与竖直速度等。红球上方的 `.prob-label` / `.prob-label--red` 为 DOM 悬浮百分比标签，位置在动画循环中随投影更新。                                         |


---

## 4) 关卡与目标分数

配置在源码 `levels` 数组中（第 1～9 关有 `score` 目标，第 10 关 `score: -1` 表示无限模式）。`updateUI()` 将分数写在 `#score`（右上），关卡写在 `#level`（右上下方）：常规关为 ``关卡: ${level} / 目标: ${目标分}``，无限模式为 ``关卡: ${level} (无限模式)``。`#debug` 常驻显示 ``速度: ${config.speed} | 关卡: ${level}``；`?perf=1` 时在下一行追加 `perfHud` 产出的性能摘要字符串（见上文 URL 参数）。


| 关卡  | 目标分数  | 基础前进速度（配置值） |
| --- | ----- | ----------- |
| 1   | 100   | 0.25        |
| 2   | 250   | 0.30        |
| 3   | 450   | 0.35        |
| 4   | 700   | 0.40        |
| 5   | 1000  | 0.45        |
| 6   | 1350  | 0.50        |
| 7   | 1750  | 0.55        |
| 8   | 2200  | 0.60        |
| 9   | 2700  | 0.65        |
| 10  | 无（无限） | 0.70        |


过关时，若 `currentLevelCollectibleCollected > 0` 且 `currentLevelCollectibleMissed === 0`（本关至少收集过一颗场上积分，且没有任一颗积分在未被收集的情况下越过身后移除阈值），则记为**大满贯**：该关**首次**达成时 `reviveChances++` 并写入 `perfectLevelsAchieved`。**漏捡**计数：路段更新里 `updateCollectibleOnRoad` 在积分 `position.z > 10` 时返回 `true`，经 `sweepMissedRoadEntities` 移出场景；若此时仍未 `collected`，则 `currentLevelCollectibleMissed++`（与路段滑动窗口、`roadMotionStamp` 配合，避免重复处理）。

第九关达成目标后，过关面板标题为 **「恭喜通关第九关！」**，主按钮文案为 **「进入无限模式」**（仍由 `nextLevel()` 进入第 10 关）。

---

## 5) 复活、暂停与重开

- **复活次数**（`reviveChances`）：通过大满贯奖励累积；在 `endGame()` 时若次数大于 0，会优先展示 `#revive-modal`（界面文案含「成功复活」类提示），并 `saveGameState()`。确认复活后 `reviveInvincibleUntilMs = performance.now() + 1300`（约 **1.3s**）无敌。红球赌命失败会快照到 `reviveRespawnState`；距离触发的死亡在复活后 `game.distance = 0`。
- **冲刺期间**视同无敌于障碍（见上节），与复活无敌可并存判断。
- **暂停菜单**：**继续** 或 **重开本关**（`restartCurrentLevel`：本关分数清零；若本关已在 `perfectLevelsAchieved` 中则移除并 `reviveChances = max(0, reviveChances - 1)` 以回收该关大满贯发放的复活，避免重复刷）。打开暂停菜单时（`togglePause` 显示 `#pause-menu`）会调用 `saveGameState()`，便于直接关闭页面前保留进度。
- **完全重开**（`restartGame`）：回到第 1 关、分数清零，清除复活与大满贯记录，并 `localStorage.removeItem('gameState')`。
- **游戏结束界面**：展示最终分数与关卡；若当前为**第 10 关**且 `hasPerfectGrandSlamFirstNine()`（第 1～9 关均在 `perfectLevelsAchieved` 中），标题为 **「恭喜获得mvp大满贯！！！」**，否则为 **「恭喜完成挑战！」** 或 **「游戏结束」**。此时同样清除 `gameState` 存档。

---

## 6) 存档（`localStorage`）

键名：`gameState`（JSON）。`saveGameState` 仅持久化：

- `level`、`score`
- `reviveChances`
- `perfectLevelsAchieved`（数组）

**不**写入 `hasStarted`、`running`、`paused`、`gameOver` 等运行态；`loadGameState` 注释写明「只加载关卡和分数」等核心字段，并在加载后把 `game.distance`、`paused`、`running`、`hasStarted` 等重置为初始可开玩状态。

**加载策略**：若存档分数已超过当前关目标，会循环调用 `carryOverLevelScore()` 自动进关对齐。**彻底失败**（无复活的游戏结束）时 `localStorage.removeItem('gameState')`，下次进入为全新进度。

---

## 7) 涉及技术（相对基础版的扩展）

- **Three.js（`three@0.132.2`，jsDelivr npm 路径 `build/three.min.js`）**：场景、相机、渲染器、几何体、`MeshStandardMaterial`、精灵与自定义 **ShaderMaterial**（如蜡烛火焰、部分道路/特效）。
- **程序化纹理**：大量 `CanvasTexture` 在运行时绘制（道路条纹、漩涡面片、小球贴图等），配合 `anisotropy`、Mip 与 `applyTextureClarity` 统一清晰度。
- **HTML / CSS**：多层面板（开始、暂停、过关、结束、复活）、加载遮罩 `#model-loading`、响应式布局与安全区。
- **JavaScript**：主循环以 `config.fixedTimeStep`（`1/60`）为基准做 `stepScale = dt / fixedTimeStep` 的 dt 标准化，再驱动移动、路段与碰撞；对象池式路段生成、多形状碰撞体（盒、圆柱、圆盘等 `userData.collider`）。
- **性能**：
  - **移动端**：`mobileLowTier` 在「已知内存 ≤4GB」或「未知内存且线程 ≤6」时成立，对应更激进的 `mobileVisualEffectStep`（3 否则 2）。其余按 `deviceMemory`、`hardwareConcurrency`、`devicePixelRatio` 与短边分辨率分 **高/中/低** 档，限制 `mobilePixelRatioCap`、`mobileAnisotropyCap`、`enableMobileAntialias`、`mobileToneMappingExposure`；渲染器尺寸优先取 `visualViewport` 宽高。
  - **桌面端**：`getRenderPixelRatioCap()` 在未传 `?dpr=` 时先取 **`defaultDesktopCap = 0.85`** 与档位上限的较小值，减轻高分屏全屏下的片元压力；`adaptiveQuality` 用约 45 帧窗口的 **p95 帧耗时**：高于 **18ms** 累积坏 streak、低于 **14ms** 累积好 streak，至少 **900ms** 才调整一次，`desktopVisualEffectStep` 在 **1～4** 间升降，并写入 `perfStats.adaptiveFxStep`，同时驱动远景标签投影步进 `farFieldLabelProjectStep`。

---

## 8) 核心功能实现（模块视角）

- **场景初始化**（`initGame` / `initGameWithLoading`）：创建道路分段、小球、环境；`loadGameState()`；加载 UI 与 `minLoadingVisibleMs` 等节流后进入可玩状态。
- **路段与对象生成**（`generateObjects` + 分帧消费）：路段回收时通过 `enqueueSegmentSpawn` 入队，在 `updateRoad` 末尾调用 `processSegmentSpawnQueue()`，以 `segmentSpawnBudgetMs` / `segmentSpawnPlanBudgetMs` 限额逐帧消化，降低单帧尖峰；积分放置带 `collectibleMinRowGap` / `collectibleNearRowGap` 等间距约束；部分位置允许与障碍重叠放置积分（`allowObstacleOverlap`）。实体按 Z 桶 `entityBucketSize` 划分，碰撞与更新可做范围裁剪。
- **障碍类型**（示例）：圆环（`donut`）、蜡烛（`candle`，含点光源与火焰 shader）、漩涡圆盘（`vortex`）等。
- **物理与移动**：横向 `moveSpeed`（随关卡与 `config.speed` 绑定上限）、跳跃 `jumpVelocity` / `jumpGravity`、地面高度 `groundY`；**相机**在移动端固定目标 X，且随离地高度增加略微抬高（跳跃镜头）。
- **碰撞**：障碍用 AABB/圆柱/圆盘等与球体近似检测；积分为球-球距离判定。高速位移时 `checkCollisions` 在「当前帧球心」与「上一帧记录」之间插值 **中点采样**（最多 3 点），减轻穿模。
- **冲刺状态机**：按前进消耗递减 `sprintDistanceRemaining`，`getSprintMultiplier` 在尾部缓和。
- **关卡与 UI**：`checkLevelUp` 在 `game.running` 且未结束时检测分数；驱动过关面板与大满贯提示 `#level-bonus-tip`；`updateUI` 同步 HUD。

**随关卡变化的最大前进距离**（初始化赛道时计算）：

`config.maxDistance = 1800 + (game.level - 1) * 200 + (game.level - 1)² * 100`（源码内有关卡 1 / 关卡 5 的经验注释）。

---

## 9) 关键代码块（与基础版对照）

### 初始化入口

```js
function initGame() {
  // loadGameState()
  // 构建 scene / camera / renderer、道路与小球
  // 绑定输入与视口同步
  // requestAnimationFrame(animate)
}
```

### 动画主循环（逻辑顺序）

```js
function animate(currentTime) {
  // 1) 计算 dt，必要时自适应画质
  // 2) 若 game.running && !paused：stepScale = dt / config.fixedTimeStep，写入临时 speed/moveSpeed，handleMovement / updateRoad（内含 processSegmentSpawnQueue 分帧生成）/ checkCollisions / 尾迹与相机
  // 3) render
}
```

### 积分类型抽取

```js
const rollCollectibleType = () => {
  const r = Math.random();
  const redP = redCollectibleSpawnProbability;
  const blueP = blueCollectibleSpawnProbability;
  if (r < redP) return 'red';
  if (r < redP + blueP) return 'blue';
  return 'normal';
};
```

### 过关与大满贯

```js
function checkLevelUp() {
  // 第十关（无限）直接 return
  // 需 game.running 且分数达标
  // isGrandSlam = collected > 0 && missed === 0 → 可能 reviveChances++ 与 perfectLevelsAchieved.add(level)
  // saveGameState()，展示 level-complete
}
```

整体主链路可概括为：

**输入 → dt 标准化状态更新 → 碰撞（障碍 / 积分含红球赌命与冲刺无敌）→ 计分与冲刺 → 关卡与存档 → 渲染与自适应画质**。

---

## 10) 运行方式与依赖

1. 使用现代浏览器（推荐 Chrome / Edge / Safari 最新版）。
2. 确保可访问 **jsDelivr** CDN 以加载 Three.js（当前脚本为 `https://cdn.jsdelivr.net/npm/three@0.132.2/build/three.min.js?v=3`）。
3. 直接用浏览器打开 `index_beautify.html`，或通过本地静态服务器打开（避免部分环境对 `file://` 的限制）。
4. 也可通过 GitHub Pages 在线打开：`https://afresh-breeze.github.io/AI-creation/3D_ball_parkour_game/v2.0.0/index_beautify.html`

---

## 11) 与 `base_README.md`（基础版）的对比小结


| 项目   | 基础版      | 美化版（本文件）                                                |
| ---- | -------- | ------------------------------------------------------- |
| 操作维度 | 主要为左右平移  | 左右 + **跳跃**；移动端触摸条 + 双跳跃键                               |
| 积分   | 以单一收集物为主 | **普通 / 蓝冲刺 / 红高风险**（红蓝各约 1/21）                          |
| 关卡数值 | 基础版独立配置  | 本版 **更高目标分与速度曲线**                                       |
| 复活   | 无或较简     | **大满贯奖励复活**；红球失败可 deferred 计分 + 快照恢复                    |
| 画面   | 偏简洁      | **Shader、程序化贴图、尾迹、加载页** 等                               |
| 性能   | 常规       | **移动分级 + 桌面默认 DPR 上限 0.85 叠档位 + 自适应 FX 步进**；可选 `?perf=1`（含 `window.__runtimePerfSnapshot`）/ `?trail=low` / `?lite=1|2` / `?dpr=`；`visualViewport` 校正底部安全留白 |


若你正在进行对比学习或二次开发，建议在 `index_beautify.html` 中对照 `levels`、`rollCollectibleType` / 红球分支、`checkCollisions`、`checkLevelUp`、`saveGameState` / `loadGameState`、`handleSpaceAction` 与 `config.maxDistance` 几处阅读，即可较快建立规则与数据流的心智模型。