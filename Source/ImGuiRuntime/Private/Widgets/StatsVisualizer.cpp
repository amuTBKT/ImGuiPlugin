#include "GlobalWidgets.h"

#if STATS

#include "ImGuiRuntimeModule.h"
#include "Stats/StatsData.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Subsystems/AssetEditorSubsystem.h"
#endif //#if WITH_EDITOR

PRAGMA_DISABLE_OPTIMIZATION

// config
static constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;
static constexpr float TableRowHeight = 0.f; //autosize, atm Tables API doesn't allow overriding RowHeader height
static constexpr float TableItemIconSize = 8.f; //icon size to use for row item, should use >=12 to make the image clear but doesn't fit properly with default row size

static constexpr float HeaderSizeY = 52.f; //TODO: is there a way to auto size child window?

struct FStatGroupData
{
    const FString DisplayName = "None";
    const FString StatName = "None";
    bool IsActive = false;
};
static TMap<FName, FStatGroupData> StatGroups;

static ImGuiTextFilter StatFilter = {};

static FImGuiImageBindParams ClearTextIcon;
static FImGuiImageBindParams BrowseAssetIcon;
static FImGuiImageBindParams EditAssetIcon;

// helpers
FORCEINLINE static FString ShortenName(TCHAR const* LongName)
{
	FString Result(LongName);
	const int32 Limit = 72;
	if (Result.Len() > Limit)
	{
		Result = FString(TEXT("...")) + Result.Right(Limit);
	}
	return Result;
}

FORCEINLINE static bool AddSelectableRow(const char* Label, FName ItemName)
{
	static TSet<FName> SelectedItems;

	bool WasSelected = SelectedItems.Contains(ItemName);

	bool IsSelected = ImGui::Selectable(Label, WasSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap, ImVec2(0, TableRowHeight));
	
	if (IsSelected)
	{
		const int32 NumItemsSelected = SelectedItems.Num();
		if (!ImGui::GetIO().KeyCtrl)
		{
			SelectedItems.Reset();
		}

		if (WasSelected && NumItemsSelected == 1)
		{
			SelectedItems.Remove(ItemName);
		}
		else
		{
			SelectedItems.Add(ItemName);
		}
	}

	return IsSelected;
}

// headings
FORCEINLINE static int32 GetCycleStatsColumnCount()
{
	return 7;
}
FORCEINLINE static int32 GetMemoryStatsColumnCount()
{
	return 5;
}
FORCEINLINE static int32 GetCounterStatsColumnCount()
{
	return 4;
}

FORCEINLINE static void RenderGroupedHeadings(const bool bIsHierarchy)
{
    // The heading looks like:
    // Stat [32chars]	CallCount [8chars]	IncAvg [8chars]	IncMax [8chars]	ExcAvg [8chars]	ExcMax [8chars]

    static const char* CaptionFlat = "Cycle counters (flat)";
    static const char* CaptionHier = "Cycle counters (hierarchy)";

    ImGui::TableSetupColumn(bIsHierarchy ? CaptionHier : CaptionFlat, ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoHide);
    ImGui::TableSetupColumn("CallCount", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("InclusiveAvg", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("InclusiveMax", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("ExclusiveAvg", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("ExclusiveMax", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible

    //ImGui::TableHeadersRow();
	ImGui::TableNextRow(ImGuiTableRowFlags_Headers, TableRowHeight);
	for (int32 column = 0; column < GetCycleStatsColumnCount(); column++)
	{
		ImGui::TableSetColumnIndex(column);
		ImGui::TableHeader(ImGui::TableGetColumnName(column));
	}
}

FORCEINLINE static void RenderMemoryHeadings()
{
    // The heading looks like:
    // Stat [32chars]	MemUsed [8chars]	PhysMem [8chars]

    ImGui::TableSetupColumn("Memory Counters", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoHide);
    ImGui::TableSetupColumn("UsedMax", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Mem%", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("MemPool", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Pool Capacity", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
    
	//ImGui::TableHeadersRow();
	ImGui::TableNextRow(ImGuiTableRowFlags_Headers, TableRowHeight);
	for (int column = 0; column < GetMemoryStatsColumnCount(); column++)
	{
		ImGui::TableSetColumnIndex(column);
		ImGui::TableHeader(ImGui::TableGetColumnName(column));
	}
}

FORCEINLINE static void RenderCounterHeadings()
{
    // The heading looks like:
    // Stat [32chars]	Value [8chars]	Average [8chars]

    ImGui::TableSetupColumn("Counters", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoHide);
    ImGui::TableSetupColumn("Average", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Max", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Min", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
    
	//ImGui::TableHeadersRow();
	ImGui::TableNextRow(ImGuiTableRowFlags_Headers, TableRowHeight);
	for (int column = 0; column < GetCounterStatsColumnCount(); column++)
	{
		ImGui::TableSetColumnIndex(column);
		ImGui::TableHeader(ImGui::TableGetColumnName(column));
	}
}

// body
static void RenderCycle(const FComplexStatMessage& Item, const bool bStackStat)
{
    check(Item.NameAndInfo.GetFlag(EStatMetaFlags::IsCycle));

    const bool bIsInitialized = Item.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64;

    if (bIsInitialized)
    {
        // #Stats: Move to the stats thread to avoid expensive computation on the game thread.
        const FString StatDesc = Item.GetDescription();
        const FString StatDisplay = StatDesc.Len() == 0 ? ShortenName(*Item.GetShortName().GetPlainNameString()) : StatDesc;

		const FName RawStatName = Item.NameAndInfo.GetRawName();

        if (!StatFilter.PassFilter(TCHAR_TO_ANSI(*StatDisplay)))
        {
            return;
        }

        ImGui::TableNextRow(ImGuiTableRowFlags_None, TableRowHeight);
        
        ImGui::TableSetColumnIndex(0);
        {
			AddSelectableRow(TCHAR_TO_ANSI(*StatDisplay), RawStatName);
        }

        if (bStackStat)
        {
			ImGui::TableSetColumnIndex(1);
            if (Item.NameAndInfo.GetFlag(EStatMetaFlags::IsPackedCCAndDuration))
            {
                ImGui::Text("%u", Item.GetValue_CallCount(EComplexStatField::IncAve));
            }
            else
            {
                //ImGui::Text(""); leave the column empty?
            }

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%1.2f ms", FPlatformTime::ToMilliseconds(Item.GetValue_Duration(EComplexStatField::IncAve)));

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%1.2f ms", FPlatformTime::ToMilliseconds(Item.GetValue_Duration(EComplexStatField::IncMax)));

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%1.2f ms", FPlatformTime::ToMilliseconds(Item.GetValue_Duration(EComplexStatField::ExcAve)));

            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%1.2f ms", FPlatformTime::ToMilliseconds(Item.GetValue_Duration(EComplexStatField::ExcMax)));
        }
        else
        {
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%1.2f ms", FPlatformTime::ToMilliseconds(Item.GetValue_Duration(EComplexStatField::IncAve)));

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%1.2f ms", FPlatformTime::ToMilliseconds(Item.GetValue_Duration(EComplexStatField::IncMax)));

            ImGui::TableSetColumnIndex(3);
            //ImGui::Text(""); leave the column empty?
        }

		if (bStackStat)
		{
			ImGui::TableSetColumnIndex(6);

#if WITH_EDITOR
			static TMap<FName, TWeakObjectPtr<UObject>> LinkedAsset;
									
			if (!LinkedAsset.Find(RawStatName))
			{
				FString AssetPath = Item.GetShortName().GetPlainNameString();
				AssetPath.RemoveFromEnd(" [GT_CNC]", ESearchCase::IgnoreCase);
				AssetPath.RemoveFromEnd(" [RT]", ESearchCase::IgnoreCase);
				AssetPath.RemoveFromEnd(" [GT]", ESearchCase::IgnoreCase);

				FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
				IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
				FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(*AssetPath);
				if (AssetData.IsValid())
				{
					LinkedAsset.Add(RawStatName) = AssetData.GetAsset();
				}
				else
				{
					LinkedAsset.Add(RawStatName) = nullptr;
				}
			}

			if (UObject* Asset = LinkedAsset.Find(RawStatName)->Get())
			{
				static const uint32 BrowseHash = GetTypeHash(TEXT("_Browse"));
				static const uint32 EditHash = GetTypeHash(TEXT("_Edit"));
				
				const uint32 AssetHash = GetTypeHash(StatDisplay);

				ImGui::PushID(HashCombine(AssetHash, BrowseHash));
				if (ImGui::ImageButton(BrowseAssetIcon.Id, BrowseAssetIcon.Size, BrowseAssetIcon.UV0, BrowseAssetIcon.UV1))
				{
					TArray<UObject*> Objects = { Asset };
					GEditor->SyncBrowserToObjects(Objects);					
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Browse to asset.");
				}
				ImGui::PopID();
				
				ImGui::SameLine();
				
				ImGui::PushID(HashCombine(AssetHash, EditHash));
				if (ImGui::ImageButton(EditAssetIcon.Id, EditAssetIcon.Size, EditAssetIcon.UV0, EditAssetIcon.UV1))
				{
					UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
					if (AssetEditorSubsystem)
					{
						AssetEditorSubsystem->OpenEditorForAsset(Asset);
					}
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Edit asset.");
				}
				ImGui::PopID();
			}
#else
			//ImGui::Text("");
#endif //#if WITH_EDITOR
		}
    }
}

static void RenderMemoryCounter(const FGameThreadStatsData& ViewData, const FComplexStatMessage& All)
{
    if (!StatFilter.PassFilter(TCHAR_TO_ANSI(*All.GetDescription())))
    {
        return;
    }

    ImGui::TableNextRow(ImGuiTableRowFlags_None, TableRowHeight);

    FPlatformMemory::EMemoryCounterRegion Region = FPlatformMemory::EMemoryCounterRegion(All.NameAndInfo.GetField<EMemoryRegion>());
    // At this moment we only have memory stats that are marked as non frame stats, so can't be cleared every frame.
    //const bool bDisplayAll = All.NameAndInfo.GetFlag(EStatMetaFlags::ShouldClearEveryFrame);
    const float MaxMemUsed = All.GetValue_double(EComplexStatField::IncMax);

    // Draw the label
    ImGui::TableSetColumnIndex(0);
    {
		AddSelectableRow(TCHAR_TO_ANSI(*All.GetDescription()), All.NameAndInfo.GetRawName());
    }

    // always use MB for easier comparisons
    const bool bAutoType = false;

    // Now append the max value of the stat
    ImGui::TableSetColumnIndex(1);
	ImGui::Text("%.2f MB", float(MaxMemUsed / (1024.0 * 1024.0)));
    
    ImGui::TableSetColumnIndex(2);
    if (ViewData.PoolCapacity.Contains(Region))
    {
        ImGui::Text("%.0f%%", float(100.0 * (double)MaxMemUsed / double(ViewData.PoolCapacity[Region])));
    }
    else
    {
        //ImGui::Text(""); leave the column empty?
    }
    
    ImGui::TableSetColumnIndex(3);
    if (ViewData.PoolAbbreviation.Contains(Region))
    {
        ImGui::Text(TCHAR_TO_ANSI(*ViewData.PoolAbbreviation[Region]));
    }
    else
    {
        //ImGui::Text(""); leave the column empty?
    }
    
    ImGui::TableSetColumnIndex(4);
    if (ViewData.PoolCapacity.Contains(Region))
    {
		ImGui::Text("%.2f MB", float(double(ViewData.PoolCapacity[Region]) / (1024. * 1024.)));
    }
    else
    {
        //ImGui::Text(""); leave the column empty?
    }
}

static void RenderCounter(const FGameThreadStatsData& ViewData, const FComplexStatMessage& All)
{
    // If this is a cycle, render it as a cycle. This is a special case for manually set cycle counters.
    const bool bIsCycle = All.NameAndInfo.GetFlag(EStatMetaFlags::IsCycle);
    if (bIsCycle)
    {
        RenderCycle(All, false);
        return;
    }

    const FString StatDesc = All.GetDescription();
    const FString StatDisplay = StatDesc.Len() == 0 ? All.GetShortName().GetPlainNameString() : StatDesc;
    
    if (!StatFilter.PassFilter(TCHAR_TO_ANSI(*StatDisplay)))
    {
        return;
    }

    ImGui::TableNextRow(ImGuiTableRowFlags_None, TableRowHeight);

    const bool bDisplayAll = All.NameAndInfo.GetFlag(EStatMetaFlags::ShouldClearEveryFrame);
    
    ImGui::TableSetColumnIndex(0);
    {
		AddSelectableRow(TCHAR_TO_ANSI(*StatDisplay), All.NameAndInfo.GetRawName());
    }

    ImGui::TableSetColumnIndex(1);
    if (bDisplayAll)
    {
        // Append the average.
        if (All.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_double)
        {
			ImGui::Text("%0.2f", All.GetValue_double(EComplexStatField::IncAve));
        }
        else if (All.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64)
        {
			ImGui::Text("%lld", All.GetValue_int64(EComplexStatField::IncAve));
        }
        else
        {
            //ImGui::Text(""); leave the column empty?
        }
    }
    else
    {
        //ImGui::Text(""); leave the column empty?
    }
    
    // Append the maximum.
    ImGui::TableSetColumnIndex(2);
    if (All.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_double)
    {
		ImGui::Text("%0.2f", All.GetValue_double(EComplexStatField::IncMax));
    }
    else if (All.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64)
    {
		ImGui::Text("%lld", All.GetValue_int64(EComplexStatField::IncMax));
    }
    else
    {
        //ImGui::Text(""); leave the column empty?
    }

    // Append the minimum.
    ImGui::TableSetColumnIndex(3);
    if (All.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_double)
    {
		ImGui::Text("%0.2f", All.GetValue_double(EComplexStatField::IncMin));
    }
    else if (All.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64)
    {
		ImGui::Text("%lld", All.GetValue_int64(EComplexStatField::IncMin));
    }
    else
    {
        //ImGui::Text(""); leave the column empty?
    }
}

template <typename T>
void RenderArrayOfStats(const TArray<FComplexStatMessage>& Aggregates, const FGameThreadStatsData& ViewData, const T& FunctionToCall)
{
    // Render all counters.
    if (!StatFilter.IsActive()) // clipper doesn't work properly with filter :(
    {        
        ImGuiListClipper clipper;
        clipper.Begin(Aggregates.Num());
        while (clipper.Step())
        {
            for (int32 RowIndex = clipper.DisplayStart; RowIndex < clipper.DisplayEnd; ++RowIndex)
            {
                FunctionToCall(ViewData, Aggregates[RowIndex]);
            }
        }
    }
    else
    {
        for (int32 RowIndex = 0; RowIndex < Aggregates.Num(); ++RowIndex)
        {
            FunctionToCall(ViewData, Aggregates[RowIndex]);
        }
    }
}

FORCEINLINE static void RenderFlatCycle(const FGameThreadStatsData& ViewData, const FComplexStatMessage& Item)
{
    RenderCycle(Item, true);
}

static void RenderGroupedWithHierarchy(const FGameThreadStatsData& ViewData)
{
    // Render all groups.
    for (int32 GroupIndex = 0; GroupIndex < ViewData.ActiveStatGroups.Num(); ++GroupIndex)
    {
        const FActiveStatGroupInfo& StatGroup = ViewData.ActiveStatGroups[GroupIndex];

        // If the stat isn't enabled for this particular viewport, skip
        const FName& StatGroupFName = ViewData.GroupNames[GroupIndex];
        const FName& GroupName = ViewData.GroupNames[GroupIndex];
        const FString& GroupDesc = ViewData.GroupDescriptions[GroupIndex];
        
        FStatGroupData* StatGroupData = StatGroups.Find(StatGroupFName);
        if (!StatGroupData)
        {
            FString StatName = GroupName.ToString();
            StatName.RemoveFromStart(TEXT("STATGROUP_"), ESearchCase::CaseSensitive);

            StatGroupData = &StatGroups.Add(StatGroupFName, { GroupDesc, StatName, true });
        };
        StatGroupData->IsActive = true;

        if (ImGui::CollapsingHeader(TCHAR_TO_ANSI(*StatGroupData->DisplayName), ImGuiTreeNodeFlags_DefaultOpen))
        {
            // Render cycles.
            {
                const bool bHasHierarchy = !!StatGroup.HierAggregate.Num();
                const bool bHasFlat = !!StatGroup.FlatAggregate.Num();

                if (bHasHierarchy || bHasFlat)
                {
                    const FString CycleStatsTableName = StatGroupData->DisplayName + TEXT("_CycleStats");

                    if (ImGui::BeginTable(TCHAR_TO_ANSI(*CycleStatsTableName), GetCycleStatsColumnCount(), TableFlags))
                    {
                        RenderGroupedHeadings(bHasHierarchy);

                        if (bHasHierarchy)
                        {
							RenderArrayOfStats(StatGroup.HierAggregate, ViewData, RenderFlatCycle);
                        }
                        
                        if (bHasFlat)
                        {
                            RenderArrayOfStats(StatGroup.FlatAggregate, ViewData, RenderFlatCycle);
                        }

                        ImGui::EndTable();
                    }
                }
            }

            // Render memory counters.
            if (StatGroup.MemoryAggregate.Num())
            {
                const FString MemoryStatsTableName = StatGroupData->DisplayName + TEXT("_MemoryStats");

                if (ImGui::BeginTable(TCHAR_TO_ANSI(*MemoryStatsTableName), GetMemoryStatsColumnCount(), TableFlags))
                {
                    RenderMemoryHeadings();

                    RenderArrayOfStats(StatGroup.MemoryAggregate, ViewData, RenderMemoryCounter);
                
                    ImGui::EndTable();
                }
            }

            // Render remaining counters.
            if (StatGroup.CountersAggregate.Num())
            {
                const FString CounterStatsTableName = StatGroupData->DisplayName + TEXT("_CounterStats");

                if (ImGui::BeginTable(TCHAR_TO_ANSI(*CounterStatsTableName), GetCounterStatsColumnCount(), TableFlags))
                {
                    RenderCounterHeadings();

                    RenderArrayOfStats(StatGroup.CountersAggregate, ViewData, RenderCounter);

                    ImGui::EndTable();
                }
            }
        }
    }
}

static void RenderStatsHeader()
{
	// add new stat
	{
		ImGui::PushItemWidth(64);

		static char StatSourceBuffer[64] = { 0 };
		if (ImGui::InputTextWithHint("###NewStat", "Add Stat", StatSourceBuffer, sizeof(StatSourceBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
		{
			// execute command
			const FString StatCommand = FString::Printf(TEXT("stat %s -nodisplay"), ANSI_TO_TCHAR(StatSourceBuffer));
			GEngine->Exec(nullptr, *StatCommand);

			// reset and keep focus
			StatSourceBuffer[0] = 0;
			ImGui::SetKeyboardFocusHere(-1);
		}

		ImGui::PopItemWidth();
	}

    int32 Index = -1;
    for (auto& Itr : StatGroups)
    {
        ++Index;

        ImGui::SameLine();

        const bool bApplyStyle = Itr.Value.IsActive;
        if (bApplyStyle)
        {
            ImGui::PushID(Index);

            ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(2.f / 7.0f, 0.6f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(2.f / 7.0f, 0.7f, 0.7f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(2.f / 7.0f, 0.8f, 0.8f));
        }

        const TCHAR* GroupName = *Itr.Value.DisplayName;
        if (ImGui::Button(TCHAR_TO_ANSI(GroupName)))
        {
            const FString StatCommand = FString::Printf(TEXT("stat %s -nodisplay"), *Itr.Value.StatName);
            GEngine->Exec(nullptr, *StatCommand);
        }

        if (bApplyStyle)
        {
            ImGui::PopStyleColor(3);
            ImGui::PopID();
        }

        // disable at the end, we'll re-enable when stat group is encountered when displaying..
        Itr.Value.IsActive = false;
    }

    ImGui::Separator();
    
	StatFilter.Draw("");
	if (StatFilter.IsActive())
	{
		// allow overlapping with filter text input
		ImGui::SetItemAllowOverlap();
		ImGui::SameLine();
		
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() - ClearTextIcon.Size.x * 2.2f);
		if (ImGui::ImageButton(ClearTextIcon.Id, ClearTextIcon.Size, ClearTextIcon.UV0, ClearTextIcon.UV1))
		{
			StatFilter.Clear();
		}
	}
	ImGui::SameLine();
	ImGui::Text("Filter (inc,-exc)");

    if (StatFilter.IsActive())
    {
        const ImVec2 FilterInputTextSize = ImVec2(ImGui::CalcItemWidth(), ImGui::GetFrameHeight());
        const ImVec2 CursorPosition = ImGui::GetCursorScreenPos();

        const ImVec2 p0 = ImVec2(CursorPosition.x, CursorPosition.y - FilterInputTextSize.y - 4.f);
        const ImVec2 p1 = ImVec2(CursorPosition.x + FilterInputTextSize.x, CursorPosition.y - 4.f);
        
        ImDrawList* DrawList = ImGui::GetWindowDrawList();
        DrawList->AddRect(p0, p1, ImColor(ImVec4(0.26f, 0.59f, 0.98f, 0.67f)), 0.f, ImDrawFlags_None, 2.f);
    }

    ImGui::Separator();
}

namespace ImGuiStatsVizualizer
{
    static void Initialize(FImGuiRuntimeModule& ImGuiRuntimeModule)
    {
        StatGroups.Add(FName(TEXT("STATGROUP_GPU")),             { TEXT("GPU"),              TEXT("GPU"),               false });
        StatGroups.Add(FName(TEXT("STATGROUP_SceneRendering")),  { TEXT("Scene Rendering"),  TEXT("SceneRendering"),    false });
        StatGroups.Add(FName(TEXT("STATGROUP_Niagara")),         { TEXT("Niagara"),          TEXT("Niagara"),           false });
        StatGroups.Add(FName(TEXT("STATGROUP_NiagaraSystems")),  { TEXT("Niagara Systems"),  TEXT("NiagaraSystems"),    false });
        StatGroups.Add(FName(TEXT("STATGROUP_NiagaraEmitters")), { TEXT("Niagara Emitters"), TEXT("NiagaraEmitters"),   false });
        StatGroups.Add(FName(TEXT("STATGROUP_ImGui")),           { TEXT("ImGui"),            TEXT("ImGui"),             false });
    }

    static void RegisterOneFrameResources()
    {
        FImGuiRuntimeModule& ImGuiRuntimeModule = FModuleManager::GetModuleChecked<FImGuiRuntimeModule>("ImGuiRuntime");
		ClearTextIcon = ImGuiRuntimeModule.RegisterOneFrameResource(FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.X").GetIcon(), { 13.f, 13.f }, 1.f);
		BrowseAssetIcon = ImGuiRuntimeModule.RegisterOneFrameResource(FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Search").GetIcon(), { TableItemIconSize, TableItemIconSize }, 1.f);
		EditAssetIcon = ImGuiRuntimeModule.RegisterOneFrameResource(FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Edit").GetIcon(), { TableItemIconSize, TableItemIconSize }, 1.f);
    }

    static void Tick(float DeltaTime)
    {
	    if (ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	    {
            RegisterOneFrameResources();
            
            if (ImGui::BeginChild("Header", ImVec2(0.f, HeaderSizeY), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
            {
                RenderStatsHeader();
            }
            ImGui::EndChild();

            if (ImGui::BeginChild("Body"))
            {
                FGameThreadStatsData* ViewData = FLatestGameThreadStatsData::Get().Latest;
                if (ViewData/* || !ViewData->bRenderStats*/)
                {
                    if (!ViewData->RootFilter.IsEmpty())
                    {
                        ImGui::Text("Root filter is active. ROOT=%s", TCHAR_TO_ANSI(*ViewData->RootFilter));
                        
                        ImGui::Separator();
                    }

                    if (!ViewData->bDrawOnlyRawStats)
                    {
                        RenderGroupedWithHierarchy(*ViewData);
                    }
                }
                else
                {
                    ImGui::Text("Not recording stats...");
                }

            }
            ImGui::EndChild();

            ImGui::End();
	    }
    }
    
    IMGUI_REGISTER_GLOBAL_WIDGET(Initialize, Tick);
}

PRAGMA_ENABLE_OPTIMIZATION

#endif //#if STATS
