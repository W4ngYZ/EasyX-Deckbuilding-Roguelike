// 内嵌资源 ID 定义：res/resource.rc 与 src/game.cpp 共用。
// 编号约定：字体 101；背景 110 起；玩家 130；敌人 201 起顺序排列。
#pragma once

// ---------- 字体 ----------
#define IDR_FONT_MINECRAFT               101

// ---------- 背景 ----------
#define IDR_BG_MENU                      110
#define IDR_BG_MAP                       111
#define IDR_BG_FAIL                      112
#define IDR_BG_SUCCESS                   113
#define IDR_BG_BATTLE1                   114
#define IDR_BG_BATTLE2                   115
#define IDR_BG_BATTLE3                   116
#define IDR_BG_BATTLE4                   117
#define IDR_BG_BATTLE5                   118
#define IDR_BG_BATTLE6                   119

// ---------- 玩家 ----------
#define IDR_PLAYER_FIGHTER               130

// ---------- 敌人（27 张，与 game.cpp 资源映射表一一对应） ----------
#define IDR_EN_LAGAVULIN                 201
#define IDR_EN_HEXAGHOST                 202
#define IDR_EN_LOOTER_ASSASSIN           203
#define IDR_EN_LOOTER_CROSSBOW           204
#define IDR_EN_LOOTER_AXE                205
#define IDR_EN_LOOTER_MUGGER             206
#define IDR_EN_LOOTER_TRACKER            207
#define IDR_EN_SLIME_BOSS                208
#define IDR_EN_SENTRY                    209
#define IDR_EN_GREMLIN_NOB               210
#define IDR_EN_JAW_WORM                  211
#define IDR_EN_GUARDIAN                  212
#define IDR_EN_SPIKED_SLIME_M            213
#define IDR_EN_SPIKED_SLIME_L            214
#define IDR_EN_SPIKED_SLIME_S            215
#define IDR_EN_LOOTER                    216
#define IDR_EN_MUGGER                    217
#define IDR_EN_FUNGI_BEAST               218
#define IDR_EN_SLAVER_RED                219
#define IDR_EN_RED_LOUSE                 220
#define IDR_EN_GREEN_LOUSE                221
#define IDR_EN_SLAVER_BLUE               222
#define IDR_EN_CULTIST                   223
#define IDR_EN_ACID_SLIME_M              224
#define IDR_EN_ACID_SLIME_L              225
#define IDR_EN_ACID_SLIME_S              226
#define IDR_EN_FLY_FUNGUS                 227
