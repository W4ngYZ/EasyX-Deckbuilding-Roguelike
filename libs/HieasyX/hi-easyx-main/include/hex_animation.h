/*
 * Copyright (c) 2025 HiEasyX
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * \file hex_animation.h
 * \brief Property interpolation animation system for HiEasyX
 *
 * Provides 26 easing types and animated interpolation for float, point,
 * color and rectangle values. Designed for the IMGUI frame-by-frame paradigm:
 * declare your HXAnimationXxx as static (or in HXStatePool) and call
 * Animate() every frame with the elapsed time.
 */

#pragma once

#include <include/hex_geo.h>
#include <include/impl/hex_impl.h>

namespace HX {

/**
 * Easing curve types. Formulas follow the standard Penner equations.
 */
enum class HXEaseType {
	Linear,
	EaseInQuad, EaseOutQuad, EaseInOutQuad,
	EaseInCubic, EaseOutCubic, EaseInOutCubic,
	EaseInQuart, EaseOutQuart, EaseInOutQuart,
	EaseInSine, EaseOutSine, EaseInOutSine,
	EaseInCirc, EaseOutCirc, EaseInOutCirc,
	EaseInExpo, EaseOutExpo, EaseInOutExpo,
	EaseInBack, EaseOutBack, EaseInOutBack,
	EaseInElastic, EaseOutElastic, EaseInOutElastic,
	EaseInBounce, EaseOutBounce, EaseInOutBounce,
};

/**
 * Apply an easing function to a normalized time t in [0, 1].
 * @param t Normalized time, clamped internally to [0, 1].
 * @param Type The easing curve to use.
 * @return The eased value in [0, 1].
 */
float Ease(float t, HXEaseType Type);

// ------------------------------------------------------------------
// Float animation
// ------------------------------------------------------------------
struct HXAnimationFloat {
	float Start = 0.0f;
	float End = 1.0f;
	float DurationMs = 300.0f;
	float DelayMs = 0.0f;
	HXEaseType Ease = HXEaseType::Linear;
	bool Loop = false;
	bool PingPong = false;

	float _currentTime = 0.0f;
	bool _running = false;
};

float Animate(HXAnimationFloat &Anim, float DeltaMs);
void StartAnimation(HXAnimationFloat &Anim);
void StopAnimation(HXAnimationFloat &Anim);
void ResetAnimation(HXAnimationFloat &Anim);
bool IsAnimationRunning(const HXAnimationFloat &Anim);
bool IsAnimationFinished(const HXAnimationFloat &Anim);

// ------------------------------------------------------------------
// Point animation
// ------------------------------------------------------------------
struct HXAnimationPoint {
	HXPoint Start = {0, 0};
	HXPoint End = {0, 0};
	float DurationMs = 300.0f;
	float DelayMs = 0.0f;
	HXEaseType Ease = HXEaseType::Linear;
	bool Loop = false;
	bool PingPong = false;

	float _currentTime = 0.0f;
	bool _running = false;
};

HXPoint Animate(HXAnimationPoint &Anim, float DeltaMs);
void StartAnimation(HXAnimationPoint &Anim);
void StopAnimation(HXAnimationPoint &Anim);
void ResetAnimation(HXAnimationPoint &Anim);
bool IsAnimationRunning(const HXAnimationPoint &Anim);
bool IsAnimationFinished(const HXAnimationPoint &Anim);

// ------------------------------------------------------------------
// Color animation
// ------------------------------------------------------------------
struct HXAnimationColor {
	HXColor Start = {0, 0, 0, 255};
	HXColor End = {0, 0, 0, 255};
	float DurationMs = 300.0f;
	float DelayMs = 0.0f;
	HXEaseType Ease = HXEaseType::Linear;
	bool Loop = false;
	bool PingPong = false;

	float _currentTime = 0.0f;
	bool _running = false;
};

HXColor Animate(HXAnimationColor &Anim, float DeltaMs);
void StartAnimation(HXAnimationColor &Anim);
void StopAnimation(HXAnimationColor &Anim);
void ResetAnimation(HXAnimationColor &Anim);
bool IsAnimationRunning(const HXAnimationColor &Anim);
bool IsAnimationFinished(const HXAnimationColor &Anim);

// ------------------------------------------------------------------
// Rect animation
// ------------------------------------------------------------------
struct HXAnimationRect {
	HXRect Start = {0, 0, 0, 0};
	HXRect End = {0, 0, 0, 0};
	float DurationMs = 300.0f;
	float DelayMs = 0.0f;
	HXEaseType Ease = HXEaseType::Linear;
	bool Loop = false;
	bool PingPong = false;

	float _currentTime = 0.0f;
	bool _running = false;
};

HXRect Animate(HXAnimationRect &Anim, float DeltaMs);
void StartAnimation(HXAnimationRect &Anim);
void StopAnimation(HXAnimationRect &Anim);
void ResetAnimation(HXAnimationRect &Anim);
bool IsAnimationRunning(const HXAnimationRect &Anim);
bool IsAnimationFinished(const HXAnimationRect &Anim);

}
