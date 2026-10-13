# 未对齐清单（全量复核，2026-10-13）

> 对照原版 Warsmash Java（`warsmash_ref/`）与 C++ 版（`src/`）。
> 联机（Battle.net/UDP）明确排除。
> 已完成 Gap-fill 1–30（技能/行为主体、JASS 1445 natives、地形基础、赏金/XP、复活事件、语音、榜单、昼夜雾基础等）。

## P0：战斗模拟正确性（影响对局）

- [x] 1. 受击反击/逃跑（Gap-31：`MaybeReturnFire`，idle/default 门控 + 敌对 + 非 PEON + 攻击行为/逃跑；`interruptable` 精化见第 2 项）。
- [ ] 2. 打断框架：`behavior.h` 无 `interruptable` 虚函数，引导/施法可被任意订单打断；stun 不保存恢复 `interruptedBehavior`。
- [ ] 3. 高地 miss：`combat_system.cpp` 只有闪避 roll，无地形高差 + `ChanceToMiss`（`CUnitDefaultAccuracyCheckListener`）。
- [ ] 4. MLINE 穿刺线 / ARTILLERY 对地：迫击炮溅射变单体，无地面点攻击路径（`behavior_attack.cpp` 只分叉 SPLASH/BOUNCE）。
- [ ] 5. 伤害替换框架：`unit_attack.h` 无 `attackReplacement` 字段，Orb/灼热之箭只能硬编码。
- [ ] 6. 闪避/暴击多源叠加：后写覆盖（酒仙闪避清零 DH 闪避）；暴击单 roll、无 CRIT/BASH floating text。
- [ ] 7. 荆棘/吸血语义：按实际伤害而非基础伤害，无敌对阵营门、无 `ResistsThorns/Leech` 门，只认单 buff 源。
- [ ] 8. 死亡序列：缺 popout 工人、食物返还、死亡视野雾、建造中死亡爆炸效果。
- [ ] 9. 非叠加攻击属性：咆哮/内火等多源应取最大，当前线性叠加（`unit_attack.h` 无表）。
- [ ] 10. 小项：溅射 miss 应减伤而非归零、负伤害钳制、无敌仍跑受击监听。

## P1：数据/地图正确性（错起来隐蔽）

- [ ] 11. 建筑/装饰占位像素图未生效：`pathTex` 存了从不解码，只查 W3M 单格（对位 `PathingGrid.loadPathingTexture`）。
- [ ] 12. w3i 开局科技/升级可用性解析了不用（`w3i.h` → `map_loader` 不消费）。
- [ ] 13. w3i 全局随机单位/物品表未接入（`PlaceRandomUnit/Item` 只用自带表）。
- [ ] 14. 通行/高度/水面三套定义打架：缺 BOUNDARY 旗、`FLOAT/AMPH` 映射；`DecodeGroundHeight` 忽略崖层高+水位偏移；WPM 与 W3E 水面定义不一致。
- [ ] 15. 可破坏物/装饰物：缺 `targetedAs/occlusionHeight/pathingPixelMap/selectionScale`，桥/浅滩不分类，`w3d.doo` 随机掉落套丢弃。

## P2：JASS 桩（按常用度）

- [ ] 16. `Blz*Frame` 全系 ~30 个零注册（自定义 UI 命中即 `unregistered native`）。
- [ ] 17. `SetTerrainFog(Ex)/ResetTerrainFog` 三桩（`JassNatives.cpp:6637/7759`）。
- [ ] 18. `GetSpellTargetDestructable/Item`、`IsUnitIllusion`、`BlzGetSpecialEffectZ` 桩。
- [ ] 19. abilitybuilder 151 桩 + `CreateJassAbility/Buff`（大工程，延后）。
- [ ] 20. 单机存档 `SaveGame/CopySaveGame/SaveGameExists` 全 false。

## P3：视觉/音频/表现

- [ ] 21. 3D 肖像缺失（`PortraitFrame` 纯 2D 占位）、SpriteFrame 不播模型。
- [ ] 22. 建筑阴影无 move/remove、雾只有线性（无 EXP1/EXP2/高度雾）。
- [ ] 23. 瘟疫视觉全无（`SetBlight` + 枯树切换）、深水/岸线简化。
- [ ] 24. 小地图：ping 无脉冲动画、不分敌我色、特殊图标尺寸全 4px。
- [ ] 25. 音频：Music 播放列表（种族/昼夜轮播）、MIDI 无音色库、EAX/遮挡/水下。
- [ ] 26. 任务 FDF 皮肤只显隐空框、战役解锁进度无存档链。
- [ ] 27. KMTF 离散纹理号被线性插值（多纹理序列抖动）。

## P4：infra（长期漂移风险，低优）

- [ ] 28. Ini 无回写、TGA 无独立管线、无 `War3Map` 聚合对象、无 `MakeMeTFTBeROC`、WorldEditStrings 只 `cout`、abilitydata 与统一键模型分叉、slk_merge 双实现。

## 实施顺序

1. 受击反击/逃跑（P0-1）→ 2. `interruptable` 打断（P0-2）→ 3. 占位像素图（P1-11）→ …
