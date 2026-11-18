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
 * \file hex_state.h
 * \brief The internal state pool for persistent control state across frames
 *
 * HiEasyX uses an IMGUI paradigm, meaning controls are recreated every frame.
 * To free users from manually declaring `static ButtonProfile` for every
 * control, the HXStatePool stores persistent state keyed by control IDs.
 */

#pragma once

#include <include/hex_string.h>

#include <any>
#include <unordered_map>

namespace HX {

/**
 * The global state pool that persists across frames.
 * It is NOT reset by HX::Begin() / HX::End().
 */
class HXStatePool {
public:
	HXStatePool() = default;
	~HXStatePool() = default;

	HXStatePool(const HXStatePool &) = delete;
	HXStatePool &operator=(const HXStatePool &) = delete;

public:
	/**
	 * Get or create a state object of type T associated with Id.
	 * @tparam T The state type (e.g., ButtonProfile, SliderProfile1f)
	 * @param Id The unique identifier for this control instance
	 * @return Pointer to the persistent state
	 */
	template <class T>
	T *Get(const HXString &Id) {
		auto it = _pool.find(Id);
		if (it == _pool.end()) {
			auto [newIt, _] = _pool.emplace(Id, std::make_any<T>());
			return std::any_cast<T>(&newIt->second);
		}
		return std::any_cast<T>(&it->second);
	}

	/**
	 * Get an existing state without creating. Returns nullptr if not found.
	 */
	template <class T>
	T *Find(const HXString &Id) {
		auto it = _pool.find(Id);
		if (it == _pool.end()) {
			return nullptr;
		}
		return std::any_cast<T>(&it->second);
	}

	/**
	 * Remove a state by Id
	 */
	void Remove(const HXString &Id) {
		_pool.erase(Id);
	}

	/**
	 * Clear all states (useful for memory purge, not for normal frames)
	 */
	void Clear() {
		_pool.clear();
	}

private:
	std::unordered_map<HXString, std::any> _pool;
};

/**
	* Get the global state pool instance
	* @return Reference to the global state pool
	*/
HXStatePool &GetStatePool();

}
