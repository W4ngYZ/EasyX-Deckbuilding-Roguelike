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
 * \file hex_blueprint.h
 * \brief Blueprint node graph canvas API
 */


#pragma once

#include <include/hex_string.h>
#include <include/hex_inline_editor.h>
#include <include/impl/hex_impl.h>
#include <ctime>
#include <functional>
#include <unordered_set>
#include <vector>

struct HXTheme;

namespace HX {

// ------------------------------------------------------------------
// Pin types & directions
// ------------------------------------------------------------------

enum class HXPinType { Flow, Bool, Int, Float, String, Vector, Color, Object, Texture, Custom };
enum class HXPinDirection { Input, Output };

// ------------------------------------------------------------------
// Node category (determines title bar color)
// ------------------------------------------------------------------

enum class HXNodeCategory {
	Event,        // Red
	Function,     // Blue
	Variable,     // Green
	Math,         // Yellow
	FlowControl,  // Brown
	Material,     // Purple
	Output,       // Dark Purple
	Comment,      // Teal (transparent large box)
	Custom        // Gray
};

// ------------------------------------------------------------------
// Pin (connection port)
// ------------------------------------------------------------------

struct HXNodePin {
	HXString       Id;
	HXString       Label;
	HXPinType      Type      = HXPinType::Float;
	HXPinDirection Direction = HXPinDirection::Input;
	HXString       DefaultValue;
	bool           Connected = false;

	// Runtime: updated each frame by DrawNode
	HXPoint ScreenCenter = {0, 0};
	bool    Hovered      = false;
};

// ------------------------------------------------------------------
// Inline node value (e.g. Constant value)
// ------------------------------------------------------------------

struct HXNodeValue {
	HXString  Name;
	HXString  Value;
	HXPinType Type = HXPinType::Float;
};

// ------------------------------------------------------------------
// Node
// ------------------------------------------------------------------

struct HXNode {
	HXString               Id;
	HXString               Title;
	HXString               Icon;
	HXNodeCategory         Category = HXNodeCategory::Custom;
	HXPoint                Position = {0, 0};
	HXPoint                Size     = {200, 100}; // Auto-calculated by DrawNode
	std::vector<HXNodePin> Inputs;
	std::vector<HXNodePin> Outputs;
	std::vector<HXNodeValue> Values;
	bool                   Selected = false;
	bool                   Collapsed = false;
	HXString               TypeName; // Execution engine identifier (e.g. "Add", "Branch")
};

// ------------------------------------------------------------------
// Link
// ------------------------------------------------------------------

struct HXNodeLink {
	HXString StartPinId;
	HXString EndPinId;
};

// ------------------------------------------------------------------
// Graph container
// ------------------------------------------------------------------

struct HXNodeGraph {
	std::vector<HXNode>     Nodes;
	std::vector<HXNodeLink> Links;

	HXNode *    FindNode(const HXString &id);
	HXNodePin * FindPin(const HXString &id);
	HXNode *    FindNodeContainingPin(const HXString &pinId);
	void        DeleteNode(const HXString &id);
};

// ------------------------------------------------------------------
// Context menu item
// ------------------------------------------------------------------

struct BlueprintMenuItem {
	HXString Category;
	HXString Label;
	HXString NodeTypeId; // Used when creating node from menu
};

// ------------------------------------------------------------------
// Blueprint canvas profile
// ------------------------------------------------------------------

struct BlueprintProfile {
	HXNodeGraph *Graph  = nullptr;
	float        Zoom   = 1.0f;
	HXPoint      Offset = {0, 0};
	HXPoint      Size   = {0, 0};

	/**
	 * Optional custom node factory. If set and returns true, the built-in factory is skipped.
	 */
	std::function<bool(const HXString& typeName, HXPoint pos, HXNode& outNode)> CustomNodeFactory;

	/**
	 * Optional custom pin color mapping. If empty, theme-backed defaults are used.
	 */
	std::function<HXColor(HXPinType)> PinColorCallback;

	/**
	 * Optional custom category color mapping. If empty, theme-backed defaults are used.
	 */
	std::function<HXColor(HXNodeCategory)> CategoryColorCallback;

	// Node appearance
	HXColor NodeBodyBorderColor{56, 56, 58, 255};
	HXColor NodeBodyTopColor{28, 28, 30, 255};
	HXColor NodeBodyBottomColor{28, 28, 30, 255};
	HXColor NodeShadowColor{0, 0, 0, 180};
	HXColor NodeTitleTextColor{255, 255, 255, 255};
	HXColor NodeDefaultValueTextColor{120, 120, 125, 255};

	// Selection / hover
	HXColor NodeSelectionBorderColor{94, 155, 234, 255};
	HXColor NodeHoverBorderColor{72, 72, 74, 180};

	// Canvas
	HXColor CanvasBackgroundColor{11, 15, 20, 255};
	HXColor CanvasGridColor{30, 41, 55, 255};

	// Selection box (drag on background)
	HXColor SelectionBoxFillColor{56, 189, 248, 50};
	HXColor SelectionBoxInnerColor{56, 189, 248, 28};
	HXColor SelectionBoxBorderColor{56, 189, 248, 150};

	// Context menu (right-click)
	HXColor MenuBackgroundTopColor{37, 37, 40, 255};
	HXColor MenuBackgroundBottomColor{37, 37, 40, 255};
	HXColor MenuBorderColor{56, 56, 58, 255};
	HXColor MenuCategoryHoverColor{44, 44, 46, 255};
	HXColor MenuItemHoverColor{44, 44, 46, 255};
	HXColor MenuCategoryTextColor{94, 155, 234, 255};
	HXColor MenuItemTextColor{255, 255, 255, 255};
	HXColor MenuSeparatorColor{56, 56, 58, 255};

	// Inline editor (single-line editor inside a node)
	HXColor InlineEditorBackgroundColor{42, 42, 42, 255};
	HXColor InlineEditorBorderColor{92, 138, 198, 255};
	HXColor InlineEditorTextColor{255, 255, 255, 255};
	HXColor InlineEditorSelectionBg{80, 120, 200, 255};

	// Color picker (node inline color picker)
	HXColor ColorPickerBackgroundColor{45, 45, 45, 255};
	HXColor ColorPickerBorderColor{92, 138, 198, 255};
	HXColor ColorPickerCursorColor{255, 255, 255, 255};

	// Pins
	HXColor PinHighlightBorderColor{255, 255, 255, 200};

	// Comment node
	HXColor CommentBorderColor{94, 234, 212, 170};
	HXColor CommentTextColor{255, 255, 255, 230};

	// Texture preview fallback
	HXColor TextureFallbackBackground{35, 43, 54, 255};
	HXColor TextureFallbackBorder{71, 85, 105, 255};
	HXColor TextureFallbackText{148, 163, 184, 255};

	// Color swatch
	HXColor ColorSwatchBorder{226, 232, 240, 255};

	// Inline color picker preview
	HXColor ColorPickerPreviewBorder{200, 200, 200, 255};
	HXColor ColorPickerPreviewText{220, 220, 220, 255};

	struct {
		// Panning
		bool    IsPanning      = false;
		HXPoint PanStartMouse  = {0, 0};
		HXPoint PanStartOffset = {0, 0};

		// Hit / selection / drag
		HXNode *    HoverNode    = nullptr;
		HXNodePin * HoverPin     = nullptr;
		HXNode *    DraggingNode = nullptr;
		HXNodePin * DraggingPin  = nullptr;
		HXRect      SelectionBox = {};
		HXPoint     SelectionBoxStart = {0, 0};
		std::vector<HXString> SelectedNodes;

		// Multi-drag
		HXPoint DragStartMouseWorld = {0, 0};
		std::vector<HXPoint> DragStartNodePositions;

		// Context menu (built-in, replaces PopupMenu)
		bool    ContextMenuOpen     = false;
		HXPoint ContextMenuScreenPos = {0, 0};
		HXPoint ContextMenuWorldPos  = {0, 0};
		int     ContextMenuHoverCategory = -1;
		int     ContextMenuHoverItem     = -1;

		// Inline edit (reusable single-line editor)
		bool     Editing        = false;
		bool     EditingIsPin   = false; // true = pin.DefaultValue, false = node.Values
		bool     EditingCommentTitle = false;
		HXString EditingNodeId;
		int      EditingPinIdx    = -1;
		int      EditingValueIdx  = -1;
		HXSingleLineEditorState EditState;

		// Double-click detection for comment node title editing
		clock_t  LastNodeClickTime = 0;
		HXPoint  LastNodeClickPos  = {0, 0};
		HXString LastNodeClickId;

		// Inline color picker (for HXPinType::Color values)
		bool     EditingColor     = false;
		bool     ColorPickerDragging = false;
		float    ColorH = 0.0f, ColorS = 1.0f, ColorV = 1.0f;
		HXRect   _pickerRect = {0, 0, 0, 0};
		HXRect   _sqRect = {0, 0, 0, 0};
		HXRect   _hueRect = {0, 0, 0, 0};

		// Canvas rect (cached each frame)
		HXRect CanvasRect = {0, 0, 0, 0};
	} _internal;
};

bool BeginBlueprint(const HXString &id, BlueprintProfile &profile);
void EndBlueprint();

HXPoint WorldToScreen(const HXPoint &worldPos, float zoom, const HXPoint &offset);
HXPoint ScreenToWorld(const HXPoint &screenPos, float zoom, const HXPoint &offset);

HXString MakeBlueprintNodeId(const HXString &base);
HXNode MakeBlueprintNode(const HXString &typeName, HXPoint pos, BlueprintProfile *profile = nullptr);

// Helpers exposed for demo use
HXColor GetPinColor(HXPinType type, const HXTheme *theme = nullptr, const BlueprintProfile *profile = nullptr);
HXColor GetCategoryColor(HXNodeCategory cat, const HXTheme *theme = nullptr, const BlueprintProfile *profile = nullptr);

// ------------------------------------------------------------------
// Serialization
// ------------------------------------------------------------------

void SaveBlueprint(const HXNodeGraph& graph, float zoom, const HXPoint& offset, const HXString& path);
bool LoadBlueprint(HXNodeGraph& graph, float& zoom, HXPoint& offset, const HXString& path);

// ------------------------------------------------------------------
// Execution engine
// ------------------------------------------------------------------

struct HXBlueprintValue {
	HXPinType Type = HXPinType::Float;
	float    AsFloat  = 0.0f;
	int      AsInt    = 0;
	bool     AsBool   = false;
	HXString AsString;
	HXPoint  AsVector = {0, 0};
	HXColor  AsColor  = {255, 255, 255, 255};
	HXString AsTexture; // File path for Texture type

	bool IsTruthy() const;
	HXString ToString() const;
	static HXBlueprintValue FromString(const HXString &s, HXPinType type);
	static HXBlueprintValue FromDefault(const HXString &s, HXPinType type);
};

struct HXBlueprintRuntime {
	// Variable table (persisted within one execution)
	std::unordered_map<HXString, HXBlueprintValue> Variables;
	// Pin value cache (avoids re-evaluating the same pin during one execution)
	std::unordered_map<HXString, HXBlueprintValue> PinValues;
	// Execution log output
	HXString Log;
	HXString LastError;
};

// ------------------------------------------------------------------
// Custom node registration
// ------------------------------------------------------------------

using HXBlueprintDataHandler = std::function<HXBlueprintValue(HXNodeGraph*, HXNode&, const HXString& outputPinId, HXBlueprintRuntime&)>;
using HXBlueprintFlowHandler = std::function<void(HXNodeGraph*, HXNode&, HXBlueprintRuntime&, std::unordered_set<HXString>&)>;

struct HXBlueprintNodeTemplate {
	HXString Category;
	HXString Title;
	HXString TypeName;
	HXString Icon;
	HXNodeCategory NodeCategory = HXNodeCategory::Custom;
	HXPoint DefaultSize = {180, 80};
	std::vector<HXNodePin> Inputs;
	std::vector<HXNodePin> Outputs;
	std::vector<HXNodeValue> Values;
};

void RegisterBlueprintDataNode(const HXString& typeName, HXBlueprintDataHandler handler);
void RegisterBlueprintFlowNode(const HXString& typeName, HXBlueprintFlowHandler handler);
void RegisterBlueprintNodeTemplate(const HXBlueprintNodeTemplate& tmpl);
const std::vector<HXBlueprintNodeTemplate>& GetBlueprintNodeTemplates();

// ------------------------------------------------------------------
// Execution engine
// ------------------------------------------------------------------

// Execute the blueprint graph starting from all Event nodes.
// Populates runtime.Log with execution output.
void ExecuteBlueprint(HXNodeGraph *graph, HXBlueprintRuntime &runtime);

// Exposed for custom execution scenarios
HXBlueprintValue EvaluateBlueprintPin(HXNodeGraph *graph, const HXString &pinId, HXBlueprintRuntime &runtime);

} // namespace HX
