# 未对齐清单（全量复核，2026-10-13）

> 对照原版 Warsmash Java（`warsmash_ref/`）与 C++ 版（`src/`）。
> 联机（Battle.net/UDP）明确排除。
> 已完成 Gap-fill 1–51（技能/行为主体、JASS 1445 natives、地形基础、赏金/XP、复活事件、语音、榜单、昼夜雾基础、占位像素图+静态 stamp、高地 miss、穿刺线/炮击、伤害替换框架、闪避暴击叠加、荆棘吸血语义、死亡序列、非叠加攻击属性、溅射 miss/负伤/无敌监听、w3i 开局科技/随机表、通行 movement-type/边界环/水面定义、可破坏物 targeting/掉落、BlzFrame 全系、地形雾、spell 目标、存档槽、JassAbility 对象等）。

## P0：战斗模拟正确性（影响对局）

- [x] 1. 受击反击/逃跑（Gap-31：`MaybeReturnFire`，idle/default 门控 + 敌对 + 非 PEON + 攻击行为/逃跑；`interruptable` 精化见第 2 项）。
- [x] 2. 打断框架（Gap-32：`IsInterruptable` 虚函数默认 true、Stun false；新订单当 tick 抢占可打断行为，眩晕期队列坍缩为最新一条；反击门控同步精化）。
- [x] 3. 高地 miss（Gap-35：`GetTerrainHeight` 取悬崖层、越层时 `ChanceToMiss`（零售 0.25）掷骰，仅武器攻击、仅对单位；`CUnit::Damage` 与 `ApplyDamage` 双入口，无图/平地不耗 RNG）。
- [x] 4. MLINE 穿刺线 / ARTILLERY 对地（Gap-36：`Hfact/Qfact/damageLoss/spillDist/spillRadius` 解析；溅射族=MSPLASH+ARTILLERY、穿刺族=MLINE+ALINE；ARTILLERY 落点邮戳+对地爆炸、ATTACK_GROUND 点目标链路、抛射体/瞬发双口径点溅射与走廊穿刺）。
- [x] 5. 伤害替换框架（Gap-37：`unit_attack.h` 新增 `CAttackModifierEffect` 接口 + `CUnit` 按攻击索引槽位（安装替换、按安装者清除）；Orb 霜/腐蚀/火、灼热箭、冷箭迁出 `Damage` 硬编码，改 OnAdd/OnRemove 注册；顺带修多 Orb 叠加——单槽不叠加）。
- [x] 6. 闪避/暴击多源叠加（Gap-38：`CUnit` 闪避/暴击改按源列表（同 id 替换、按 id 清除），闪避各源独立掷骰、暴击取最强倍率；`CSimulation::CombatTextKind` MISS/CRIT/BASH 回调供表现层挂浮字；酒仙闪避不再清零 DH 闪避）。
- [x] 7. 荆棘/吸血语义（Gap-39：荆棘改按入射基础伤害（战前快照，含 bonus，抵御前），补 NORMAL 类型门+敌对阵营门+ResistsThorns 门；吸血补 PASSIVE 联盟门+ResistsLeech 门；双边多源取最强；另修魔法免疫 `||`→`&&` 使 UNIVERSAL/门关法术穿透与原版一致）。
 - [x] 8. 死亡序列（Gap-40：死亡 tick 清订单+终结行为；popout 工人——兽族/暗夜 SetWorker 链接、死亡弹出+复位，未亡/人族 outside 不弹，NE 开建扣粮/弹出补粮、完成/取消清链接；食物返还——死亡 tick 即扣 foodUsed/foodMade，移除不再重复扣；死亡视野雾——owner+killer 双 timed VISIBLE 修饰（FogFlashTime 3s/DyingRevealRadius 500，可配）；建造中死亡——FireDeathExplode（unit+explodeBuffId）表现回调，sim 保持正常 decay 记账。注：爆炸质地（buff effect art）数据未到，渲染层订阅回调即可挂载）。
 - [x] 9. 非叠加攻击属性（Gap-41：`CUnit` 新增 key=buffId 的非叠加表——伤害 flat/pct 双表、攻速表，同 key 取最大、跨 key 求和；aura DAMAGE/ATTACK_SPEED 改按 buffId 逐项同步（同 buff 多端取最大、Trueshot+Command 等跨 buff 求和）；InnerFire/Avatar/Bloodlust/Berserk/Amdf 迁出线性 `+=`/`*=`，Roar 改用自有 Broa 与 Inner Fire 求和（BattleRoar/兽卷暂守 Binf 无自创 id）；物品攻速改 stacking 累加、物品/升级/英雄伤害保持 stacking；攻速改零售 `1+clamp(sum,-0.9,4)` 加法模型（Burrow 按乘客 `*2` 遗留乘链叠乘）。残留：护甲类（DEF）仍线性叠加、近战/远程分表（MELEE/RNGD）未做——C++ 行为只打主武器，影响小）。
 - [x] 10. 小项（Gap-42：溅射 miss——主目标单次掷骰，MSPLASH/ARTILLERY 全 AoE 吃 base×MissDamageReduction（0.5，可配，bonus 全额），MSPLASH 出 miss 字、ARTILLERY 静默，非溅射武器整单归零，地面点溅射不再掷 miss；负伤害——CalculateDamage/ApplyDamage 去钳制，负伤按原版回血并 cap max（含 JASS 路径）；无敌——invuln 只压 taken-mod/掉血/击杀，闪避照掷、暴击字照出，荆棘/睡眠/dmgCb/反击照跑。残留：MLINE 走廊仍逐体掷 miss；miss 本身不唤醒睡眠（原版唤醒）；JASS-miss 不触发 damage 事件；溅射外圈 bonus 未乘 ring 系数（Gap-36 遗留）；负伤不触发吸血/重击等落地技）。

## P1：数据/地图正确性（错起来隐蔽）

- [x] 11. 占位像素图（Gap-33：pathTex 解码/缓存、`pathTex` 字段、放置校验改足迹矩形、`BuildingAngle`、吸附偏移；Gap-34：WPM 静态 stamp/死亡切换——建筑创建吸附+blit、死亡/移除 unstamp、Root/Uproot 切换，可破坏物 pathTex/pathTexDeath 双足迹+occH 视野块、复活换回）。
 - [x] 12. w3i 开局科技/升级可用性（Gap-43：`ApplyW3iTechtreeAvailability`（w3i.cpp，可单测）随 `CreateSimulation` 建完玩家即应用——tech 表 ban（max 0）、upgrade 表 0=ban/1=显式可用/2=已研究到 levelAffected+1（0-based，WE 格式经 WC3MapSpecification 交叉确认）；新增 `CPlayer::IsTechtreeAllowedByMax`（ref `isTechtreeAllowedByMax`， possession 计数），`QueueUnit`/`QueueResearch` 在扣费前设门；脚本后跑仍可覆盖。残留：排队中未造完的不计入 cap（原版算在内）；abilitybuilder 主动/被动技能无 maxAllowed 门；w3i 联盟/force/起始资源仍不消费）。
 - [x] 13. w3i 全局随机单位/物品表（Gap-44：`ResolveRandomGroupUnit`（w3i.cpp，可单测）接 `units.doo randomFlag==1`——group 先按 id 找、再按序号兜底，列取 position，行按 % 掷骰（同 flag==2 规矩）；死亡掉落——`CUnit` 存掉落表（`dropSets` 明细 + `droppedItemTable` 指表展开，v8+ 才有表索引，-1/越界无表），死亡 tick 每件独立掷 % 生成（`sim.CreateItem`，未知物品跳过）。残留：randomFlag==0（按等级/物品分类，WE 对话框语义待确认）与预置随机物品 id（`Y..I` 编码）仍回退；drop set 的“组内只掉一件 vs 独立”按独立实现（WE 未书面确认）；JASS 池序列本就可用，无需改动）。
 - [x] 14. 通行/高度/水面三套定义打架（Gap-45：`WpmData::BOUNDARY`（0xF0）+ `IsBoundary`（整 nibble 匹配，`!=0` 会误判陆地/瘟疫/视野块）+ `IsCellPathableFor`（FLY/UNFLYABLE、FLOAT/UNSWIMABLE、AMPHIBIOUS 二者取或、DISABLED 直通，其余 UNWALKABLE，边界环全禁）；`behavior_move` 目标校验与 `IsWorldPointPathable` 改走 movementType；JASS `IsTerrainPathable` 的 fly/float/amph 改读 WPM（无图回退 tile 旗），walk 有图时只认 WPM（浅滩 W3E 水面位仍可走）；`GetWaterHeight` 补 `waterHeightOffset_`（与 ref `(h+off)*128` 对齐，越界回退 off 而非 0）。残留：`FindPath` 无图直连版仍用 DISABLED；kPathBuild 沿用 FOOT 门而非 UNBUILDABLE）。
- [x] 13-dupe. `PlaceRandomUnit/Item` 只用自带表——核销：JASS 池本就是脚本自建（`CreateUnitPool`/`UnitPoolAddUnitType`），原版同样只读自带池；w3i 全局表走 `units.doo` 放置（Gap-44 已接）。非缺口。
- [x] 15. 可破坏物/装饰物（Gap-46：`CTargetTypeInfo::FromString`（ref 名表+缩写，无 C++ 对应项的 ALIVE/SAPPER/NONE 跳过）+ `CDestructableType::targetedAs`（读 `targType`/`btar`，逗号切分，零售 247 行实测 LTlt 含 TREE）；`CDestructable::CanBeTargetedBy` 接 containsAll 门（死体要 DEAD、活体拒纯 DEAD 集，无 receiver 故细分支错误码合并不返回）；死亡掉落——`CDestructable::DeathDropItem/SetDeathDrops/DropDeathItems`（与 unit 同规则），`Damage`/`SetLife` 变死即掉（无尸体 tick），`map_loader` 转交 doo `itemSets` + w3i `itemTable`（v>7 门）；`CreateDestructable` 补 `SetOccluderHeight(type)` 初值。残留：`EnumDestructablesInRect` 仍点判交+跳死（原版足迹矩形+含死）、点选半径硬编码（无 selectionScale/selectable）、terrainDoodad 地形洞未分流、修理数值（buildTime/goldRep/lumberRep）未读）。

## P2：JASS 桩（按常用度）

- [x] 16. `Blz*Frame` 全系（Gap-47：45 个——创建/销毁/按名取（`GetOriginFrame` 按 (type,index) 合成稳定单例）、point/abs/clear/allPoints、visible/enable/focus/click/alpha、text/get/add/limit、model/texture/scale/sprite/vertexColor、value/minmax/step/size、parent/children/tooltip、level、height/width、全局 autoPosition/hideOrigin/TOC；getter 读回 setter 状态+零售默认值，坏 handle 安全零值；`JassFrame` 扩字段 + 父子双向链表，destroy 摘链+回收名字。残留：纯状态层，无真实渲染/点击派发；`BlzFrameSetFocus` 恒 true-on-exists）。
- [x] 17. 地形雾三桩（Gap-48：`JassTerrainFog` 状态 + `terrainFogHook`（controller 推 `DayNightCycle`，headless 为空）；`SetTerrainFogEx` 按 ref 全量存 style/range/density/color（style 钳 0..2，短参忽略）；`ResetTerrainFog` 清 override、hook 回 w3i 基线（`defaultFogSettings` 等价）；老式 `SetTerrainFog` 零售本就无效果，best-effort 取 a/b 为线性 range、c 为 density、颜色不动并注明偏离。残留：渲染线性-only（P3-22）、exp style 存而不用）。
- [x] 18. spell 目标三桩+幻象（Gap-49：`SetSpellEventCallback` 扩参（caster/stage/targetSim/x/y），`behavior_spell` 三处 firing 带目标点，`FireSpellEvent` 按 widget 种类 AllocHandle 进 `eventSpellTarget{Unit,Destructable,Item}Handle` + X/Y（含 queued context 存取）；`GetSpellTargetDestructable/Item` 返回 handle（无则 0，同 Unit 惯例），附带修好同源全死的 `GetSpellTargetUnit/X/Y`；`CUnit::illusion_` 旗（`ABActionSummonCopyOfTarget::Run` 置位，两处生产者皆幻象），`IsUnitIllusion` 读旗；`BlzGetSpecialEffectZ` 点特效报存值、挂接特效报存值+目标 flyHeight。残留：`GetSpellAbility/OrderId` 仍无 ability 上下文；幻象无承伤加成/驱散转移（spells3.h 既有注明）。
- [x] 19. abilitybuilder（Gap-51 切口：`CreateJassAbility` 返回真 ABILITY handle（创建/查 id/挂接/冷却/移除全通工厂路径）；`CreateJassBuff` 返回真 buff handle（code+alias 入 `JassAbilityHandle::isBuff/alias`，可查 id）；buff handle 挂接显式拒绝（无 CBuff 工厂前不挂 do-nothing ability）；未知 ability 码挂接走 `GenericDoNothing` 兜底（Warsmash 容忍，零售为 false，已注明）。残留：回调执行桥（ABActionJass/ABConf action 表）、`CreatePassiveBuff` 系（JASS code 参数）、OrderButton/GameObject/ABTimer 存取仍桩）。
- [x] 20. 单机存档槽（Gap-50：`Save/slot_<safe>/save.w3s` 真目录层（与 gamecache 同根，`..` 等压平成 `_` 防逃逸）；`SaveGame` 写元数据（引擎 tag/map 路径）+ 落盘全部 live gamecache + 记 `lastSaveSlot`（`GetSaveBasicFilename` 可读）；`SaveGameExists/Copy/Rename/Remove` 真文件语义（rename 遇已存在 dst 拒绝）；`LoadGame`（槽存在才记录）/`ReloadGame`/`RestartGame` 只记 `pendingLoad` 供 P3 菜单消费。残留：整局 sim 序列化/恢复无（Load 系不实际读档）；controller 透 `mapPath_` 进元数据）。

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

1. 受击反击/逃跑（P0-1）→ 2. `interruptable` 打断（P0-2）→ 3. 占位像素图+静态 stamp（P1-11，Gap-33/34）→ 4. 高地 miss（P0-3，Gap-35）→ 5. 穿刺线/炮击（P0-4，Gap-36）→ 6. 伤害替换框架（P0-5，Gap-37）→ 7. 闪避暴击叠加（P0-6，Gap-38）→ 8. 荆棘吸血语义（P0-7，Gap-39）→ 9. 死亡序列（P0-8，Gap-40）→ 10. 非叠加攻击属性（P0-9，Gap-41）→ 11. P0-10 小项（Gap-42）→ 12. w3i 开局科技（P1-12，Gap-43）→ 13. w3i 随机表+死亡掉落（P1-13，Gap-44）→ 14. 通行 movement-type/边界环/水面定义（P1-14，Gap-45）→ 15. 可破坏物 targeting/掉落/遮挡（P1-15，Gap-46）→ 16. BlzFrame 全系 45 个（P2-16，Gap-47）→ 17. 地形雾三桩（P2-17，Gap-48）→ 18. spell 目标/幻象/特效 Z（P2-18，Gap-49）→ 20. 存档槽目录层（P2-20，Gap-50）→ 19. JassAbility/Buff 对象（P2-19 切口，Gap-51）→ …
