#include "GlobalWidgets.h"

#if STATS

#include "ImGuiRuntimeModule.h"
#include "Stats/StatsData.h"

PRAGMA_DISABLE_OPTIMIZATION

// config
static constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;
static constexpr float HeaderSizeY = 52.f; //TODO: is there a way to auto size child window?

struct FStatGroupData
{
    const FString DisplayName = "None";
    const FString StatName = "None";
    bool IsActive = false;
};
static TMap<FName, FStatGroupData> StatGroups;

static ImGuiTextFilter StatFilter = {};

// helpers
FORCEINLINE static FString FormatStatValueFloat(const float Value)
{
    const float Frac = FMath::Frac(Value);
    // #TODO: Move to stats thread, add support for int64 type, int32 may not be sufficient all the time.
    const int32 Integer = FMath::FloorToInt(Value);
    const FString IntString = FString::FormatAsNumber(Integer);
    const FString FracString = FString::Printf(TEXT("%0.2f"), Frac);
    const FString Result = FString::Printf(TEXT("%s.%s"), *IntString, *FracString.Mid(2));
    return Result;
}

FORCEINLINE static FString FormatStatValueInt64(const int64 Value)
{
    const FString IntString = FString::FormatAsNumber((int32)Value);
    return IntString;
}

// headings
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
    ImGui::TableSetupColumn("ExclusiveMax", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
    ImGui::TableHeadersRow(); 
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
    ImGui::TableHeadersRow();
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
    ImGui::TableHeadersRow();
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
        const FString StatDisplay = StatDesc.Len() == 0 ? Item.GetShortName().GetPlainNameString() : StatDesc;

        if (!StatFilter.PassFilter(TCHAR_TO_ANSI(*StatDisplay)))
        {
            return;
        }

        ImGui::TableNextRow();
        
        ImGui::TableSetColumnIndex(0);
        {
            static TSet<FString> Selections;

            const bool IsItemSelected = Selections.Contains(StatDisplay);
            if (ImGui::Selectable(TCHAR_TO_ANSI(*StatDisplay), IsItemSelected, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 0)))
            {
                if (IsItemSelected)
                {
                    Selections.Remove(StatDisplay);
                }
                else
                {
                    Selections.Add(StatDisplay);
                }
            }
        }

        // Now append the call count
        ImGui::TableSetColumnIndex(1);
        if (bStackStat)
        {
            if (Item.NameAndInfo.GetFlag(EStatMetaFlags::IsPackedCCAndDuration))
            {
                ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("%u"), Item.GetValue_CallCount(EComplexStatField::IncAve))));
            }
            else
            {
                ImGui::Text("");
            }

            ImGui::TableSetColumnIndex(2);
            ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("%1.2f ms"), FPlatformTime::ToMilliseconds(Item.GetValue_Duration(EComplexStatField::IncAve)))));

            ImGui::TableSetColumnIndex(3);
            ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("%1.2f ms"), FPlatformTime::ToMilliseconds(Item.GetValue_Duration(EComplexStatField::IncMax)))));

            ImGui::TableSetColumnIndex(4);
            ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("%1.2f ms"), FPlatformTime::ToMilliseconds(Item.GetValue_Duration(EComplexStatField::ExcAve)))));

            ImGui::TableSetColumnIndex(5);
            ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("%1.2f ms"), FPlatformTime::ToMilliseconds(Item.GetValue_Duration(EComplexStatField::ExcMax)))));
        }
        else
        {
            // Add the two inclusive columns if asked
            ImGui::TableSetColumnIndex(1);
            ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("%1.2f ms"), FPlatformTime::ToMilliseconds(Item.GetValue_Duration(EComplexStatField::IncAve)))));

            ImGui::TableSetColumnIndex(2);
            ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("%1.2f ms"), FPlatformTime::ToMilliseconds(Item.GetValue_Duration(EComplexStatField::IncMax)))));

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("");
        }
    }
}

static void RenderMemoryCounter(const FGameThreadStatsData& ViewData, const FComplexStatMessage& All)
{
    if (!StatFilter.PassFilter(TCHAR_TO_ANSI(*All.GetDescription())))
    {
        return;
    }

    ImGui::TableNextRow();

    FPlatformMemory::EMemoryCounterRegion Region = FPlatformMemory::EMemoryCounterRegion(All.NameAndInfo.GetField<EMemoryRegion>());
    // At this moment we only have memory stats that are marked as non frame stats, so can't be cleared every frame.
    //const bool bDisplayAll = All.NameAndInfo.GetFlag(EStatMetaFlags::ShouldClearEveryFrame);
    const float MaxMemUsed = All.GetValue_double(EComplexStatField::IncMax);

    // Draw the label
    ImGui::TableSetColumnIndex(0);
    {
        static TSet<FString> Selections;

        const bool IsItemSelected = Selections.Contains(All.GetDescription());
        if (ImGui::Selectable(TCHAR_TO_ANSI(*All.GetDescription()), IsItemSelected, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 0)))
        {
            if (IsItemSelected)
            {
                Selections.Remove(All.GetDescription());
            }
            else
            {
                Selections.Add(All.GetDescription());
            }
        }
    }

    // always use MB for easier comparisons
    const bool bAutoType = false;

    // Now append the max value of the stat
    ImGui::TableSetColumnIndex(1); ImGui::Text(TCHAR_TO_ANSI(*GetMemoryString(MaxMemUsed, bAutoType)));
    
    ImGui::TableSetColumnIndex(2);
    if (ViewData.PoolCapacity.Contains(Region))
    {
        ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("%.0f%%"), float(100.0 * MaxMemUsed / double(ViewData.PoolCapacity[Region])))));
    }
    else
    {
        ImGui::Text("");
    }
    
    ImGui::TableSetColumnIndex(3);
    if (ViewData.PoolAbbreviation.Contains(Region))
    {
        ImGui::Text(TCHAR_TO_ANSI(*ViewData.PoolAbbreviation[Region]));
    }
    else
    {
        ImGui::Text("");
    }
    
    ImGui::TableSetColumnIndex(4);
    if (ViewData.PoolCapacity.Contains(Region))
    {
        ImGui::Text(TCHAR_TO_ANSI(*GetMemoryString(double(ViewData.PoolCapacity[Region]), bAutoType)));
    }
    else
    {
        ImGui::Text("");
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

    ImGui::TableNextRow();

    const bool bDisplayAll = All.NameAndInfo.GetFlag(EStatMetaFlags::ShouldClearEveryFrame);
    
    ImGui::TableSetColumnIndex(0);
    {
        static TSet<FString> Selections;

        const bool IsItemSelected = Selections.Contains(StatDisplay);
        if (ImGui::Selectable(TCHAR_TO_ANSI(*StatDisplay), IsItemSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap, ImVec2(0, 0)))
        {
            if (IsItemSelected)
            {
                Selections.Remove(StatDisplay);
            }
            else
            {
                Selections.Add(StatDisplay);
            }
        }
    }

    ImGui::TableSetColumnIndex(1);
    if (bDisplayAll)
    {
        // Append the average.
        if (All.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_double)
        {
            const FString ValueFormatted = FormatStatValueFloat(All.GetValue_double(EComplexStatField::IncAve));
            ImGui::Text(TCHAR_TO_ANSI(*ValueFormatted));
        }
        else if (All.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64)
        {
            const FString ValueFormatted = FormatStatValueInt64(All.GetValue_int64(EComplexStatField::IncAve));
            ImGui::Text(TCHAR_TO_ANSI(*ValueFormatted));
        }
        else
        {
            ImGui::Text("");
        }
    }
    else
    {
        ImGui::Text("");
    }
    
    // Append the maximum.
    ImGui::TableSetColumnIndex(2);
    if (All.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_double)
    {
        const FString ValueFormatted = FormatStatValueFloat(All.GetValue_double(EComplexStatField::IncMax));
        ImGui::Text(TCHAR_TO_ANSI(*ValueFormatted));
    }
    else if (All.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64)
    {
        const FString ValueFormatted = FormatStatValueInt64(All.GetValue_int64(EComplexStatField::IncMax));
        ImGui::Text(TCHAR_TO_ANSI(*ValueFormatted));
    }
    else
    {
        ImGui::Text("");
    }

    // Append the minimum.
    ImGui::TableSetColumnIndex(3);
    if (All.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_double)
    {
        const FString ValueFormatted = FormatStatValueFloat(All.GetValue_double(EComplexStatField::IncMin));
        ImGui::Text(TCHAR_TO_ANSI(*ValueFormatted));
    }
    else if (All.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64)
    {
        const FString ValueFormatted = FormatStatValueInt64(All.GetValue_int64(EComplexStatField::IncMin));
        ImGui::Text(TCHAR_TO_ANSI(*ValueFormatted));
    }
    else
    {
        ImGui::Text("");
    }
}

FORCEINLINE static void RenderHierCycles(const FActiveStatGroupInfo& HudGroup)
{
    // Render all cycle counters.
    for (int32 RowIndex = 0; RowIndex < HudGroup.HierAggregate.Num(); ++RowIndex)
    {
        const FComplexStatMessage& ComplexStat = HudGroup.HierAggregate[RowIndex];
        const int32 Indent = HudGroup.Indentation[RowIndex];

        RenderCycle(ComplexStat, true);
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
        clipper.End();
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

                    if (ImGui::BeginTable(TCHAR_TO_ANSI(*CycleStatsTableName), 6, TableFlags))
                    {
                        RenderGroupedHeadings(bHasHierarchy);

                        if (bHasHierarchy)
                        {
                            RenderHierCycles(StatGroup);
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

                if (ImGui::BeginTable(TCHAR_TO_ANSI(*MemoryStatsTableName), 5, TableFlags))
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

                if (ImGui::BeginTable(TCHAR_TO_ANSI(*CounterStatsTableName), 4, TableFlags))
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

        // disable at the end, we'll re-enable if the stat groups are still encountered when displaying..
        Itr.Value.IsActive = false;
    }

    constexpr float MarginX = 0.f;

    ImGui::Separator();
    
    StatFilter.Draw();

    ImGui::SameLine();
    if (ImGui::Button("Clear"))
    {
        StatFilter.Clear();
    }
    
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
        //ImGuiRuntimeModule.RegisterOneFrameResource(FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Visible").GetIcon(), { 10.f, 10.f }, 1.f);
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
                        ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("Root filter is active. ROOT=%s"), *ViewData->RootFilter)));
                        
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
