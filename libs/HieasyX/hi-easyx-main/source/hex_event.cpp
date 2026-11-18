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
 * \file hex_event.cpp
 * \brief Event router compatibility layer implementation
 */


#include <include/hex.h>
#include <include/hex_event.h>

namespace HX {

void HXEventRouter::Reset() {
	_events.clear();
}

void HXEventRouter::Sync(std::size_t messageCount) {
	if (_events.size() > messageCount) {
		_events.clear();
	}
	for (std::size_t i = _events.size(); i < messageCount; ++i) {
		HXEvent event;
		event.MessageIndex = i;
		event.Phase        = HXEventPhase::Target;
		_events.push_back(event);
	}
}

std::vector<HXEvent> &HXEventRouter::Events() {
	return _events;
}

const std::vector<HXEvent> &HXEventRouter::Events() const {
	return _events;
}

HXEventRouter &GetEventRouter() {
	return GetContext().EventRouter;
}

void ResetEventRouter() {
	GetEventRouter().Reset();
}

void SyncEventRouter() {
	auto &context = GetContext();
	context.EventRouter.Sync(context.MessageQuery.size());
}

HXMessage *GetEventMessage(HXEvent &event) {
	auto &messages = GetContext().MessageQuery;
	if (event.MessageIndex >= messages.size()) return nullptr;
	return &messages[event.MessageIndex];
}

const HXMessage *GetEventMessage(const HXEvent &event) {
	const auto &messages = GetContext().MessageQuery;
	if (event.MessageIndex >= messages.size()) return nullptr;
	return &messages[event.MessageIndex];
}

HXMessage *FindMouseEventInRect(const HXRect &rect, bool requireUnprocessed) {
	SyncEventRouter();
	for (auto &event : GetEventRouter().Events()) {
		auto *msg = GetEventMessage(event);
		if (!msg || !msg->MouseAction) continue;
		if (requireUnprocessed && msg->Processed) continue;
		if (rect.PointInside({msg->MouseX, msg->MouseY})) {
			event.TargetRect = rect;
			return msg;
		}
	}
	return nullptr;
}

bool ConsumeMouseEventsInsideRect(const HXRect &rect, bool pressOnly) {
	bool consumed = false;
	SyncEventRouter();
	for (auto &event : GetEventRouter().Events()) {
		auto *msg = GetEventMessage(event);
		if (!msg || !msg->MouseAction || msg->Processed) continue;
		if (pressOnly && !msg->MouseLeftPressed && !msg->MouseRightPressed) continue;
		if (rect.PointInside({msg->MouseX, msg->MouseY})) {
			event.TargetRect       = rect;
			event.DefaultPrevented = true;
			msg->Processed         = true;
			consumed               = true;
		}
	}
	return consumed;
}

bool ConsumeMouseEventsOutsideRect(const HXRect &rect, bool detectLeftPressOutside) {
	bool leftPressOutside = false;
	SyncEventRouter();
	for (auto &event : GetEventRouter().Events()) {
		auto *msg = GetEventMessage(event);
		if (!msg || !msg->MouseAction || msg->Processed) continue;
		if (!rect.PointInside({msg->MouseX, msg->MouseY})) {
			if (detectLeftPressOutside && msg->MouseLeftPressed) {
				leftPressOutside = true;
			}
			event.TargetRect       = rect;
			event.DefaultPrevented = true;
			msg->Processed         = true;
		}
	}
	return leftPressOutside;
}

} // namespace HX
