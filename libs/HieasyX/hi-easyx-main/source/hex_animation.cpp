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
 * \file hex_animation.cpp
 * \brief Animation easing and interpolation implementation
 */

#include <include/hex_animation.h>

#include <algorithm>
#include <cmath>

namespace HX {

static constexpr float PI = 3.14159265f;

float Ease(float t, HXEaseType Type) {
	t = std::clamp(t, 0.0f, 1.0f);

	switch (Type) {
	case HXEaseType::Linear:
		return t;

	// Quad
	case HXEaseType::EaseInQuad:
		return t * t;
	case HXEaseType::EaseOutQuad:
		return 1.0f - (1.0f - t) * (1.0f - t);
	case HXEaseType::EaseInOutQuad:
		return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2) / 2.0f;

	// Cubic
	case HXEaseType::EaseInCubic:
		return t * t * t;
	case HXEaseType::EaseOutCubic:
		return 1.0f - std::pow(1.0f - t, 3);
	case HXEaseType::EaseInOutCubic:
		return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3) / 2.0f;

	// Quart
	case HXEaseType::EaseInQuart:
		return t * t * t * t;
	case HXEaseType::EaseOutQuart:
		return 1.0f - std::pow(1.0f - t, 4);
	case HXEaseType::EaseInOutQuart:
		return t < 0.5f ? 8.0f * t * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 4) / 2.0f;

	// Sine
	case HXEaseType::EaseInSine:
		return 1.0f - std::cos((t * PI) / 2.0f);
	case HXEaseType::EaseOutSine:
		return std::sin((t * PI) / 2.0f);
	case HXEaseType::EaseInOutSine:
		return -(std::cos(PI * t) - 1.0f) / 2.0f;

	// Circ
	case HXEaseType::EaseInCirc:
		return 1.0f - std::sqrt(1.0f - std::pow(t, 2));
	case HXEaseType::EaseOutCirc:
		return std::sqrt(1.0f - std::pow(t - 1.0f, 2));
	case HXEaseType::EaseInOutCirc:
		return t < 0.5f
			? (1.0f - std::sqrt(1.0f - std::pow(2.0f * t, 2))) / 2.0f
			: (std::sqrt(1.0f - std::pow(-2.0f * t + 2.0f, 2)) + 1.0f) / 2.0f;

	// Expo
	case HXEaseType::EaseInExpo:
		return t == 0.0f ? 0.0f : std::pow(2.0f, 10.0f * (t - 1.0f));
	case HXEaseType::EaseOutExpo:
		return t == 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
	case HXEaseType::EaseInOutExpo:
		return t == 0.0f ? 0.0f : t == 1.0f ? 1.0f
			: t < 0.5f ? std::pow(2.0f, 20.0f * t - 10.0f) / 2.0f
			: (2.0f - std::pow(2.0f, -20.0f * t + 10.0f)) / 2.0f;

	// Back
	case HXEaseType::EaseInBack: {
		const float c1 = 1.70158f;
		const float c3 = c1 + 1.0f;
		return c3 * t * t * t - c1 * t * t;
	}
	case HXEaseType::EaseOutBack: {
		const float c1 = 1.70158f;
		const float c3 = c1 + 1.0f;
		return 1.0f + c3 * std::pow(t - 1.0f, 3) + c1 * std::pow(t - 1.0f, 2);
	}
	case HXEaseType::EaseInOutBack: {
		const float c1 = 1.70158f;
		const float c2 = c1 * 1.525f;
		return t < 0.5f
			? (std::pow(2.0f * t, 2) * ((c2 + 1.0f) * 2.0f * t - c2)) / 2.0f
			: (std::pow(2.0f * t - 2.0f, 2) * ((c2 + 1.0f) * (t * 2.0f - 2.0f) + c2) + 2.0f) / 2.0f;
	}

	// Elastic
	case HXEaseType::EaseInElastic: {
		const float c4 = (2.0f * PI) / 3.0f;
		if (t == 0.0f) return 0.0f;
		if (t == 1.0f) return 1.0f;
		return -std::pow(2.0f, 10.0f * t - 10.0f) * std::sin((t * 10.0f - 10.75f) * c4);
	}
	case HXEaseType::EaseOutElastic: {
		const float c4 = (2.0f * PI) / 3.0f;
		if (t == 0.0f) return 0.0f;
		if (t == 1.0f) return 1.0f;
		return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
	}
	case HXEaseType::EaseInOutElastic: {
		const float c5 = (2.0f * PI) / 4.5f;
		if (t == 0.0f) return 0.0f;
		if (t == 1.0f) return 1.0f;
		if (t < 0.5f)
			return -(std::pow(2.0f, 20.0f * t - 10.0f) * std::sin((20.0f * t - 11.125f) * c5)) / 2.0f;
		return (std::pow(2.0f, -20.0f * t + 10.0f) * std::sin((20.0f * t - 11.125f) * c5)) / 2.0f + 1.0f;
	}

	// Bounce
	case HXEaseType::EaseInBounce:
		return 1.0f - Ease(1.0f - t, HXEaseType::EaseOutBounce);
	case HXEaseType::EaseOutBounce: {
		const float n1 = 7.5625f;
		const float d1 = 2.75f;
		if (t < 1.0f / d1) {
			return n1 * t * t;
		} else if (t < 2.0f / d1) {
			t -= 1.5f / d1;
			return n1 * t * t + 0.75f;
		} else if (t < 2.5f / d1) {
			t -= 2.25f / d1;
			return n1 * t * t + 0.9375f;
		} else {
			t -= 2.625f / d1;
			return n1 * t * t + 0.984375f;
		}
	}
	case HXEaseType::EaseInOutBounce:
		return t < 0.5f
			? (1.0f - Ease(1.0f - 2.0f * t, HXEaseType::EaseOutBounce)) / 2.0f
			: (1.0f + Ease(2.0f * t - 1.0f, HXEaseType::EaseOutBounce)) / 2.0f;

	default:
		return t;
	}
}

// ------------------------------------------------------------------
// Internal helpers
// ------------------------------------------------------------------
namespace Detail {

static float Lerp(float a, float b, float t) {
	return a + (b - a) * t;
}

static HXPoint Lerp(const HXPoint &a, const HXPoint &b, float t) {
	return HXPoint{
		static_cast<HXGInt>(a.X + static_cast<HXGInt>((b.X - a.X) * t)),
		static_cast<HXGInt>(a.Y + static_cast<HXGInt>((b.Y - a.Y) * t))
	};
}

static HXColor Lerp(const HXColor &a, const HXColor &b, float t) {
	return HXColor{
		static_cast<HXColInt>(std::clamp(a.R + static_cast<int>((b.R - a.R) * t), 0, 255)),
		static_cast<HXColInt>(std::clamp(a.G + static_cast<int>((b.G - a.G) * t), 0, 255)),
		static_cast<HXColInt>(std::clamp(a.B + static_cast<int>((b.B - a.B) * t), 0, 255)),
		static_cast<HXColInt>(std::clamp(a.A + static_cast<int>((b.A - a.A) * t), 0, 255))
	};
}

static HXRect Lerp(const HXRect &a, const HXRect &b, float t) {
	return HXRect{
		static_cast<HXGInt>(a.Left + static_cast<HXGInt>((b.Left - a.Left) * t)),
		static_cast<HXGInt>(a.Top + static_cast<HXGInt>((b.Top - a.Top) * t)),
		static_cast<HXGInt>(a.Right + static_cast<HXGInt>((b.Right - a.Right) * t)),
		static_cast<HXGInt>(a.Bottom + static_cast<HXGInt>((b.Bottom - a.Bottom) * t))
	};
}

template<typename AnimT, typename ValueT>
static ValueT UpdateAnimation(AnimT &Anim, float DeltaMs) {
	if (!Anim._running) {
		return Anim.Start;
	}

	Anim._currentTime += DeltaMs;

	if (Anim._currentTime < Anim.DelayMs) {
		return Anim.Start;
	}

	float effectiveTime = Anim._currentTime - Anim.DelayMs;

	if (!Anim.Loop && effectiveTime >= Anim.DurationMs) {
		return Anim.End;
	}

	float cycleTime;
	if (Anim.PingPong) {
		float totalCycle = Anim.DurationMs * 2.0f;
		float phase = std::fmod(effectiveTime, totalCycle);
		if (phase < Anim.DurationMs) {
			cycleTime = phase;
		} else {
			cycleTime = totalCycle - phase;
		}
	} else {
		cycleTime = std::fmod(effectiveTime, Anim.DurationMs);
	}

	float t = cycleTime / Anim.DurationMs;
	t = std::clamp(t, 0.0f, 1.0f);
	float eased = Ease(t, Anim.Ease);

	return Lerp(Anim.Start, Anim.End, eased);
}

} // namespace Detail

// ------------------------------------------------------------------
// Float
// ------------------------------------------------------------------
float Animate(HXAnimationFloat &Anim, float DeltaMs) {
	return Detail::UpdateAnimation<HXAnimationFloat, float>(Anim, DeltaMs);
}

void StartAnimation(HXAnimationFloat &Anim) { Anim._running = true; }
void StopAnimation(HXAnimationFloat &Anim) { Anim._running = false; }
void ResetAnimation(HXAnimationFloat &Anim) { Anim._currentTime = 0.0f; Anim._running = false; }
bool IsAnimationRunning(const HXAnimationFloat &Anim) { return Anim._running; }
bool IsAnimationFinished(const HXAnimationFloat &Anim) {
	return !Anim.Loop && !Anim.PingPong && Anim._currentTime >= Anim.DelayMs + Anim.DurationMs;
}

// ------------------------------------------------------------------
// Point
// ------------------------------------------------------------------
HXPoint Animate(HXAnimationPoint &Anim, float DeltaMs) {
	return Detail::UpdateAnimation<HXAnimationPoint, HXPoint>(Anim, DeltaMs);
}

void StartAnimation(HXAnimationPoint &Anim) { Anim._running = true; }
void StopAnimation(HXAnimationPoint &Anim) { Anim._running = false; }
void ResetAnimation(HXAnimationPoint &Anim) { Anim._currentTime = 0.0f; Anim._running = false; }
bool IsAnimationRunning(const HXAnimationPoint &Anim) { return Anim._running; }
bool IsAnimationFinished(const HXAnimationPoint &Anim) {
	return !Anim.Loop && !Anim.PingPong && Anim._currentTime >= Anim.DelayMs + Anim.DurationMs;
}

// ------------------------------------------------------------------
// Color
// ------------------------------------------------------------------
HXColor Animate(HXAnimationColor &Anim, float DeltaMs) {
	return Detail::UpdateAnimation<HXAnimationColor, HXColor>(Anim, DeltaMs);
}

void StartAnimation(HXAnimationColor &Anim) { Anim._running = true; }
void StopAnimation(HXAnimationColor &Anim) { Anim._running = false; }
void ResetAnimation(HXAnimationColor &Anim) { Anim._currentTime = 0.0f; Anim._running = false; }
bool IsAnimationRunning(const HXAnimationColor &Anim) { return Anim._running; }
bool IsAnimationFinished(const HXAnimationColor &Anim) {
	return !Anim.Loop && !Anim.PingPong && Anim._currentTime >= Anim.DelayMs + Anim.DurationMs;
}

// ------------------------------------------------------------------
// Rect
// ------------------------------------------------------------------
HXRect Animate(HXAnimationRect &Anim, float DeltaMs) {
	return Detail::UpdateAnimation<HXAnimationRect, HXRect>(Anim, DeltaMs);
}

void StartAnimation(HXAnimationRect &Anim) { Anim._running = true; }
void StopAnimation(HXAnimationRect &Anim) { Anim._running = false; }
void ResetAnimation(HXAnimationRect &Anim) { Anim._currentTime = 0.0f; Anim._running = false; }
bool IsAnimationRunning(const HXAnimationRect &Anim) { return Anim._running; }
bool IsAnimationFinished(const HXAnimationRect &Anim) {
	return !Anim.Loop && !Anim.PingPong && Anim._currentTime >= Anim.DelayMs + Anim.DurationMs;
}

}
