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
 * \file hex_filewatcher.h
 * \brief File system watcher for asset hot-reload and directory monitoring
 *
 * Uses Windows ReadDirectoryChangesW with overlapped I/O.
 * Call Poll() once per frame (e.g. inside your main loop before HXBegin).
 */

#pragma once

#include <include/hex_string.h>
#include <functional>
#include <memory>
#include <vector>

namespace HX {

/**
 * File system change callback.
 * @param file    Relative path of the changed file/directory
 * @param added   True if the item was created or renamed into the watched path
 * @param removed True if the item was deleted or renamed out of the watched path
 * @param modified True if the item's content or attributes were modified
 */
using FileWatchCallback = std::function<void(const HXString &file, bool added, bool removed, bool modified)>;

/**
 * Watch a directory for changes.
 * @param path     Absolute or relative directory path to watch
 * @param callback Invoked for every change detected during Poll()
 */
void WatchDirectory(const HXString &path, FileWatchCallback callback);

/**
 * Stop watching a directory.
 * @param path The path previously passed to WatchDirectory
 */
void UnwatchDirectory(const HXString &path);

/**
 * Poll all watched directories for pending changes.
 * Should be called once per frame.
 */
void PollFileWatcher();

} // namespace HX
