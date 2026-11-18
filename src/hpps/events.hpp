// =======================================
// events.hpp 事件系统：局外随机事件（10 个：第一批 6 + 引擎就绪 4）
//
// 纯逻辑层：不渲染、不读输入。仅读写 Player(deck/hp/maxHp) 与金币，
// 选牌 / 选选项由上层注入回调完成（RunCallbacks::chooseDeckCard / chooseEvent）。
// 只允许在局外（run.hpp playNode 的 NodeType::Event 分支）调用，禁止在战斗
// 进行中修改牌组。
// 随机一律走注入的 rng::Rng（域 EVENTS）。
// =======================================

#pragma once

#include "core.hpp"   // Player / Card
#include "data.hpp"   // allCardFactories / CardFactory
#include "rng.hpp"

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace events {

// 选择牌组中一张牌的回调：返回下标、-1 取消（与 RunCallbacks::chooseRewardCard 约定一致）。
using ChooseDeckCard = std::function<int(const std::vector<Card>&)>;
using LogFn = std::function<void(const std::string&)>;

// 事件中的一个选项：label 供上层渲染；apply 结算该选项效果（纯逻辑，直接改状态）。
struct EventChoice {
    std::string label;
    std::function<void(Player&, int& gold, rng::Rng&,
        const ChooseDeckCard&, const LogFn&)> apply;
};

// 一次已抉择的事件：
//   name/description  供上层渲染
//   canTrigger        该事件当前是否可能出现（只读玩家牌组 / 金币）
//   onEnter           进入即结算（World of Goop 先损失金币），可为空
//   choices           玩家可选项
struct GameEvent {
    std::string name;
    std::string description;
    std::function<bool(const Player&, int gold)> canTrigger;
    std::function<void(Player&, int& gold, rng::Rng&, const LogFn&)> onEnter;
    std::vector<EventChoice> choices;
};

// ---- 移除 / 复制 / 转化 共用的选牌小工具 ----

// 选一张有效卡并返回其下标（取消 / 非法下标返回 -1）。
// 付费移除等需要「先选牌、再扣钱」的场景据此拿到合法下标后再动手。
inline int pickDeckIndex(Player& p, const ChooseDeckCard& choose) {
    if (!choose) return -1;
    int idx = choose(p.deck);
    if (idx < 0 || idx >= (int)p.deck.size()) return -1;
    return idx;
}

// 选一张并移除（免费移除用）
inline void pickAndRemove(Player& p, const ChooseDeckCard& choose, const LogFn& log) {
    int idx = pickDeckIndex(p, choose);
    if (idx < 0) return;
    std::string nm = p.deck[idx].name;
    p.deck.erase(p.deck.begin() + idx);
    log("移除了「" + nm + "」");
}

// 选一张并复制一份加入牌组
inline void pickAndDuplicate(Player& p, const ChooseDeckCard& choose, const LogFn& log) {
    int idx = pickDeckIndex(p, choose);
    if (idx < 0) return;
    Card copy = p.deck[idx];
    p.deck.push_back(copy);
    log("复制了「" + copy.name + "」");
}

// 选一张并原地转化为随机职业卡。
// 结果池 = allCardFactories() 排除与源牌同名（工厂名唯一，最多排除 1 个），
// 每个工厂仅实例化一次、随机消耗固定 1 次 events 域（range 选 1）。
inline void pickAndTransform(Player& p, rng::Rng& eventsRng,
    const ChooseDeckCard& choose, const LogFn& log) {
    int idx = pickDeckIndex(p, choose);
    if (idx < 0) return;
    std::string src = p.deck[idx].name;

    std::vector<Card> pool;
    for (auto& f : allCardFactories()) {
        Card c = f();
        if (c.name != src) pool.push_back(std::move(c));
    }
    if (pool.empty()) { log("没有可供转化的目标卡牌"); return; }

    int pick = eventsRng.range(0, (int)pool.size() - 1);
    p.deck[idx] = pool[pick];     // 原地替换：长度与顺序不变
    log("「" + src + "」转化为了「" + p.deck[idx].name + "」");
}

// ==================== 公共判定小工具 ====================

// 牌组中是否存在攻击牌（翼形雕像 Destroy 简化门槛）。
inline bool hasAttackCard(const std::vector<Card>& deck) {
    for (auto& c : deck)
        if (c.type == CardType::Attack) return true;
    return false;
}

// 随机移除一张指定类型的卡牌（坠落）。
// 无对应类型时不动；选择走 events 域（range 选 1）。
inline void removeRandomOfType(Player& p, CardType type, rng::Rng& eventsRng,
    const LogFn& log, const char* typeName) {
    std::vector<int> idxs;
    for (int i = 0; i < (int)p.deck.size(); ++i)
        if (p.deck[i].type == type) idxs.push_back(i);
    if (idxs.empty()) { log(std::string("牌组中没有") + typeName + "牌"); return; }
    int pick = eventsRng.range(0, (int)idxs.size() - 1);
    int idx = idxs[pick];
    std::string nm = p.deck[idx].name;
    p.deck.erase(p.deck.begin() + idx);
    log("坠落后失去了「" + nm + "」");
}

// ==================== 十个事件 ====================

inline GameEvent purifierEvent() {
    GameEvent e;
    e.name = "净化祭坛（Purifier）";
    e.description = "你发现一座拥有净化力量的祭坛，可以免费移除牌组中的一张卡牌。";
    e.canTrigger = [](const Player& p, int) { return !p.deck.empty(); };
    e.choices = {
        { "Pray（祈祷）：移除一张卡牌",
          [](Player& p, int&, rng::Rng&, const ChooseDeckCard& choose, const LogFn& log) {
              pickAndRemove(p, choose, log);
          } },
        { "Leave（离开）：什么也不做",
          [](Player&, int&, rng::Rng&, const ChooseDeckCard&, const LogFn&) {} },
    };
    return e;
}

inline GameEvent oldBeggarEvent() {
    GameEvent e;
    e.name = "老乞丐（Old Beggar）";
    e.description = "一位老乞丐拦住你，请求你接济。作为回报，他愿意帮你移除一张卡牌。";
    e.canTrigger = [](const Player& p, int gold) { return gold >= 75 && !p.deck.empty(); };
    e.choices = {
        { "Offer Gold（给予金币）：支付 75 金币，移除一张卡牌",
          [](Player& p, int& gold, rng::Rng&, const ChooseDeckCard& choose, const LogFn& log) {
              int idx = pickDeckIndex(p, choose);   // 先选牌，取消则不扣钱
              if (idx < 0) return;
              gold -= 75;
              log("支付了 75 金币");
              std::string nm = p.deck[idx].name;
              p.deck.erase(p.deck.begin() + idx);
              log("移除了「" + nm + "」");
          } },
        { "Leave（离开）：什么也不做",
          [](Player&, int&, rng::Rng&, const ChooseDeckCard&, const LogFn&) {} },
    };
    return e;
}

inline GameEvent clericEvent() {
    GameEvent e;
    e.name = "牧师（The Cleric）";
    e.description = "你遇到一位热情的牧师，他愿意为你提供治疗和净化服务。";
    e.canTrigger = [](const Player&, int gold) { return gold >= 35; };
    e.choices = {
        { "Heal（治疗）：支付 35 金币，恢复最大生命的 25%",
          [](Player& p, int& gold, rng::Rng&, const ChooseDeckCard&, const LogFn& log) {
              gold -= 35;
              int heal = p.maxHp * 25 / 100;
              int before = p.hp;
              p.hp = std::min(p.maxHp, p.hp + heal);
              log("支付 35 金币，恢复 " + std::to_string(p.hp - before) + " 点生命");
          } },
        { "Purify（净化）：支付 50 金币，移除一张卡牌",
          [](Player& p, int& gold, rng::Rng&, const ChooseDeckCard& choose, const LogFn& log) {
              // 该选项自身判金币 ≥ 50，且选牌前就拒绝，避免 [35,49] 时成死选项。
              if (gold < 50) { log("金币不足 50，无法净化"); return; }
              int idx = pickDeckIndex(p, choose);   // 先选牌，取消则不扣钱
              if (idx < 0) return;
              gold -= 50;
              log("支付了 50 金币");
              std::string nm = p.deck[idx].name;
              p.deck.erase(p.deck.begin() + idx);
              log("移除了「" + nm + "」");
          } },
        { "Leave（离开）：什么也不做",
          [](Player&, int&, rng::Rng&, const ChooseDeckCard&, const LogFn&) {} },
    };
    return e;
}

inline GameEvent duplicatorEvent() {
    GameEvent e;
    e.name = "复制祭坛（Duplicator）";
    e.description = "你发现一座具有复制能力的祭坛，可以复制牌组中的一张卡牌。";
    e.canTrigger = [](const Player& p, int) { return !p.deck.empty(); };
    e.choices = {
        { "Pray（祈祷）：复制一张卡牌",
          [](Player& p, int&, rng::Rng&, const ChooseDeckCard& choose, const LogFn& log) {
              pickAndDuplicate(p, choose, log);
          } },
        { "Leave（离开）：什么也不做",
          [](Player&, int&, rng::Rng&, const ChooseDeckCard&, const LogFn&) {} },
    };
    return e;
}

inline GameEvent transmogrifierEvent() {
    GameEvent e;
    e.name = "转化祭坛（Transmogrifier）";
    e.description = "你发现一座拥有转化力量的祭坛，可以将一张卡牌变成另一张随机卡牌。";
    e.canTrigger = [](const Player& p, int) { return !p.deck.empty(); };
    e.choices = {
        { "Pray（祈祷）：转化一张卡牌",
          [](Player& p, int&, rng::Rng& eventsRng, const ChooseDeckCard& choose, const LogFn& log) {
              pickAndTransform(p, eventsRng, choose, log);
          } },
        { "Leave（离开）：什么也不做",
          [](Player&, int&, rng::Rng&, const ChooseDeckCard&, const LogFn&) {} },
    };
    return e;
}

inline GameEvent worldOfGoopEvent() {
    GameEvent e;
    e.name = "黏液世界（World of Goop）";
    e.description = "你一头栽进一片黏液池，身上的金币被黏液吞噬。你可以冒险取回金币，也可以放弃。";
    // 任意幕均可出现（本阶段 Act 1/2/3 均视为「任意」）。
    e.canTrigger = [](const Player&, int) { return true; };
    // 进入即损失 [20,50] 金币，且不超过现有金币（events 域）。
    e.onEnter = [](Player&, int& gold, rng::Rng& eventsRng, const LogFn& log) {
        int loss = std::min(eventsRng.range(20, 50), gold);
        gold -= loss;
        log("黏液吞噬了 " + std::to_string(loss) + " 金币（剩余 " + std::to_string(gold) + "）");
        };
    e.choices = {
        { "Gather Gold（收集金币）：获得 75 金币，损失 11 生命",
          [](Player& p, int& gold, rng::Rng&, const ChooseDeckCard&, const LogFn& log) {
              gold += 75;
              p.hp = std::max(1, p.hp - 11);   // 事件伤害保底 1 点，不致死
              log("获得 75 金币，损失 11 点生命");
          } },
        { "Leave It（放弃）：不再受到伤害，保留剩余金币",
          [](Player&, int&, rng::Rng&, const ChooseDeckCard&, const LogFn&) {} },
    };
    return e;
}

inline GameEvent wingStatueEvent() {
    GameEvent e;
    e.name = "翼形雕像（Wing Statue）";
    e.description = "你发现一座翼形雕像，裂缝中流出金币。可以祈祷，也可以砸碎它。";
    e.canTrigger = [](const Player& p, int) { return !p.deck.empty(); };
    e.choices = {
        { "Pray（祈祷）：损失 7 生命，移除一张卡牌",
          [](Player& p, int&, rng::Rng&, const ChooseDeckCard& choose, const LogFn& log) {
              int idx = pickDeckIndex(p, choose);   // 先选牌，取消则不扣生命
              if (idx < 0) return;
              p.hp = std::max(1, p.hp - 7);        // 事件伤害保底 1 点
              std::string nm = p.deck[idx].name;
              p.deck.erase(p.deck.begin() + idx);
              log("损失 7 点生命，移除了「" + nm + "」");
          } },
        { "Destroy（摧毁）：获得 50~80 金币（需拥有攻击牌）",
          [](Player& p, int& gold, rng::Rng& eventsRng, const ChooseDeckCard&, const LogFn& log) {
              // 简化门槛：原为「至少一张单次伤害 ≥10」，这里降级为「有攻击牌」。
              if (!hasAttackCard(p.deck)) { log("没有足够力量的牌，无法摧毁雕像"); return; }
              int gain = eventsRng.range(50, 80);
              gold += gain;
              log("摧毁雕像，获得 " + std::to_string(gain) + " 金币");
          } },
        { "Leave（离开）：什么也不做",
          [](Player&, int&, rng::Rng&, const ChooseDeckCard&, const LogFn&) {} },
    };
    return e;
}

inline GameEvent joustEvent() {
    GameEvent e;
    e.name = "骑士决斗（The Joust）";
    e.description = "一名骑士为复仇而来，邀请你下注预测决斗的胜者。";
    e.canTrigger = [](const Player&, int gold) { return gold >= 50; };
    e.choices = {
        { "Murderer（凶手）：押 50，胜率 70%，赢 100",
          [](Player&, int& gold, rng::Rng& eventsRng, const ChooseDeckCard&, const LogFn& log) {
              gold -= 50;
              log("押下 50 金币");
              if (eventsRng.range(0, 99) < 70) { gold += 100; log("凶手获胜，赢得 100 金币"); }
              else log("主人获胜，输掉赌注");
          } },
        { "Owner（主人）：押 50，胜率 30%，赢 250",
          [](Player&, int& gold, rng::Rng& eventsRng, const ChooseDeckCard&, const LogFn& log) {
              gold -= 50;
              log("押下 50 金币");
              if (eventsRng.range(0, 99) < 30) { gold += 250; log("主人获胜，赢得 250 金币"); }
              else log("凶手获胜，输掉赌注");
          } },
    };
    return e;
}

inline GameEvent libraryEvent() {
    GameEvent e;
    e.name = "图书馆（The Library）";
    e.description = "你发现一座装饰华丽的图书馆，可以阅读学习，也可以休息。";
    e.canTrigger = [](const Player&, int) { return true; };
    e.choices = {
        { "Read（阅读）：从 20 张随机牌中选择 1 张加入牌组",
          [](Player& p, int&, rng::Rng& eventsRng, const ChooseDeckCard& choose, const LogFn& log) {
              std::vector<CardFactory> all = allCardFactories();
              eventsRng.shuffle(all);
              std::vector<Card> options;
              for (int i = 0; i < 20 && i < (int)all.size(); ++i)
                  options.push_back(all[i]());
              int idx = choose ? choose(options) : -1;
              if (idx < 0 || idx >= (int)options.size()) return;
              p.deck.push_back(options[idx]);
              log("习得了「" + options[idx].name + "」");
          } },
        { "Sleep（睡觉）：恢复最大生命的 33%",
          [](Player& p, int&, rng::Rng&, const ChooseDeckCard&, const LogFn& log) {
              int heal = p.maxHp * 33 / 100;
              int before = p.hp;
              p.hp = std::min(p.maxHp, p.hp + heal);
              log("休息，恢复 " + std::to_string(p.hp - before) + " 点生命");
          } },
    };
    return e;
}

inline GameEvent fallingEvent() {
    GameEvent e;
    e.name = "坠落（Falling）";
    e.description = "你在攀登中失足坠落，必须舍弃一张卡牌来求生。";
    e.canTrigger = [](const Player& p, int) { return !p.deck.empty(); };
    e.choices = {
        { "Land（着陆）：随机移除一张【技能】牌",
          [](Player& p, int&, rng::Rng& eventsRng, const ChooseDeckCard&, const LogFn& log) {
              removeRandomOfType(p, CardType::Skill, eventsRng, log, "技能");
          } },
        { "Channel（引导）：随机移除一张【能力】牌",
          [](Player& p, int&, rng::Rng& eventsRng, const ChooseDeckCard&, const LogFn& log) {
              removeRandomOfType(p, CardType::Power, eventsRng, log, "能力");
          } },
        { "Strike（打击）：随机移除一张【攻击】牌",
          [](Player& p, int&, rng::Rng& eventsRng, const ChooseDeckCard&, const LogFn& log) {
              removeRandomOfType(p, CardType::Attack, eventsRng, log, "攻击");
          } },
    };
    return e;
}

// 全部事件（固定顺序：A1..A10，保证候选集合顺序确定、结果可复现）。
inline std::vector<GameEvent> allEvents() {
    return {
        purifierEvent(), oldBeggarEvent(), clericEvent(),
        duplicatorEvent(), transmogrifierEvent(), worldOfGoopEvent(),
        wingStatueEvent(), joustEvent(), libraryEvent(), fallingEvent(),
    };
}

// 从「当前可触发事件」里掷选一场（仅消耗一次 events 域随机）。
inline GameEvent rollEvent(rng::Rng& eventsRng, const Player& player, int gold) {
    std::vector<GameEvent> candidates;
    for (auto& e : allEvents())
        if (e.canTrigger(player, gold)) candidates.push_back(e);
    if (candidates.empty()) {
        // 空集合兜底：避免 release(NDEBUG) 下越界 UB；也防将来给「恒触发」的
        // 黏液世界加触发条件后出现无候选可掷的情形。
        GameEvent none;
        none.name = "空房间";
        none.description = "这里似乎什么都没有。";
        none.canTrigger = [](const Player&, int) { return true; };
        return none;
    }
    int idx = eventsRng.range(0, (int)candidates.size() - 1);
    return candidates[idx];
}

}  // namespace events