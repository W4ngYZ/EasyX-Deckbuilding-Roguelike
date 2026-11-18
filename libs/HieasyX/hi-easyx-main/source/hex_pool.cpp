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
 * \file hex_pool.cpp
 * \brief SubPainter pool implementation
 */

#include <include/hex_pool.h>
#include <include/hex_render_backend.h>

namespace HX {

HXBufferPainter *HXSubPainterPool::Acquire(HXGInt width, HXGInt height) {
	if (width <= 0 || height <= 0) {
		return nullptr;
	}
	auto key = std::make_pair(width, height);
	auto it = buckets.find(key);
	if (it != buckets.end() && !it->second.empty()) {
		auto *painter = it->second.back();
		it->second.pop_back();
		if (it->second.empty()) {
			buckets.erase(it);
		}
		total_painters--;
		reused++;
		return painter;
	}
	allocated++;
	auto *backend = GetRenderBackend();
	if (backend) {
		return backend->CreatePainter(width, height);
	}
	return nullptr;
}

void HXSubPainterPool::Release(HXBufferPainter *painter) {
	if (!painter) return;

	auto size = painter->GetSize();
	if (size.X <= 0 || size.Y <= 0) {
		delete painter;
		return;
	}

	auto key = std::make_pair(size.X, size.Y);
	auto it = buckets.find(key);
	if (it != buckets.end()) {
		if (it->second.size() >= MAX_PER_BUCKET) {
			// FIFO: evict oldest painter in this bucket
			delete it->second.front();
			it->second.erase(it->second.begin());
			total_painters--;
		}
		it->second.push_back(painter);
		total_painters++;
	} else {
		if (total_painters >= MAX_TOTAL_PAINTERS) {
			// Global cap reached: evict the smallest bucket (or any bucket)
			// Simplest: delete the incoming painter directly
			delete painter;
			return;
		}
		buckets[key] = {painter};
		total_painters++;
	}
}

void HXSubPainterPool::Clear() {
	for (auto &kv : buckets) {
		for (auto *p : kv.second) {
			delete p;
		}
	}
	buckets.clear();
	allocated = 0;
	reused = 0;
	total_painters = 0;
}

////////////////////////////////////////////////////////////
/// Global instance

HXSubPainterPool &GetSubPainterPool() {
	static HXSubPainterPool instance;
	return instance;
}

} // namespace HX
