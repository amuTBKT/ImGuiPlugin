// Helper for staged/multi-threaded rendering v0.10, for Dear ImGui
// Get latest version at http://www.github.com/ocornut/imgui_club
// Licensed under The MIT License (MIT)

// Based on a discussion at https://github.com/ocornut/imgui/issues/1860#issuecomment-1927630727
// Since 1.92.0, textures also needs to be updated. See discussion at https://github.com/ocornut/imgui/issues/8597

// CHANGELOG:
// - v0.10: (2025/04/30): initial version. Not well tested.

// Usage:
/*
	// Storage. Keep persistent as we reuse buffers across frames.
	static ImDrawDataSnapshot snapshot;

	// [Update thread] Take a snapshot of the ImDrawData
	snapshot.SnapUsingSwap(ImGui::GetDrawData(), ImGui::GetTime());

	// [Render thread] Render later
	ImGui_ImplDX11_RenderDrawData(&snapshot.DrawData);
*/

// FIXME: Could store an ID in ImDrawList to make this easier for user.
#pragma once
#include "imgui/imgui_internal.h" // ImPool<>, ImHashData

//-----------------------------------------------------------------------------
// ImDrawDataSnapshot - HEADERS
//-----------------------------------------------------------------------------

struct ImDrawDataSnapshotEntry
{
	ImDrawList*	SrcCopy = NULL;		// Drawlist owned by main context
	ImDrawList*	OurCopy = NULL;		// Our copy
	int32		InitializedFrame = 0;
	int32		LastUsedFrame = 0;
};

struct ImDrawDataSnapshot
{
	// Members
	ImDrawData						DrawData;
	ImPool<ImDrawDataSnapshotEntry>	Cache;
	int								MemoryCompactTimer = 60; // Discard unused data after 60 frames

	// Functions
	~ImDrawDataSnapshot()			{ Clear(); }
	void							Clear();
	void							SnapUsingSwap(ImDrawData* src, int current_frame); // Efficient snapshot by swapping data, meaning "src" is unusable.
	//void							SnapUsingCopy(ImDrawData* src, double current_time); // Deep-copy snapshop. Probably not needed.

	// Internals
	ImGuiID                         GetDrawListID(ImDrawList* src_list, int frame_index) { return ImHashData(&src_list, sizeof(src_list), (frame_index & 0x1)); }     // Hash pointer
	ImDrawDataSnapshotEntry*        GetOrAddEntry(ImDrawList* src_list, int frame_index) { return Cache.GetOrAddByKey(GetDrawListID(src_list, frame_index)); }
};

//-----------------------------------------------------------------------------
// ImDrawDataSnapshot - IMPLEMENTATION
//-----------------------------------------------------------------------------

inline void ImDrawDataSnapshot::Clear()
{
	for (int n = 0; n < Cache.GetMapSize(); n++)
		if (ImDrawDataSnapshotEntry* entry = Cache.TryGetMapData(n))
			IM_DELETE(entry->OurCopy);
	Cache.Clear();
	DrawData.Clear();
}

inline void ImDrawDataSnapshot::SnapUsingSwap(ImDrawData* src, int current_frame)
{
	ImDrawData* dst = &DrawData;
	IM_ASSERT(src != dst && src->Valid);

	// Copy all fields except CmdLists[]
	ImVector<ImDrawList*> backup_draw_list;
	backup_draw_list.swap(src->CmdLists);
	IM_ASSERT(src->CmdLists.Data == NULL);
	*dst = *src;
	backup_draw_list.swap(src->CmdLists);

	// Swap and mark as used
	for (ImDrawList* src_list : src->CmdLists)
	{
		ImDrawDataSnapshotEntry* entry = GetOrAddEntry(src_list, current_frame);
		if (entry->OurCopy == NULL)
		{
			entry->SrcCopy = src_list;
			entry->OurCopy = IM_NEW(ImDrawList)(src_list->_Data);
			entry->InitializedFrame = current_frame;
		}
		IM_ASSERT(entry->SrcCopy == src_list);
		entry->SrcCopy->CmdBuffer.swap(entry->OurCopy->CmdBuffer); // Cheap swap
		entry->SrcCopy->IdxBuffer.swap(entry->OurCopy->IdxBuffer);
		entry->SrcCopy->VtxBuffer.swap(entry->OurCopy->VtxBuffer);
		entry->SrcCopy->CmdBuffer.reserve(entry->OurCopy->CmdBuffer.Capacity); // Preserve bigger size to avoid reallocs for two consecutive frames
		entry->SrcCopy->IdxBuffer.reserve(entry->OurCopy->IdxBuffer.Capacity);
		entry->SrcCopy->VtxBuffer.reserve(entry->OurCopy->VtxBuffer.Capacity);
		entry->LastUsedFrame = current_frame;
		dst->CmdLists.push_back(entry->OurCopy);
	}

	// Cleanup unused data
	const int gc_threshold = current_frame - MemoryCompactTimer;
	for (int n = 0; n < Cache.GetMapSize(); n++)
		if (ImDrawDataSnapshotEntry* entry = Cache.TryGetMapData(n))
		{
			if (entry->LastUsedFrame < gc_threshold)
			{
				IM_DELETE(entry->OurCopy);
				Cache.Remove(GetDrawListID(entry->SrcCopy, entry->InitializedFrame), entry);
			}
		}
};

//-----------------------------------------------------------------------------
