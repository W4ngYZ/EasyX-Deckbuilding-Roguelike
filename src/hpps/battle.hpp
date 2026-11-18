// =======================================
// battle.hpp 战斗规则层
//
// 伤害 / 格挡 / 抽牌 / 消耗 / 回合流程的原子实现。
// 随机数一律通过 BattleContext 注入的 Rng 指针（域分离见 rng.hpp）。
//
// ==================== 战斗与效果触发流程（阅读导航） ====================
// 一场战斗按下面流程推进。所有"能力 / 一次性效果"都在固定检查点触发，
// 标记字段集中定义在 core.hpp BattleContext 的"能力标记"区（随战斗构造清零）。
//
//   [回合循环]
//     startPlayerTurn(ctx)             玩家回合开始
//       ① 壁垒：格挡不清零（否则清零）
//       ② playerTurn 置位、lostHpThisTurn 清零（敌人回合的伤害不算"本回合"）
//       ③ 薪火之源：每层 +1 能量；恶魔形态：每层 +2 力量
//       ④ 狱火：回合开始失去 1 生命（触发自己的全体 6 伤）
//       ⑤ 覆甲(PlatedArmor)：回合开始获得 1×层数 格挡，然后覆甲 -1 层
//       ⑥ 仪式(敌)：存活敌人按 Ritual 层数获得力量（跳过玩家第 1 回合）
//       ⑦ 抽 5 张 → drawCards（受 战斗专注 noDraw 限制）
//     → 玩家反复 playCard(ctx, handIdx, targetIdx)   打出卡牌
//       ① 可打出条件：canPlay 判定失败则无法打出（交锋：手牌全为攻击牌）
//       ② 费用计算：腐化(技能)=0（X 费用技能同时 X=0）/ X费用=全部能量 / 踩踏=本回合攻击数减费
//       ③ 自身效果（selfEffects）固定结算 1 次 → 再按目标分发 effects
//          （连环拳：命中标记时对同一目标再结算一次 effects）
//       ④ 攻击牌：attacksPlayed++ → 狂怒(+3格挡) / 杂耍(仅第3张复制入手)
//       ⑤ 去向：能力牌打出后移出本场战斗 / 腐化技能 或 exhaust 标记 → 统一走 exhaustCard；否则进弃牌堆
//     → endPlayerTurn(ctx)             玩家回合结束
//       ① 金属化：获得 3×层数 格挡
//       ② 惊逃：随机打出手牌中 1 张攻击牌攻击随机敌人
//       ③ 弃手牌 / 易伤·虚弱 -1 / 临时力量清零 / 回合级标记清零
//          （noDraw / rage / flameBarrier 等均为"本回合"语义，回合末复位；
//            惊逃是 Power，战斗内持续，不清零）
//     → enemyTurn(ctx)                 敌人回合（执行预掷意图 current，随后
//       planNextIntent 规划下一意图）→ 回到 startPlayerTurn
//       ① 冷却递减 → ①.5 行动前分裂（大史莱姆：分裂代替意图）→ ② act + 置冷却 + 衰减 + onTurnEnd
//       → ③ resolveEnemyTransitions（史莱姆大王等：行动后分裂）
//       → ④ planNextIntent（select 分支 / 冷却 / 分裂 / 逃跑在此层生效）
//     消极状态统一经 applyDebuff 施加（人工制品拦截）；playCard 内技能牌触发"愤怒"。
//
//   [原子规则与触发点]
//     dealDamage(ctx, src, dst, base, offhand=false)
//       base+力量+临时力量 → 虚弱×3/4 → 易伤×3/2
//       巨像：dst=玩家 且 任意敌人有易伤 → 伤害减半
//       残酷：src=玩家 且 目标有易伤 → 伤害 ×(4+层数)/4（每层 +25%）
//       敌人实际掉血 → onDamaged 回调（乐加维林：睡眠中受伤立即醒来并改为眩晕）
//       反伤规则：非脱手、非自伤伤害一定触发反伤；脱手伤害一定不触发。
//         火焰屏障（玩家受击 → 回击攻击者 4，以脱手伤害发出，不引发对弹）
//         锋利外甲（玩家命中带外甲敌 → 玩家失去生命，无视格挡，不触发反伤）
//     loseHp(ctx, e, n)                失去生命统一入口（所有玩家扣血必经之路）
//       玩家且在玩家回合内 → lostHpThisTurn 置位；狱火 → 对全体敌人 6 伤（固定 6）
//       （怨恨读 lostHpThisTurn；敌人回合的伤害不算"本回合"）
//     gainBlock(ctx, self, base, fromCard=false)
//       base+敏捷 → 坚定不移：玩家本回合首次"从卡牌"获得格挡翻倍（fromCard=true）
//       → 脆弱：玩家获得格挡 ×3/4
//       → 势不可当：玩家获得格挡 → 随机敌人 6×层数 伤（脱手伤害，决策事件 cardRng）
//     drawCards(ctx, n)
//       noDraw（战斗专注）时直接拒绝，其余正常抽牌 / 空堆洗回弃牌堆
//       地狱狂徒：抽到名字含"打击"的牌 → 对随机敌人自动打出一次（决策事件，cardRng）
//     exhaustCard(ctx, c)              所有"卡牌被消耗"的统一入口（禁止直接写 exhaustPile）
//       → 置 exhaustedThisTurn → 战鼓(+1能量) / 无惧疼痛(+3格挡×层) / 黑暗之拥(抽1×层)
//       （邪眼 / 被遗忘的仪式读 exhaustedThisTurn 判断"本回合是否消耗过牌"）
//   [自动打出]
//     autoPlayCard(ctx, c, targetIdx)  自动打出：费用视为 0 结算效果、X 费用视为 0，
//       不触发 狂怒 / 杂耍 / attacksPlayed 等手动打出专属触发
//       （倾泻 / 破灭 / 地狱狂徒 / 惊逃使用）。
//       targetIdx ≥0 时 SingleEnemy 卡打指定目标（地狱狂徒 / 惊逃用），否则打第一个存活敌人。
// =======================================

#pragma once

#include "core.hpp"
#include "rng.hpp"

#include <algorithm>
#include <cassert>
#include <sstream>

// ==================== 核心规则 ====================

inline void loseHp(BattleContext& ctx, Entity& e, int n);  // 前向声明（定义在 dealDamage 之后）
inline void exhaustCard(BattleContext& ctx, Card& c);       // 前向声明（定义在 drawCards 之后）
inline void autoPlayCard(BattleContext& ctx, Card& c, int targetIdx = -1);  // 前向声明（定义在 playCard 之后）

// offhand = true 表示"脱手伤害"（能力 / 状态 / 反伤自身自动产生）；默认 false = 主动伤害
// （打出一张牌、敌人意图实招）。反伤统一规则：
//   R1 单位受到【非脱手、非自伤】伤害 → 一定触发其反伤；
//   R2 脱手伤害 → 一定不触发任何反伤（反伤回击本身也以 offhand=true 发出，杜绝 ping-pong）。
inline void dealDamage(BattleContext& ctx, Entity& src, Entity& dst, int base, bool offhand = false) {
    if (!dst.inPlay()) return;
    int dmg = base + src.getStatus(Status::Strength) + src.tempStrength;  // tempStrength 回合末清零
    if (src.getStatus(Status::Weak) > 0)       dmg = dmg * 3 / 4;
    if (dst.getStatus(Status::Vulnerable) > 0) dmg = dmg * 3 / 2;

    // —— 能力标记：巨像（dst=玩家 且 攻击者自身有易伤 → 伤害减半）——
    if (&dst == &ctx.player && ctx.colossus && src.getStatus(Status::Vulnerable) > 0)
        dmg /= 2;
    // —— 能力标记：残酷（src=玩家 且 目标有易伤 → 每层 +25%）——
    if (&src == &ctx.player && ctx.cruelty > 0 && dst.getStatus(Status::Vulnerable) > 0)
        dmg = dmg * (4 + ctx.cruelty) / 4;

    dmg = std::max(0, dmg);

    int absorbed = std::min(dst.block, dmg);
    dst.block -= absorbed;
    int lost = dmg - absorbed;
    // 玩家扣血一律走 loseHp 统一入口（触发失去生命类标记：怨恨 / 狱火）
    if (&dst == &ctx.player) {
        loseHp(ctx, dst, lost);
    }
    else {
        dst.hp -= lost;
        // 敌人受伤回调（乐加维林：睡梦中实际掉血 → 立即醒来）
        if (lost > 0 && dst.onDamaged) dst.onDamaged(ctx, dst);
    }

    std::ostringstream oss;
    oss << src.name << " → " << dst.name << " 造成 " << dmg << " 点伤害";
    if (absorbed) oss << "（格挡 " << absorbed << "）";
    oss << "，剩余生命 " << std::max(0, dst.hp);
    ctx.log(oss.str());

    // —— 能力标记：火焰屏障（dst=玩家 受"非脱手"伤害 → 对攻击者反伤 4；符合 R2）——
    if (!offhand && &dst == &ctx.player && ctx.flameBarrier && &src != &dst) {
        ctx.log("火焰屏障：反击 4 点伤害");
        dealDamage(ctx, ctx.player, src, 4, true);   // 反伤以脱手发出，不触发对方反伤
    }

    // —— 状态：锋利外甲（玩家以"非脱手、非自伤"伤害命中带锋利外甲的敌人 → 玩家失去生命）——
    // 按规格以 loseHp 结算：无视格挡，也不受攻击者力量 / 虚弱 / 易伤影响（只按层数扣血）。
    if (!offhand && &src == &ctx.player && &dst != &ctx.player) {
        int layers = dst.getStatus(Status::SharpHide);
        if (layers > 0) {
            ctx.log(dst.name + " 锋利外甲：" + ctx.player.name + " 失去 " + std::to_string(layers) + " 点生命");
            loseHp(ctx, ctx.player, layers);
        }
    }
}

// 失去生命统一入口：扣血并触发"失去生命"类能力标记（怨恨 / 狱火）。
// 所有"玩家扣血"路径（敌人攻击 / 自伤 / 狱火回合开始掉血）都必须走这里；
// 只在玩家回合内触发（playerTurn），敌人回合的伤害不算"本回合失去生命"。
inline void loseHp(BattleContext& ctx, Entity& e, int n) {
    if (n <= 0) return;
    e.hp -= n;
    if (&e == &ctx.player && ctx.playerTurn) {
        ctx.lostHpThisTurn = true;
        if (ctx.infernoFire) {                    // 狱火：每次失去生命对全体敌人 6 伤（固定 6，不缩放；脱手伤害）
            ctx.log("狱火：对所有敌人造成 6 点伤害");
            for (auto& en : ctx.enemies)
                if (en.inPlay()) dealDamage(ctx, ctx.player, en, 6, true);
        }
    }
}

// fromCard = true 表示该格挡来自正在结算的卡牌效果（Block 工厂 / 卡牌内联效果）。
// 坚定不移只翻倍"从卡牌中获得"的格挡，能力 / 状态（金属化 / 覆甲 / 无惧疼痛）不触发。
inline void gainBlock(BattleContext& ctx, Entity& self, int base, bool fromCard = false) {
    int b = std::max(0, base + self.getStatus(Status::Dexterity));
    bool isPlayer = (&self == &ctx.player);

    // —— 能力标记：坚定不移（玩家本回合首次从卡牌获得格挡翻倍）——
    if (isPlayer && fromCard && ctx.steadfast && !ctx.steadfastUsed) {
        b *= 2;
        ctx.steadfastUsed = true;
    }
    // —— 状态：脆弱（玩家获得格挡 ×3/4；顺序：base+Dex → 坚定不移翻倍 → 脆弱 ×3/4 → 入账）——
    if (isPlayer && self.getStatus(Status::Frail) > 0)
        b = b * 3 / 4;

    self.block += b;
    std::ostringstream oss;
    oss << self.name << " 获得 " << b << " 点格挡（当前 " << self.block << "）";
    ctx.log(oss.str());

    // —— 能力标记：势不可当（玩家获得格挡 → 随机敌人 6×层数 伤；决策事件走 cardRng）——
    // 该伤害属"脱手伤害"，不触发对方反伤。
    if (isPlayer && ctx.relentless > 0) {
        std::vector<int> targets;
        for (size_t i = 0; i < ctx.enemies.size(); ++i)
            if (ctx.enemies[i].inPlay()) targets.push_back((int)i);
        if (!targets.empty()) {
            int idx = targets[ctx.cardRng->range(0, (int)targets.size() - 1)];
            dealDamage(ctx, ctx.player, ctx.enemies[idx], 6 * ctx.relentless, true);
        }
    }
}

inline void tickStatus(Entity& e, Status s, int delta) {
    auto it = e.status.find(s);
    if (it == e.status.end()) return;
    it->second += delta;
    if (it->second <= 0) e.status.erase(it);
}

// 洗牌是"事件"，必须消耗随机流（rng.hpp 域：SHUFFLE），由 ctx.shuffleRng 注入。
// 未注入随机流时用 assert 立即暴露（正式兜底行为待后续实现）。
inline void shufflePile(BattleContext& ctx, std::vector<Card>& pile) {
    assert(ctx.shuffleRng && "shuffleRng not injected");
    ctx.shuffleRng->shuffle(pile);
}

// —— 手牌上限（全局战斗规则）——
// 抽牌时若手牌已达上限，本次抽到的牌直接进入弃牌堆，不进入手牌。
// 说明：仅约束"抽牌"路径；卡牌效果主动把牌放入手牌（复制/回收/加入手牌）不受此限。
inline constexpr int kMaxHandSize = 9;

inline void drawCards(BattleContext& ctx, int n) {
    // —— 能力标记：战斗专注（本回合不能再抽牌）——
    if (ctx.noDraw) { ctx.log("本回合无法抽牌（战斗专注）"); return; }
    for (int i = 0; i < n; ++i) {
        if (ctx.drawPile.empty()) {
            if (ctx.discardPile.empty()) { ctx.log("没有可抽的牌"); return; }
            ctx.drawPile = std::move(ctx.discardPile);
            ctx.discardPile.clear();
            shufflePile(ctx, ctx.drawPile);  // 洗牌事件（SHUFFLE 域）
            ctx.log("弃牌堆洗入抽牌堆");
        }
        // —— 手牌上限：满 9 张时，抽到的牌直接进入弃牌堆 ——
        if ((int)ctx.hand.size() >= kMaxHandSize) {
            ctx.log("手牌已满（" + std::to_string(kMaxHandSize) + "），"
                    + ctx.drawPile.back().name + " 直接进入弃牌堆");
            ctx.discardPile.push_back(std::move(ctx.drawPile.back()));
            ctx.drawPile.pop_back();
            continue;
        }
        ctx.hand.push_back(std::move(ctx.drawPile.back()));
        ctx.drawPile.pop_back();
        // —— 能力标记：地狱狂徒（抽到名字含"打击"的牌 → 对随机敌人打出一次）——
        if (ctx.hellbound &&
            ctx.hand.back().name.find("打击") != std::string::npos) {
            Card drawn = std::move(ctx.hand.back());
            ctx.hand.pop_back();
            std::vector<int> targets;
            for (size_t i = 0; i < ctx.enemies.size(); ++i)
                if (ctx.enemies[i].inPlay()) targets.push_back((int)i);
            if (!targets.empty()) {
                int idx = targets[ctx.cardRng->range(0, (int)targets.size() - 1)];
                ctx.log("地狱狂徒：自动对随机敌人打出 " + drawn.name);
                autoPlayCard(ctx, drawn, idx);
            }
            if ((ctx.corruption && drawn.type == CardType::Skill) || drawn.exhaust)
                exhaustCard(ctx, drawn);           // 去向与手动打出一致
            else
                ctx.discardPile.push_back(std::move(drawn));
        }
    }
}

// ==================== 消耗统一入口 ====================
// 所有"卡牌被消耗"的路径都必须走这里（禁止直接向 exhaustPile 写入），
// 集中触发消耗系能力。触发顺序：置标记 → 战鼓回能 → 移入消耗堆 →
// 无惧疼痛(格挡) → 黑暗之拥(抽牌)。邪眼 / 被遗忘的仪式读 exhaustedThisTurn。
inline void exhaustCard(BattleContext& ctx, Card& c) {
    ctx.exhaustedThisTurn = true;                 // 本回合消耗过牌的标记（回合末清零）
    if (c.gainEnergyOnExhaust) {                  // 战鼓：这张牌被消耗时 +1 能量
        ctx.energy += 1;
        ctx.log("战鼓：消耗 " + c.name + " 获得 1 点能量");
    }
    ctx.exhaustPile.push_back(std::move(c));
    if (ctx.feelNoPain > 0)                       // 无惧疼痛：每消耗一张牌 +3 格挡（×层数）
        gainBlock(ctx, ctx.player, 3 * ctx.feelNoPain);
    if (ctx.darkEmbrace > 0)                      // 黑暗之拥：每消耗一张牌抽 1（×层数）
        drawCards(ctx, ctx.darkEmbrace);
}

// ==================== Effect 工厂 ====================
// 所有工厂返回 EffectFn（签名 void(ctx, self, target)），组合进 Card 的
// effects / selfEffects。target 语义由卡牌 TargetMode 决定：Self = 玩家自身 /
// SingleEnemy = 被选敌人 / AllEnemies = 对每个存活敌人各调一次 /
// RandomEnemy = 目标由效果内部随机选。各工厂用法见定义处注释；
// 组合示例与"如何加新卡"见 data.hpp 头部说明。
inline EffectFn Damage(int n) {
    return [n](BattleContext& ctx, Entity& self, Entity& t) { dealDamage(ctx, self, t, n); };
}
inline EffectFn Block(int n) {
    return [n](BattleContext& ctx, Entity& self, Entity&) { gainBlock(ctx, self, n, true); };
}
// —— 负面状态统一入口：经人工制品拦截（抵消一次并 -1 层）——
// 注意：熔融之拳的"易伤翻倍"是翻倍已有层数（非施加），必须直写 addStatus，
// 不得走 applyDebuff，否则会错误消耗人工制品层。
inline bool isDebuff(Status s) {
    return s == Status::Vulnerable || s == Status::Weak ||
        s == Status::Frail || s == Status::Entangle;
}

inline void applyDebuff(BattleContext& ctx, Entity& target, Status s, int n) {
    assert(isDebuff(s) && "applyDebuff 只能施加负面状态");
    if (n <= 0) return;
    if (target.getStatus(Status::Artifact) > 0) {
        target.addStatus(Status::Artifact, -1);
        ctx.log(target.name + " 人工制品抵消了一次负面状态");
        return;
    }
    target.addStatus(s, n);
    ctx.log(target.name + " 获得 " + std::to_string(n) + " 层" + statusName(s));
}

// 施加状态：负面状态走 applyDebuff（人工制品拦截），其余直写。
inline EffectFn ApplyStatus(Status s, int n) {
    return [s, n](BattleContext& ctx, Entity&, Entity& t) {
        if (isDebuff(s)) { applyDebuff(ctx, t, s, n); return; }
        t.addStatus(s, n);
        std::ostringstream oss;
        oss << t.name << " 获得 " << n << " 层" << statusName(s);
        ctx.log(oss.str());
        };
}
inline EffectFn Draw(int n) {
    return [n](BattleContext& ctx, Entity&, Entity&) { drawCards(ctx, n); };
}

// 回复能量：给 ctx.energy 加 n，可配合自伤（Bloodletting）或高费卡使用。
// 本回合已支付费用的能量不可退回，故只做"追加"。
inline EffectFn GainEnergy(int n) {
    return [n](BattleContext& ctx, Entity&, Entity&) {
        ctx.energy += n;
        std::ostringstream oss;
        oss << "获得 " << n << " 点能量（当前 " << ctx.energy << "）";
        ctx.log(oss.str());
        };
}

// ==================== 进阶效果工厂 ====================
// 需要随机时一律走 ctx.cardRng（域 CARD_EFFECTS，由 run 注入），禁止 rand()。
// 数值缩放用 ValueFn 在"解析时"读取实体状态 / 血量，天然反映当前战斗状态。
using ValueFn = std::function<int(const Entity&)>;

// 自伤：直接扣自身 HP，无视格挡 / 力量加成（贴合 STS 语义）。
// 需要走格挡结算的自伤可改用 dealDamage(ctx, self, self, n)。
inline EffectFn SelfDamage(int n) {
    return [n](BattleContext& ctx, Entity& self, Entity&) {
        if (&self == &ctx.player) loseHp(ctx, self, n);  // 玩家自伤走统一入口（触发怨恨 / 狱火）
        else self.hp -= n;
        std::ostringstream oss;
        oss << self.name << " 失去 " << n << " 点生命，剩余生命 " << std::max(0, self.hp);
        ctx.log(oss.str());
        };
}

// 按来源实体数值缩放伤害：base + fn(self)。fn 读玩家 Strength / 血量等。
// 注意：dealDamage 会【自动】叠加来源力量 / 目标易伤并结算格挡，fn 的结果还会再叠一层。
// 因此"按力量缩放的卡"直接用 Damage(base) 即可（例：Heavy Blade = 14+力量 → Damage(14)）；
// DamageScaled 适合读取其它状态（格挡 / 敏捷 / 血量）做缩放。
inline EffectFn DamageScaled(int base, ValueFn fn) {
    return [base, fn](BattleContext& ctx, Entity& self, Entity& t) {
        dealDamage(ctx, self, t, base + fn(self));
        };
}

// 按目标实体数值缩放伤害：base + fn(target)。fn 读敌人血量 / 状态等。
inline EffectFn DamageByTarget(int base, ValueFn fn) {
    return [base, fn](BattleContext& ctx, Entity& self, Entity& t) {
        dealDamage(ctx, self, t, base + fn(t));
        };
}

// 随机攻击一个存活敌人（决策事件，消耗 cardRng 一次）。
inline EffectFn RandomTargetDamage(int n) {
    return [n](BattleContext& ctx, Entity& self, Entity&) {
        std::vector<int> targets;
        for (size_t i = 0; i < ctx.enemies.size(); ++i)
            if (ctx.enemies[i].inPlay()) targets.push_back((int)i);
        if (targets.empty()) return;
        int idx = targets[ctx.cardRng->range(0, (int)targets.size() - 1)];
        dealDamage(ctx, self, ctx.enemies[idx], n);
        };
}

// 随机消耗手牌：每消耗一张掷一次（每次都是决策事件）。走 exhaustCard 统一入口。
inline EffectFn ExhaustRandom(int n) {
    return [n](BattleContext& ctx, Entity&, Entity&) {
        for (int k = 0; k < n && !ctx.hand.empty(); ++k) {
            int idx = ctx.cardRng->range(0, (int)ctx.hand.size() - 1);
            Card c = std::move(ctx.hand[idx]);
            ctx.hand.erase(ctx.hand.begin() + idx);
            ctx.log("随机消耗了一张牌");
            exhaustCard(ctx, c);
        }
        };
}

// 消耗全部手牌，每张对同一目标造成 perCard 点伤害（Fiend Fire）。
// 正在打出的卡已先从手牌移除，不会计入张数；本卡自带消耗用 Card 的 exhaust 字段。
// 每张都走 exhaustCard（黑暗之拥抽来的新牌不会被本效果继续消耗，因为先移出再结算）。
inline EffectFn ExhaustHandAndDamage(int perCard) {
    return [perCard](BattleContext& ctx, Entity& self, Entity& t) {
        std::vector<Card> tmp;
        for (auto& c : ctx.hand) tmp.push_back(std::move(c));
        ctx.hand.clear();
        for (auto& c : tmp) exhaustCard(ctx, c);
        if (!tmp.empty()) dealDamage(ctx, self, t, perCard * (int)tmp.size());
        };
}

// ==================== 玩家选择效果（输入钩子） ====================
// 需要玩家选一张卡时走 ctx.chooseCard（由 UI/CLI 注入）：
//   返回 0..n-1 中的源牌堆下标表示选中；返回 -1 表示取消；
//   回调为空时默认选第一张（供 core 冒烟测试等无 UI 场景使用，确定性、零随机）。
// 选择本身是玩家输入，不消耗任何 Rng 流。
inline void moveChosenCard(BattleContext& ctx, std::vector<Card>& src,
    std::vector<Card>& dst, const char* label) {
    std::vector<int> cands;
    for (size_t i = 0; i < src.size(); ++i) cands.push_back((int)i);
    if (cands.empty()) {
        ctx.log(std::string(label) + "为空，无可选择");
        return;
    }
    ChoiceRequest req{ cands, &src, label };
    int idx = ctx.chooseCard ? ctx.chooseCard(req) : cands.front();
    if (idx < 0) { ctx.log("已取消选择"); return; }
    Card moved = std::move(src[idx]);
    src.erase(src.begin() + idx);
    if (&dst == &ctx.exhaustPile) exhaustCard(ctx, moved);  // 消耗目标统一走 exhaustCard
    else                          dst.push_back(std::move(moved));
    std::ostringstream oss;
    oss << "已从" << label << "移动一张牌";
    ctx.log(oss.str());
}

// 弃牌堆选 1 张 -> 抽牌堆顶（Headbutt）
inline EffectFn ChosenFromDiscardToDraw() {
    return [](BattleContext& ctx, Entity&, Entity&) {
        moveChosenCard(ctx, ctx.discardPile, ctx.drawPile, "弃牌堆");
        };
}

// 手牌选 1 张 -> 消耗堆（True Grit）
inline EffectFn ExhaustChoose() {
    return [](BattleContext& ctx, Entity&, Entity&) {
        moveChosenCard(ctx, ctx.hand, ctx.exhaustPile, "手牌");
        };
}

// 抽牌堆选 1 张 -> 手牌（Seek）
inline EffectFn ChosenFromDrawToHand() {
    return [](BattleContext& ctx, Entity&, Entity&) {
        moveChosenCard(ctx, ctx.drawPile, ctx.hand, "抽牌堆");
        };
}

// ==================== 卡牌堆写入（复制类卡牌通用） ====================
// 目标牌堆：手牌 / 抽牌堆 / 弃牌堆 / 消耗牌堆。
// 消耗牌堆必须走 exhaustCard 统一入口（触发 战鼓 / 无惧疼痛 / 黑暗之拥）。
enum class PileDest { Hand, Draw, Discard, Exhaust };

inline void addCardToPile(BattleContext& ctx, Card c, PileDest dst) {
    std::string name = c.name;
    switch (dst) {
    case PileDest::Hand:    ctx.hand.push_back(std::move(c)); break;
    case PileDest::Draw:    ctx.drawPile.push_back(std::move(c)); break;
    case PileDest::Discard: ctx.discardPile.push_back(std::move(c)); break;
    case PileDest::Exhaust: exhaustCard(ctx, c); break;
    }
    static const char* pileName[] = { "手牌", "抽牌堆", "弃牌堆", "消耗堆" };
    ctx.log("将 " + name + " 加入" + pileName[(int)dst]);
}

// 通用工厂：把一张指定卡加入任意牌堆（复制 / 生成类卡牌用，零随机）。
inline EffectFn AddCardToPile(Card c, PileDest dst) {
    return [c, dst](BattleContext& ctx, Entity&, Entity&) {
        addCardToPile(ctx, c, dst);
        };
}

// "此牌"的复制品加入指定牌堆（愤怒 / 群情激愤）：读 ctx.resolvingCard，
// 复制出含完整效果定义的独立副本；复制品不会再次触发复制（除非再被打出）。
inline EffectFn AddCopyOfSelf(PileDest dst) {
    return [dst](BattleContext& ctx, Entity&, Entity&) {
        if (!ctx.resolvingCard) { ctx.log("没有正在结算的牌可复制"); return; }
        addCardToPile(ctx, *ctx.resolvingCard, dst);
        };
}

// 手牌中选 1 张攻击牌 / 能力牌，复制一张加入手牌（双持）。
// 选择是玩家输入（不消耗随机），原牌保留，复制品为独立副本。
inline EffectFn CopyChosenToHand() {
    return [](BattleContext& ctx, Entity&, Entity&) {
        std::vector<int> cands;
        for (size_t i = 0; i < ctx.hand.size(); ++i)
            if (ctx.hand[i].type == CardType::Attack || ctx.hand[i].type == CardType::Power)
                cands.push_back((int)i);
        if (cands.empty()) { ctx.log("手牌中没有可复制的攻击牌或能力牌"); return; }
        ChoiceRequest req{ cands, &ctx.hand, "手牌（攻击牌或能力牌）" };
        int idx = ctx.chooseCard ? ctx.chooseCard(req) : cands.front();
        if (idx < 0) { ctx.log("已取消选择"); return; }
        Card copy = ctx.hand[idx];
        ctx.hand.push_back(std::move(copy));
        ctx.log("将一张牌的复制品加入手牌");
        };
}

// ==================== 敌人意图：pattern 表推进（pre-roll） ====================
// 【选择】分三种：
//   - e.select 非空：由 select 决定下标（越界无条件兜底为 0）；
//   - select 为空且开局（patternIdx == -1）或刚执行过 reroll 意图：enemyAi 随机起点；
//   - 其余：按表顺序循环（旧默认路径）。
// 选定后立即调用 roll 预掷数值并写入 self.rolls，再由 describe 生成精确描述
// 存入 current.desc —— 玩家看到的值 = 执行用的值（pre-roll 纪律）。
// Rng 只在"选择起点"和"预掷数值"时被消耗，act 执行阶段零消耗。
inline void planNextIntent(BattleContext& ctx, Enemy& e) {
    assert(ctx.enemyAi && "enemyAi not injected");
    if (e.patterns.empty()) {                    // 空表：日志 + 空意图，不崩
        ctx.log("敌人未配置意图表");
        e.current = Intent{}; e.rolls.clear();
        return;
    }
    if (e.cooldown.size() != e.patterns.size())
        e.cooldown.assign(e.patterns.size(), 0);   // 冷却数组与 pattern 表对齐

    e.lastPatternIdx = e.patternIdx;
    if (e.select) {
        int p = e.select(ctx, e);
        if (p < 0 || p >= (int)e.patterns.size()) p = 0;   // 越界无条件兜底（勿只 assert）
        assert(p >= 0 && p < (int)e.patterns.size());
        e.patternIdx = p;
    }
    else if (e.patternIdx < 0 || e.patterns[e.patternIdx].reroll) {
        e.patternIdx = ctx.enemyAi->range(0, (int)e.patterns.size() - 1);
    }
    else {
        e.patternIdx = (e.patternIdx + 1) % (int)e.patterns.size();
    }

    auto& p = e.patterns[e.patternIdx];
    e.rolls.clear();
    if (p.roll) p.roll(ctx, e);  // 预掷数值（决策事件，enemyAi 流）
    std::string desc = p.describe ? p.describe(ctx, e) : std::string{};
    e.current = Intent{ desc, p.act };
}

// ==================== 敌人过渡结算（分裂 / 新敌补规划） ====================
// 分裂走"后置重建"：先把存活且配了 split 的敌人的分裂产物收集为新列表，
// 全部收集完毕后才整体回写 ctx.enemies，遍历期间绝不增删。
// 约束：split 回调只读 ctx.enemies，阈值判断由回调内部完成（返回空 vector = 不分裂）。
// planNewEnemies = false 时不为分裂产物补规划（enemyTurn 的 ④ 会统一规划，避免重复消耗 RNG）。
// onlyReplacing = true 时只处理 splitReplacesAction 的敌人（行动前分裂，见 enemyTurn ①.5）。
// ★调用点：playCard / autoPlayCard 的【函数最末端】（所有对目标引用的使用之后），
//   enemyTurn 的 ①.5（传 false, true）与 ③（传 false）。
inline void resolveEnemyTransitions(BattleContext& ctx, bool planNewEnemies = true,
    bool onlyReplacing = false) {
    // 先探测，避免无谓重建；仅当存在"在场且配了 split"的敌人才做重建。
    bool hasSplit = false;
    for (auto& e : ctx.enemies)
        if (e.inPlay() && e.split && (!onlyReplacing || e.splitReplacesAction)) { hasSplit = true; break; }
    if (!hasSplit) return;

    std::vector<Enemy> out;
    bool changed = false;
    for (auto& e : ctx.enemies) {
        if (e.inPlay() && e.split && (!onlyReplacing || e.splitReplacesAction)) {
            std::vector<Enemy> parts = e.split(ctx, e);
            if (!parts.empty()) {
                ctx.log(e.name + " 分裂！");
                for (auto& part : parts) {
                    // 分裂产物统一重置，禁止沿用父敌残留意图 / 冷却
                    part.patternIdx = -1;
                    part.current = Intent{};
                    part.rolls.clear();
                    part.cooldown.clear();
                    out.push_back(std::move(part));
                }
                changed = true;
                continue;                       // 原敌被替换
            }
        }
        out.push_back(std::move(e));
    }
    ctx.enemies = std::move(out);    // 必须无条件回写（元素已被 move）
    if (changed && planNewEnemies)
        for (auto& e : ctx.enemies)
            if (e.inPlay() && !e.current.act) planNextIntent(ctx, e);  // 新敌补规划
}

// ==================== Playing cards ====================
inline bool playCard(BattleContext& ctx, int handIdx, int targetIdx) {
    if (handIdx < 0 || handIdx >= (int)ctx.hand.size()) return false;
    const Card& c = ctx.hand[handIdx];
    // —— 校验顺序（统一为此序）：不可打出 → 缠绕 → canPlay → 费用 ——
    if (c.unplayable) {
        ctx.log("无法打出 " + c.name + "：不可打出");
        return false;
    }
    if (c.type == CardType::Attack && ctx.player.getStatus(Status::Entangle) > 0) {
        ctx.log("无法打出 " + c.name + "：被缠绕，本回合不能打出攻击牌");
        return false;
    }
    // 可打出条件（交锋：手牌全为攻击牌；autoPlayCard 自动打出不检查此条件）
    if (c.canPlay && !c.canPlay(ctx)) {
        ctx.log("无法打出 " + c.name + "：条件不满足");
        return false;
    }
    if (c.target == TargetMode::SingleEnemy) {
        if (targetIdx < 0 || targetIdx >= (int)ctx.enemies.size() || !ctx.enemies[targetIdx].inPlay()) {
            ctx.log("无效目标");
            return false;
        }
    }

    // —— 费用计算（能力标记：X费用 / 腐化 / 踩踏）——
    // 腐化优先：技能费用变 0；若同时是 X 费用牌，则 X = 花费的能量 = 0。
    const bool freeSkill = ctx.corruption && c.type == CardType::Skill;
    int cost = c.cost;
    if (c.xCost) {                       // X 费用：花费全部能量，X 值供效果读取
        cost = freeSkill ? 0 : ctx.energy;
        ctx.xValue = cost;
    }
    else if (freeSkill) {
        cost = 0;                        // 腐化：技能费用变 0
    }
    else if (c.attackCostDiscount) {   // 踩踏：本回合每打过一张攻击牌使费用 -1
        cost = std::max(0, c.cost - ctx.attacksPlayed);
    }
    if (ctx.energy < cost) { ctx.log("能量不足！"); return false; }

    ctx.energy -= cost;
    // 先把手牌移出再结算：效果可能抽牌导致 vector 重新分配，引用会失效
    Card played = std::move(ctx.hand[handIdx]);
    ctx.hand.erase(ctx.hand.begin() + handIdx);

    Entity& self = ctx.player;
    // 统一结算入口：sole != nullptr 表示只对指定目标结算（Self / SingleEnemy）；
    // sole == nullptr 时按卡牌目标模式分发（RandomEnemy 传 self，AllEnemies 逐个存活敌人）。
    auto resolveEffects = [&](Entity* sole) {
        if (sole) {
            for (auto& fx : played.effects) fx(ctx, self, *sole);
        }
        else if (played.target == TargetMode::RandomEnemy) {
            // 随机目标卡：不要求玩家选目标（上方已跳过目标校验）。
            // 目标由效果内部随机挑选；t 传 self，便于组合"对自己施加状态"类效果。
            for (auto& fx : played.effects) fx(ctx, self, self);
        }
        else {
            for (auto& en : ctx.enemies) {
                if (!en.inPlay()) continue;
                for (auto& fx : played.effects) fx(ctx, self, en);
            }
        }
        };

    // resolvingCard：结算期间指向当前卡，供 AddCopyOfSelf 复制"此牌"（嵌套打出时恢复外层）
    const Card* savedResolving = ctx.resolvingCard;
    ctx.resolvingCard = &played;

    // —— 自身效果：固定结算 1 次（AllEnemies 卡不会随敌人数重复，如突破的自伤）——
    for (auto& fx : played.selfEffects) fx(ctx, self, self);

    Entity* sole = nullptr;
    if (played.target == TargetMode::Self)             sole = &self;
    else if (played.target == TargetMode::SingleEnemy) sole = &ctx.enemies[targetIdx];
    resolveEffects(sole);

    // —— 能力标记：连环拳（本回合下一张攻击牌额外结算一次）——
    if (played.type == CardType::Attack && ctx.doubleNextAttack) {
        ctx.doubleNextAttack = false;
        ctx.log("连环拳：再次结算 " + played.name);
        resolveEffects(sole);
    }
    ctx.resolvingCard = savedResolving;

    // —— 打出后触发（能力标记：狂怒 / 杂耍 / 攻击计数 | 状态：愤怒）——
    if (played.type == CardType::Attack) {
        ++ctx.attacksPlayed;
        if (ctx.rage) gainBlock(ctx, self, 3);                       // 狂怒
        if (ctx.juggle > 0 && ctx.attacksPlayed == 3) {             // 杂耍：仅本回合第 3 张复制入手
            ctx.hand.push_back(played);
            ctx.log("杂耍：将 " + played.name + " 的复制品加入手牌");
        }
    }
    else if (played.type == CardType::Skill) {
        // —— 状态：愤怒（玩家每打出一张技能牌 → 该敌 +Strength×层，不衰减；autoPlayCard 不触发）——
        for (auto& en : ctx.enemies) {
            if (!en.inPlay()) continue;
            int layers = en.getStatus(Status::Enrage);
            if (layers > 0) {
                en.addStatus(Status::Strength, layers);
                ctx.log(en.name + " 愤怒：获得 " + std::to_string(layers) + " 点力量");
            }
        }
    }

    // —— 去向：能力牌打出后从本场战斗移除（不进弃牌堆，避免再次抽到并重复叠加，与 STS 一致）；
    //          腐化技能 / exhaust 标记 → 统一走 exhaustCard；其余进弃牌堆 ——
    if (played.type != CardType::Power) {
        if ((ctx.corruption && played.type == CardType::Skill) || played.exhaust)
            exhaustCard(ctx, played);
        else
            ctx.discardPile.push_back(std::move(played));
    }

    // —— 敌人过渡结算：必须放在函数最末端（sole 指向的敌人在重建后会悬垂）——
    // 上方 sole（指向 ctx.enemies 元素）在重建后会悬垂，故必须在其所有引用使用之后调用。
    resolveEnemyTransitions(ctx);
    return true;
}

// ==================== 自动打出（倾泻 / 破灭 / 地狱狂徒 / 惊逃） ====================
// 与 playCard 的区别：费用视为 0、X 费用视为 0（防止连锁倾泻无限循环）、
// 不触发 狂怒 / 杂耍 / attacksPlayed 等手动打出专属触发。
// 目标选择与 playCard 一致：SingleEnemy → 第一个存活敌人（可传 targetIdx 指定，
// 地狱狂徒随机目标 / 惊逃用）；AllEnemies → 逐个存活敌人；Self / RandomEnemy → 目标由效果内部处理。
inline void autoPlayCard(BattleContext& ctx, Card& c, int targetIdx) {
    int savedX = ctx.xValue;
    const Card* savedResolving = ctx.resolvingCard;
    if (c.xCost) ctx.xValue = 0;
    ctx.resolvingCard = &c;  // 供 AddCopyOfSelf 复制"此牌"
    Entity& self = ctx.player;
    // —— 自身效果：固定结算 1 次（AllEnemies 卡不会随敌人数重复）——
    for (auto& fx : c.selfEffects) fx(ctx, self, self);
    if (c.target == TargetMode::Self) {
        for (auto& fx : c.effects) fx(ctx, self, self);
    }
    else if (c.target == TargetMode::SingleEnemy) {
        if (targetIdx >= 0 && targetIdx < (int)ctx.enemies.size() && ctx.enemies[targetIdx].inPlay()) {
            for (auto& fx : c.effects) fx(ctx, self, ctx.enemies[targetIdx]);
        }
        else {
            for (auto& en : ctx.enemies)
                if (en.inPlay()) { for (auto& fx : c.effects) fx(ctx, self, en); break; }
        }
    }
    else if (c.target == TargetMode::RandomEnemy) {
        for (auto& fx : c.effects) fx(ctx, self, self);
    }
    else {
        for (auto& en : ctx.enemies)
            if (en.inPlay()) for (auto& fx : c.effects) fx(ctx, self, en);
    }
    ctx.resolvingCard = savedResolving;
    ctx.xValue = savedX;

    // —— 敌人过渡结算：必须放在函数最末端（c 由调用方持有，回写不受影响）——
    resolveEnemyTransitions(ctx);
}

// ==================== Turn flow ====================
inline void startPlayerTurn(BattleContext& ctx) {
    // —— 能力标记：壁垒（回合开始格挡不清零）——
    if (!ctx.barricade) ctx.player.block = 0;
    else                ctx.log("壁垒：格挡保留");
    ctx.energy = ctx.player.maxEnergy;
    ctx.log("===== 玩家回合 =====");
    ctx.playerTurn = true;             // 进入玩家回合：失去生命类标记在此回合内生效
    ctx.lostHpThisTurn = false;        // 敌人回合的伤害不算"本回合失去生命"
    // —— 能力标记：薪火之源 / 恶魔形态（层数不衰减，每层分别 +1 能量 / +2 力量）——
    if (ctx.pyreSource > 0) {
        ctx.energy += ctx.pyreSource;
        ctx.log("薪火之源：获得 " + std::to_string(ctx.pyreSource) + " 点能量");
    }
    if (ctx.demonForm > 0) {
        ctx.player.addStatus(Status::Strength, 2 * ctx.demonForm);
        ctx.log("恶魔形态：获得 " + std::to_string(2 * ctx.demonForm) + " 点力量");
    }
    // —— 能力标记：狱火（回合开始失去 1 生命，触发自己的全体 6 伤）——
    if (ctx.infernoFire) {
        ctx.log("狱火：回合开始时失去 1 点生命");
        loseHp(ctx, ctx.player, 1);
    }
    // —— 能力标记：覆甲（回合开始获得 1×层数 格挡，然后 -1 层）——
    int pa = ctx.player.getStatus(Status::PlatedArmor);
    if (pa > 0) {
        gainBlock(ctx, ctx.player, pa);
        tickStatus(ctx.player, Status::PlatedArmor, -1);
    }
    // —— 状态：仪式（敌人）——在玩家回合开始时结算上一个敌人回合的仪式 ——
    // 跳过玩家第 1 回合（turn == 0，敌人尚未行动），保证"获得仪式的当回合不立即转力量"；
    // 之后每个玩家回合开始时按 Ritual 层数获得力量（不衰减）。
    for (auto& e : ctx.enemies) {
        if (!e.inPlay() || e.turn == 0) continue;
        int ritual = e.getStatus(Status::Ritual);
        if (ritual > 0) {
            e.addStatus(Status::Strength, ritual);
            ctx.log(e.name + " 仪式：获得 " + std::to_string(ritual) + " 点力量");
        }
    }
    drawCards(ctx, 5);
}

inline void endPlayerTurn(BattleContext& ctx) {
    // —— 能力标记：金属化（回合结束获得 3×层数 格挡）——
    if (ctx.metallize > 0) {
        ctx.log("金属化：获得 " + std::to_string(3 * ctx.metallize) + " 点格挡");
        gainBlock(ctx, ctx.player, 3 * ctx.metallize);
    }
    // —— 能力标记：惊逃（回合结束随机打出手牌中 1 张攻击牌攻击随机敌人）——
    if (ctx.flee) {
        std::vector<int> attacks, targets;
        for (size_t i = 0; i < ctx.hand.size(); ++i)
            if (ctx.hand[i].type == CardType::Attack) attacks.push_back((int)i);
        for (size_t i = 0; i < ctx.enemies.size(); ++i)
            if (ctx.enemies[i].inPlay()) targets.push_back((int)i);
        if (attacks.empty() || targets.empty()) {
            ctx.log("惊逃：没有攻击牌或没有目标");
        }
        else {
            int hidx = attacks[ctx.cardRng->range(0, (int)attacks.size() - 1)];
            int eidx = targets[ctx.cardRng->range(0, (int)targets.size() - 1)];
            Card c = std::move(ctx.hand[hidx]);
            ctx.hand.erase(ctx.hand.begin() + hidx);
            ctx.log("惊逃：自动对随机敌人打出 " + c.name);
            autoPlayCard(ctx, c, eidx);
            if ((ctx.corruption && c.type == CardType::Skill) || c.exhaust)
                exhaustCard(ctx, c);
            else
                ctx.discardPile.push_back(std::move(c));
        }
    }
    // —— 手牌结算：先整体移出到局部 vector 再逐个结算 ——
    // 若逐张边结算边遍历，虚无消耗→黑暗之拥抽牌会向 hand 追加元素导致迭代器失效。
    // 灼伤卡本身不消耗（进弃牌堆）；只有 ethereal(眩晕) 消耗。
    std::vector<Card> ending = std::move(ctx.hand);
    ctx.hand.clear();
    for (auto& c : ending) {
        if (c.endTurnDamage > 0) {                    // 灼伤：回合末若在手牌，对玩家造成伤害
            ctx.log(c.name + "：回合结束失去 " + std::to_string(c.endTurnDamage) + " 点生命");
            loseHp(ctx, ctx.player, c.endTurnDamage);
        }
        if (c.ethereal) exhaustCard(ctx, c);          // 虚无：回合末若在手牌则消耗
        else            ctx.discardPile.push_back(std::move(c));
    }
    tickStatus(ctx.player, Status::Vulnerable, -1);
    tickStatus(ctx.player, Status::Weak, -1);
    tickStatus(ctx.player, Status::Frail, -1);        // 脆弱衰减
    tickStatus(ctx.player, Status::Entangle, -1);     // 缠绕（本回合禁攻击牌）衰减
    ctx.player.tempStrength = 0;  // 回合末失去临时力量（预备打击的一次性效果）
    ctx.playerTurn = false;       // 退出玩家回合：敌人回合的伤害不再触发"本回合"标记
    // —— 回合级能力标记清零（Power 类标记如壁垒/腐化/无惧疼痛/惊逃等跨回合保留）——
    ctx.noDraw = false;
    ctx.exhaustedThisTurn = false;
    ctx.steadfastUsed = false;
    ctx.rage = false;
    ctx.flameBarrier = false;
    ctx.colossus = false;
    ctx.attacksPlayed = 0;
    ctx.lostHpThisTurn = false;
    ctx.doubleNextAttack = false;
}

inline void enemyTurn(BattleContext& ctx) {
    ctx.log("===== 敌人回合 =====");
    // ① 冷却递减（行动前递减，本轮新设的冷却不被本轮递减）
    for (auto& e : ctx.enemies) {
        if (!e.inPlay()) continue;
        for (auto& cd : e.cooldown) if (cd > 0) --cd;
    }
    // ①.5 分裂替代行动：splitReplacesAction 的敌人（大史莱姆）在本回合行动前分裂，
    //     不执行当前意图；分裂产物本回合不行动（current.act 为空，下面跳过）。
    resolveEnemyTransitions(ctx, false, true);
    // ② 执行意图 + 状态衰减 + 回合末被动（仪式 / 金属化(敌)）
    // 用下标循环：行动完成回调需要敌人下标；② 期间不增删敌人，索引稳定。
    for (int i = 0; i < (int)ctx.enemies.size(); ++i) {
        Enemy& e = ctx.enemies[i];
        if (!e.inPlay()) continue;
        if (!e.current.act) continue;              // 本回合刚分裂出的产物：本回合不行动
        e.block = 0;
        ctx.log(e.name + " 使用：" + e.current.desc);
        // 意图 act 为空说明 pattern 表配置有误，用 assert 立即暴露（正式兜底行为待后续实现）。
        assert(e.current.act && "enemy intent act is empty");
        // 只观察玩家 HP / 格挡的变化用于通知 GUI，不参与伤害结算本身。
        const int playerHpBefore = ctx.player.hp;
        const int playerBlockBefore = ctx.player.block;
        e.current.act(ctx, e);
        const bool hurtPlayer = ctx.player.hp < playerHpBefore ||
                                ctx.player.block < playerBlockBefore;
        // 可选通知（GUI 播放突进）；未设置回调时行为与原来完全一致。
        if (ctx.onEnemyActionResolved) ctx.onEnemyActionResolved(i, hurtPlayer);
        // 冷却"赋值"必须放在执行后（不能在 planNextIntent 内，否则与递减错位）
        if (e.patternIdx >= 0 && e.patterns[e.patternIdx].cooldown > 0)
            e.cooldown[e.patternIdx] = e.patterns[e.patternIdx].cooldown;
        tickStatus(e, Status::Vulnerable, -1);
        tickStatus(e, Status::Weak, -1);
        if (e.onTurnEnd) e.onTurnEnd(ctx, e);
    }
    // ③ 过渡结算（分裂 / 重建）；分裂产物由 ④ 统一规划意图
    resolveEnemyTransitions(ctx, false);
    // ④ 规划下一意图，供玩家在下个回合看到
    for (auto& e : ctx.enemies) {
        if (!e.inPlay()) continue;
        ++e.turn;
        planNextIntent(ctx, e);
    }
}