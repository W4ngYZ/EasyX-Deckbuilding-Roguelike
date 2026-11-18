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
 * \file hex_pool.h
 * \brief SubPainter object pool to eliminate per-frame allocation
 *
 * DockSpace slots, Viewports, and Window painters all create SubPainters
 * every frame.  This pool reuses them by exact (width, height) buckets.
 */

#pragma once

#include <include/impl/hex_impl.h>
#include <map>
#include <utility>
#include <vector>

namespace HX {

class HXSubPainterPool {
public:
	HXBufferPainter *Acquire(HXGInt width, HXGInt height);
	void Release(HXBufferPainter *painter);
	void Clear();

	size_t GetTotalAllocated() const { return allocated; }
	size_t GetTotalReused() const { return reused; }

private:
	std::map<std::pair<HXGInt, HXGInt>, std::vector<HXBufferPainter *>> buckets;
	size_t allocated = 0;
	size_t reused = 0;
	size_t total_painters = 0;
	static constexpr size_t MAX_PER_BUCKET = 4;
	static constexpr size_t MAX_TOTAL_PAINTERS = 64;
};

/**
 * Global SubPainter pool used by Window, DockSpace, and Viewport.
 */
HXSubPainterPool &GetSubPainterPool();

} // namespace HX
