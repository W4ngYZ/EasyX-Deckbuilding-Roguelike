// =======================================
// rewards.hpp 战利品系统：综合处理所有奖励
//
// 战斗胜利 / 事件奖励 / 未来的遗物、药水都汇聚在本层。
// 纯逻辑层：只生成奖励数据，不渲染、不读输入；选择由上层注入回调。
// 所有随机走注入的 rng::Rng（域 REWARDS）。
// =======================================

#pragma once

#include "map.hpp"   // NodeType
#include "data.hpp"  // allCardFactories
#include "rng.hpp"

// 一场战斗的战利品
struct BattleRewards {
    int gold = 0;                  // 金币（普通 10-20 / 精英 30-40 / Boss 45-60）
    std::vector<Card> cardOptions; // 三张候选卡（全卡池洗牌取前 3，不重复）
};

// 金币档位表（按节点类型）。事件奖励可复用；Boss 档位预留，
// 当前 Boss 胜利即通关不发放（见 run.hpp playNode）。
inline int rollGold(NodeType t, rng::Rng& rewardsRng) {
    switch (t) {
    case NodeType::Elite: return rewardsRng.range(30, 40);
    case NodeType::Boss:  return rewardsRng.range(45, 60);
    default:              return rewardsRng.range(10, 20);
    }
}

// 三选一候选：全卡池洗牌取前 3（决策事件，REWARDS 域）。候选不重复；
// 精简版不做"排除玩家已拥有"与稀有度加权（预留，后续可加）。
inline std::vector<Card> rollCardChoices(rng::Rng& rewardsRng) {
    std::vector<CardFactory> all = allCardFactories();
    std::vector<Card> choices;
    if (all.size() <= 3) {
        for (auto& f : all) choices.push_back(f());
        return choices;
    }
    rewardsRng.shuffle(all);
    for (int i = 0; i < 3; ++i) choices.push_back(all[i]());
    return choices;
}

// 掷一场战斗的全部战利品（金币 + 三张候选卡）
inline BattleRewards rollBattleRewards(NodeType t, rng::Rng& rewardsRng) {
    BattleRewards rw;
    rw.gold = rollGold(t, rewardsRng);
    rw.cardOptions = rollCardChoices(rewardsRng);
    return rw;
}
