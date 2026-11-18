// =======================================
// run.hpp 冒险流程层（run 系统）：地图推进 / 节点处理 / 战利品 / 通关判定
//
// 纯逻辑层：不渲染、不读输入。战斗交互（渲染 + 玩家回合决策）与
// 奖励选择（三选一）由 CLI/UI 注入的回调完成（见 RunCallbacks）。
// 随机一律走传入的 rng::RunSeed（RngSet 由 run 持有）。
// =======================================

#pragma once

#include "map.hpp"
#include "battle.hpp"
#include "rewards.hpp"
#include "events.hpp"

#include <functional>

// 战斗执行回调：由 CLI/UI 注入（负责渲染 + 玩家回合决策），
// 返回玩家是否存活（true = 战斗胜利）。
using BattleRunner = std::function<bool(BattleContext&)>;

// 上层注入的全部回调（为后续事件奖励 / 商店等交互预留位）
struct RunCallbacks {
    BattleRunner battle;                                  // 战斗执行
    std::function<int(const std::vector<Card>&)> chooseRewardCard;  // 三选一选卡（返回下标，-1 跳过）
    std::function<int(const std::vector<Card>&)> chooseDeckCard;    // 局外事件选牌（返回下标，-1 取消）
    std::function<int(const std::vector<std::string>&)> chooseEvent;  // 局外事件选选项（返回下标，-1 离开）
    std::function<void(const std::string&)> log;          // 规则层消息
};

// 一场完整冒险的状态：玩家（含角色卡组 / 血量）与金币跨战斗保留，
// 地图与出怪池固定。
struct RunState {
    Player player;              // 全局玩家（战斗后回写 HP / 状态；卡组 = player.deck）
    Map map;
    EncounterPool pool;
    int pos = 0;                // 当前待处理节点下标
    int gold = 0;               // 累计金币（战利品，暂无用途，商店/遗物阶段再用）
    bool won = false;           // 通关（Boss 战胜利）
    bool dead = false;          // 阵亡（战斗失败）
};

// 开局：玩家 / 初始卡组（5 Strike + 5 Defend + 1 Bash，卡组是角色属性）/
// 生成地图 / 出怪池。
inline RunState beginRun(rng::Rng& mapRng) {
    RunState rs;
    rs.player.name = "铁甲战士";
    rs.player.hp = rs.player.maxHp = 80;

    for (int i = 0; i < 5; ++i) rs.player.deck.push_back(makeStrike());
    for (int i = 0; i < 5; ++i) rs.player.deck.push_back(makeDefend());
    rs.player.deck.push_back(makeBash());

    rs.map = generateMap(mapRng);
    rs.pool = makeDefaultEncounterPool();
    return rs;
}

inline NodeType currentType(const RunState& rs) { return rs.map.at(rs.pos); }

// 处理当前节点并推进一格：
//   Rest   -> 篝火回血（按 maxHp 35%）
//   Battle / Elite / Boss -> 洗牌开局 -> 出怪 -> 注入 battle 回调战斗
//              -> 胜利发放战利品（金币累计 + 三选一选卡入角色卡组）
//   Event  -> 掷选一场可触发事件（events 域）→ 进入结算 → 渲染选项 → 应用选择
// Boss 胜利置 won、战斗失败置 dead，二者均不再推进。
// 玩家战斗后的 HP / 最大生命回写全局；战斗产生的临时状态（block / status）清空。
inline void playNode(RunState& rs, rng::RunSeed& run, const RunCallbacks& cb) {
    switch (currentType(rs)) {
    case NodeType::Rest: {
        // 篝火 ctx 不注入 Rng 指针（shuffleRng/enemyAi/cardRng 为 nullptr）。
        // 当前 restHeal 不使用随机，安全；一旦误用，battle.hpp 中对应入口的
        // assert 会在 Debug 下立即暴露（正式兜底行为待后续实现）。
        BattleContext ctx;
        ctx.player = rs.player;
        ctx.log = cb.log;
        restHeal(ctx);
        rs.player.hp = ctx.player.hp;
        break;
    }
    case NodeType::Battle:
    case NodeType::Elite:
    case NodeType::Boss: {
        BattleContext ctx;
        ctx.player = rs.player;
        ctx.shuffleRng = &run.rng.shuffle;   // 洗牌流（SHUFFLE 域）
        ctx.enemyAi = &run.rng.enemyAi;      // 敌人意图流（ENEMY_AI 域）
        ctx.cardRng = &run.rng.effects;      // 卡牌效果流（CARD_EFFECTS 域）
        ctx.log = cb.log;
        ctx.gold = &rs.gold;                 // 金币引用（敌人"盗窃"真扣，Q4）
        ctx.drawPile = rs.player.deck;       // 角色卡组复制入战（开局洗牌）
        shufflePile(ctx, ctx.drawPile);      // 开局洗牌（SHUFFLE 域事件）
        // 前 3 场普通战斗走弱怪池，之后走强怪池；精英 / BOSS 各用各池。
        int battlesDone = 0;
        for (int i = 0; i < rs.pos; ++i)
            if (rs.map.at(i) == NodeType::Battle) ++battlesDone;
        const bool strongPhase = battlesDone >= 3;
        ctx.enemies = spawnEncounter(rs.pool, currentType(rs), run.rng.map, strongPhase);  // 出怪（MAP 域事件）
        bool win = cb.battle(ctx);
        rs.player.hp = ctx.player.hp;        // 回写血量
        rs.player.maxHp = ctx.player.maxHp;  // 回写最大生命（狂宴等永久提升不能丢）
        rs.player.block = 0;                 // 战斗结束清临时状态
        rs.player.status.clear();
        rs.player.tempStrength = 0;          // 战斗结束清回合级临时力量（战斗中途胜利时 endPlayerTurn 未执行）
        if (!win) { rs.dead = true; return; }
        if (currentType(rs) == NodeType::Boss) { rs.won = true; return; }  // 通关即止，不发战利品
        // 战斗胜利战利品：金币 + 三选一（决策事件，REWARDS 域）
        BattleRewards rw = rollBattleRewards(currentType(rs), run.rng.rewards);
        rs.gold += rw.gold;
        if (cb.chooseRewardCard) {
            int idx = cb.chooseRewardCard(rw.cardOptions);
            if (idx >= 0 && idx < (int)rw.cardOptions.size())
                rs.player.deck.push_back(rw.cardOptions[idx]);  // 选中的卡加入角色卡组
        }
        break;
    }
    case NodeType::Event: {
        // 局外事件：掷选一场可触发事件（events 域）→ 进入结算 → 渲染选项 → 应用选择。
        events::GameEvent ev = events::rollEvent(run.rng.events, rs.player, rs.gold);
        cb.log("【事件】" + ev.name);
        cb.log(ev.description);
        if (ev.onEnter) ev.onEnter(rs.player, rs.gold, run.rng.events, cb.log);
        std::vector<std::string> opts;
        for (auto& c : ev.choices) opts.push_back(c.label);
        int opt = cb.chooseEvent ? cb.chooseEvent(opts) : -1;
        if (opt >= 0 && opt < (int)ev.choices.size())
            ev.choices[opt].apply(rs.player, rs.gold, run.rng.events, cb.chooseDeckCard, cb.log);
        break;
    }
    }
    ++rs.pos;
}
