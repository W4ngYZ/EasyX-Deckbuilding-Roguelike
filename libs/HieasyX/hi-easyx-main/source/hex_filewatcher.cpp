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
 * \file hex_filewatcher.cpp
 * \brief Windows ReadDirectoryChangesW implementation
 */

#include <include/hex_filewatcher.h>

#include <windows.h>
#include <vector>
#include <memory>

namespace HX {

struct WatchEntry {
	HXString          Path;
	HANDLE            DirHandle = INVALID_HANDLE_VALUE;
	OVERLAPPED        Overlapped{};
	std::vector<BYTE> Buffer;
	FileWatchCallback Callback;
	bool              Pending = false;
	bool              Closed  = false;

	static constexpr size_t BUFFER_SIZE = 4096;

	WatchEntry(const HXString &path, FileWatchCallback cb)
		: Path(path), Buffer(BUFFER_SIZE), Callback(std::move(cb)) {
		ZeroMemory(&Overlapped, sizeof(Overlapped));
		Overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	}

	~WatchEntry() {
		Close();
	}

	bool Open() {
		DirHandle = CreateFileW(
			Path.c_str(),
			FILE_LIST_DIRECTORY,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr,
			OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
			nullptr);
		return DirHandle != INVALID_HANDLE_VALUE;
	}

	void Close() {
		Closed = true;
		if (DirHandle != INVALID_HANDLE_VALUE) {
			CancelIo(DirHandle);
			CloseHandle(DirHandle);
			DirHandle = INVALID_HANDLE_VALUE;
		}
		if (Overlapped.hEvent) {
			CloseHandle(Overlapped.hEvent);
			Overlapped.hEvent = nullptr;
		}
	}

	bool BeginRead() {
		if (DirHandle == INVALID_HANDLE_VALUE || Closed) return false;
		Pending = true;
		DWORD bytesReturned = 0;
		BOOL ok = ReadDirectoryChangesW(
			DirHandle,
			Buffer.data(), static_cast<DWORD>(Buffer.size()),
			FALSE,  // non-recursive (watch only this directory)
			FILE_NOTIFY_CHANGE_FILE_NAME |
			FILE_NOTIFY_CHANGE_DIR_NAME |
			FILE_NOTIFY_CHANGE_LAST_WRITE |
			FILE_NOTIFY_CHANGE_SIZE,
			&bytesReturned,
			&Overlapped,
			nullptr);
		return ok != FALSE;
	}

	bool HasResult() const {
		if (!Pending) return false;
		return HasOverlappedIoCompleted(const_cast<LPOVERLAPPED>(&Overlapped)) != FALSE;
	}

	void ProcessResults() {
		if (!Pending) return;
		Pending = false;

		DWORD bytesTransferred = 0;
		BOOL  ok               = GetOverlappedResult(DirHandle, &Overlapped, &bytesTransferred, FALSE);
		if (!ok || bytesTransferred == 0) return;

		BYTE *ptr = Buffer.data();
		while (true) {
			auto *info = reinterpret_cast<FILE_NOTIFY_INFORMATION *>(ptr);

			HXString fileName(info->FileName, info->FileNameLength / sizeof(WCHAR));

			bool added   = false;
			bool removed = false;
			bool modified = false;

			switch (info->Action) {
			case FILE_ACTION_ADDED:
			case FILE_ACTION_RENAMED_NEW_NAME:
				added = true;
				break;
			case FILE_ACTION_REMOVED:
			case FILE_ACTION_RENAMED_OLD_NAME:
				removed = true;
				break;
			case FILE_ACTION_MODIFIED:
				modified = true;
				break;
			}

			if (Callback) {
				Callback(fileName, added, removed, modified);
			}

			if (info->NextEntryOffset == 0) break;
			ptr += info->NextEntryOffset;
		}
	}
};

static std::vector<std::unique_ptr<WatchEntry>> G_WatchEntries;

void WatchDirectory(const HXString &path, FileWatchCallback callback) {
	auto entry = std::make_unique<WatchEntry>(path, std::move(callback));
	if (!entry->Open()) {
		return; // silently fail; caller can check by watching for events
	}
	if (!entry->BeginRead()) {
		return;
	}
	G_WatchEntries.push_back(std::move(entry));
}

void UnwatchDirectory(const HXString &path) {
	for (auto it = G_WatchEntries.begin(); it != G_WatchEntries.end(); ++it) {
		if ((*it)->Path == path) {
			(*it)->Close();
			G_WatchEntries.erase(it);
			break;
		}
	}
}

void PollFileWatcher() {
	for (auto &entry : G_WatchEntries) {
		if (entry->Closed) continue;
		if (entry->HasResult()) {
			entry->ProcessResults();
			entry->BeginRead();
		}
	}
}

} // namespace HX
