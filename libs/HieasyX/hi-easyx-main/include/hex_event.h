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
 * \file hex_event.h
 * \brief Event router compatibility layer for deterministic input routing
 */


#pragma once

#include <include/impl/hex_impl.h>

#include <cstddef>
#include <vector>

namespace HX {

enum class HXEventPhase {
	Capture,
	Target,
	Bubble
};

struct HXEvent {
	std::size_t  MessageIndex = 0;
	HXEventPhase Phase        = HXEventPhase::Target;
	HXRect       TargetRect   = {0, 0, 0, 0};
	bool         CancelBubble = false;
	bool         DefaultPrevented = false;
};

class HXEventRouter {
public:
	void Reset();
	void Sync(std::size_t messageCount);

	std::vector<HXEvent> &Events();
	const std::vector<HXEvent> &Events() const;

private:
	std::vector<HXEvent> _events;
};

HXEventRouter &GetEventRouter();
void ResetEventRouter();
void SyncEventRouter();

HXMessage *GetEventMessage(HXEvent &event);
const HXMessage *GetEventMessage(const HXEvent &event);

HXMessage *FindMouseEventInRect(const HXRect &rect, bool requireUnprocessed = true);
bool ConsumeMouseEventsInsideRect(const HXRect &rect, bool pressOnly = false);
bool ConsumeMouseEventsOutsideRect(const HXRect &rect, bool detectLeftPressOutside);

} // namespace HX
