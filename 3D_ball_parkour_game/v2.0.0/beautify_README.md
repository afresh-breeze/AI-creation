# 美化版 3D 小球跑酷（`index_beautify.html`）

本文件对应 `**v2.0.0/index_beautify.html**`：在基础跑酷玩法之上，强化了画面表现、操作维度（跳跃 / 冲刺）、风险收益型积分与存档系统。单文件即可运行（内联样式与脚本，Three.js 通过 CDN 加载）。

与 `v1.0.0` 中的基础版相比，本版本关卡目标分数、前进速度与障碍生成策略均独立配置，并增加了移动端性能分级、桌面端自适应画质等工程向优化。

---

## 1) 游戏定位与体验概要

- **核心玩法**：小球在持续向前的三维赛道上**左右移动**并**跳跃**，躲避障碍，收集积分；达到当前关目标分数后出现过关界面，进入下一关。
- **关卡结构**：共 **9 个有目标分数的关卡** + **第 10 关无限模式**（无目标分数，可一直挑战）。
- **胜负条件**：
  - 碰到障碍物、或累计前进距离达到设置最大上限（`maxDistance`）→ 触发结束流程（在拥有复活次数时可先进入复活确认，复活后距离重置）。
  - 收集**红色积分**时，除加高额积分外还会按该球标注的**死亡概率**掷骰，失败则视同死亡（可走复活流程）。

---

## 2) 操作说明

### PC（键盘）


| 操作          | 按键                            |
| ----------- | ----------------------------- |
| 左移          | `A` 或 `←`                     |
| 右移          | `D` 或 `→`                     |
| 跳跃          | `W` 或 `↑`                     |
| 暂停 / 在各界面确认 | `空格`（开始界面开始游戏、过关后进下一关、复活后继续等） |


### 移动端

- **左右移动**：在屏幕**下方触摸条**（`#touch-area`）内**左右滑动**。
- **跳跃**：点击左下角或右下角 **「跳跃」** 圆形按钮（`#jump-left` / `#jump-right`）。
- **暂停**：点击触摸条**上方**区域（不在触摸条内）触发暂停。
- 有触摸控制时，页面会加上 `mobile-controls-active` 相关样式；刘海屏等设备使用 `safe-area-inset-bottom` 抬高底部可操作区域。

---

## 3) 积分类型与风险收益（重要）

场景中积分由随机类型roll 产生（约 **每种特殊色各 1/21**，其余为普通积分；蓝色与普通比例由 `blueCollectibleNormalRatio` 等常量约束，详见源码 `rollCollectibleType`）。


| 类型         | 作用说明                                                                                                                                                                                |
| ---------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **普通（星光）** | 基础收集物，默认分值 **10**。                                                                                                                                                                  |
| **蓝色**     | 收集后触发 **冲刺**：在一段“距离预算”内提高前进速度倍数，接近结束时平滑回落；同时横向移动速度也有加成。具体参数见源码中 `sprintDistanceSeconds`、`sprintBoostMultiplier` 等。                                                                  |
| **红色**     | 收集后先按规则 **加分**：分值与悬浮标注的**死亡概率**挂钩（概率越高，单次得分越高，源码中为 `scoreValue ≈ round(prob * 1000)`）。随后按该概率判定：若判定“死亡”，则进入与撞墙相同的结束流程（分数已加算），并可能弹出**复活**界面。红色球上方的 `**.prob-label`** 会显示死亡概率百分比，便于决策。 |


---

## 4) 关卡与目标分数

配置在源码 `levels` 数组中（第 1～9 关有 `score` 目标，第 10 关 `score: -1` 表示无限模式）。界面左上角/右上角会通过 `updateUI()` 显示当前分数与目标（无限模式显示为「无限模式」文案）。


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


过关时若本关 **无遗漏**（在达成目标分数之前没有出现漏捡 / miss），则记为**大满贯**（`Grand Slam`）：该关首次达成时 **增加 1 次复活机会**，并写入存档；集合 `perfectLevelsAchieved` 用于记录已大满贯的关卡编号。

---

## 5) 复活、暂停与重开

- **复活次数**（`reviveChances`）：通过大满贯奖励累积；在 `**endGame()`** 时若次数大于 0，会优先弹出 **复活** 弹窗，而不是直接游戏结束。确认复活后会短暂无敌（`reviveInvincibleUntilMs`）。红色积分“赌命”失败时会把小球状态快照到 `reviveRespawnState`，复活后可恢复位置与竖直速度等；撞障碍触发的结束也可消耗复活，但无位置快照时仅从当前场景继续逻辑。
- **暂停菜单**：可 **继续** 或 **重开本关**（`restartCurrentLevel`：本关分数清零；**本关大满贯进度清零**，允许重新挑战本关大满贯；同时会回收本关已发放的大满贯复活奖励，避免重复刷复活）。
- **完全重开**（`restartGame`）：回到第 1 关、分数清零，清除复活与大满贯记录，并清空 `localStorage` 中的 `gameState`。
  - 达到 `maxDistance` 且有复活机会时，会按“死亡一次”弹出复活；确认复活后会**刷新距离计数**（`game.distance = 0`）继续游戏。

---

## 6) 存档（`localStorage`）

键名：`**gameState`**（JSON）。主要字段包括：

- `level`、`score`
- `reviveChances`
- `perfectLevelsAchieved`（数组形式持久化）
- 以及 `gameStarted`、`gameOver`、`paused` 等标志

**加载策略**（`loadGameState`）：恢复关卡、分数、复活次数与大满贯集合；若检测到“分数已超过当前关目标”的存档状态，会循环执行分数结转（`carryOverLevelScore`）以与关卡进度对齐。**彻底失败**（无复活的游戏结束）时会 `**removeItem('gameState')`**，下次进入为全新进度。

---

## 7) 涉及技术（相对基础版的扩展）

- **Three.js（r132，CDN）**：场景、相机、渲染器、几何体、`MeshStandardMaterial`、精灵与自定义 **ShaderMaterial**（如蜡烛火焰、部分道路/特效）。
- **程序化纹理**：大量 `CanvasTexture` 在运行时绘制（道路条纹、漩涡面片、小球贴图等），配合 `anisotropy`、Mip 与清晰度统一处理（`applyTextureClarity`）。
- **HTML / CSS**：多层面板（开始、暂停、过关、结束、复活）、加载遮罩 `#model-loading`、响应式布局与安全区。
- **JavaScript**：固定时间步主循环（`fixedTimeStep: 1/60`）、输入、对象池式路段生成、多形状碰撞体（盒、圆柱、圆盘等 `userData.collider`）。
- **性能**：
  - **移动端**：根据 `deviceMemory`、`hardwareConcurrency`、`devicePixelRatio` 等分级限制像素比、各向异性、抗锯齿与色调映射曝光；部分视觉更新降频（`mobileVisualEffectStep`）。
  - **桌面端**：`adaptiveQuality` 根据帧时间滑动调整 `pixelRatio` 与特效步进，在卡顿与清晰度之间折中。

---

## 8) 核心功能实现（模块视角）

- **场景初始化**（`initGame` / `initGameWithLoading`）：创建道路分段、小球、环境；加载存档；显示加载 UI 后进入可玩状态。
- **路段与对象生成**（`generateObjects`）：按列随机布障，保证路段内障碍下限；圆环类障碍中心可额外生成积分；积分放置带间距约束，减少重叠与无解密度。
- **障碍类型**（示例）：圆环（`donut`）、蜡烛（`candle`，含点光源与火焰 shader）、漩涡圆盘（`vortex`）等，各自碰撞体定义不同。
- **物理与移动**：横向 `moveSpeed`、跳跃 `jumpVelocity` / `jumpGravity`、地面高度 `groundY`；相机跟随策略在移动端固定部分视角以减少晃动感（见 animate 内注释）。
- **碰撞**：障碍用 AABB/圆柱/圆盘等与球体近似检测；积分为球-球距离判定。
- **冲刺状态机**：按前进消耗距离递减 `sprintDistanceRemaining`，倍数曲线在尾部用 `smoothstep` 缓和（`getSprintMultiplier`）。
- **关卡与 UI**：`checkLevelUp` 驱动过关面板、第九关通关文案与进入无限模式按钮；`updateUI` 同步分数与目标。

---

## 9) 关键代码块（与基础版对照）

### 初始化入口

```js
function initGame() {
  // 加载存档 loadGameState()
  // 构建 scene / camera / renderer、道路与小球
  // 绑定输入与视口同步
  // 启动 requestAnimationFrame(animate)
}
```

### 动画主循环（逻辑顺序）

```js
function animate(currentTime) {
  // 1) 计算 dt，必要时自适应画质
  // 2) 固定步长推进：距离、冲刺、路段回收与 generateObjects
  // 3) 小球移动、跳跃、相机
  // 4) checkCollisions（障碍 + 积分，含红色概率死亡）
  // 5) checkLevelUp
  // 6) 渲染
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
  // 无限模式直接 return
  // 分数达标 → 判定大满贯 → 可能 reviveChances++
  // saveGameState()，展示 level-complete
}
```

整体主链路可概括为：

**输入 → 固定步长状态更新 → 碰撞（障碍 / 积分含风险判定）→ 计分与冲刺 → 关卡与存档 → 渲染与自适应画质**。

---

## 10) 运行方式与依赖

1. 使用现代浏览器（推荐 Chrome / Edge / Safari 最新版）。
2. 确保可访问 **jsDelivr** CDN 以加载 `three.min.js`。
3. 直接用浏览器打开 `**index_beautify.html`**，或通过本地静态服务器打开（避免部分环境对 `file://` 的限制）。
4. 也可通过 GitHub Pages 在线打开：`https://afresh-breeze.github.io/AI-creation/3D_ball_parkour_game/v2.0.0/index_beautify.html`

---

## 11) 与 `base_README.md`（基础版）的对比小结


| 项目   | 基础版      | 美化版（本文件）                    |
| ---- | -------- | --------------------------- |
| 操作维度 | 主要为左右平移  | 左右 + **跳跃**                 |
| 积分   | 以单一收集物为主 | **普通 / 蓝冲刺 / 红高风险**         |
| 关卡数值 | 基础版独立配置  | 本版 **更高目标分与速度曲线**           |
| 复活   | 无或较简     | **大满贯奖励复活**，红球失败可快照恢复       |
| 画面   | 偏简洁      | **Shader、程序化贴图、尾迹、加载页** 等   |
| 性能   | 常规       | **移动分级 + 桌面自适应 pixelRatio** |


若你正在进行对比学习或二次开发，建议同时打开 `index_beautify.html` 中的 `config`、`levels`、`checkCollisions`、`checkLevelUp` 与 `saveGameState` / `loadGameState` 四处对照阅读，即可快速建立对规则与数据流的完整心智模型。