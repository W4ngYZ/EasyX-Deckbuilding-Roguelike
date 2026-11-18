// =======================================
// core.hpp 数据层：类型 / 实体 / 卡牌 / 敌人 / 战斗状态容器
//
// 只存数据，不含任何战斗规则函数（规则在 battle.hpp）。
// 纯 C++ 标准库，禁止 include 图形库 / 随机库。
// =======================================

#pragma once

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace rng { class Rng; }  // 仅前向声明，保持数据层纯净（指针由 battle.hpp 解引用）

// ==================== Basic types ====================
// 如何加新状态：在此枚举追加 + 下面 statusName 补对应分支；
// 若状态需要每回合衰减（如 Vulnerable / Weak），
// 还要在 battle.hpp 的 endPlayerTurn / enemyTurn 的 tickStatus 处补衰减。
enum class Status {
    Strength, Dexterity, Vulnerable, Weak, PlatedArmor,
    Frail, Ritual, Enrage, Entangle, Metallicize, Artifact, SharpHide
};
enum class CardType { Attack, Skill, Power };
enum class TargetMode { Self, SingleEnemy, AllEnemies, RandomEnemy };

inline const char* statusName(Status s) {
    switch (s) {
    case Status::Strength:   return "力量";
    case Status::Dexterity:  return "敏捷";
    case Status::Vulnerable: return "易伤";
    case Status::Weak:       return "虚弱";
    case Status::PlatedArmor: return "覆甲";
    case Status::Frail:      return "脆弱";
    case Status::Ritual:     return "仪式";
    case Status::Enrage:     return "愤怒";
    case Status::Entangle:   return "缠绕";
    case Status::Metallicize: return "金属化(敌)";  // 与玩家侧 ctx.metallize 同名不同物
    case Status::Artifact:   return "人工制品";
    case Status::SharpHide:  return "锋利外甲";
    }
    return "?";
}

struct BattleContext;  // forward declaration

// ==================== Entities ====================
struct Entity {
    std::string name;
    int hp = 1, maxHp = 1, block = 0;
    int tempStrength = 0;        // 回合级临时力量：回合末清零（目前仅"预备打击"使用）
    std::map<Status, int> status;
    bool fled = false;           // 逃跑标记（敌人使用；玩家恒为 false）

    // 受伤回调：实际掉血后触发（被格挡全额吸收时不触发）。
    // 目前仅乐加维林使用：睡梦中掉血 → 立即醒来并把当前意图改为「眩晕」。
    std::function<void(BattleContext&, Entity&)> onDamaged = nullptr;

    bool alive() const { return hp > 0; }
    // 在场统一判定：活着且未逃跑。伤害 / 反伤 / 目标选择 / 胜负判定一律以此为准，
    // 逃跑单位不再被结算。
    bool inPlay() const { return hp > 0 && !fled; }
    int getStatus(Status s) const {
        auto it = status.find(s);
        return it == status.end() ? 0 : it->second;
    }
    void addStatus(Status s, int v) {
        status[s] += v;
        if (status[s] == 0) status.erase(s);
    }
};

struct Enemy;  // forward declaration

struct Intent {
    std::string desc;
    std::function<void(BattleContext&, Enemy&)> act;
};

// 敌人意图模式（pre-roll 设计，展示精确值）：
//   roll      预掷本意图的数值（enemyAi 决策事件），写入 self.rolls
//   describe  由已掷值生成精确意图描述，作为 current.desc 展示给玩家
//   act       执行意图动作，从 self.rolls 读取同一批预掷值（与描述一致）
// 执行完该意图后，reroll 为 true 表示下一意图重新随机起点，否则按表顺序循环。
struct IntentPattern {
    std::function<void(BattleContext&, Enemy&)> roll;                    // 掷值（可空）
    std::function<std::string(BattleContext&, Enemy&)> describe;         // 生成精确 desc（可空）
    std::function<void(BattleContext&, Enemy&)> act;                     // 执行动作
    bool reroll = false;
    int cooldown = 0;   // 使用后冷却回合数（0=无）；仅 select 制敌人有意义
};

// 如何加新怪物：在 data.hpp "Enemy data" 区新增 makeXxx()，
// 填 name / hp，patterns 每项 = { roll, describe, act } + reroll
// （reroll=true 的意图执行完后重新随机起点，适合加攻、蓄力类技能）。
struct Enemy : Entity {
    std::vector<IntentPattern> patterns;  // pattern 表：开局随机起点，之后顺序循环
    int patternIdx = -1;                  // 当前下标，-1 = 尚未启动
    Intent current;                       // 本回合已规划、待展示执行的意图（desc 为精确值）
    std::vector<int> rolls;               // 本回合预掷的意图数值，供 current.act 读取
    int turn = 0;
    // 引擎扩充字段
    std::function<int(BattleContext&, Enemy&)> select = nullptr;      // 分支选择器；空 = 旧默认路径（随机起点 / 顺序循环）
    std::function<void(BattleContext&, Enemy&)> onTurnEnd = nullptr;  // 回合末被动（仪式 / 金属化(敌)）
    std::vector<int> cooldown;      // 每个 pattern 的冷却剩余（与 patterns 等长）
    int aiState = 0;                // 脚本状态机游标
    int aiCounter = 0;              // 通用计数器（连续次数 / X 值缓存 / 形态值）
    int lastPatternIdx = -1;        // 上一次执行的 pattern 下标
    std::function<std::vector<Enemy>(BattleContext&, Enemy&)> split = nullptr;  // 分裂回调（只读 ctx.enemies，返回空 = 不分裂）
    // 分裂时机：true = 敌人回合开始时用分裂代替当前意图（大史莱姆）；
    //           false = 执行完当前意图后再分裂（史莱姆大王）。
    bool splitReplacesAction = false;
    // 出怪层生命掷选（MAP 域）：闭区间 [hpLo, hpHi]；hpHi <= 0 表示固定生命（不掷）。
    // 由 spawnEncounter 统一掷选，工厂只写区间，不直接依赖随机流。
    int hpLo = 0, hpHi = 0;
};

// ==================== Cards ====================
using EffectFn = std::function<void(BattleContext&, Entity& self, Entity& target)>;

// 如何加新效果：在 battle.hpp "Effect factories" 区新增工厂函数（返回 EffectFn lambda）。
// 如何加新卡牌：在 data.hpp "Card data" 区新增 makeXxx() 工厂，聚合初始化
//   Card{ 名称, 费用, 类型, 目标, 效果列表, 是否消耗 }，效果直接组合 Effect 工厂。
//   可选字段（xCost / attackCostDiscount / gainEnergyOnExhaust / canPlay）见下方成员注释。
struct Card {
    std::string name;
    int cost = 1;
    CardType type = CardType::Attack;
    TargetMode target = TargetMode::SingleEnemy;
    std::vector<EffectFn> effects;
    bool exhaust = false;
    bool xCost = false;               // X 费用：打出花费全部能量，效果读 ctx.xValue（倾泻 / 旋风斩）
    bool attackCostDiscount = false;  // 踩踏：本回合每打过一张攻击牌使费用 -1
    bool gainEnergyOnExhaust = false; // 战鼓：这张牌被消耗时获得 1 能量
    std::function<bool(BattleContext&)> canPlay = nullptr;  // 可打出条件（交锋：手牌全为攻击牌）；空 = 总是可打出
    // 只结算一次的自身效果（自伤 / 回能 / 抽牌等）：在 effects 之前固定执行 1 次，
    // 不随 AllEnemies 的敌人数重复（突破：自伤 1 + 全体 9 伤）。
    std::vector<EffectFn> selfEffects = {};
    // 引擎机制字段：必须追加在结构体最末——现有聚合初始化最长只到
    // gainEnergyOnExhaust，插在 bool 之间会因隐式转换静默错位。
    bool unplayable = false;      // 不可打出（灼伤 / 眩晕）
    bool ethereal = false;        // 虚无：回合末若在手牌则消耗（眩晕）
    int  endTurnDamage = 0;       // 回合末若在手牌，对玩家造成该伤害（灼伤）
};

// 玩家：继承 Entity（血量/格挡/状态），额外持有能量上限与角色卡组。
// 卡组是角色属性：战斗时复制进 BattleContext.drawPile，奖励选卡追加（见 run.hpp）。
struct Player : Entity {
    int maxEnergy = 3;
    std::vector<Card> deck;
};

// ==================== 玩家选择请求（输入钩子） ====================
// 引擎需要玩家从某个牌堆选一张卡时，构造 ChoiceRequest 并调用 ctx.chooseCard；
// UI/CLI 层注入回调实现（渲染候选卡名 -> 返回选中下标）。
struct ChoiceRequest {
    std::vector<int> candidates;                 // 候选下标（相对 source 牌堆）
    const std::vector<Card>* source = nullptr;   // 源牌堆指针，供 UI 读取卡名
    const char* label = "";                      // 提示文案（如 "discard pile"）
};

// ==================== Battle context（数据容器） ====================
// 随机流按"非拥有"方式注入：battle 只借用指针，RngSet 由 run 持有并负责存档。
struct BattleContext {
    Player player;
    std::vector<Enemy> enemies;
    std::vector<Card> drawPile, hand, discardPile, exhaustPile;
    int energy = 0;
    std::function<void(const std::string&)> log = [](const std::string&) {};

    rng::Rng* shuffleRng = nullptr;  // 洗牌流（rng.hpp 域 SHUFFLE），由 run 注入
    rng::Rng* enemyAi = nullptr;     // 敌人意图流（rng.hpp 域 ENEMY_AI），由 run 注入
    rng::Rng* cardRng = nullptr;     // 卡牌效果随机流（rng.hpp 域 CARD_EFFECTS），由 run 注入

    // 玩家选择回调（引擎问、UI/CLI 答）：nullptr 时默认选第一张候选；
    // 返回 -1 表示取消。选择是玩家输入，不消耗任何 Rng 流。
    std::function<int(const ChoiceRequest&)> chooseCard;

    // 敌人行动完成回调（可选通知，纯表现层用）：
    //   int  = 刚行动的敌人下标；
    //   bool = 本次行动是否使玩家 HP 或 Block 减少（格挡全额吸收也算，便于表现受击）。
    // 不参与任何规则结算；CLI / 未设置回调时 enemyTurn 的行为与原来完全一致。
    std::function<void(int, bool)> onEnemyActionResolved = nullptr;

    // ==================== 能力标记（战斗级，随本结构构造清零；Power / 一次性效果置位） ====================
    // 触发点见 battle.hpp 顶部"战斗与效果触发流程"注释：
    //   playCard / drawCards / dealDamage / gainBlock / startPlayerTurn / endPlayerTurn / exhaustCard
    int  xValue = 0;                  // X 费用卡的 X 值（playCard 结算前 = 花费的全部能量）
    bool colossus = false;            // 巨像：本回合有易伤的敌人对你减伤 50%（回合末清零）
    int  cruelty = 0;                 // 残酷：对易伤目标额外 +25% 伤害（层数，Power）
    bool barricade = false;           // 壁垒：回合开始格挡不清零（Power）
    bool corruption = false;          // 腐化：技能费用变 0 且打出即消耗（Power）
    bool noDraw = false;              // 战斗专注：本回合不能再抽牌（回合末清零）
    bool exhaustedThisTurn = false;   // 本回合消耗过卡牌（邪眼 / 被遗忘的仪式；回合末清零）
    int  feelNoPain = 0;              // 无惧疼痛：每消耗一张牌 +3 格挡（层数，Power）
    int  darkEmbrace = 0;             // 黑暗之拥：每消耗一张牌抽 1（层数，Power）
    bool steadfast = false;           // 坚定不移：本回合首次从卡牌获得格挡翻倍（Power）
    bool steadfastUsed = false;       // 本回合是否已翻倍（回合末清零）
    int  relentless = 0;              // 势不可当：获得格挡 -> 随机敌人 6 伤（层数，Power，走 cardRng）
    bool rage = false;                // 狂怒：本回合每打出攻击牌 +3 格挡（回合末清零）
    int  juggle = 0;                  // 杂耍：每回合仅第 3 张攻击牌复制入手（层数，Power）
    int  attacksPlayed = 0;           // 本回合已打出的攻击牌数（回合末清零；狂怒 / 杂耍 / 踩踏用）
    bool flameBarrier = false;        // 火焰屏障：本回合受击反伤 4（回合末清零）
    bool playerTurn = false;          // 当前是否玩家回合（startPlayerTurn 置位 / endPlayerTurn 复位；
    // 失去生命类标记只在玩家回合内生效，敌人回合的伤害不算"本回合"）
    bool lostHpThisTurn = false;      // 玩家本回合失去过生命（怨恨读；startPlayerTurn 清零）
    bool infernoFire = false;         // 狱火：本回合内每次失去生命对全体敌人 6 伤（Power，固定 6 不缩放）
    bool hellbound = false;           // 地狱狂徒：抽到名字含"打击"的牌 → 对随机敌人打出（Power，不叠层）
    int  pyreSource = 0;              // 薪火之源：回合开始 +1 能量/层（Power，层数不衰减）
    int  demonForm = 0;               // 恶魔形态：回合开始 +2 力量/层（Power，层数不衰减）
    int  metallize = 0;               // 金属化：回合结束 +3 格挡/层（Power，层数不衰减）
    bool flee = false;                // 惊逃：每回合结束随机打出手牌中 1 张攻击牌（Power，战斗内持续）
    bool doubleNextAttack = false;    // 连环拳：本回合下一张攻击牌额外结算一次（回合末清零）

    // 当前正在结算的卡牌（playCard / autoPlayCard 在结算期间指向该卡）。
    // AddCopyOfSelf 通过它读取"此牌"的完整定义，复制出独立副本；
    // 嵌套打出（倾泻 / 破灭）时由调用方保存并恢复外层指针。
    const Card* resolvingCard = nullptr;

    // 金币引用（由 run 层注入 rs.gold）：敌人"盗窃"扣金币用；战斗外节点为 nullptr。
    // 只借用指针，不拥有；金币归属与跨战斗持久化仍由 RunState 负责。
    int* gold = nullptr;
};
