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
 * \file hex_plugin.cpp
 * \brief Plugin loader implementation (Windows)
 */

#include <include/hex_plugin.h>

#include <windows.h>

#undef max
#undef min

namespace HX {

struct LoadedPlugin {
	HXPlugin *Plugin = nullptr;
	HMODULE   Module = nullptr;
};

static std::vector<LoadedPlugin> G_LoadedPlugins;

using CreatePluginFunc = HXPlugin *(*)();

bool LoadPlugin(const HXString &dllPath) {
	HMODULE hMod = LoadLibraryW(dllPath.c_str());
	if (!hMod) {
		return false;
	}

	auto createFn = reinterpret_cast<CreatePluginFunc>(
		GetProcAddress(hMod, "CreatePlugin"));
	if (!createFn) {
		FreeLibrary(hMod);
		return false;
	}

	HXPlugin *plugin = createFn();
	if (!plugin) {
		FreeLibrary(hMod);
		return false;
	}

	plugin->OnLoad();
	plugin->RegisterControls();
	plugin->RegisterCommands();
	plugin->RegisterThemes();

	G_LoadedPlugins.push_back({plugin, hMod});
	return true;
}

void UnloadAllPlugins() {
	for (auto &lp : G_LoadedPlugins) {
		if (lp.Plugin) {
			lp.Plugin->OnUnload();
			delete lp.Plugin;
		}
		if (lp.Module) {
			FreeLibrary(lp.Module);
		}
	}
	G_LoadedPlugins.clear();
}

const std::vector<HXPlugin *> &GetLoadedPlugins() {
	static std::vector<HXPlugin *> cache;
	cache.clear();
	for (const auto &lp : G_LoadedPlugins) {
		cache.push_back(lp.Plugin);
	}
	return cache;
}

}
