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
 * \file hex_render_backend.h
 * \brief Render backend abstraction for HiEasyX
 *
 * Decouples the core framework from the underlying graphics API.
 * Implementations: HXEasyXBackend (default), future HXOpenGLBackend, etc.
 */

#pragma once

#include <include/impl/hex_impl.h>

namespace HX {

/**
 * Abstract render backend interface.
 * Responsible for painter creation and frame-level lock/unlock.
 */
struct HXRenderBackend {
	virtual ~HXRenderBackend() = default;

	/** Initialize the backend (called once at startup). */
	virtual void Init() = 0;

	/** Shutdown the backend (called once at exit). */
	virtual void Shutdown() = 0;

	/** Called at the beginning of HX::Begin() to lock/prepare the render context. */
	virtual void NewFrame() = 0;

	/** Called at the end of HX::End() to unlock/cleanup the render context. */
	virtual void EndFrame() = 0;

	/** Create an off-screen painter with the given dimensions. */
	virtual HXBufferPainter *CreatePainter(HXGInt width, HXGInt height) = 0;
};

/** Set the global render backend. Must be called before HX::Begin(). */
void SetRenderBackend(HXRenderBackend *backend);

/** Get the current global render backend (may be nullptr). */
HXRenderBackend *GetRenderBackend();

} // namespace HX
