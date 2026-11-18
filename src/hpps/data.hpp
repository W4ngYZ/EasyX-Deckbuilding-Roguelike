// =======================================
// data.hpp 卡牌与敌人实例数据
//
// 卡牌 / 敌人的"具体实例"集中在这里定义（battle.hpp 只留规则与流程）。
// 依赖：core.hpp（数据结构）+ battle.hpp（效果工厂 / 原子规则）。
// 新卡牌、新怪物一律在此文件新增工厂函数，battle.hpp 不再新增数据。
// =======================================

#pragma once

#include "battle.hpp"

// 卡牌工厂别名与全卡池声明（定义在文末 allCardFactories）。
// 提前声明是为了让“添柴”等随机生成卡牌的效果能复用奖励池（不含基础牌）。
using CardFactory = std::function<Card()>;
inline std::vector<CardFactory> allCardFactories();

// ==================== Effect factories 使用指南 ====================
// 所有工厂都返回 EffectFn（签名 void(ctx, self, target)），组合进：
//   Card{ 名称, 费用, 类型, 目标, { 效果列表 }, 是否消耗(exhaust),
//        是否X费用(xCost), 是否攻击减费(attackCostDiscount), 消耗时回能(gainEnergyOnExhaust) }
// exhaust 之后的三个可选字段默认 false，按需显式传入（见 倾泻 / 旋风斩 / 踩踏 / 战鼓）。
// 可打出条件（交锋）：建卡后单独给 c.canPlay 赋 lambda（返回当前能否打出，
// 空 = 总是可打出）。playCard 打出前与 CLI 渲染时都会调用它，仅影响"手动打出"，
// 倾泻 / 破灭等自动打出不检查。
// target 语义由卡牌 TargetMode 决定：Self = 玩家自身 / SingleEnemy = 被选敌人 /
// AllEnemies = 对每个存活敌人各调一次 / RandomEnemy = 目标由效果内部随机选。
// AllEnemies 卡的 effects 会被每个存活敌人各执行一遍，因此"自伤 / 回能 / 抽牌"等
// 只应结算一次的自身效果必须放进 Card.selfEffects（它在目标效果之前固定执行 1 次）。
//
// 现有工厂用法：
//   Damage(n)                对目标造成 n 点伤害（自动叠加来源力量、目标易伤并结算格挡）
//   Block(n)                 获得 n 点格挡（自动叠加敏捷）
//   ApplyStatus(Status, n)   给目标施加 n 层状态（Strength / Dexterity / Vulnerable / Weak / PlatedArmor）
//   Draw(n)                  抽 n 张牌（零随机消耗；抽牌堆空时自动洗回弃牌堆）
//   GainEnergy(n)            回复 n 点能量（追加到当前回合剩余能量）
//   SelfDamage(n)            自伤 n：直接扣自身 HP，无视格挡 / 力量（STS 语义）
//   DamageScaled(b, fn)      伤害 = b + fn(玩家)，fn 读玩家状态 / 血量（例：全身撞击读当前格挡）
//   DamageByTarget(b, fn)    伤害 = b + fn(目标敌人)，fn 读敌人状态 / 血量（例：欺凌读易伤层数）
//   RandomTargetDamage(n)    随机攻击一个存活敌人 n 点（决策事件，消耗 cardRng）
//   ExhaustRandom(n)         随机消耗 n 张手牌（每张掷一次，决策事件，消耗 cardRng）
//   ExhaustHandAndDamage(p)  消耗全部手牌，每张对同一目标造成 p 点伤害
//   ChosenFromDiscardToDraw() 弃牌堆选 1 张放回抽牌堆顶（玩家输入钩子，零随机）
//   ExhaustChoose()          手牌选 1 张消耗（玩家输入钩子，零随机）
//   ChosenFromDrawToHand()   抽牌堆选 1 张入手（玩家输入钩子，零随机）
//   AddCardToPile(c, pile)   把指定卡加入任意牌堆（PileDest::Hand/Draw/Discard/Exhaust，零随机）
//   AddCopyOfSelf(pile)      把"正在结算的这张牌"的复制品加入指定牌堆（愤怒 / 群情激愤）
//   CopyChosenToHand()       手牌选 1 张攻击/能力牌，复制品入手（双持，玩家输入钩子）
//
// 统一入口（非工厂）：
//   exhaustCard(ctx, c)      所有"卡牌被消耗"必须走这里，触发 战鼓 / 无惧疼痛 / 黑暗之拥；
//                            卡牌效果如需"本回合是否消耗过牌"读 ctx.exhaustedThisTurn。
//   autoPlayCard(ctx, c)     自动打出（倾泻用）：费用 0 结算效果，不触发手动打出专属触发。
//
// 单卡一次性逻辑（无需复用、不宜抽象成工厂）可直接在 effects 里写内联 lambda，
// 如：完美打击统计"打击"牌、巩固格挡翻倍、预备打击回合级临时力量。
//
// 注意事项：
//   - dealDamage 自动叠加力量 / 易伤并结算格挡，所以"按力量缩放"的卡直接用 Damage(base)；
//     DamageScaled / DamageByTarget 的 fn 结果还会再叠一层，适合读格挡 / 敏捷 / 血量等。
//   - 需要随机时禁止 rand()，必须走注入的 rng 流：ctx.cardRng（rng.hpp 域 CARD_EFFECTS）。
//   - 效果在"打出时"解析：打出卡已先从手牌移除，Exhaust* 不会把正在打的卡计入。
//   - 复制"此牌"用 AddCopyOfSelf：playCard / autoPlayCard 结算期间 ctx.resolvingCard
//     指向当前卡（嵌套打出时自动恢复外层），复制品含完整效果、可再次被打出。
// ==================== Card data ====================
// 如何加新卡牌：仿照下面工厂写 makeXxx()，
//   Card{ "卡名", 费用, CardType, TargetMode, { 效果1, 效果2, ... }, 是否消耗(exhaust) }
//   - 效果直接组合 battle.hpp 的 Effect 工厂；全新机制先加 Effect 工厂再加卡。
//   - 新卡必须登记进 allCardFactories()（文末），否则不进奖励池。
// 卡牌按 攻击 / 技能 / 能力 三大类组织；基础牌（打击 / 防御 / 痛击）在各类下方注明。

// ==================== 攻击牌 ====================
// 基础牌：打击（角色初始 5 张，见 run.hpp beginRun）
inline Card makeStrike() { return Card{ "打击", 1, CardType::Attack, TargetMode::SingleEnemy,{ Damage(6) } }; }
// 基础牌：痛击（角色初始 1 张）
inline Card makeBash() { return Card{ "痛击", 2, CardType::Attack, TargetMode::SingleEnemy,{ Damage(8), ApplyStatus(Status::Vulnerable, 2) } }; }
inline Card makeIronWave() { return Card{ "铁斩波", 1, CardType::Attack, TargetMode::SingleEnemy,{ Damage(5), Block(5) } }; }
inline Card makePommelStrike() { return Card{ "剑柄打击", 1, CardType::Attack, TargetMode::SingleEnemy,{ Damage(9), Draw(1) } }; }
inline Card makeThunderClap() { return Card{ "闪电霹雳", 1, CardType::Attack, TargetMode::AllEnemies,{ Damage(4), ApplyStatus(Status::Vulnerable, 1) } }; }
inline Card makeBodySlam() { return Card{ "全身撞击", 1, CardType::Attack, TargetMode::SingleEnemy,{ DamageScaled(0, [](const Entity& e) { return e.block; }) } }; }
inline Card makeHemokinesis() { return Card{ "御血术", 1, CardType::Attack, TargetMode::SingleEnemy,{ SelfDamage(2), Damage(15) } }; }
inline Card makeHeadbutt() { return Card{ "头槌", 1, CardType::Attack, TargetMode::SingleEnemy,{ Damage(9), ChosenFromDiscardToDraw() } }; }
inline Card makeDoubleStrike() { return Card{ "双重打击", 1, CardType::Attack, TargetMode::SingleEnemy,{ Damage(5), Damage(5) } }; }
inline Card makeBoomerang() {
    return Card{ "飞剑回旋镖", 1, CardType::Attack, TargetMode::RandomEnemy,
{ RandomTargetDamage(3), RandomTargetDamage(3), RandomTargetDamage(3) } };
}
// 突破：失去 1 点生命，对所有敌人造成 9 点伤害
// 自伤放入 selfEffects，只结算 1 次；Damage 放入 effects，由 AllEnemies 逐敌人分发。
inline Card makeBreakthrough() {
    Card c{ "突破", 1, CardType::Attack, TargetMode::AllEnemies,{ Damage(9) } };
    c.selfEffects = { SelfDamage(1) };
    return c;
}
inline Card makeBully() {
    return Card{ "欺凌", 0, CardType::Attack, TargetMode::SingleEnemy,
{ DamageByTarget(4, [](const Entity& e) { return e.getStatus(Status::Vulnerable) * 2; }) } };
}
inline Card makeEmber() { return Card{ "余烬", 2, CardType::Attack, TargetMode::SingleEnemy,{ Damage(18), ExhaustRandom(1) } }; }
inline Card makeSledgehammer() { return Card{ "重锤", 3, CardType::Attack, TargetMode::SingleEnemy,{ Damage(32) } }; }
inline Card makeBludgeon() { return Card{ "破击", 1, CardType::Attack, TargetMode::SingleEnemy,{ Damage(20), ApplyStatus(Status::Vulnerable, 5) } }; }
inline Card makeInferno() {
    return Card{ "焚烧", 1, CardType::Attack, TargetMode::AllEnemies,
{ Damage(2), Damage(2), Damage(2), Damage(2) } };
}

// 完美打击：6 伤 + 名字含"打击"的牌每张 +2（统计抽牌/手牌/弃牌/消耗全牌堆）
inline Card makePerfectedStrike() {
    return Card{ "完美打击", 2, CardType::Attack, TargetMode::SingleEnemy,{
        [](BattleContext& ctx, Entity& self, Entity& target) {
            int n = 0;
            for (auto* pile : { &ctx.drawPile, &ctx.hand, &ctx.discardPile, &ctx.exhaustPile })
                for (auto& c : *pile)
                    if (c.name.find("打击") != std::string::npos) ++n;
            dealDamage(ctx, self, target, 6 + n * 2);
        }
    } };
}

// 灰烬打击：6 伤 + 消耗牌堆每张 +3
inline Card makeAshStrike() {
    return Card{ "灰烬打击", 0, CardType::Attack, TargetMode::SingleEnemy,{
        [](BattleContext& ctx, Entity& self, Entity& target) {
            dealDamage(ctx, self, target, 6 + (int)ctx.exhaustPile.size() * 3);
        }
    } };
}

// 拆卸：8 伤；目标有易伤状态则再攻击一次
inline Card makeDismantle() {
    return Card{ "拆卸", 1, CardType::Attack, TargetMode::SingleEnemy,{
        [](BattleContext& ctx, Entity& self, Entity& target) {
            dealDamage(ctx, self, target, 8);
            if (target.getStatus(Status::Vulnerable) > 0) dealDamage(ctx, self, target, 8);
        }
    } };
}

// 预备打击：7 伤 + 本回合 +2 临时力量（回合末清零，见 endPlayerTurn）
inline Card makePrepareStrike() {
    return Card{ "预备打击", 1, CardType::Attack, TargetMode::SingleEnemy,{
        [](BattleContext& ctx, Entity& self, Entity& target) {
            dealDamage(ctx, self, target, 7);
            ctx.player.tempStrength += 2;               // 仅本回合，回合末失去
        }
    } };
}

// 旋风斩：X 费用（花费全部能量），对每个存活敌人造成 6 点伤害 X 次（X = ctx.xValue）。
// 注意：AllEnemies 分发会对每个存活敌人各调一次效果，故循环写在 lambda 内部
// （若在外面再套一层 X 循环，会退化成 X² 次）。每次结算都独立吃力量 / 易伤 / 格挡。
inline Card makeWhirlwind() {
    return Card{ "旋风斩", 0, CardType::Attack, TargetMode::AllEnemies,{
        [](BattleContext& ctx, Entity& self, Entity& t) {
            for (int i = 0; i < ctx.xValue; ++i) {
                if (!t.alive()) break;
                dealDamage(ctx, self, t, 6);
            }
        }
    }, false, true };
}

// 踩踏：本回合每打过一张攻击牌使费用 -1（费用在 playCard 计算，不能低于 0）
inline Card makeTrample() {
    return Card{ "踩踏", 3, CardType::Attack, TargetMode::AllEnemies,
        { Damage(12) }, false, false, true, false };
}

// 邪眼：获得 8 点格挡；若本回合消耗过一张牌，额外获得 8 点格挡
inline Card makeEvilEye() {
    return Card{ "邪眼", 1, CardType::Skill, TargetMode::Self,{
        [](BattleContext& ctx, Entity& self, Entity&) {
            gainBlock(ctx, self, 8, true);
            if (ctx.exhaustedThisTurn) gainBlock(ctx, self, 8, true);
        }
    } };
}

// 交锋：0 费；只有手牌全部是攻击牌时才能打出（canPlay 条件，playCard 与渲染都会检查），14 伤
inline Card makeClash() {
    Card c{ "交锋", 0, CardType::Attack, TargetMode::SingleEnemy,{ Damage(14) } };
    c.canPlay = [](BattleContext& ctx) {
        for (auto& h : ctx.hand)
            if (h.type != CardType::Attack) return false;
        return true;
        };
    return c;
}

// 契约终结：0 费；消耗牌堆 ≥3 张时对所有敌人造成 17 点伤害
// 注意：AllEnemies 分发会对每个存活敌人各调一次效果，故 lambda 只打传入的 t
inline Card makeContractEnd() {
    return Card{ "契约终结", 0, CardType::Attack, TargetMode::AllEnemies,{
        [](BattleContext& ctx, Entity& self, Entity& t) {
            if (ctx.exhaustPile.size() >= 3) dealDamage(ctx, self, t, 17);
        }
    } };
}

// 狂宴：1 费；造成 10 点伤害，若以此击杀敌人则永久 +3 最大生命并回复等量生命，消耗
inline Card makeFeast() {
    return Card{ "狂宴", 1, CardType::Attack, TargetMode::SingleEnemy,{
        [](BattleContext& ctx, Entity& self, Entity& t) {
            dealDamage(ctx, self, t, 10);
            if (!t.alive()) {  // 斩杀判定：本卡伤害致死
                ctx.player.maxHp += 3;
                ctx.player.hp += 3;  // 与 STS 一致：最大生命提升时同步回复
                ctx.log("狂宴：+3 最大生命（当前 " + std::to_string(ctx.player.maxHp) + "）");
            }
        }
    }, true };
}

// 怨恨：0 费；造成 5 点伤害；本回合失去过生命则再造成 5 点伤害
// （lostHpThisTurn 只统计玩家回合内的扣血：自伤 / 狱火掉血，敌人回合不算）
inline Card makeResentment() {
    return Card{ "怨恨", 0, CardType::Attack, TargetMode::SingleEnemy,{
        [](BattleContext& ctx, Entity& self, Entity& t) {
            dealDamage(ctx, self, t, 5);
            if (ctx.lostHpThisTurn) dealDamage(ctx, self, t, 5);
        }
    } };
}

// 熔融之拳：1 费；造成 10 点伤害，将该敌人身上的易伤层数翻倍，消耗
inline Card makeMoltenFist() {
    return Card{ "熔融之拳", 1, CardType::Attack, TargetMode::SingleEnemy,{
        [](BattleContext& ctx, Entity& self, Entity& t) {
            dealDamage(ctx, self, t, 10);
            int vul = t.getStatus(Status::Vulnerable);
            if (vul > 0) {
                t.addStatus(Status::Vulnerable, vul);
                ctx.log("熔融之拳：易伤层数翻倍至 " + std::to_string(vul * 2));
            }
        }
    }, true };
}

// 愤怒：0 费；造成 6 点伤害，将一张此牌的复制品加入弃牌堆
inline Card makeAnger() {
    return Card{ "愤怒", 0, CardType::Attack, TargetMode::SingleEnemy,
        { Damage(6), AddCopyOfSelf(PileDest::Discard) } };
}

// 群情激愤：0 费；造成 9 点伤害，将一张此牌的复制品加入弃牌堆
// （单机版：只加入自己的弃牌堆；多人版本需扩展牌堆归属）
inline Card makeOutrage() {
    return Card{ "群情激愤", 0, CardType::Attack, TargetMode::SingleEnemy,
        { Damage(9), AddCopyOfSelf(PileDest::Discard) } };
}

// ==================== 技能牌 ====================
// 基础牌：防御（角色初始 5 张）
inline Card makeDefend() { return Card{ "防御", 1, CardType::Skill, TargetMode::Self,{ Block(5) } }; }
inline Card makeShrugItOff() { return Card{ "耸肩无视", 1, CardType::Skill, TargetMode::Self,{ Block(8), Draw(1) } }; }
inline Card makeBloodletting() { return Card{ "放血", 0, CardType::Skill, TargetMode::Self,{ SelfDamage(3), GainEnergy(2) } }; }
inline Card makeTrueGrit() { return Card{ "坚毅", 1, CardType::Skill, TargetMode::Self,{ Block(7), ExhaustRandom(1) } }; }
inline Card makeProvoke() { return Card{ "挑衅", 1, CardType::Skill, TargetMode::SingleEnemy,{ Block(6), ApplyStatus(Status::Vulnerable, 1) } }; }
inline Card makeTremor() { return Card{ "战栗", 1, CardType::Skill, TargetMode::AllEnemies,{ ApplyStatus(Status::Vulnerable, 3) }, true }; }
inline Card makeBurningPact() { return Card{ "燃烧契约", 1, CardType::Skill, TargetMode::Self,{ ExhaustChoose(), Draw(2) } }; }
inline Card makeBloodWall() { return Card{ "血墙", 2, CardType::Skill, TargetMode::Self,{ SelfDamage(2), Block(16) } }; }
inline Card makeOffering() { return Card{ "祭品", 0, CardType::Skill, TargetMode::Self,{ SelfDamage(6), GainEnergy(2), Draw(3) }, true }; }
inline Card makeBrand() { return Card{ "烙印", 0, CardType::Skill, TargetMode::Self,{ SelfDamage(1), ExhaustChoose(), ApplyStatus(Status::Strength, 1) } }; }
inline Card makeCrimsonCloak() { return Card{ "绯红披风", 1, CardType::Skill, TargetMode::Self,{ SelfDamage(1), Block(10) } }; }

// 巩固：当前格挡翻倍
inline Card makeEntrench() {
    return Card{ "巩固", 2, CardType::Skill, TargetMode::Self,{
        [](BattleContext& ctx, Entity&, Entity&) { ctx.player.block *= 2; }
    } };
}

// 重振精神：消耗手牌中所有非攻击牌，每张 +5 格挡（先全部移出再结算，避免抽牌混入）
inline Card makeReinvigorate() {
    return Card{ "重振精神", 1, CardType::Skill, TargetMode::Self,{
        [](BattleContext& ctx, Entity& self, Entity&) {
            std::vector<Card> nonAttack;
            auto it = ctx.hand.begin();
            while (it != ctx.hand.end()) {
                if (it->type != CardType::Attack) {
                    nonAttack.push_back(std::move(*it));
                    it = ctx.hand.erase(it);
                }
 else ++it;
}
            for (auto& c : nonAttack) {
                gainBlock(ctx, self, 5, true);
                exhaustCard(ctx, c);
            }
}
} };
}

// 倾泻：X 费用（花费全部能量）。自动打出抽牌堆顶的 X 张牌（费用视为 0，
// 绕过战斗专注的抽牌限制；自动打出的 X 费用牌 X 视为 0，防止连锁）
inline Card makeDump() {
    return Card{ "倾泻", 0, CardType::Skill, TargetMode::Self,{
        [](BattleContext& ctx, Entity&, Entity&) {
            int x = ctx.xValue;
            for (int i = 0; i < x; ++i) {
                if (ctx.drawPile.empty()) {
                    if (ctx.discardPile.empty()) { ctx.log("抽牌堆为空，倾泻停止"); break; }
                    ctx.drawPile = std::move(ctx.discardPile);
                    ctx.discardPile.clear();
                    shufflePile(ctx, ctx.drawPile);
                    ctx.log("弃牌堆洗入抽牌堆");
                }
                Card c = std::move(ctx.drawPile.back());
                ctx.drawPile.pop_back();
                ctx.log("倾泻自动打出 " + c.name);
                autoPlayCard(ctx, c);
                // 去向与手动打出一致：能力牌打出后移出本场战斗（不得进弃牌堆反复叠加）；
                // 腐化技能 / 自带消耗 → 统一走 exhaustCard；其余进弃牌堆。
                if (c.type != CardType::Power) {
                    if ((ctx.corruption && c.type == CardType::Skill) || c.exhaust)
                        exhaustCard(ctx, c);
                    else
                        ctx.discardPile.push_back(std::move(c));
                }
            }
        }
    }, false, true };
}

// 破灭：1 费；打出抽牌堆顶部的牌，并将其消耗（复用倾泻的"取顶"逻辑 + autoPlayCard）
inline Card makeRuin() {
    return Card{ "破灭", 1, CardType::Skill, TargetMode::Self,{
        [](BattleContext& ctx, Entity&, Entity&) {
            if (ctx.drawPile.empty()) {
                if (ctx.discardPile.empty()) { ctx.log("抽牌堆为空，破灭无牌可打"); return; }
                ctx.drawPile = std::move(ctx.discardPile);
                ctx.discardPile.clear();
                shufflePile(ctx, ctx.drawPile);
                ctx.log("弃牌堆洗入抽牌堆");
            }
            Card c = std::move(ctx.drawPile.back());
            ctx.drawPile.pop_back();
            ctx.log("破灭打出 " + c.name);
            autoPlayCard(ctx, c);        // 自动打出（不触发狂怒/杂耍/攻击计数）
            exhaustCard(ctx, c);         // 打出后即消耗（统一入口，触发无惧疼痛/黑暗之拥）
        }
    } };
}

// 巨像：1 费；获得 4 点格挡；本回合内有易伤状态的敌人对你造成的伤害降低 50%（回合末清零）
inline Card makeColossus() {
    return Card{ "巨像", 1, CardType::Skill, TargetMode::Self,
        { Block(4), [](BattleContext& ctx, Entity&, Entity&) { ctx.colossus = true; } } };
}

// 战斗专注：抽 3 张牌；本回合不能再抽牌（noDraw 回合末清零）
inline Card makeConcentration() {
    return Card{ "战斗专注", 0, CardType::Skill, TargetMode::Self,{
        [](BattleContext& ctx, Entity&, Entity&) {
            drawCards(ctx, 3);
            ctx.noDraw = true;
        }
    } };
}

// 战鼓：1 费；抽 2 张牌；这张牌被消耗时获得 1 能量（走 exhaustCard 统一入口）
inline Card makeWarDrum() {
    return Card{ "战鼓", 1, CardType::Skill, TargetMode::Self,
        { Draw(2) }, true, false, false, true };
}

// 被遗忘的仪式：1 费；若本回合消耗过一张牌，获得 2 点能量；消耗
inline Card makeForgottenRitual() {
    return Card{ "被遗忘的仪式", 1, CardType::Skill, TargetMode::Self,{
        [](BattleContext& ctx, Entity&, Entity&) {
            if (ctx.exhaustedThisTurn) {
                ctx.energy += 2;
                ctx.log("被遗忘的仪式：获得 2 点能量（当前 " + std::to_string(ctx.energy) + "）");
            }
            else {
                ctx.log("被遗忘的仪式：本回合未消耗过牌");
            }
        }
    }, true };
}

// 狂怒：本回合内每打出一张攻击牌获得 3 格挡（回合末清零）
inline Card makeRage() {
    return Card{ "狂怒", 0, CardType::Skill, TargetMode::Self,
        { [](BattleContext& ctx, Entity&, Entity&) { ctx.rage = true; } } };
}

// 火焰屏障：获得 12 格挡；本回合内受到攻击时对攻击者反伤 4（回合末清零）
inline Card makeFlameBarrier() {
    return Card{ "火焰屏障", 2, CardType::Skill, TargetMode::Self,
        { Block(12), [](BattleContext& ctx, Entity&, Entity&) { ctx.flameBarrier = true; } } };
}

// 添柴：1 费；消耗所有手牌，每消耗一张牌，将 1 张随机牌（非基础牌）加入手牌。
// 随机池 = allCardFactories()（与奖励池一致，不含打击 / 防御 / 痛击），
// 走 cardRng（CARD_EFFECTS 域）；"加入手牌"不受战斗专注 noDraw 限制。
inline Card makeAddFuel() {
    return Card{ "添柴", 1, CardType::Skill, TargetMode::Self,{
        [](BattleContext& ctx, Entity&, Entity&) {
            std::vector<Card> tmp;               // 先全部移出再结算，黑暗之拥抽来的新牌不参与
            for (auto& c : ctx.hand) tmp.push_back(std::move(c));
            ctx.hand.clear();
            std::vector<CardFactory> pool = allCardFactories();
            for (auto& c : tmp) {
                exhaustCard(ctx, c);             // 先消耗（触发无惧疼痛 / 黑暗之拥）
                if (pool.empty()) continue;
                int idx = ctx.cardRng->range(0, (int)pool.size() - 1);
                addCardToPile(ctx, pool[idx](), PileDest::Hand);
            }
        }
    } };
}

// 主宰：1 费；给予目标 1 层易伤，目标每有 1 层易伤（含刚给的 1 层）获得 1 点力量，消耗
inline Card makeDominator() {
    return Card{ "主宰", 1, CardType::Skill, TargetMode::SingleEnemy,{
        [](BattleContext& ctx, Entity& self, Entity& t) {
            applyDebuff(ctx, t, Status::Vulnerable, 1);   // 负面状态统一入口（人工制品拦截）
            int layers = t.getStatus(Status::Vulnerable);
            self.addStatus(Status::Strength, layers);
            ctx.log("主宰：获得 " + std::to_string(layers) + " 点力量");
        }
    }, true };
}

// 双持：1 费；选择手牌中 1 张攻击牌或能力牌，将其复制品加入手牌（选择走 ctx.chooseCard）
inline Card makeDualWield() {
    return Card{ "双持", 1, CardType::Skill, TargetMode::Self,{ CopyChosenToHand() } };
}

// 连环拳：1 费；本回合打出的下一张攻击牌会被额外结算一次（回合末清零）
inline Card makeComboFist() {
    return Card{ "连环拳", 1, CardType::Skill, TargetMode::Self,{
        [](BattleContext& ctx, Entity&, Entity&) { ctx.doubleNextAttack = true; }
    } };
}

// ==================== 能力牌 ====================
// 惊逃：2 费能力；每回合结束时随机打出手牌中 1 张攻击牌攻击随机敌人（战斗内持续）
inline Card makeFlee() {
    return Card{ "惊逃", 2, CardType::Power, TargetMode::Self,{
        [](BattleContext& ctx, Entity&, Entity&) { ctx.flee = true; }
    } };
}

inline Card makeInflame() { return Card{ "燃烧", 1, CardType::Power, TargetMode::Self,{ ApplyStatus(Status::Strength, 2) } }; }

// 岩石铠甲：技能牌；获得 6 层覆甲（打出后进弃牌堆，可再次抽到）
inline Card makeRockArmor() {
    return Card{ "岩石铠甲", 1, CardType::Skill, TargetMode::Self,{
        [](BattleContext& ctx, Entity&, Entity&) { ctx.player.addStatus(Status::PlatedArmor, 6); }
    } };
}

// 残酷：对易伤目标造成的伤害每层 +25%（见 dealDamage）
inline Card makeCruelty() {
    return Card{ "残酷", 1, CardType::Power, TargetMode::Self,{
        [](BattleContext& ctx, Entity&, Entity&) { ctx.cruelty += 1; }
    } };
}

// 无惧疼痛：每消耗一张牌获得 3 格挡（层数，见 exhaustCard）
inline Card makeFeelNoPain() {
    return Card{ "无惧疼痛", 1, CardType::Power, TargetMode::Self,{
        [](BattleContext& ctx, Entity&, Entity&) { ctx.feelNoPain += 1; }
    } };
}

// 黑暗之拥：每消耗一张牌抽 1 张（层数，见 exhaustCard）
inline Card makeDarkEmbrace() {
    return Card{ "黑暗之拥", 2, CardType::Power, TargetMode::Self,{
        [](BattleContext& ctx, Entity&, Entity&) { ctx.darkEmbrace += 1; }
    } };
}

// 坚定不移：每回合首次从卡牌中获得格挡时翻倍（见 gainBlock 的 fromCard）
inline Card makeSteadfast() {
    return Card{ "坚定不移", 2, CardType::Power, TargetMode::Self,{
        [](BattleContext& ctx, Entity&, Entity&) { ctx.steadfast = true; }
    } };
}

// 势不可当：每次获得格挡时，对随机敌人造成 6 点伤害（层数，决策事件走 cardRng）
inline Card makeRelentless() {
    return Card{ "势不可当", 3, CardType::Power, TargetMode::Self,{
        [](BattleContext& ctx, Entity&, Entity&) { ctx.relentless += 1; }
    } };
}

// 杂耍：每回合仅第 3 张攻击牌复制入手（层数，见 playCard 打出后触发）
inline Card makeJuggle() {
    return Card{ "杂耍", 1, CardType::Power, TargetMode::Self,{
        [](BattleContext& ctx, Entity&, Entity&) { ctx.juggle += 1; }
    } };
}

// 壁垒：回合开始时格挡不再清零（见 startPlayerTurn）
inline Card makeBarricade() {
    return Card{ "壁垒", 3, CardType::Power, TargetMode::Self,{
        [](BattleContext& ctx, Entity&, Entity&) { ctx.barricade = true; }
    } };
}

// 腐化：3 费能力；技能费用变为 0，且打出的技能牌消耗（见 playCard）
inline Card makeCorruption() {
    return Card{ "腐化", 3, CardType::Power, TargetMode::Self,{
        [](BattleContext& ctx, Entity&, Entity&) { ctx.corruption = true; }
    } };
}

// 狱火：1 费能力；回合开始失去 1 生命；本回合内每次失去生命对全体敌人 6 伤（固定 6，不缩放）
inline Card makeInfernoFire() {
    return Card{ "狱火", 1, CardType::Power, TargetMode::Self,{
        [](BattleContext& ctx, Entity&, Entity&) { ctx.infernoFire = true; }
    } };
}

// 地狱狂徒：1 费能力；每抽到名字含"打击"的牌，对随机敌人打出（重复打出无额外效果，见 drawCards）
inline Card makeHellbound() {
    return Card{ "地狱狂徒", 1, CardType::Power, TargetMode::Self,{
        [](BattleContext& ctx, Entity&, Entity&) { ctx.hellbound = true; }
    } };
}

// 薪火之源：2 费能力；回合开始获得 1×层数 点能量（层数不衰减，见 startPlayerTurn）
inline Card makePyreSource() {
    return Card{ "薪火之源", 2, CardType::Power, TargetMode::Self,{
        [](BattleContext& ctx, Entity&, Entity&) { ctx.pyreSource += 1; }
    } };
}

// 恶魔形态：3 费能力；回合开始获得 2×层数 点力量（层数不衰减，见 startPlayerTurn）
inline Card makeDemonForm() {
    return Card{ "恶魔形态", 3, CardType::Power, TargetMode::Self,{
        [](BattleContext& ctx, Entity&, Entity&) { ctx.demonForm += 1; }
    } };
}

// 金属化：1 费能力；回合结束获得 3×层数 点格挡（层数不衰减，见 endPlayerTurn）
inline Card makeMetallize() {
    return Card{ "金属化", 1, CardType::Power, TargetMode::Self,{
        [](BattleContext& ctx, Entity&, Entity&) { ctx.metallize += 1; }
    } };
}

// ==================== 垃圾牌（引擎生成，不进奖励池） ====================
// 由敌人 act 经 addCardToPile(ctx, makeSlimed(), PileDest::Discard) 塞入玩家牌堆（零随机）。
// 注意：这些牌【不登记】进 allCardFactories()，永不进三选一奖励池。
inline Card makeSlimed() { return Card{ "黏液", 1, CardType::Skill, TargetMode::Self,{}, true }; }

// 灼伤：不可打出；回合末若在手牌，对玩家造成 2 点伤害（本身不消耗，进弃牌堆）
inline Card makeBurn() {
    Card c{ "灼伤", 0, CardType::Skill, TargetMode::Self,{} };
    c.unplayable = true;
    c.endTurnDamage = 2;
    return c;
}

// 灼伤+：不可打出；回合末 4 点伤害
inline Card makeBurnPlus() {
    Card c{ "灼伤+", 0, CardType::Skill, TargetMode::Self,{} };
    c.unplayable = true;
    c.endTurnDamage = 4;
    return c;
}

// 眩晕：不可打出；虚无（回合末若在手牌则消耗）
inline Card makeDaze() {
    Card c{ "眩晕", 0, CardType::Skill, TargetMode::Self,{} };
    c.unplayable = true;
    c.ethereal = true;
    return c;
}

// ==================== Enemy data ====================
// 如何加新怪物：仿照下方工厂写 makeXxx()。
//   生命：只填 e.hpLo / e.hpHi（闭区间），由 spawnEncounter 用 MAP 域统一掷选；
//         hpHi <= 0 表示固定生命（不掷随机数）。
//   意图：每项 { roll, describe, act, reroll, cooldown }：
//     - roll      预掷需要展示的数值（enemyAi 域），写 self.rolls；固定值可省略；
//     - describe  由已掷值生成精确描述（玩家看到的值 == 执行的值）；
//     - act       执行动作，从 self.rolls（或己方状态）读取同一批值；
//     - reroll    true = 执行完后下一意图重新随机起点（旧默认路径用）；
//     - cooldown  使用后封锁回合数（仅 select 制敌人有意义）。
//   选择：不设 select = 旧默认路径（随机起点 + 顺序循环）；
//         设 select = 返回 pattern 下标（越界自动兜底 0）；
//         设 onTurnEnd = 回合末被动（仪式 / 金属化）；
//         设 split = 血量阈值分裂（回调内判断，返回空 = 不分裂）。
// 数值纪律：展示值必须来自 pre-roll，禁止在 describe / act 中各掷一次。

// ---------- 意图构造工具（减少逐怪重复代码） ----------
inline IntentPattern ipNoop(std::string desc) {
    IntentPattern p;
    p.describe = [desc](BattleContext&, Enemy&) { return desc; };
    p.act = [](BattleContext&, Enemy&) {};
    return p;
}

inline IntentPattern ipDamage(int dmg, std::string name) {
    IntentPattern p;
    p.describe = [dmg, name](BattleContext&, Enemy&) { return name + " " + std::to_string(dmg); };
    p.act = [dmg](BattleContext& ctx, Enemy& self) { dealDamage(ctx, self, ctx.player, dmg); };
    return p;
}

// 连击符号用 ASCII "*"：Minecraft AE 没有 U+00D7 的字形（实测渲染成方框）。
inline IntentPattern ipMultiDamage(int dmg, int times, std::string name) {
    IntentPattern p;
    p.describe = [dmg, times, name](BattleContext&, Enemy&) {
        return name + " " + std::to_string(dmg) + "*" + std::to_string(times);
        };
    p.act = [dmg, times](BattleContext& ctx, Enemy& self) {
        for (int i = 0; i < times; ++i) dealDamage(ctx, self, ctx.player, dmg);
        };
    return p;
}

inline IntentPattern ipBlock(int n, std::string name) {
    IntentPattern p;
    p.describe = [n, name](BattleContext&, Enemy&) { return name + " 格挡 " + std::to_string(n); };
    p.act = [n](BattleContext& ctx, Enemy& self) { gainBlock(ctx, self, n); };
    return p;
}

inline IntentPattern ipDamageBlock(int dmg, int block, std::string name) {
    IntentPattern p;
    p.describe = [dmg, block, name](BattleContext&, Enemy&) {
        return name + " " + std::to_string(dmg) + " + 格挡 " + std::to_string(block);
        };
    p.act = [dmg, block](BattleContext& ctx, Enemy& self) {
        dealDamage(ctx, self, ctx.player, dmg);
        gainBlock(ctx, self, block);
        };
    return p;
}

inline IntentPattern ipDebuff(Status s, int n, std::string name) {
    IntentPattern p;
    p.describe = [s, n, name](BattleContext&, Enemy&) {
        return name + " " + std::to_string(n) + " 层" + statusName(s);
        };
    p.act = [s, n](BattleContext& ctx, Enemy&) { applyDebuff(ctx, ctx.player, s, n); };
    return p;
}

inline IntentPattern ipDamageDebuff(int dmg, Status s, int n, std::string name) {
    IntentPattern p;
    p.describe = [dmg, s, n, name](BattleContext&, Enemy&) {
        return name + " " + std::to_string(dmg) + " + " + std::to_string(n) + " 层" + statusName(s);
        };
    p.act = [dmg, s, n](BattleContext& ctx, Enemy& self) {
        dealDamage(ctx, self, ctx.player, dmg);
        applyDebuff(ctx, ctx.player, s, n);
        };
    return p;
}

inline IntentPattern ipBuff(Status s, int n, std::string name) {
    IntentPattern p;
    p.describe = [s, n, name](BattleContext&, Enemy&) {
        return name + " +" + std::to_string(n) + " " + statusName(s);
        };
    p.act = [s, n](BattleContext& ctx, Enemy& self) {
        self.addStatus(s, n);
        ctx.log(self.name + " 获得 " + std::to_string(n) + " 层" + statusName(s));
        };
    return p;
}

inline IntentPattern ipAddCards(Card c, int count, std::string name) {
    IntentPattern p;
    p.describe = [c, count, name](BattleContext&, Enemy&) {
        return name + "：" + std::to_string(count) + " 张" + c.name + "入弃牌堆";
        };
    p.act = [c, count](BattleContext& ctx, Enemy&) {
        for (int i = 0; i < count; ++i) addCardToPile(ctx, c, PileDest::Discard);
        };
    return p;
}

inline IntentPattern ipDamageAndAdd(int dmg, Card c, int count, std::string name) {
    IntentPattern p;
    p.describe = [dmg, c, count, name](BattleContext&, Enemy&) {
        return name + " " + std::to_string(dmg) + " + " + std::to_string(count) + " 张" + c.name;
        };
    p.act = [dmg, c, count](BattleContext& ctx, Enemy& self) {
        dealDamage(ctx, self, ctx.player, dmg);
        for (int i = 0; i < count; ++i) addCardToPile(ctx, c, PileDest::Discard);
        };
    return p;
}

inline IntentPattern ipMultiDamageAndAdd(int dmg, int times, Card c, int count, std::string name) {
    IntentPattern p;
    p.describe = [dmg, times, c, count, name](BattleContext&, Enemy&) {
        return name + " " + std::to_string(dmg) + "*" + std::to_string(times)
            + " + " + std::to_string(count) + " 张" + c.name;
        };
    p.act = [dmg, times, c, count](BattleContext& ctx, Enemy& self) {
        for (int i = 0; i < times; ++i) dealDamage(ctx, self, ctx.player, dmg);
        for (int i = 0; i < count; ++i) addCardToPile(ctx, c, PileDest::Discard);
        };
    return p;
}

inline IntentPattern ipCustom(std::string desc, std::function<void(BattleContext&, Enemy&)> act) {
    IntentPattern p;
    p.describe = [desc](BattleContext&, Enemy&) { return desc; };
    p.act = std::move(act);
    return p;
}

// 两招交替（首招为 0 号）
inline std::function<int(BattleContext&, Enemy&)> selectAlternate() {
    return [](BattleContext&, Enemy& self) { return self.lastPatternIdx == 0 ? 1 : 0; };
}

// 盗窃：从玩家金币中扣取（真扣，Q4 已拍板）；无金币引用时仅提示。
inline void stealGold(BattleContext& ctx, int n) {
    if (!ctx.gold) { ctx.log("盗窃：没有可偷的金币"); return; }
    int take = std::min(*ctx.gold, n);
    *ctx.gold -= take;
    ctx.log("盗窃：失去 " + std::to_string(take) + " 金币（剩余 "
        + std::to_string(*ctx.gold) + "）");
}

// ---------- 基础怪（保留：测试与弱怪池使用） ----------
inline Enemy makeJawWorm() {
    Enemy e;
    e.name = "大颚虫";
    e.hp = e.maxHp = 14;
    e.patterns = {
        {
            [](BattleContext& ctx, Enemy& self) { self.rolls = { ctx.enemyAi->range(9, 14) }; },
            [](BattleContext&, Enemy& self) { return "攻击 " + std::to_string(self.rolls[0]); },
            [](BattleContext& ctx, Enemy& self) { dealDamage(ctx, self, ctx.player, self.rolls[0]); },
            false
        },
        {
            [](BattleContext& ctx, Enemy& self) { self.rolls = { ctx.enemyAi->range(2, 4) }; },
            [](BattleContext&, Enemy& self) { return "特殊 +" + std::to_string(self.rolls[0]) + " 力量"; },
            [](BattleContext& ctx, Enemy& self) {
                int v = self.rolls[0];
                self.addStatus(Status::Strength, v);
                ctx.log(self.name + " 获得 " + std::to_string(v) + " 点力量");
            },
            true
        },
        {
            [](BattleContext& ctx, Enemy& self) {
                self.rolls = { ctx.enemyAi->range(6, 9), ctx.enemyAi->range(4, 6) };
            },
            [](BattleContext&, Enemy& self) {
                return "攻击 " + std::to_string(self.rolls[0])
                    + " + 格挡 " + std::to_string(self.rolls[1]);
            },
            [](BattleContext& ctx, Enemy& self) {
                dealDamage(ctx, self, ctx.player, self.rolls[0]);
                gainBlock(ctx, self, self.rolls[1]);
            },
            false
        },
    };
    // 首回合避开纯攻击（9–14），只在"特殊 +力量"与"攻击 +格挡"中随机；
    // 之后按原顺序循环。避免与同场敌人开局同时猛击。
    e.select = [](BattleContext& ctx, Enemy& self) {
        if (self.turn == 0) return ctx.enemyAi->range(1, 2);
        return (self.lastPatternIdx + 1) % (int)self.patterns.size();
        };
    return e;
}

inline Enemy makeLouse() {
    Enemy e;
    e.name = "虱虫";
    e.hp = e.maxHp = 10;
    e.patterns = {
        {
            [](BattleContext& ctx, Enemy& self) { self.rolls = { ctx.enemyAi->range(3, 6) }; },
            [](BattleContext&, Enemy& self) { return "攻击 " + std::to_string(self.rolls[0]); },
            [](BattleContext& ctx, Enemy& self) { dealDamage(ctx, self, ctx.player, self.rolls[0]); },
            false
        },
        {
            [](BattleContext& ctx, Enemy& self) { self.rolls = { ctx.enemyAi->range(4, 7) }; },
            [](BattleContext&, Enemy& self) { return "格挡 " + std::to_string(self.rolls[0]); },
            [](BattleContext& ctx, Enemy& self) { gainBlock(ctx, self, self.rolls[0]); },
            false
        },
    };
    return e;
}

// ---------- 弱怪池 ----------
// 邪教徒：首回合念咒（+3 仪式），之后黑暗打击。
// 仪式不在本回合结算：由 startPlayerTurn 在下一个玩家回合开始时统一转化为力量，
// 因此"念咒"的当回合不会同时获得力量。
inline Enemy makeCultist() {
    Enemy e;
    e.name = "邪教徒";
    e.hpLo = 48; e.hpHi = 54; e.hp = e.maxHp = 50;
    e.patterns = {
        ipBuff(Status::Ritual, 3, "念咒"),
        ipDamage(6, "黑暗打击"),
    };
    e.select = [](BattleContext&, Enemy& self) { return self.turn == 0 ? 0 : 1; };
    return e;
}

// 红虱虫 / 绿虱虫：噬咬的 X 在战斗开始时（首次 select）随机 5-7，整场固定。
// 两者都不能连续三回合使用相同行动（aiCounter 记连续次数）。
inline Enemy makeRedLouse() {
    Enemy e;
    e.name = "红虱虫";
    e.hpLo = 10; e.hpHi = 15; e.hp = e.maxHp = 12;
    IntentPattern bite;
    bite.describe = [](BattleContext&, Enemy& self) { return "噬咬 " + std::to_string(self.aiState); };
    bite.act = [](BattleContext& ctx, Enemy& self) { dealDamage(ctx, self, ctx.player, self.aiState); };
    e.patterns = { ipBuff(Status::Strength, 3, "生长"), bite };
    e.select = [](BattleContext& ctx, Enemy& self) {
        if (self.aiState == 0) self.aiState = ctx.enemyAi->range(5, 7);  // 战斗开始随机 X
        int last = self.lastPatternIdx;
        int pick;
        if (last < 0 || self.aiCounter < 2) pick = ctx.enemyAi->range(0, 1);
        else                                pick = 1 - last;             // 已连两次 → 必须换
        self.aiCounter = (pick == last) ? self.aiCounter + 1 : 1;
        return pick;
        };
    return e;
}

inline Enemy makeGreenLouse() {
    Enemy e;
    e.name = "绿虱虫";
    e.hpLo = 11; e.hpHi = 17; e.hp = e.maxHp = 14;
    IntentPattern bite;
    bite.describe = [](BattleContext&, Enemy& self) { return "噬咬 " + std::to_string(self.aiState); };
    bite.act = [](BattleContext& ctx, Enemy& self) { dealDamage(ctx, self, ctx.player, self.aiState); };
    e.patterns = { ipDebuff(Status::Weak, 2, "吐网"), bite };
    e.select = [](BattleContext& ctx, Enemy& self) {
        if (self.aiState == 0) self.aiState = ctx.enemyAi->range(5, 7);  // 战斗开始随机 X
        int last = self.lastPatternIdx;
        int pick;
        if (last < 0 || self.aiCounter < 2) pick = ctx.enemyAi->range(0, 1);
        else                                pick = 1 - last;
        self.aiCounter = (pick == last) ? self.aiCounter + 1 : 1;
        return pick;
        };
    return e;
}

// ---------- 尖刺史莱姆 ----------
inline Enemy makeSpikeSlimeSmall() {
    Enemy e;
    e.name = "尖刺史莱姆（小）";
    e.hpLo = 10; e.hpHi = 14; e.hp = e.maxHp = 12;
    e.patterns = { ipDamage(5, "攻击") };
    return e;
}

inline Enemy makeSpikeSlimeMedium() {
    Enemy e;
    e.name = "尖刺史莱姆（中）";
    e.hpLo = 28; e.hpHi = 32; e.hp = e.maxHp = 30;
    e.patterns = {
        ipDamageAndAdd(8, makeSlimed(), 1, "腐蚀喷吐"),
        ipDebuff(Status::Frail, 1, "舔舔"),
    };
    e.select = [](BattleContext& ctx, Enemy& self) {
        int last = self.lastPatternIdx;
        int pick;
        if (last < 0 || self.aiCounter < 2) pick = ctx.enemyAi->range(0, 1);
        else                                pick = 1 - last;   // 不可连续三次相同
        self.aiCounter = (pick == last) ? self.aiCounter + 1 : 1;
        return pick;
        };
    return e;
}

// 大尖刺：≤50% 生命时分裂为 2 只中尖刺，产物 hp = maxHp = 分裂前剩余生命。
// 分裂时机：玩家回合内只标记；敌人回合开始时用分裂代替当前意图（不执行该意图）。
inline Enemy makeSpikeSlimeLarge() {
    Enemy e;
    e.name = "尖刺史莱姆（大）";
    e.hpLo = 64; e.hpHi = 70; e.hp = e.maxHp = 67;
    e.patterns = {
        ipDamageAndAdd(16, makeSlimed(), 2, "腐蚀喷吐"),
        ipDebuff(Status::Frail, 2, "舔舔"),
    };
    // 两招随机，但不可连续三次相同（原为无记忆随机，会无限复读）
    e.select = [](BattleContext& ctx, Enemy& self) {
        int last = self.lastPatternIdx;
        int pick;
        if (last < 0 || self.aiCounter < 2) pick = ctx.enemyAi->range(0, 1);
        else                                pick = 1 - last;
        self.aiCounter = (pick == last) ? self.aiCounter + 1 : 1;
        return pick;
        };
    e.split = [](BattleContext& ctx, Enemy& self) -> std::vector<Enemy> {
        if (ctx.playerTurn) return {};               // 玩家回合内只标记
        if (self.hp * 2 > self.maxHp) return {};
        std::vector<Enemy> out;
        for (int i = 0; i < 2; ++i) {
            Enemy c = makeSpikeSlimeMedium();
            c.hp = c.maxHp = self.hp;
            out.push_back(std::move(c));
        }
        return out;
        };
    e.splitReplacesAction = true;                    // 敌人回合开始时分裂，代替当前意图
    return e;
}

// ---------- 酸液史莱姆 ----------
inline Enemy makeAcidSlimeSmall() {
    Enemy e;
    e.name = "酸液史莱姆（小）";
    e.hpLo = 8; e.hpHi = 12; e.hp = e.maxHp = 10;
    e.patterns = {
        ipDamage(3, "轻击"),
        ipDebuff(Status::Weak, 1, "舔舔"),
    };
    e.select = selectAlternate();
    return e;
}

// 中酸液：权重 3:4:3；重击不可连续两次，其余不可连续三次。
inline Enemy makeAcidSlimeMedium() {
    Enemy e;
    e.name = "酸液史莱姆（中）";
    e.hpLo = 28; e.hpHi = 32; e.hp = e.maxHp = 30;
    e.patterns = {
        ipDamageAndAdd(11, makeSlimed(), 2, "腐蚀喷吐"),
        ipDamage(16, "重击"),
        ipDebuff(Status::Weak, 2, "舔舔"),
    };
    e.select = [](BattleContext& ctx, Enemy& self) {
        static const int w[3] = { 3, 4, 3 };
        int last = self.lastPatternIdx;
        int ok[3] = { 1, 1, 1 };
        if (last == 1) ok[1] = 0;                              // 重击不可连续
        if (last >= 0 && self.aiCounter >= 2) ok[last] = 0;     // 其余不可连续三次
        if (self.turn == 0) ok[1] = 0;                          // 首回合不出重击（避免同场同时猛击）
        int total = 0;
        for (int i = 0; i < 3; ++i) if (ok[i]) total += w[i];
        int pick = 1;
        if (total > 0) {
            int r = ctx.enemyAi->range(0, total - 1);
            for (int i = 0; i < 3; ++i) {
                if (!ok[i]) continue;
                if (r < w[i]) { pick = i; break; }
                r -= w[i];
            }
        }
        self.aiCounter = (pick == last) ? self.aiCounter + 1 : 1;
        return pick;
        };
    return e;
}

// 大酸液：≤50% 分裂为 2 只中酸液，产物 hp = maxHp = 分裂前剩余生命。
// 分裂时机：玩家回合内只标记；敌人回合开始时用分裂代替当前意图（不执行该意图）。
inline Enemy makeAcidSlimeLarge() {
    Enemy e;
    e.name = "酸液史莱姆（大）";
    e.hpLo = 65; e.hpHi = 69; e.hp = e.maxHp = 67;
    e.patterns = {
        ipDamageAndAdd(11, makeSlimed(), 2, "腐蚀喷吐"),
        ipDamage(16, "重击"),
        ipDebuff(Status::Weak, 2, "舔舔"),
    };
    // 权重 3:4:3；重击不可连续两次，其余不可连续三次（与中酸液一致；原为无记忆随机）
    e.select = [](BattleContext& ctx, Enemy& self) {
        static const int w[3] = { 3, 4, 3 };
        int last = self.lastPatternIdx;
        int ok[3] = { 1, 1, 1 };
        if (last == 1) ok[1] = 0;                              // 重击不可连续
        if (last >= 0 && self.aiCounter >= 2) ok[last] = 0;     // 其余不可连续三次
        if (self.turn == 0) ok[1] = 0;                          // 首回合不出重击（与中酸液一致，避免开局猛击）
        int total = 0;
        for (int i = 0; i < 3; ++i) if (ok[i]) total += w[i];
        int pick = 1;
        if (total > 0) {
            int r = ctx.enemyAi->range(0, total - 1);
            for (int i = 0; i < 3; ++i) {
                if (!ok[i]) continue;
                if (r < w[i]) { pick = i; break; }
                r -= w[i];
            }
        }
        self.aiCounter = (pick == last) ? self.aiCounter + 1 : 1;
        return pick;
        };
    e.split = [](BattleContext& ctx, Enemy& self) -> std::vector<Enemy> {
        if (ctx.playerTurn) return {};               // 玩家回合内只标记
        if (self.hp * 2 > self.maxHp) return {};
        std::vector<Enemy> out;
        for (int i = 0; i < 2; ++i) {
            Enemy c = makeAcidSlimeMedium();
            c.hp = c.maxHp = self.hp;
            out.push_back(std::move(c));
        }
        return out;
        };
    e.splitReplacesAction = true;                    // 敌人回合开始时分裂，代替当前意图
    return e;
}

// ---------- 菌类 ----------
// 飞蝇菌子：首回合只出脆弱孢子（不出猛砸，避免与同场敌人开局同时猛击）；
//   之后三招等权（受冷却与不可连续约束）。
//   脆弱孢子 冷却 2；易伤孢子 冷却 3 且第 2 次行动起可用；猛砸不可连续。
inline Enemy makeFlyingFungi() {
    Enemy e;
    e.name = "飞蝇菌子";
    e.hpLo = 47; e.hpHi = 49; e.hp = e.maxHp = 48;
    IntentPattern spore = ipDamageDebuff(8, Status::Frail, 2, "脆弱孢子");
    spore.cooldown = 2;
    IntentPattern smash = ipDamage(11, "猛砸");
    IntentPattern vulnSpore = ipDebuff(Status::Vulnerable, 2, "易伤孢子");
    vulnSpore.cooldown = 3;
    e.patterns = { spore, smash, vulnSpore };
    e.select = [](BattleContext& ctx, Enemy& self) {
        std::vector<int> ok;
        if (self.turn == 0) {
            ok = { 0 };                                      // 首回合只出脆弱孢子
        }
        else {
            if (self.cooldown[0] == 0) ok.push_back(0);
            if (self.cooldown[1] == 0 && self.lastPatternIdx != 1) ok.push_back(1);
            if (self.cooldown[2] == 0) ok.push_back(2);      // turn >= 1：第 2 次行动起
        }
        // 兜底：不可在上一招是猛砸时再出猛砸；其余情况才允许猛砸。
        if (ok.empty()) ok.push_back(self.lastPatternIdx == 1 ? 0 : 1);
        return ok[ctx.enemyAi->range(0, (int)ok.size() - 1)];
        };
    return e;
}

// 真菌兽：攻击概率略高（60%）；不可连续三次攻击，不可连续两次生长。
inline Enemy makeFungiBeast() {
    Enemy e;
    e.name = "真菌兽";
    e.hpLo = 22; e.hpHi = 28; e.hp = e.maxHp = 25;
    e.patterns = {
        ipDamage(6, "攻击"),
        ipBuff(Status::Strength, 3, "生长"),
    };
    e.select = [](BattleContext& ctx, Enemy& self) {
        int last = self.lastPatternIdx;
        if (last == 1) { self.aiCounter = 1; return 0; }                        // 生长后的这一刀计入连续攻击
        if (last == 0 && self.aiCounter >= 2) { self.aiCounter = 0; return 1; } // 不可连续三次攻击
        int pick = ctx.enemyAi->chance(0.6) ? 0 : 1;
        self.aiCounter = (pick == 0) ? self.aiCounter + 1 : 0;
        return pick;
        };
    return e;
}

// ---------- 劫掠者 ----------
inline Enemy makeRaiderBruiser() {
    Enemy e;
    e.name = "劫掠者暴徒";
    e.hpLo = 30; e.hpHi = 33; e.hp = e.maxHp = 31;
    e.patterns = { ipDamage(7, "殴打"), ipBuff(Status::Strength, 3, "怒吼") };
    e.select = selectAlternate();
    return e;
}

inline Enemy makeRaiderAssassin() {
    Enemy e;
    e.name = "劫掠者刺客";
    e.hpLo = 18; e.hpHi = 23; e.hp = e.maxHp = 20;
    e.patterns = { ipDamage(10, "致命射击") };
    return e;
}

inline Enemy makeRaiderAxe() {
    Enemy e;
    e.name = "劫掠者斧手";
    e.hpLo = 20; e.hpHi = 22; e.hp = e.maxHp = 21;
    e.patterns = { ipDamageBlock(5, 5, "挥击"), ipDamage(12, "大力挥舞") };
    e.select = selectAlternate();
    return e;
}

inline Enemy makeRaiderCrossbow() {
    Enemy e;
    e.name = "劫掠者弩手";
    e.hpLo = 18; e.hpHi = 21; e.hp = e.maxHp = 19;
    e.patterns = { ipBlock(3, "装填"), ipDamage(14, "射击") };
    e.select = selectAlternate();
    return e;
}

inline Enemy makeRaiderTracker() {
    Enemy e;
    e.name = "劫掠者追踪手";
    e.hpLo = 21; e.hpHi = 25; e.hp = e.maxHp = 23;
    e.patterns = {
        ipDebuff(Status::Frail, 2, "追踪"),
        ipMultiDamage(1, 8, "放狗"),
    };
    e.select = [](BattleContext&, Enemy& self) { return self.turn == 0 ? 0 : 1; };
    return e;
}

// ---------- 奴隶商人 ----------
// 红奴隶商人：首回合刺伤；之后随机刮伤 / 丢网。丢网后固定走 刮伤→刮伤→刺伤 再回到随机。
inline Enemy makeRedSlaver() {
    Enemy e;
    e.name = "红奴隶商人";
    e.hpLo = 46; e.hpHi = 50; e.hp = e.maxHp = 48;
    e.patterns = {
        ipDamage(13, "刺伤"),
        ipDamageDebuff(8, Status::Frail, 1, "刮伤"),
        ipDebuff(Status::Entangle, 1, "丢网"),
    };
    e.select = [](BattleContext& ctx, Enemy& self) {
        if (self.turn == 0) return 0;                    // 首回合必定刺伤
        if (self.aiState == 1) {                         // 丢网后的固定脚本
            int step = self.aiCounter;
            if (step < 3) {
                ++self.aiCounter;
                return step < 2 ? 1 : 0;                 // 刮伤、刮伤、刺伤
            }
            self.aiState = 0;
            self.aiCounter = 0;
        }
        // 非脚本阶段：刮伤 50% / 丢网 50%，但刮伤不可连续三次
        int last = self.lastPatternIdx;
        bool scrapOk = !(last == 1 && self.aiCounter >= 2);
        if (scrapOk && ctx.enemyAi->range(0, 1) == 0) {
            self.aiCounter = (last == 1) ? self.aiCounter + 1 : 1;
            return 1;                                    // 刮伤
        }
        self.aiState = 1; self.aiCounter = 0;            // 丢网 → 进入脚本
        return 2;
        };
    return e;
}

// 蓝奴隶商人：刺伤 60% / 耙伤 40%
inline Enemy makeBlueSlaver() {
    Enemy e;
    e.name = "蓝奴隶商人";
    e.hpLo = 46; e.hpHi = 50; e.hp = e.maxHp = 48;
    e.patterns = {
        ipDamage(12, "刺伤"),
        ipDamageDebuff(7, Status::Weak, 1, "耙伤"),
    };
    // 刺伤 60% / 耙伤 40%，但不可连续三次相同（原为无记忆随机，会无限复读刺伤）
    e.select = [](BattleContext& ctx, Enemy& self) {
        int last = self.lastPatternIdx;
        int pick;
        if (last < 0 || self.aiCounter < 2) pick = ctx.enemyAi->chance(0.6) ? 0 : 1;
        else                                pick = 1 - last;
        self.aiCounter = (pick == last) ? self.aiCounter + 1 : 1;
        return pick;
        };
    return e;
}

// ---------- 抢劫的 / 暴徒 ----------
// 前两回合盗窃（同时造成 11 点伤害）；第 3 回合 50% 重击（随后必接找烟雾弹）
// 或直接找烟雾弹；找烟雾弹后下回合逃跑。
// 重击伤害文本未给（沿用 STS 量级）：抢劫的 10 / 暴徒 18；盗窃 15 / 20。
inline Enemy makeLooterImpl(const char* name, int lo, int hi, int heavy, int steal) {
    Enemy e;
    e.name = name;
    e.hpLo = lo; e.hpHi = hi; e.hp = e.maxHp = (lo + hi) / 2;
    e.patterns = {
        ipCustom("盗窃 " + std::to_string(steal) + " 金币 + 攻击 11",
            [steal](BattleContext& ctx, Enemy& self) {
                dealDamage(ctx, self, ctx.player, 11);   // 先攻击，再偷钱
                stealGold(ctx, steal);
            }),
        ipDamage(heavy, "重击"),
        ipCustom("找烟雾弹", [](BattleContext& ctx, Enemy&) {
            ctx.log("找烟雾弹：下回合逃跑");
            }),
        ipCustom("逃跑", [](BattleContext& ctx, Enemy& self) {
            self.fled = true;
            ctx.log(self.name + " 逃跑了");
            }),
    };
    e.select = [](BattleContext& ctx, Enemy& self) {
        if (self.turn < 2) return 0;                              // 前两回合盗窃
        if (self.aiState == 1) { self.aiState = 2; return 2; }    // 重击后 → 找烟雾弹
        if (self.aiState == 2) return 3;                          // 找烟雾弹后 → 逃跑
        if (ctx.enemyAi->range(0, 1) == 0) { self.aiState = 1; return 1; }
        self.aiState = 2;
        return 2;
        };
    return e;
}

inline Enemy makeLooter() { return makeLooterImpl("抢劫的", 44, 48, 10, 15); }
inline Enemy makeMugger() { return makeLooterImpl("暴徒", 48, 52, 18, 20); }

// ---------- 精英 ----------
// 地精大块头：首回合咆哮（+2 愤怒），之后颅骨重击 33% / 蛮牛冲撞 67%。
inline Enemy makeGremlinNob() {
    Enemy e;
    e.name = "地精大块头";
    e.hpLo = 82; e.hpHi = 86; e.hp = e.maxHp = 84;
    e.patterns = {
        ipBuff(Status::Enrage, 2, "咆哮"),
        ipDamageDebuff(6, Status::Vulnerable, 2, "颅骨重击"),
        ipDamage(14, "蛮牛冲撞"),
    };
    e.select = [](BattleContext& ctx, Enemy& self) {
        if (self.turn == 0) return 0;
        return ctx.enemyAi->chance(0.33) ? 1 : 2;
        };
    return e;
}

// 乐加维林：初始 8 金属化；前 3 回合沉睡。
// 睡眠中实际掉血 → 立即醒来，当前意图改为「眩晕」（不行动），之后进入
// 重击、重击、灵魂汲取 循环；若未受伤则睡满 3 回合后自然醒来。
inline Enemy makeLagavulin() {
    Enemy e;
    e.name = "乐加维林";
    e.hpLo = 109; e.hpHi = 111; e.hp = e.maxHp = 110;
    e.addStatus(Status::Metallicize, 8);
    e.patterns = {
        ipNoop("沉睡"),                              // 0
        ipDamage(18, "重击"),                        // 1
        ipCustom("灵魂汲取 -1 力量 -1 敏捷", [](BattleContext& ctx, Enemy&) {   // 2
            ctx.player.addStatus(Status::Strength, -1);
            ctx.player.addStatus(Status::Dexterity, -1);
            ctx.log("灵魂汲取：玩家 -1 力量 -1 敏捷");
            }),
        ipNoop("眩晕"),                              // 3：被打醒后的空过回合
    };
    // 睡梦中掉血 → 立即失去睡眠，并把当前展示的意图替换为「眩晕」
    e.onDamaged = [](BattleContext& ctx, Entity& ent) {
        Enemy& self = static_cast<Enemy&>(ent);
        if (self.aiState != 0) return;               // 已醒：不再重复触发
        self.aiState = 1;                            // 失去睡眠
        const int stun = 3;
        self.patternIdx = stun;
        self.rolls.clear();
        auto& p = self.patterns[stun];
        if (p.roll) p.roll(ctx, self);
        self.current = Intent{ p.describe ? p.describe(ctx, self) : std::string{}, p.act };
        };
    e.onTurnEnd = [](BattleContext& ctx, Enemy& self) {
        int metal = self.getStatus(Status::Metallicize);
        if (metal > 0) gainBlock(ctx, self, metal);
        };
    e.select = [](BattleContext&, Enemy& self) {
        if (self.aiState == 0) {
            if (self.turn < 3) return 0;             // 未受伤：睡满 3 回合
            self.aiState = 1;                        // 自然醒
        }
        int t = self.aiCounter % 3;                  // 醒来后的循环
        ++self.aiCounter;
        return t < 2 ? 1 : 2;                        // 重击、重击、灵魂汲取
        };
    return e;
}

// 哨卫：初始 1 人工制品；激光与螺旋催眠固定交替。
// 三哨卫中只有一个以螺旋催眠开局（见 map.hpp 出怪配置），其余以激光开局，
// 形成"首回合两攻击一塞牌"的交错节奏（对应 STS 的相位错开）。
inline Enemy makeSentinel(int firstPattern = 0) {
    Enemy e;
    e.name = "哨卫";
    e.hpLo = 38; e.hpHi = 42; e.hp = e.maxHp = 40;
    e.addStatus(Status::Artifact, 1);
    e.patterns = {
        ipDamage(9, "激光"),
        ipAddCards(makeDaze(), 2, "螺旋催眠"),
    };
    e.select = [firstPattern](BattleContext&, Enemy& self) {
        if (self.lastPatternIdx < 0) return firstPattern;              // 开局相位
        return self.lastPatternIdx == 0 ? 1 : 0;                       // 之后严格交替
        };
    return e;
}

// ---------- BOSS ----------
// 六火亡魂：首回合准备；第 2 回合特殊地狱火（X = 玩家生命/12+1，6 次）；
// 之后 7 回合循环：灼烧 → 双击 → 灼烧 → 高热 → 双击 → 灼烧 → 地狱火。
inline Enemy makeHexaghost() {
    Enemy e;
    e.name = "六火亡魂";
    e.hp = e.maxHp = 250;
    IntentPattern special;
    special.roll = [](BattleContext& ctx, Enemy& self) {
        self.rolls = { ctx.player.hp / 12 + 1 };
        };
    special.describe = [](BattleContext&, Enemy& self) {
        return "特殊地狱火 " + std::to_string(self.rolls[0]) + "*6";
        };
    special.act = [](BattleContext& ctx, Enemy& self) {
        for (int i = 0; i < 6; ++i) dealDamage(ctx, self, ctx.player, self.rolls[0]);
        };
    e.patterns = {
        ipNoop("准备"),
        special,
        ipDamageAndAdd(6, makeBurn(), 1, "灼烧"),
        ipMultiDamage(5, 2, "双击"),
        ipCustom("高热 +2 力量 12 格挡", [](BattleContext& ctx, Enemy& self) {
            self.addStatus(Status::Strength, 2);
            gainBlock(ctx, self, 12);
            ctx.log("高热：获得 2 点力量与 12 点格挡");
            }),
        ipMultiDamageAndAdd(2, 6, makeBurnPlus(), 3, "地狱火"),
    };
    e.select = [](BattleContext&, Enemy& self) {
        static const int cyc[7] = { 2, 3, 2, 4, 3, 2, 5 };
        if (self.turn == 0) return 0;                 // 准备
        if (self.turn == 1) return 1;                 // 特殊地狱火
        return cyc[(self.turn - 2) % 7];
        };
    return e;
}

// 守护者：纯计数形态循环——7 回合一轮：
//   攻击形态 蓄能→猛击→蒸汽喷射→旋风；防御形态 强化→撞击→双斩。
//   每轮结束（双斩后）形态转换 +10（aiCounter 仅作数值记录，纯计数不参与伤害阈值）。
inline Enemy makeGuardian() {
    Enemy e;
    e.name = "守护者";
    e.hp = e.maxHp = 240;
    e.patterns = {
        ipBlock(9, "蓄能"),
        ipDamage(32, "猛击"),
        ipCustom("蒸汽喷射 2 虚弱 + 2 易伤", [](BattleContext& ctx, Enemy&) {
            applyDebuff(ctx, ctx.player, Status::Weak, 2);
            applyDebuff(ctx, ctx.player, Status::Vulnerable, 2);
            }),
        ipMultiDamage(5, 4, "旋风"),
        ipBuff(Status::SharpHide, 3, "强化"),
        ipDamage(9, "撞击"),
        ipMultiDamage(8, 2, "双斩"),
    };
    e.select = [](BattleContext&, Enemy& self) {
        self.aiCounter = 30 + 10 * (self.turn / 7);   // 形态转换（纯计数记录）
        return self.turn % 7;
        };
    return e;
}

// 史莱姆老大：固定循环 负面效果攻击（3 张黏液）→ 准备 → 史莱姆撞击 35；
// ≤50% 生命时分裂为 1 只大酸液 + 1 只大尖刺，产物 hp = maxHp = 分裂前剩余生命。
// 分裂时机：玩家回合内只标记；敌人回合开始时用分裂代替当前意图（不执行该意图）。
inline Enemy makeSlimeBoss() {
    Enemy e;
    e.name = "史莱姆老大";
    e.hp = e.maxHp = 140;
    e.patterns = {
        ipAddCards(makeSlimed(), 3, "负面效果攻击"),
        ipNoop("准备"),
        ipDamage(35, "史莱姆撞击"),
    };
    e.select = [](BattleContext&, Enemy& self) { return self.turn % 3; };
    e.split = [](BattleContext& ctx, Enemy& self) -> std::vector<Enemy> {
        if (ctx.playerTurn) return {};               // 玩家回合内只标记
        if (self.hp * 2 > self.maxHp) return {};
        Enemy acid = makeAcidSlimeLarge();
        Enemy spike = makeSpikeSlimeLarge();
        acid.hp = acid.maxHp = self.hp;
        spike.hp = spike.maxHp = self.hp;
        std::vector<Enemy> out;
        out.push_back(std::move(acid));
        out.push_back(std::move(spike));
        return out;
        };
    e.splitReplacesAction = true;                    // 敌人回合开始时分裂，代替当前意图
    return e;
}

// ==================== 全卡池（奖励系统候选池） ====================

// 全部卡牌工厂集合：rewards.hpp 的三选一从这里洗牌取候选。
// 新增卡牌时记得在列表里登记，否则永远不会进奖励池。
// 注意：打击 / 防御 / 痛击 是初始牌组自带的基础牌，不进奖励池。
inline std::vector<CardFactory> allCardFactories() {
    return {
        // —— 攻击（28）——
        makeIronWave, makePommelStrike, makeThunderClap,
        makeBodySlam, makeHemokinesis, makeHeadbutt, makeDoubleStrike,
        makeBoomerang, makeBreakthrough, makeBully, makeEmber, makeSledgehammer,
        makeBludgeon, makeInferno, makePerfectedStrike, makeAshStrike,
        makeDismantle, makePrepareStrike, makeWhirlwind, makeTrample, makeEvilEye,
        makeClash, makeContractEnd, makeFeast, makeResentment, makeMoltenFist,
        makeAnger, makeOutrage,
        // —— 技能（24）——
        makeShrugItOff, makeBloodletting, makeTrueGrit, makeProvoke,
        makeTremor, makeBurningPact, makeBloodWall, makeOffering, makeBrand,
        makeCrimsonCloak, makeEntrench, makeReinvigorate, makeDump, makeRuin,
        makeColossus, makeConcentration, makeWarDrum, makeForgottenRitual, makeRage,
        makeFlameBarrier, makeAddFuel, makeDominator, makeDualWield, makeComboFist,
        // —— 能力（16）——
        makeFlee, makeInflame, makeRockArmor, makeCruelty, makeFeelNoPain, makeDarkEmbrace,
        makeSteadfast, makeRelentless, makeJuggle, makeBarricade, makeCorruption,
        makeInfernoFire, makeHellbound, makePyreSource, makeDemonForm, makeMetallize,
    };
}
// ==================== 全卡池清单（含实现状态） ====================
// 标注：[已实现] = 已进 allCardFactories；[基础牌] = 初始牌组自带、不进奖励池；
//       未实现 / 搁置的卡牌统一列在文末。
//       [与实现不一致-待确认] = 卡牌原文与当前代码的数值 / 类型不符，尚未决定以哪边为准。
// 打击 / 1 / 攻击 / 造成6点伤害。                                     [基础牌]
// 防御 / 1 / 技能 / 获得5点格挡。                                     [基础牌]
// 痛击 / 2 / 攻击 / 造成8点伤害，给予2层易伤。                        [基础牌]
// 愤怒 / 0 / 攻击 / 造成6点伤害，将一张此牌的复制品加入弃牌堆。       [已实现]
// 铁斩波 / 1 / 攻击 / 获得5点格挡，造成5点伤害。                      [已实现]
// 剑柄打击 / 1 / 攻击 / 造成9点伤害，抽1张牌。                        [已实现]
// 全身撞击 / 1 / 攻击 / 造成等同于你当前格挡值的伤害。                [已实现]
// 飞剑回旋镖 / 1 / 攻击 / 随机对敌人造成3点伤害3次。                  [已实现]
// 双重打击 / 1 / 攻击 / 造成5点伤害两次。                             [已实现]
// 闪电霹雳 / 1 / 攻击 / 对所有敌人造成4点伤害，给予1层易伤。          [已实现]
// 头槌 / 1 / 攻击 / 造成9点伤害，将你弃牌堆中的一张牌放到抽牌堆顶部。  [已实现]
// 突破 / 1 / 攻击 / 失去1点生命，对所有敌人造成9点伤害。              [已实现]
// 预备打击 / 1 / 攻击 / 造成7点伤害，在本回合内获得2点力量。          [已实现]
// 熔融之拳 / 1 / 攻击 / 造成10点伤害，将该敌人身上的易伤层数翻倍，消耗。 [已实现]
// 破灭 / 1 / 技能 / 打出你抽牌堆顶部的牌，并将其消耗。                [已实现]
// 耸肩无视 / 1 / 技能 / 获得8点格挡，抽1张牌。                        [已实现]
// 挑衅 / 1 / 技能 / 获得6点格挡，给予1层易伤。                        [已实现]
// 战栗 / 1 / 技能 / 给予3层易伤，消耗。                               [已实现]
// 坚毅 / 1 / 技能 / 获得7点格挡，随机消耗1张牌。                      [已实现]
// 燃烧契约 / 1 / 技能 / 消耗1张牌，抽2张牌。                          [已实现]
// 巨像 / 1 / 技能 / 获得4点格挡，在本回合中，有易伤状态的敌人对你造成的伤害降低50%。 [已实现]
// 残酷 / 1 / 能力 / 有易伤状态的敌人额外受到25%的伤害。               [已实现]
// 拆卸 / 1 / 攻击 / 造成8点伤害，如果该敌人有易伤状态，则攻击两次。   [已实现]
// 战鼓 / 1 / 技能 / 抽2张牌，这张牌被消耗时，获得能量。               [已实现]
// 邪眼 / 1 / 技能 / 获得8点格挡，如果你在本回合消耗过卡牌，则额外获得8点格挡。 [已实现]
// 无惧疼痛 / 1 / 能力 / 每当有一张牌被消耗时，获得3点格挡。           [已实现]
// 被遗忘的仪式 / 1 / 技能 / 如果你在本回合消耗过卡牌，则获得能量，消耗。 [已实现]
// 御血术 / 1 / 攻击 / 失去2点生命，造成15点伤害。                     [已实现]
// 血墙 / 2 / 技能 / 失去2点生命，获得16点格挡。                       [已实现]
// 余烬 / 2 / 攻击 / 造成18点伤害，随机消耗1张牌。                     [已实现]
// 完美打击 / 2 / 攻击 / 造成6点伤害，你每有一张名字中含有"打击"的牌，伤害+2。 [已实现]
// 战斗专注 / 0 / 技能 / 抽3张牌，你在本回合内不能再抽任何牌。         [已实现]
// 放血 / 0 / 技能 / 失去3点生命，获得能量。                           [已实现]
// 欺凌 / 0 / 攻击 / 造成4点伤害，该敌人身上每有一层易伤就额外造成2点伤害。 [已实现]
// 群情激愤 / 0 / 攻击 / 造成9点伤害，在所有人的弃牌堆中加入一张此牌的复制品。 [已实现]
// 狂怒 / 0 / 技能 / 打出此牌后，你在这个回合内每打出一张攻击牌，获得3点格挡。 [已实现]
// 怨恨 / 0 / 攻击 / 造成5点伤害，如果你在本回合失去过生命值，则攻击2次。 [已实现]
// 灰烬打击 / 0 / 攻击 / 造成6点伤害，你的消耗牌堆中每有一张牌，伤害增加3。 [已实现]
// 契约终结 / 0 / 攻击 / 如果你的消耗牌堆拥有大于等于3张牌，则对所有敌人造成17点伤害。 [已实现]
// 交锋 / 0 / 攻击 / 只有在手牌中每一张牌都是攻击牌时才能被打出，造成14点伤害。 [已实现]
// 祭品 / 0 / 技能 / 失去6点生命，获得2点能量，抽3张牌，消耗。         [已实现]
// 烙印 / 0 / 技能 / 失去1点生命，消耗1张牌，获得1点力量。             [已实现]
// 添柴 / 1 / 技能 / 消耗所有手牌，每消耗一张牌，将1张随机牌加入你的手牌。 [已实现]
// 连环拳 / 1 / 技能 / 在这个回合，你打出的下1张攻击牌会被额外打出一次。 [已实现]
// 双持 / 1 / 技能 / 选择一张攻击牌或能力牌，将该牌的一张复制品加入你的手牌。 [已实现]
// 黑暗之拥 / 2 / 能力 / 每当有一张牌被消耗时，抽1张牌。               [已实现]
// 坚定不移 / 2 / 能力 / 翻倍你每回合第一次从卡牌中获得的格挡。        [已实现]
// 势不可当 / 3 / 能力 / 每当你获得格挡时，对随机敌人造成6点伤害。      [已实现]
// 薪火之源 / 2 / 能力 / 在回合开始时，获得1点能量。                   [已实现]
// 惊逃 / 2 / 能力 / 在你的回合结束时，随机打出你手牌中的1张攻击牌攻击随机敌人。 [已实现]
// 杂耍 / 1 / 能力 / 将你在每回合打出的第三张攻击牌的复制品加入你的手牌。 [已实现]
// 火焰屏障 / 2 / 技能 / 获得12点格挡，你在这个回合每受到一次攻击，都会对攻击者造成4点伤害。 [已实现]
// 重振精神 / 1 / 技能 / 消耗手牌中所有非攻击牌，每张获得5点格挡。     [已实现]
// 主宰 / 1 / 技能 / 给予1层易伤，敌人身上每有一层易伤，就获得1点力量，消耗。 [已实现]
// 倾泻 / X / 技能 / 打出你抽牌堆顶部的X张牌。                         [已实现]
// 旋风斩 / X / 攻击 / 对所有敌人造成6点伤害X次。                      [已实现]
// 巩固 / 2 / 技能 / 将你当前的格挡翻倍。                               [已实现]
// 重锤 / 3 / 攻击 / 造成32点伤害。                                    [已实现]
// 破击 / 1 / 攻击 / 造成20点伤害，给予5层易伤。                       [已实现]
// 狂宴 / 1 / 攻击 / 造成10点伤害，斩杀时永久获得3点最大生命值，消耗。 [已实现]
// 壁垒 / 3 / 能力 / 格挡不再在你的回合开始时消失。                    [已实现]
// 恶魔形态 / 3 / 能力 / 在你的回合开始时，获得2点力量。               [已实现]
// 腐化 / 3 / 能力 / 技能牌费用变为0，每当你打出一张技能牌时，将其消耗。 [已实现]
// 燃烧 / 1 / 能力 / 获得2点力量。                                     [已实现]
// 金属化 / 1 / 能力 / 在你的回合结束时，获得3点格挡。                 [已实现]
// 地狱狂徒 / 1 / 能力 / 每当你抽到名字中有"打击"的牌时，对一名随机敌人打出这张牌。 [已实现]
// 焚烧 / 1 / 攻击 / 对所有敌人造成2点伤害4次。                        [已实现]
// 踩踏 / 3 / 攻击 / 对所有敌人造成12点伤害，你在本回合中每打出过一张攻击牌，其耗能减少1。 [已实现]
// 狱火 / 1 / 能力 / 在你的回合开始时，失去1点生命，每当你在你的回合内失去生命时，对所有敌人造成6点伤害。 [已实现]
// 岩石铠甲 / 1 / 技能 / 获得6层覆甲。                                 [已实现]
// 绯红披风 / 1 / 技能 / 失去1点生命，获得8点格挡。                    [已实现]
// ==================== 未实现 / 搁置卡牌 ====================
// 武装 / 1 / 技能 / 获得5点格挡，升级你手牌中的一张牌。               [未实现-升级系统]
// 恶魔护盾 / 0 / 技能 / 失去1点生命，给予另一位玩家你当前格挡值的格挡，消耗。 [搁置-多人元素]
// 原始力量 / 0 / 技能 / 将手牌中的所有攻击牌变化为巨石。              [搁置-机制复杂]
// 彼岸咆哮 / 3 / 攻击 / 对所有敌人造成21点伤害，在你的回合结束时，如果这张牌在你的消耗牌堆中，则将其打出。 [搁置-暂不实现]