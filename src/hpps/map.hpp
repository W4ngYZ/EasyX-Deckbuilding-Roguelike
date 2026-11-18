// =======================================
// map.hpp 地图层：单一路线生成 / 出怪 / 节点行为（篝火）
//
// 只负责"这一关"的路线结构、遭遇抽取与节点效果，不持有图形库。
// 依赖：core.hpp（数据结构）、data.hpp（怪工厂）、rng.hpp（MAP 域流）。
// 随机一律走注入的 rng::Rng（域 MAP）。
// =======================================

#pragma once

#include "core.hpp"
#include "rng.hpp"
#include "data.hpp"

#include <algorithm>
#include <sstream>

// ==================== 地图结构 ====================
enum class NodeType { Battle, Elite, Event, Rest, Boss };

inline const char* nodeTypeName(NodeType t) {
    switch (t) {
    case NodeType::Battle: return "战斗";
    case NodeType::Elite:  return "精英";
    case NodeType::Event:  return "事件";
    case NodeType::Rest:   return "休息";
    case NodeType::Boss:   return "首领";
    }
    return "?";
}

struct MapNode {
    NodeType type = NodeType::Battle;
};

// 单一路线地图：固定 15 个节点（下标 0..14），无分支。
struct Map {
    static constexpr int LENGTH = 15;
    std::vector<MapNode> nodes;

    int size() const { return (int)nodes.size(); }
    NodeType at(int i) const { return nodes[i].type; }
};

// ==================== 生成单一路线 ====================
// 规则（已定稿）：
//   [0]  普通战斗（强制起点）
//   [1..4] 前 5 个节点不出精英
//   [13] 篝火（强制，Boss 前一站恢复）
//   [14] Boss（强制终点）
//   精英 1-2 个，限 [5..12]
//   篝火 3-5 个（含 [13] 的强制篝火，其余随机分布且互不相邻）
// 数量守恒：C + E + EL + F + Boss = 15，平衡 C = E + EL + F
//   → 战斗数恒为 6，事件数 E = 8 - EL - F（所有组合均为整数解，E 最小 1）
// 说明：战斗数恒为 6（较短路线、保证续航），事件数 E = 8 - EL - F（最小 1）。
// 所有随机（精英数 / 篝火数 / 位置洗牌）走 mapRng（域 MAP），
// 生成一次后路线定死，此后零消耗。存档经 RngSet MAP 行可完整复现。
inline Map generateMap(rng::Rng& mapRng) {
    Map m;
    m.nodes.assign(Map::LENGTH, MapNode{ NodeType::Battle });
    m.nodes[13].type = NodeType::Rest;   // Boss 前一站必为篝火
    m.nodes[14].type = NodeType::Boss;

    const int EL = mapRng.range(1, 2);  // 精英 1-2
    const int F = mapRng.range(3, 5);  // 篝火 3-5（含 [13] 强制篝火）
    const int E = 8 - EL - F;           // 事件数（由守恒推出，最小 1）

    // 精英：限 [5..12]，洗牌取前 EL 个（[1..4] 天然不受影响）
    std::vector<int> eliteSlots;
    for (int i = 5; i <= 12; ++i) eliteSlots.push_back(i);
    mapRng.shuffle(eliteSlots);
    for (int k = 0; k < EL; ++k) m.nodes[eliteSlots[k]].type = NodeType::Elite;

    // 篝火（其余 F-1 个）：在 [1..11] 未占位处选，且任意两个篝火不得相邻。
    // [12] 排除：它紧挨强制篝火 [13]。先枚举所有"选 F-1 个且互不相邻"的合法组合，
    // 再等概率抽一组（只消耗 1 次 MAP 域随机）。
    std::vector<int> restSlots;
    for (int i = 1; i <= 11; ++i)
        if (m.nodes[i].type == NodeType::Battle) restSlots.push_back(i);

    std::vector<std::vector<int>> restCombos;
    const int rn = (int)restSlots.size();
    for (int mask = 0; mask < (1 << rn); ++mask) {
        int cnt = 0;
        for (int i = 0; i < rn; ++i) cnt += (mask >> i) & 1;
        if (cnt != F - 1) continue;
        bool ok = true;
        for (int i = 1; i < rn && ok; ++i)
            if (((mask >> i) & 1) && ((mask >> (i - 1)) & 1) && restSlots[i] == restSlots[i - 1] + 1)
                ok = false;                       // 相邻节点同时为篝火 → 非法
        if (!ok) continue;
        std::vector<int> sel;
        for (int i = 0; i < rn; ++i)
            if ((mask >> i) & 1) sel.push_back(restSlots[i]);
        restCombos.push_back(std::move(sel));
    }
    assert(!restCombos.empty() && "no valid rest layout");
    for (int pos : restCombos[mapRng.range(0, (int)restCombos.size() - 1)])
        m.nodes[pos].type = NodeType::Rest;

    // 事件 E 个：剩余自由位洗牌取前 E 个，其余保持 Battle
    std::vector<int> eventSlots;
    for (int i = 1; i <= 12; ++i)
        if (m.nodes[i].type == NodeType::Battle) eventSlots.push_back(i);
    mapRng.shuffle(eventSlots);
    for (int k = 0; k < E; ++k) m.nodes[eventSlots[k]].type = NodeType::Event;

    return m;
}

// ==================== 出怪系统 ====================
// 每种战斗类型一个"排列池"；进入战斗节点时从对应池掷选一场排列：
//   Battle → 前 3 场普通战斗用弱怪池(weak)，之后用强怪池(strong)；
//   Elite  → 精英池(elite)；
//   Boss   → BOSS 池(boss)。
// 每次出怪只掷 1 次 mapRng 选排列（池内元素个数不影响随机数消耗次数）。
using EnemyFactory = std::function<Enemy()>;

struct EncounterSpec {
    std::vector<EnemyFactory> spawn;                                  // 固定构成
    std::function<std::vector<Enemy>(rng::Rng&)> compose = nullptr;   // 动态构成（非空时优先）
};

struct EncounterPool {
    std::vector<EncounterSpec> weak;    // 普通·弱怪池
    std::vector<EncounterSpec> strong;  // 普通·强怪池
    std::vector<EncounterSpec> elite;   // 精英池
    std::vector<EncounterSpec> boss;    // BOSS 池
};

// 完整出怪池（内容取自怪物文本"出怪组合池"）：
//   弱怪池 5 种 / 强怪池 11 种 / 精英池 3 种 / BOSS 池 3 种。
// 生命区间在 spawnEncounter 内以 MAP 域统一掷选（见 Enemy::hpLo/hpHi）。
inline EncounterSpec specFixed(std::vector<EnemyFactory> spawn) {
    EncounterSpec s;
    s.spawn = std::move(spawn);
    return s;
}

// 劫掠者组合：从斧手 / 刺客 / 弩手 / 暴徒 / 追踪手中随机抽 3 名不重复成员。
// 洗牌是 MAP 域事件（mapRng），一次 shuffle 决定阵容。
inline EncounterSpec specRaiderParty() {
    EncounterSpec s;
    s.compose = [](rng::Rng& mapRng) {
        std::vector<EnemyFactory> members = {
            makeRaiderAxe, makeRaiderAssassin, makeRaiderCrossbow,
            makeRaiderBruiser, makeRaiderTracker
        };
        mapRng.shuffle(members);
        std::vector<Enemy> out;
        for (int i = 0; i < 3; ++i) out.push_back(members[i]());
        return out;
        };
    return s;
}

// 飞蝇菌子 + 随机中型/小型史莱姆（MAP 域掷选 1 次）。
inline EncounterSpec specFungiWithSlime() {
    EncounterSpec s;
    s.compose = [](rng::Rng& mapRng) {
        std::vector<EnemyFactory> opts = {
            makeSpikeSlimeMedium, makeSpikeSlimeSmall,
            makeAcidSlimeMedium, makeAcidSlimeSmall
        };
        std::vector<Enemy> out;
        out.push_back(makeFlyingFungi());
        out.push_back(opts[mapRng.range(0, (int)opts.size() - 1)]());
        return out;
        };
    return s;
}

inline EncounterPool makeDefaultEncounterPool() {
    EncounterPool pool;
    // —— 弱怪池 ——
    pool.weak = {
        specFixed({ makeJawWorm }),                                  // 大鳄虫（单只）
        specFixed({ makeCultist }),                                  // 邪教徒（单只）
        specFixed({ makeRedLouse, makeGreenLouse }),                 // 双虱虫
        specFixed({ makeSpikeSlimeSmall, makeSpikeSlimeMedium }),    // 尖刺小 + 尖刺中
        specFixed({ makeAcidSlimeSmall, makeAcidSlimeMedium }),      // 酸液小 + 酸液中
    };
    // —— 强怪池 ——
    pool.strong = {
        specFixed({ makeLooter }),                                   // 抢劫的（单只）
        specFixed({ makeMugger }),                                   // 暴徒（单只）
        specFixed({ makeJawWorm, makeSpikeSlimeMedium }),            // 大颚虫 + 尖刺
        specFixed({ makeJawWorm, makeAcidSlimeMedium }),             // 大颚虫 + 酸液
        specFixed({ makeFungiBeast, makeFungiBeast }),               // 真菌兽 x2
        specRaiderParty(),                                           // 劫掠者组合（三人）
        specFungiWithSlime(),                                        // 飞蝇菌子 + 随机史莱姆
        specFixed({ makeSpikeSlimeLarge }),                          // 尖刺史莱姆（大）
        specFixed({ makeAcidSlimeLarge }),                           // 酸液史莱姆（大）
        specFixed({ makeRedSlaver }),                                // 红奴隶商人
        specFixed({ makeBlueSlaver }),                               // 蓝奴隶商人
    };
    // —— 精英池 ——
    pool.elite = {
        // 三哨卫：中间一只以螺旋催眠开局，左右两只以激光开局（首回合两攻击一塞牌，之后各自交替）
        specFixed({ [] { return makeSentinel(0); }, [] { return makeSentinel(1); }, [] { return makeSentinel(0); } }),
        specFixed({ makeGremlinNob }),                               // 地精大块头
        specFixed({ makeLagavulin }),                                // 乐加维林
    };
    // —— BOSS 池 ——
    pool.boss = {
        specFixed({ makeHexaghost }),                                // 六火亡魂
        specFixed({ makeGuardian }),                                 // 守护者
        specFixed({ makeSlimeBoss }),                                // 史莱姆老大
    };
    return pool;
}

// 进入战斗节点时懒生成敌人：按节点类型 / 阶段掷选排列（MAP 域决策事件）→ 构造敌人列表。
// strongPhase = true 表示已过前 3 场普通战斗（Battle 改用强怪池）。
// 非战斗节点（Event / Rest）无出怪，返回空。
// spec 生成：compose 非空 → compose(mapRng)（动态构成）；否则遍历 spawn 逐个 f()。
inline std::vector<Enemy> spawnEncounter(const EncounterPool& pool, NodeType t,
    rng::Rng& mapRng, bool strongPhase = false) {
    const std::vector<EncounterSpec>* list = nullptr;
    switch (t) {
    case NodeType::Battle: list = strongPhase ? &pool.strong : &pool.weak; break;
    case NodeType::Elite:  list = &pool.elite; break;
    case NodeType::Boss:   list = &pool.boss;  break;
    default: return {};
    }
    if (list->empty()) return {};
    int idx = mapRng.range(0, (int)list->size() - 1);  // 掷选一场排列（每次只掷 1 次）
    const EncounterSpec& spec = (*list)[idx];
    std::vector<Enemy> out;
    if (spec.compose) out = spec.compose(mapRng);
    else for (auto& f : spec.spawn) out.push_back(f());
    // 生命掷选（MAP 域事件）：工厂只写 [hpLo, hpHi]，此处统一落到 hp/maxHp。
    // hpHi <= 0 表示固定生命；range(lo, hi) 在 hi <= lo 时不消耗随机数。
    for (auto& e : out)
        if (e.hpHi > 0) e.hp = e.maxHp = mapRng.range(e.hpLo, e.hpHi);
    return out;
}

// ==================== 节点行为：篝火 ====================
// 恢复当前最大生命值的 35%（取整）。maxHp 可变（升级 / 其它系统可能
// 改变体力上限），故按比例动态计算，不硬编码固定恢复量。
inline void restHeal(BattleContext& ctx) {
    int heal = ctx.player.maxHp * 35 / 100;
    int before = ctx.player.hp;
    ctx.player.hp = std::min(ctx.player.maxHp, ctx.player.hp + heal);
    std::ostringstream oss;
    oss << ctx.player.name << " 休息，恢复 " << (ctx.player.hp - before)
        << " 点生命（" << ctx.player.hp << "/" << ctx.player.maxHp << "）";
    ctx.log(oss.str());
}
