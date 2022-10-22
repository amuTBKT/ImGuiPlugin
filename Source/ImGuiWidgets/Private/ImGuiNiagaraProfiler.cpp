#if STATS

#include "StaticWidgets.h"

#include "ImGuiRuntimeModule.h"
#include "Stats/StatsData.h"

#include "NiagaraWorldManager.h"
#include "NiagaraGPUProfilerInterface.h"
#include "NiagaraGpuComputeDispatch.h"

#include "implot.h"

PRAGMA_DISABLE_OPTIMIZATION

namespace ImGuiNiagaraProfiler
{
	static TWeakObjectPtr<UWorld> CurrentWorld = nullptr;
	static FNiagaraGpuProfilerListener* NiagaraGPUProfilerListener = nullptr;
	
	static ImGuiTextFilter StatFilter = {};
	static FImGuiImageBindingParams ClearTextIcon;
	static FImGuiImageBindingParams GPUCaptureIcon;

	static bool IsEnabled = false;

	struct FSimStageStatData
	{
		FName StageName;
		float Time;
	};
	static TMap<UNiagaraSystem*, TMap<FVersionedNiagaraEmitterWeakPtr, TArray<FSimStageStatData>>> CapturedStats;

    static void Initialize()
    {
		NiagaraGPUProfilerListener = new FNiagaraGpuProfilerListener();
		NiagaraGPUProfilerListener->SetEnabled(true);
		NiagaraGPUProfilerListener->SetHandler(
			[](const FNiagaraGpuFrameResultsPtr& FrameResults)
			{
				CapturedStats.Reset();
				if (IsEnabled)
				{
					CapturedStats.Reserve(FrameResults->DispatchResults.Num());

					for (const auto& DispatchResult : FrameResults->DispatchResults)
					{
						if (DispatchResult.OwnerComponent.IsValid() &&
							DispatchResult.OwnerComponent->GetWorld() == CurrentWorld)
						{
							if (!DispatchResult.OwnerEmitter.Emitter.IsExplicitlyNull())
							{
								UNiagaraEmitter* OwnerEmitter = DispatchResult.OwnerEmitter.Emitter.Get();
								UNiagaraSystem* OwnerSystem = OwnerEmitter ? OwnerEmitter->GetTypedOuter<UNiagaraSystem>() : nullptr;
								if (OwnerSystem)
								{
									CapturedStats.FindOrAdd(OwnerSystem).FindOrAdd(DispatchResult.OwnerEmitter).Add({ DispatchResult.StageName, (float)DispatchResult.DurationMicroseconds / 1000.f });
								}
							}
						}
					}
				}
			}
		);
		
		FWorldDelegates::OnPreWorldInitialization.AddLambda(
			[](UWorld* World, const UWorld::InitializationValues IVS)
			{
				if (World->WorldType == EWorldType::Game ||
					World->WorldType == EWorldType::GamePreview ||
					World->WorldType == EWorldType::Editor ||
					World->WorldType == EWorldType::PIE)
				{
					CurrentWorld = World;
				}
			}
		);
    }

    static void RegisterOneFrameResources()
    {
		FImGuiRuntimeModule& ImGuiRuntimeModule = FModuleManager::GetModuleChecked<FImGuiRuntimeModule>("ImGuiRuntime");
		ClearTextIcon = ImGuiRuntimeModule.RegisterOneFrameResource(FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.X").GetIcon(), { 13.f, 13.f }, 1.f);
		GPUCaptureIcon = ImGuiRuntimeModule.RegisterOneFrameResource(FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Profiler.Tab").GetIcon(), { 16.f, 16.f }, 1.f);
    }

    static void Tick(ImGuiContext* Context)
    {
		FImGuiTickScope Scope{ Context };

		if (ImGui::Begin("Niagara Profiler", nullptr, ImGuiWindowFlags_None))
		{
			ImGui::Checkbox("Enable profiling", &IsEnabled);
			if (IsEnabled)
			{
				RegisterOneFrameResources();

				if (UWorld* World = CurrentWorld.Get())
				{
					StatFilter.Draw("###FilterLabel");
					//if (StatFilter.IsActive())
					{
						// allow overlapping with filter text input
						ImGui::SetItemAllowOverlap();
						ImGui::SameLine();

						if (StatFilter.IsActive())
						{
							ImGui::SetCursorPosX(ImGui::GetCursorPosX() - ClearTextIcon.Size.x * 2.2f);
							if (ImGui::ImageButton(ClearTextIcon.Id, ClearTextIcon.Size, ClearTextIcon.UV0, ClearTextIcon.UV1))
							{
								StatFilter.Clear();
							}
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

					for (auto& [System, EmitterStats] : CapturedStats)
					{
						if (System)
						{
							const FString SystemName = System->GetFName().ToString();
							ImGui::SetNextItemOpen(true, ImGuiCond_Once);
							if (ImGui::CollapsingHeader(TCHAR_TO_ANSI(*SystemName)))
							{
								for (auto& [VerEmitterWeakPtr, Stats] : EmitterStats)
								{
									UNiagaraEmitter* OwnerEmitter = !VerEmitterWeakPtr.Emitter.IsExplicitlyNull() ? VerEmitterWeakPtr.Emitter.Get() : nullptr;
									if (OwnerEmitter)
									{
										const FString EmitterName = OwnerEmitter->GetName();
										ImGui::SetNextItemOpen(true, ImGuiCond_Once);
										if (ImGui::TreeNode(TCHAR_TO_ANSI(*EmitterName)))
										{
											static constexpr ImGuiTableFlags TableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;
											char ScratchTableIdBuffer[128];
											sprintf_s(ScratchTableIdBuffer, sizeof(ScratchTableIdBuffer), "%s_SimStages", TCHAR_TO_ANSI(*EmitterName));
											if (ImGui::BeginTable(ScratchTableIdBuffer, 3, TableFlags))
											{
												ImGui::TableSetupColumn("Stage Name", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoHide);
												ImGui::TableSetupColumn("Duration(ms)", ImGuiTableColumnFlags_WidthStretch);
												ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthStretch);
												ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible

												static constexpr float TableRowHeight = 0.f; //autosize, having issues setting center alignment for text
												ImGui::TableNextRow(ImGuiTableRowFlags_Headers, TableRowHeight);
												for (int32 column = 0; column < 3; column++)
												{
													ImGui::TableSetColumnIndex(column);
													ImGui::TableHeader(ImGui::TableGetColumnName(column));
												}

												for (auto& Stat : Stats)
												{
													const FString N = Stat.StageName.ToString();
													std::string StageName = std::string(TCHAR_TO_ANSI(*N));

													if (StatFilter.PassFilter(StageName.c_str()))
													{
														ImGui::TableNextRow(ImGuiTableRowFlags_None, TableRowHeight);

														ImGui::TableSetColumnIndex(0);
														{
															ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.f);
															ImGui::Text(StageName.c_str());
														}

														ImGui::TableSetColumnIndex(1);
														{
															ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.f);
															ImGui::Text("%f", Stat.Time);
														}

														ImGui::TableSetColumnIndex(2);
														{
															ImGui::PushID(GetTypeHash(Stat.StageName));
															if (ImGui::ImageButton(GPUCaptureIcon.Id, GPUCaptureIcon.Size, GPUCaptureIcon.UV0, GPUCaptureIcon.UV1))
															{
																ENQUEUE_RENDER_COMMAND(AddNiagaraCapture)(
																	[StageFName = Stat.StageName](FRHICommandListImmediate& RHICmdList)
																	{
																		g_SimStagesToCapture.Add(StageFName);
																	}
																);
															}
															if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
															{
																ImGui::SetTooltip("GPU Capture");
															}
															ImGui::PopID();
														}
													}
												}

												ImGui::EndTable();
											}
											ImGui::TreePop();

											//struct StatData
											//{
											//	std::string Name = "";
											//	int32 ArrayIndex = -1;
											//};
											//TMap<FName, StatData> StatLabels;
											//TArray<const char*> StatLabelsAsChar;
											//TArray<float> StatValue;
											//for (auto& Stat : Stats)
											//{
											//	const FString N = Stat.StageName.ToString();
											//	if (auto* Data = StatLabels.Find(Stat.StageName))
											//	{
											//		StatValue[Data->ArrayIndex] += Stat.Time;
											//	}
											//	else
											//	{
											//		auto& V = StatLabels.Add(Stat.StageName, { std::string(TCHAR_TO_ANSI(*N)), StatLabelsAsChar.Num() });
											//		StatLabelsAsChar.Emplace(V.Name.c_str());
											//		StatValue.Add(Stat.Time);
											//	}
											//}

											//ImPlot::PushColormap(ImPlotColormap_Pastel);
											//if (ImPlot::BeginPlot("##Pie2", ImVec2(1024, 1024), ImPlotFlags_Equal)) {
											//	ImPlot::SetupAxes(NULL, NULL, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
											//	ImPlot::SetupAxesLimits(0, 1, 0, 1);
											//	ImPlot::PlotPieChart(&StatLabelsAsChar[0], &StatValue[0], StatValue.Num(), 0.5, 0.5, 0.4, "%f", 180, ImPlotPieChartFlags_Normalize);
											//	ImPlot::EndPlot();
											//}
											//ImPlot::PopColormap();
										}
									}
								}
							}
						}
					}
				}
			}			
		}
		ImGui::End();
    }

    IMGUI_REGISTER_STATIC_WIDGET(Initialize, Tick);
}

PRAGMA_ENABLE_OPTIMIZATION

#endif //#if STATS
