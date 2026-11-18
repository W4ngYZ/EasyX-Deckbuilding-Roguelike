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
 * \file hex_plugin.h
 * \brief Plugin architecture for HiEasyX
 *
 * Plugins are dynamic libraries that extend HiEasyX with custom controls,
 * commands, and themes. On Windows they are loaded via LoadLibrary.
 */

#pragma once

#include <include/hex_string.h>
#include <vector>

namespace HX {

/**
 * Base class for all HiEasyX plugins.
 * A plugin DLL must export a C function:
 *   extern "C" __declspec(dllexport) HX::HXPlugin* CreatePlugin();
 */
struct HXPlugin {
	HXString Name;
	HXString Version;

	virtual ~HXPlugin() = default;

	virtual void OnLoad()    = 0;
	virtual void OnUnload()  = 0;
	virtual void RegisterControls() = 0;
	virtual void RegisterCommands() = 0;
	virtual void RegisterThemes()   = 0;
};

/**
 * Load a plugin from a DLL path.
 * @param dllPath Absolute or relative path to the plugin DLL.
 * @return true if the plugin was loaded successfully.
 */
bool LoadPlugin(const HXString &dllPath);

/**
 * Unload all loaded plugins.
 */
void UnloadAllPlugins();

/**
 * Get the list of currently loaded plugins.
 */
const std::vector<HXPlugin *> &GetLoadedPlugins();

}
