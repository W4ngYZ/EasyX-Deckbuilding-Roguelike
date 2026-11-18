// =======================================
// cli.cpp CLI 版入口（控制台调试用）
//
// 提供不依赖图形库的逻辑验证通道：CLI 渲染 / 输入循环 / 流程编排 / main。
// 正式入口为 gui.cpp，本文件保留用于快速验证玩法与复现问题。
// 数据与规则见 hpps/core.hpp（数据层）、hpps/battle.hpp（规则层）、
// hpps/data.hpp（卡牌与敌人实例）、hpps/map.hpp（地图与出怪）、
// hpps/run.hpp（冒险流程）、hpps/rewards.hpp（战利品）。
// =======================================

#include "hpps/data.hpp"
#include "hpps/battle.hpp"
#include "hpps/run.hpp"

#include <conio.h>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include <windows.h>  // SetConsoleOutputCP / SetConsoleCP：修正控制台代码页（仅 CLI 测试壳使用）

using namespace std;

// 本局种子短码（调试用）：main 中从 rng::RunSeed 取，两个渲染入口都作为首行输出。
static string g_seedCode;

// ==================== CLI 渲染 ====================
void render(BattleContext& ctx) {
    system("cls");  // 每次操作后清屏重绘，观感更佳（渲染层，仅 CLI 测试壳使用）
    cout << "种子：" << g_seedCode << "\n";
    cout << "\n-------------------------------\n";
    cout << "玩家 " << ctx.player.name
        << "  生命 " << ctx.player.hp << "/" << ctx.player.maxHp
        << "  格挡 " << ctx.player.block
        << "  能量 " << ctx.energy << "/" << ctx.player.maxEnergy << "\n";
    if (!ctx.player.status.empty()) {
        cout << "  状态：";
        for (auto& [s, v] : ctx.player.status) cout << " " << statusName(s) << v;
        cout << "\n";
    }
    cout << "敌人：\n";
    for (size_t i = 0; i < ctx.enemies.size(); ++i) {
        auto& e = ctx.enemies[i];
        cout << "  [" << i + 1 << "] " << e.name;
        if (!e.inPlay()) {  // 不在场：死亡或已逃跑
            cout << (e.hp > 0 ? " [已逃跑]" : " [已死亡]") << "\n";
            continue;
        }
        cout << "  生命 " << e.hp << "/" << e.maxHp << "  格挡 " << e.block;
        if (!e.status.empty()) {
            cout << "  状态：";
            for (auto& [s, v] : e.status) cout << " " << statusName(s) << v;
        }
        cout << "  意图：" << e.current.desc << "\n";
    }
    cout << "手牌：\n";
    for (size_t i = 0; i < ctx.hand.size(); ++i) {
        auto& c = ctx.hand[i];
        cout << "  [" << i + 1 << "] " << c.name << " ("
            << (c.xCost ? "X" : std::to_string(c.cost)) << " 能量)"
            << (!c.xCost && c.cost > ctx.energy ? "  [能量不足]" : "")
            << (c.unplayable ? "  [不可打出]"
                : (c.canPlay && !c.canPlay(ctx) ? "  [无法打出]" : "")) << "\n";
    }
    cout << "抽牌堆 " << ctx.drawPile.size()
        << " | 弃牌堆 " << ctx.discardPile.size()
        << " | 消耗堆 " << ctx.exhaustPile.size() << "\n";
}

void playerPhase(BattleContext& ctx) {
    while (true) {
        if (!ctx.player.alive()) return;
        bool anyInPlay = false;
        for (auto& e : ctx.enemies) if (e.inPlay()) anyInPlay = true;
        if (!anyInPlay) return;

        render(ctx);
        cout << "指令：数字=打出该牌，e=结束回合，q=退出\n> ";
        string line;
        if (!getline(cin, line)) return;
        if (line == "e" || line == "end") break;
        if (line == "q" || line == "quit") exit(0);
        if (line.empty()) continue;

        int idx;
        try { idx = stoi(line) - 1; }
        catch (...) { continue; }
        if (idx < 0 || idx >= (int)ctx.hand.size()) { cout << "无效输入\n"; continue; }

        int target = -1;
        if (ctx.hand[idx].target == TargetMode::SingleEnemy) {
            vector<int> targets;
            for (size_t i = 0; i < ctx.enemies.size(); ++i)
                if (ctx.enemies[i].inPlay()) targets.push_back((int)i);
            if (targets.empty()) continue;
            if (targets.size() == 1) {
                target = targets[0];
            }
            else {
                cout << "选择目标：";
                for (int i : targets) cout << "[" << i + 1 << "]" << ctx.enemies[i].name << " ";
                cout << "> ";
                string tl;
                if (!getline(cin, tl)) return;
                int t;
                try { t = stoi(tl) - 1; }
                catch (...) { continue; }
                if (t >= 0 && t < (int)ctx.enemies.size() && ctx.enemies[t].inPlay()) target = t;
                else { cout << "无效目标\n"; continue; }
            }
        }
        playCard(ctx, idx, target);
    }
    endPlayerTurn(ctx);
}

// 单场战斗：返回玩家是否存活（true = 胜利）。
// 洗牌由 run 层（playNode）在进入战斗前完成，这里不再重复消耗 SHUFFLE 序列。
bool runBattle(BattleContext& ctx) {
    for (auto& e : ctx.enemies) planNextIntent(ctx, e);

    // 在场判定用 inPlay：全员死亡或逃跑 → 战斗结束；玩家存活即按胜利发战利品（D3）。
    auto anyEnemyInPlay = [&]() {
        for (auto& e : ctx.enemies) if (e.inPlay()) return true;
        return false;
        };

    while (ctx.player.alive() && anyEnemyInPlay()) {
        startPlayerTurn(ctx);
        playerPhase(ctx);
        if (!ctx.player.alive() || !anyEnemyInPlay()) break;
        enemyTurn(ctx);
    }

    return ctx.player.alive();
}

// 战斗 runner（适配 run.hpp 的 BattleRunner）：注入选择 / 日志钩子后执行 runBattle。
bool runBattleCLI(BattleContext& ctx) {
    ctx.chooseCard = [](const ChoiceRequest& req) -> int {
        std::cout << "  [" << req.label << "] 选择一张牌：\n";
        for (size_t i = 0; i < req.candidates.size(); ++i) {
            const Card& c = req.source->at(req.candidates[i]);
            std::cout << "    [" << i + 1 << "] " << c.name << "\n";
        }
        std::cout << "  （0 = 取消）> ";
        std::string s;
        if (!std::getline(std::cin, s)) return -1;   // 用 getline 读取：不残留行尾换行，避免主循环读到空行
        int n = -1;
        try { n = std::stoi(s); }
        catch (...) { return -1; }
        if (n < 1 || n >(int)req.candidates.size()) return -1;
        return req.candidates[n - 1];
        };
    ctx.log = [](const string& s) { cout << s << "\n"; };
    return runBattle(ctx);
}

// 冒险界面：玩家状态 + 单一路线图（[x] = 当前位置）
void renderAdventure(const RunState& rs) {
    cout << "种子：" << g_seedCode << "\n";   // 调试：每帧首行输出本局种子
    cout << "玩家 " << rs.player.name
        << "  生命 " << rs.player.hp << "/" << rs.player.maxHp
        << "  金币 " << rs.gold
        << "  牌组 " << rs.player.deck.size() << "\n";
    cout << "路线：";
    for (int i = 0; i < rs.map.size(); ++i) {
        if (i == rs.pos) cout << '[';
        switch (rs.map.at(i)) {
        case NodeType::Battle: cout << 'B'; break;
        case NodeType::Elite:  cout << 'E'; break;
        case NodeType::Event:  cout << '?'; break;
        case NodeType::Rest:   cout << 'R'; break;
        case NodeType::Boss:   cout << 'X'; break;
        }
        if (i == rs.pos) cout << ']';
        cout << ' ';
    }
    cout << "\n";
}

// ==================== Entry ====================
// 用法：cli.exe [种子短码]
//   不带参数        → 随机开局
//   带 1-11 位短码  → 复现该种子开局（0-9 / A-Z / a-z）
int main(int argc, char** argv) {
    // —— 控制台中文编码修正 ——
    // 编译时 -fexec-charset=UTF-8 把中文字符串以 UTF-8 字节写入 exe，
    // 但 Windows 控制台默认按系统代码页解码（中文系统为 936/GBK），
    // 两者不一致就会出现"闃插尽"式乱码。这里把本进程的控制台输入/输出
    // 统一切到 UTF-8(65001)，保证 exe 拷到任何 Windows 机器上都显示正常。
    // （仅影响控制台；未来换成 hiEasyX 图形界面后由宽字符绘制接管，无需此设置）
    SetConsoleOutputCP(CP_UTF8);  // 输出代码页 -> UTF-8
    SetConsoleCP(CP_UTF8);        // 输入代码页 -> UTF-8（当前只读数字/字母，保险起见一并设置）

    if (argc > 2) {
        cerr << "用法：cli.exe [种子短码]\n";
        return 1;
    }
    std::optional<rng::RunSeed> seeded;
    if (argc == 2) {
        seeded = rng::startRunFromCode(argv[1]);
        if (!seeded) {
            // 不静默回退随机：否则玩家以为在复现，实际拿到的是另一局。
            cerr << "无效种子短码：" << argv[1] << "\n"
                 << "需为 1-11 位 0-9/A-Z/a-z；11 位短码还可能超出 64 位种子范围，\n"
                 << "请直接使用游戏显示的短码，或输入不超过 10 位。\n";
            return 1;
        }
    }

    rng::RunSeed run = seeded ? std::move(*seeded) : rng::startNewRun();  // 随机流由 run 持有，battle 只借用指针
    g_seedCode = run.code;                  // 调试：渲染首行显示的本局种子短码
    RunState rs = beginRun(run.rng.map);    // 开局：玩家 / 初始卡组（5S+5D+Bash）/ 地图 / 出怪池

    RunCallbacks cb;
    cb.battle = runBattleCLI;
    // 三选一选卡：渲染候选卡名 -> 读 stdin 数字 -> 返回选项下标（0 基，-1 跳过）
    cb.chooseRewardCard = [](const std::vector<Card>& options) -> int {
        std::cout << "\n  选择卡牌奖励：\n";
        for (size_t i = 0; i < options.size(); ++i)
            std::cout << "    [" << i + 1 << "] " << options[i].name
            << " (" << (options[i].xCost ? "X" : std::to_string(options[i].cost)) << " 能量)\n";
        std::cout << "  （0 = 跳过）> ";
        std::string s;
        if (!std::getline(std::cin, s)) return -1;   // 用 getline 读取：不残留行尾换行，避免主循环读到空行
        int n = 0;
        try { n = std::stoi(s); }
        catch (...) { return -1; }
        return n >= 1 && n <= (int)options.size() ? n - 1 : -1;
        };
    // 局外事件选牌：渲染玩家牌组 -> 读 stdin 数字 -> 返回下标（0 基，-1 取消）
    cb.chooseDeckCard = [](const std::vector<Card>& deck) -> int {
        std::cout << "\n  选择一张牌：\n";
        for (size_t i = 0; i < deck.size(); ++i)
            std::cout << "    [" << i + 1 << "] " << deck[i].name
            << " (" << (deck[i].xCost ? "X" : std::to_string(deck[i].cost)) << " 能量)\n";
        std::cout << "  （0 = 取消）> ";
        std::string s;
        if (!std::getline(std::cin, s)) return -1;
        int n = 0;
        try { n = std::stoi(s); }
        catch (...) { return -1; }
        return n >= 1 && n <= (int)deck.size() ? n - 1 : -1;
        };
    // 局外事件选选项：渲染选项文案 -> 读 stdin 数字 -> 返回下标（0 基，-1 离开）
    cb.chooseEvent = [](const std::vector<std::string>& options) -> int {
        std::cout << "\n  选项：\n";
        for (size_t i = 0; i < options.size(); ++i)
            std::cout << "    [" << i + 1 << "] " << options[i] << "\n";
        std::cout << "  （0 = 取消）> ";
        std::string s;
        if (!std::getline(std::cin, s)) return -1;
        int n = 0;
        try { n = std::stoi(s); }
        catch (...) { return -1; }
        return n >= 1 && n <= (int)options.size() ? n - 1 : -1;
        };
    cb.log = [](const string& s) { cout << s << "\n"; };

    while (!rs.won && !rs.dead) {
        system("cls");
        renderAdventure(rs);
        cout << "\n按回车继续（q = 退出）> ";
        string line;
        if (!getline(cin, line)) return 0;
        if (line == "q" || line == "quit") return 0;
        playNode(rs, run, cb);   // 处理当前节点并推进（含战利品发放）
    }

    system("cls");  // 胜负结算：清屏后输出结果，等任意输入退出
    if (rs.won) cout << "\n胜利！\n";
    else        cout << "\n失败！\n";
    cout << "\n按任意键退出...";
    cout << flush;  // 测试外挂：强制刷出缓冲的胜负文案（不改任何游戏逻辑）
    _getch();
    return 0;
}
