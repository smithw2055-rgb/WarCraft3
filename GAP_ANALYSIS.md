# 未对齐清单（全量复核，2026-10-13）

> 对照原版 Warsmash Java（`warsmash_ref/`）与 C++ 版（`src/`）。
> 联机（Battle.net/UDP）明确排除。
> 已完成 Gap-fill 1–40（技能/行为主体、JASS 1445 natives、地形基础、赏金/XP、复活事件、语音、榜单、昼夜雾基础、占位像素图+静态 stamp、高地 miss、穿刺线/炮击、伤害替换框架、闪避暴击叠加、荆棘吸血语义、死亡序列等）。

## P0：战斗模拟正确性（影响对局）

- [x] 1. 受击反击/逃跑（Gap-31：`MaybeReturnFire`，idle/default 门控 + 敌对 + 非 PEON + 攻击行为/逃跑；`interruptable` 精化见第 2 项）。
- [x] 2. 打断框架（Gap-32：`IsInterruptable` 虚函数默认 true、Stun false；新订单当 tick 抢占可打断行为，眩晕期队列坍缩为最新一条；反击门控同步精化）。
- [x] 3. 高地 miss（Gap-35：`GetTerrainHeight` 取悬崖层、越层时 `ChanceToMiss`（零售 0.25）掷骰，仅武器攻击、仅对单位；`CUnit::Damage` 与 `ApplyDamage` 双入口，无图/平地不耗 RNG）。
- [x] 4. MLINE 穿刺线 / ARTILLERY 对地（Gap-36：`Hfact/Qfact/damageLoss/spillDist/spillRadius` 解析；溅射族=MSPLASH+ARTILLERY、穿刺族=MLINE+ALINE；ARTILLERY 落点邮戳+对地爆炸、ATTACK_GROUND 点目标链路、抛射体/瞬发双口径点溅射与走廊穿刺）。
- [x] 5. 伤害替换框架（Gap-37：`unit_attack.h` 新增 `CAttackModifierEffect` 接口 + `CUnit` 按攻击索引槽位（安装替换、按安装者清除）；Orb 霜/腐蚀/火、灼热箭、冷箭迁出 `Damage` 硬编码，改 OnAdd/OnRemove 注册；顺带修多 Orb 叠加——单槽不叠加）。
- [x] 6. 闪避/暴击多源叠加（Gap-38：`CUnit` 闪避/暴击改按源列表（同 id 替换、按 id 清除），闪避各源独立掷骰、暴击取最强倍率；`CSimulation::CombatTextKind` MISS/CRIT/BASH 回调供表现层挂浮字；酒仙闪避不再清零 DH 闪避）。
- [x] 7. 荆棘/吸血语义（Gap-39：荆棘改按入射基础伤害（战前快照，含 bonus，抵御前），补 NORMAL 类型门+敌对阵营门+ResistsThorns 门；吸血补 PASSIVE 联盟门+ResistsLeech 门；双边多源取最强；另修魔法免疫 `||`→`&&` 使 UNIVERSAL/门关法术穿透与原版一致）。
 - [x] 8. 死亡序列（Gap-40：死亡 tick 清订单+终结行为；popout 工人——兽族/暗夜 SetWorker 链接、死亡弹出+复位，未亡/人族 outside 不弹，NE 开建扣粮/弹出补粮、完成/取消清链接；食物返还——死亡 tick 即扣 foodUsed/foodMade，移除不再重复扣；死亡视野雾——owner+killer 双 timed VISIBLE 修饰（FogFlashTime 3s/DyingRevealRadius 500，可配）；建造中死亡——FireDeathExplode（unit+explodeBuffId）表现回调，sim 保持正常 decay 记账。注：爆炸质地（buff effect art）数据未到，渲染层订阅回调即可挂载）。
 - [ ] 9. 非叠加攻击属性：咆哮/内火等多源应取最大，当前线性叠加（`unit_attack.h` 无表）。
- [ ] 10. 小项：溅射 miss 应减伤而非归零、负伤害钳制、无敌仍跑受击监听。

## P1：数据/地图正确性（错起来隐蔽）

- [x] 11. 占位像素图（Gap-33：pathTex 解码/缓存、`pathTex` 字段、放置校验改足迹矩形、`BuildingAngle`、吸附偏移；Gap-34：WPM 静态 stamp/死亡切换——建筑创建吸附+blit、死亡/移除 unstamp、Root/Uproot 切换，可破坏物 pathTex/pathTexDeath 双足迹+occH 视野块、复活换回）。
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

1. 受击反击/逃跑（P0-1）→ 2. `interruptable` 打断（P0-2）→ 3. 占位像素图+静态 stamp（P1-11，Gap-33/34）→ 4. 高地 miss（P0-3，Gap-35）→ 5. 穿刺线/炮击（P0-4，Gap-36）→ 6. 伤害替换框架（P0-5，Gap-37）→ 7. 闪避暴击叠加（P0-6，Gap-38）→ 8. 荆棘吸血语义（P0-7，Gap-39）→ 9. 死亡序列（P0-8，Gap-40）→ …
