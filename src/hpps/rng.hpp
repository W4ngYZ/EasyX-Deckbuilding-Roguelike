// =======================================
// rng.hpp 随机数系统：种子编码 / 域分离流 / 存档
//
// 全项目唯一熵源：只有 generateSeed() 允许使用 std::random_device，
// 其余任何地方出现 rand() / random_device / std::shuffle 都是 bug。
// =======================================

#pragma once

#include<cassert>
#include<cstdint>
#include<optional>
#include<random>
#include<sstream>
#include<string>
#include<string_view>
#include<utility>
#include<vector>

// ---------------------------------------
// Base62 字符表
// 0..9 -> '0'..'9'
// 10..35 -> 'A'..'Z'
// 36..61 -> 'a'..'z'
// ---------------------------------------
namespace rng {
    inline constexpr char BASE62[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    // 字符转 base62
    inline int base62Value(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
        if (c >= 'a' && c <= 'z') return c - 'a' + 36;
        return -1;
    }

    // uint64 转 base62
    inline std::string encodeBase62(uint64_t v) {
        std::string s(11, '0'); //一个uint64_t肯定可以用11位base62存下来
        for (int i = 10;i >= 0;--i) {
            s[i] = BASE62[v % 62];
            v /= 62;
        }
        return s;
    }

    // base62 转 uint64
    inline std::optional<uint64_t> decodeBase62(std::string_view s) {
        if (s.empty() || s.size() > 11) return std::nullopt; //长度合法性
        uint64_t v = 0;
        for (char c : s) {
            int d = base62Value(c);
            if (d < 0) return std::nullopt; //合法性检查
            if (v > (UINT64_MAX - (uint64_t)d) / 62) //溢出检查
                return std::nullopt;
            v = v * 62 + (uint64_t)d;
        }
        return v;
    }

    // 真随机种子：开局调用
    inline uint64_t generateSeed() {
        std::random_device rd;
        uint64_t hi = (uint64_t)rd() << 32;
        uint64_t lo = (uint64_t)rd();
        return hi | lo;
    }

    //splitmix64
    inline uint64_t splitmix64(uint64_t& x) {
        uint64_t z = (x += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    //区域枚举
    enum Domain : uint64_t {
        SHUFFLE = 1,
        ENEMY_AI = 2,
        REWARDS = 3,
        MAP = 4,
        EVENTS = 5,
        CARD_EFFECTS = 6,  // 卡牌效果随机（随机目标 / 随机消耗）
    };

    // 区域种子
    inline uint64_t deriveSeed(uint64_t baseSeed, uint64_t domain) {
        uint64_t s = baseSeed ^ (domain * 0xD1B54A32D192ED03ULL);
        return splitmix64(s);
    }

    // Rng 单流实现
    class Rng {
    public:
        explicit Rng(uint64_t seed) : engine_(seed), baseSeed_(seed) {}
        uint64_t baseSeed() const { return baseSeed_; }
        uint64_t next() { return engine_(); }

        int range(int lo, int hi) {
            if (hi <= lo) return lo;
            uint64_t span = (uint64_t)((int64_t)hi - lo) + 1;
            uint64_t limit = UINT64_MAX - (UINT64_MAX % span);
            uint64_t x;
            do { x = engine_(); } while (x >= limit);
            return (int)(lo + (int64_t)(x % span));
        }

        bool chance(double p) {
            if (p <= 0.0) return false;
            if (p >= 1.0) return true;
            uint64_t x = engine_() >> 11;
            return (double)x / 9007199254740992.0 < p;
        }

        template <class T>
        void shuffle(std::vector<T>& v) {
            for (size_t i = v.size(); i > 1; --i) {
                size_t j = (size_t)range(0, (int)(i - 1));
                std::swap(v[i - 1], v[j]);
            }
        }

        template <class T>
        const T& pick(const std::vector<T>& v) {
            assert(!v.empty() && "oi oi 抽牌堆是空的");
            return v[(size_t)range(0, (int)v.size() - 1)];
        }

        // 存档
        std::string saveState() const {
            std::ostringstream os;
            os << baseSeed_ << ' ' << engine_;
            return os.str();
        }

        // 读档
        void restoreState(const std::string& s) {
            std::istringstream is(s);
            is >> baseSeed_ >> engine_;
        }

    private:
        std::mt19937_64 engine_;
        uint64_t baseSeed_;
    };

    // 按域分好的随机流集合
    struct RngSet {
        Rng shuffle;
        Rng enemyAi;
        Rng rewards;
        Rng map;
        Rng events;
        Rng effects;  // 卡牌效果随机流（域 CARD_EFFECTS）

        explicit RngSet(uint64_t seed)
            : shuffle(deriveSeed(seed, SHUFFLE)),
            enemyAi(deriveSeed(seed, ENEMY_AI)),
            rewards(deriveSeed(seed, REWARDS)),
            map(deriveSeed(seed, MAP)),
            events(deriveSeed(seed, EVENTS)),
            effects(deriveSeed(seed, CARD_EFFECTS)) {
        }

        // 存档
        std::string saveState() const {
            std::ostringstream os;
            os << shuffle.saveState() << '\n'
                << enemyAi.saveState() << '\n'
                << rewards.saveState() << '\n'
                << map.saveState() << '\n'
                << events.saveState() << '\n'
                << effects.saveState() << '\n';
            return os.str();
        }

        // 读档
        void restoreState(const std::string& s) {
            std::istringstream is(s);
            std::string line;
            std::getline(is, line); shuffle.restoreState(line);
            std::getline(is, line); enemyAi.restoreState(line);
            std::getline(is, line); rewards.restoreState(line);
            std::getline(is, line); map.restoreState(line);
            std::getline(is, line); events.restoreState(line);
            std::getline(is, line); effects.restoreState(line);
        }
    };
    struct RunSeed {
        uint64_t raw;       // 真实种子（内部用）
        std::string code;   // 玩家看到的 11 位 base62 短码
        RngSet rng;         // 已按域分好的随机流

        explicit RunSeed(uint64_t r)
            : raw(r), code(encodeBase62(r)), rng(r) {
        }
    };

    // 系统开局：生成 -> 编码 -> 构造 RngSet
    inline RunSeed startNewRun() {
        return RunSeed(generateSeed());
    }

    // 玩家输入：解码 -> 校验 -> 构造 RngSet
    inline std::optional<RunSeed> startRunFromCode(std::string_view code) {
        auto raw = decodeBase62(code);
        if (!raw) return std::nullopt;
        return RunSeed(*raw);
    }
}

// ============================================================================
//  rng.hpp 使用指南
// ============================================================================
//
//  入口：
//    rng::startNewRun()              系统开局（random_device 真随机）
//    rng::startRunFromCode(code)     按短码复现，非法短码返回 nullopt
//    run.code                        11 位 base62 短码，展示给玩家
//    run.rng                         按域分好的 6 条随机流
//
//  六个域（Domain 枚举只追加、不复用、不改编号，否则老存档序列错位）：
//    shuffle 洗牌 / enemyAi 敌人意图 / rewards 战利品 /
//    map 地图与出怪 / events 局外事件 / effects 卡牌效果随机
//
//  Rng 操作：next() 原始 64 位 / range(lo,hi) 闭区间整数（拒绝采样去偏）/
//    chance(p) 概率判定 / shuffle(v) Fisher-Yates / pick(v) 均匀取一。
//    不用 std::shuffle / std::uniform_int_distribution——它们跨平台不一致。
//
//  消耗纪律：Rng 只在「事件」发生时消耗（洗牌、决策、掉落、生成）；
//    读取已定状态的操作（抽牌、渲染、查看、移动）永不消耗 Rng。
//
//  存档纪律：存 run.code + run.rng.saveState()，缺一不可——
//    种子只决定序列起点，saveState() 才记录消费进度。