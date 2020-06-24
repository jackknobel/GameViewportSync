// Fill out your copyright notice in the Description page of Project Settings.

#include "SyncViewportSubsystem.h"
#include "ViewportSyncSettings.h"

// UE Includes
#include "Editor.h"
#include "IAssetViewport.h"
#include "LevelEditor.h"
#include "LevelEditorViewport.h"
#include "SceneOutlinerModule.h"
#include "Engine/Selection.h"
#include "Styling/SlateIconFinder.h"
#include "SceneOutlinerPublicTypes.h"
#include "SEditorViewport.h"
#include "SLevelViewport.h"
#include "ViewportSyncEditorCommands.h"
#include "ToolMenus.h"
#include "Slate/SceneViewport.h"

#define LOCTEXT_NAMESPACE "SyncViewportSubsystem"

static const FName LevelEditorModuleName("LevelEditor");

DEFINE_LOG_CATEGORY_STATIC(LogViewportSync, Log, All);

const FText USyncViewportSubsystem::SectionExtensionPointText(LOCTEXT("ViewportSync", "Viewport Sync"));

const FName USyncViewportSubsystem::SectionExtensionPointName("ViewportSync");
const FName USyncViewportSubsystem::FollowActorExtensionPointName("ViewportSync_FollowActor");
const FName USyncViewportSubsystem::SelectActorExtensionPointName("ViewportSync_SelectActor");

void USyncViewportSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	FViewportSyncEditorCommands::Register();

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(LevelEditorModuleName);

	TSharedRef<FUICommandList> CommandList = LevelEditorModule.GetGlobalLevelEditorActions();
	RegisterCommands(CommandList);

	// Register our viewport drop down extension
	const FLevelEditorModule::FLevelEditorMenuExtender ViewportExtender = FLevelEditorModule::FLevelEditorMenuExtender::CreateUObject(this, &USyncViewportSubsystem::OnExtendLevelViewportOptionMenu);
	LevelEditorModule.GetAllLevelViewportOptionsMenuExtenders().Add(ViewportExtender);

	// Register our right click menu
	ExtendLevelEditorActorContextMenu();

	GEditor->OnPostEditorTick().AddUObject(this, &USyncViewportSubsystem::OnPostEditorTick);
	GEditor->OnLevelViewportClientListChanged().AddUObject(this, &USyncViewportSubsystem::OnLevelViewportClientListChanged);
	OnLevelViewportClientListChanged();

	FEditorDelegates::PreBeginPIE.AddUObject(this, &USyncViewportSubsystem::OnPrePIEBegin);
	FEditorDelegates::PostPIEStarted.AddUObject(this, &USyncViewportSubsystem::OnPIEPostStarted);
	FEditorDelegates::EndPIE.AddUObject(this, &USyncViewportSubsystem::OnPIEEnded);
}

void USyncViewportSubsystem::OnPostEditorTick(float DeltaTime)
{
	if(PIEWorldContext == nullptr)
	{
		// Exit we don't need to do any processing
		return;
	}
	
	const float FollowActorSmoothSpeed = GetDefault<UViewportSyncSettings>()->FollowActorSmoothSpeed;
	
	for(auto& ViewportInfo : ViewportInfos)
	{
		if(ViewportInfo.Value.bIsPIEViewport)
		{
			// We don't want to mess around with the active PIE viewport
			continue;
		}
		
		if(ViewportInfo.Value.bSync)
		{
			const AActor* FollowActor = GlobalFollowActorOverride.IsValid() ? GlobalFollowActorOverride.Get() : ViewportInfo.Value.FollowActor.Get();
			if(FollowActor != nullptr)
			{
				/*
				 * This could happen if the Actor we intended to follow wasn't first available when we hit play
				 * Such as a player pawn or some other object
				 */ 
				if(ViewportInfo.Key->bUsingOrbitCamera == false)
				{
					ApplyViewportFollowActor(ViewportInfo.Key, FollowActor);
				}
				/*
				 * To get the combination required to allow for orbiting and *not* allowing the user to control was getting really convoluted.
				 * Ultimately it meant setting the actor lock but I didn't like the "Controlling Actor" UI or behaviour
				 * 
				 * The result of this means each frame we update the look at location so the camera correctly updates to the follow actor's new location
				 */
				else
				{					
					const FVector ActorLocation = FollowActor->GetActorLocation();
					
					ViewportInfo.Key->SetViewLocationForOrbiting
					(
						FMath::VInterpConstantTo(ViewportInfo.Value.PreviousFollowLocation, ActorLocation, DeltaTime, FollowActorSmoothSpeed),
						(ViewportInfo.Key->GetLookAtLocation() - ViewportInfo.Key->GetViewLocation()).Size()
					);

					ViewportInfo.Value.PreviousFollowLocation = ActorLocation;
				}
			}
		}
	}
}

void USyncViewportSubsystem::Deinitialize()
{
	GEditor->OnPostEditorTick().RemoveAll(this);
	GEditor->OnLevelViewportClientListChanged().RemoveAll(this);

	FEditorDelegates::PreBeginPIE.RemoveAll(this);
	FEditorDelegates::PostPIEStarted.RemoveAll(this);
	FEditorDelegates::EndPIE.RemoveAll(this);

	if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(LevelEditorModuleName))
	{
		UnRegisterCommands(LevelEditorModule->GetGlobalLevelEditorActions());
	}
}

void USyncViewportSubsystem::SaveInformationForViewport(FLevelEditorViewportClient* ViewportClient, const FLiveViewportInfo& InfoToSave)
{
	// TODO: Saving	
}

void USyncViewportSubsystem::LoadInformationForViewport(FLevelEditorViewportClient* ViewportClient, FLiveViewportInfo& LoadedInfo)
{
	// TODO: Real Loading
	
	const UViewportSyncSettings* ViewportDefault = GetDefault<UViewportSyncSettings>();
	LoadedInfo = { false, ViewportDefault->bSyncByDefault, nullptr, FVector::ZeroVector };
}


void USyncViewportSubsystem::OnLevelViewportClientListChanged()
{
	const TArray<FLevelEditorViewportClient*> LevelViewportClients = GEditor->GetLevelViewportClients();

	// Remove viewports that don't exist anymore.
	for (auto It = ViewportInfos.CreateIterator(); It; ++It)
	{
		if (!LevelViewportClients.Contains(It.Key()))
		{
			SaveInformationForViewport(It.Key(), It.Value());

			ViewportInfos.Remove(It.Key());
		}
	}
	
	// Add recently added viewports that we don't know about yet.
	for (FLevelEditorViewportClient* LevelViewportClient : LevelViewportClients)
	{
		if (!ViewportInfos.Contains(LevelViewportClient))
		{
			// TODO: Load
			FLiveViewportInfo LoadedInfo;
			LoadInformationForViewport(LevelViewportClient, LoadedInfo);

			UE_LOG(LogTemp, Warning, TEXT("%s"), *LevelViewportClient->GetEditorViewportWidget()->ToString());
			
			ViewportInfos.Add(LevelViewportClient, MoveTemp(LoadedInfo));
		}
	}
}

void USyncViewportSubsystem::ApplyViewportSettings(FLevelEditorViewportClient* const Client, const FLiveViewportInfo& ViewportInfo)
{
	checkf(PIEWorldContext, TEXT("Tried to enable viewport settings but we're currently not in a PIE session"));

	// Don't apply our PIE viewport settings
	if(ViewportInfo.bIsPIEViewport)
	{
		return;
	}
	
	if (ViewportInfo.bSync)
	{
		ApplyViewportSync(Client);
	}
	
	if(ViewportInfo.FollowActor.IsValid())
	{
		ApplyViewportFollowActor(Client, ViewportInfo.FollowActor.Get());
	}
}

void USyncViewportSubsystem::RevertViewportSettings(FLevelEditorViewportClient* const Client, const FLiveViewportInfo& ViewportInfo)
{	
	if (ViewportInfo.bSync)
	{
		RevertViewportSync(Client);
	}

	if(!ViewportInfo.FollowActor.IsNull() || !GlobalFollowActorOverride.IsNull())
	{
		RevertViewportFollowActor(Client);
	}
}

//////////////////////////////////////////////
// Live Update
//////////////////////////////////////////////

void USyncViewportSubsystem::SetViewportSyncState(FLevelEditorViewportClient* const ViewportClient, bool bState)
{
	if(FLiveViewportInfo* ViewportInfo = ViewportInfos.Find(ViewportClient))
	{
		ViewportInfo->bSync = bState;
		if(PIEWorldContext != nullptr)
		{
			UE_LOG(LogViewportSync, Log, TEXT("Setting Viewport Live Updating State to %s"), bState ? TEXT("Enabled") : TEXT("Disabled"));
			
			if(bState)
			{
				// We enabled it so also enable tracking
				ApplyViewportSettings(ViewportClient, *ViewportInfo);
			}
			else
			{
				RevertViewportSync(ViewportClient);

				// Force it to redraw so we return back to normal and our viewport doesn't have game elements
				ViewportClient->Viewport->Invalidate();
			}
		}
	}
}

void USyncViewportSubsystem::ApplyViewportSync(FLevelEditorViewportClient* const ViewportClient)
{
	ViewportClient->SetReferenceToWorldContext(*PIEWorldContext);
#if ENGINE_MAJOR_VERSION <= 4 && ENGINE_MINOR_VERSION <= 24
	ViewportClient->SetRealtime(true, true);
#else
	ViewportClient->SetRealtimeOverride(true, LOCTEXT("ViewportSync", "Viewport Sync"));
#endif
}

void USyncViewportSubsystem::RevertViewportSync(FLevelEditorViewportClient* const ViewportClient)
{
	ViewportClient->SetReferenceToWorldContext(GEditor->GetEditorWorldContext());

#if ENGINE_MAJOR_VERSION <= 4 && ENGINE_MINOR_VERSION <= 24
	ViewportClient->RestoreRealtime(true);
#else
	ViewportClient->RemoveRealtimeOverride();
#endif
}

//////////////////////////////////////////////
// Follow
//////////////////////////////////////////////

void USyncViewportSubsystem::SetGlobalViewportFollowTargetOverride(AActor* FollowTarget)
{
	GlobalFollowActorOverride = FollowTarget;
}

void USyncViewportSubsystem::SetViewportFollowActor(FLevelEditorViewportClient* const ViewportClient, const AActor* Actor)
{
	if (FLiveViewportInfo* ViewportInfo = ViewportInfos.Find(ViewportClient))
	{
		ViewportInfo->FollowActor = Actor;

		UE_LOG(LogViewportSync, Log, TEXT("Set the follow actor to %s"), Actor != nullptr ? *Actor->GetActorLabel() : TEXT("None"));

		if (ViewportInfo->FollowActor.IsValid())
		{	
			// We check that the world is valid here because I don't want to take away user control while PIE is not active
			if (PIEWorldContext)
			{
				ApplyViewportFollowActor(ViewportClient, ViewportInfo->FollowActor.Get());
			}
		}
		else
		{
			RevertViewportFollowActor(ViewportClient);
		}
	}
}

void USyncViewportSubsystem::ApplyViewportFollowActor(FLevelEditorViewportClient* const ViewportClient, const AActor* Actor)
{
	// This will be modified during the camera lock and we want to restore it
	const FRotator ViewportViewPreLock	= ViewportClient->GetViewRotation();
	const FVector PreViewportLockLoc	= ViewportClient->GetViewLocation();
	
	ViewportClient->bUsingOrbitCamera = true;

	// This is done so we can set the DefaultOrbit<Type> values because otherwise when we try and lock the viewport client it assert will invalid transforms
	ViewportClient->SetCameraSetup(
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		FVector::ZeroVector,
		FVector::ZeroVector,
		FVector::ZeroVector,
		FRotator::ZeroRotator
	);
	
	if(!ViewportClient->IsCameraLocked())
	{
		// This is *actually* a toggle so only apply when we're not locked
		ViewportClient->SetCameraLock();
	}

	// Restore our values Pre-lock
	ViewportClient->SetViewRotation(ViewportViewPreLock);
	ViewportClient->SetViewLocation(PreViewportLockLoc);

	// Recalculate our view now so our viewport correctly updates
	ViewportClient->SetLookAtLocation(Actor->GetActorLocation(), true);
}

void USyncViewportSubsystem::RevertViewportFollowActor(FLevelEditorViewportClient* const ViewportClient)
{
	if(ViewportClient->IsCameraLocked())
	{
		// This is *actually* a toggle so only apply when we're locked and also turns off orbit camera
		ViewportClient->SetCameraLock();
	}
}

//////////////////////////////////////////////
// PIE Notifications
//////////////////////////////////////////////

void USyncViewportSubsystem::OnPrePIEBegin(const bool bIsSimulating)
{
	/*
	 * Find and mark our PIE Viewport
	 */

	
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(LevelEditorModuleName);
	
	TSharedPtr<SLevelViewport> ActiveLevelViewport = LevelEditorModule.GetFirstActiveLevelViewport();	
	TSharedPtr<FSceneViewport> SharedActiveViewport = ActiveLevelViewport->GetSharedActiveViewport();

	for (auto& ViewportInfo : ViewportInfos)
	{
		if(ViewportInfo.Key->Viewport == SharedActiveViewport.Get())
		{
			ViewportInfo.Value.bIsPIEViewport = true;
		}
	}
}


void USyncViewportSubsystem::OnPIEPostStarted(const bool bIsSimulating)
{
	PIEWorldContext = GEditor->GetPIEWorldContext();
	
	if(PIEWorldContext != nullptr)
	{		
		for(auto& ViewportInfo : ViewportInfos)
		{
			// We reset this so it resolves to the correct PIE instance
			ViewportInfo.Value.FollowActor.ResetWeakPtr();
			const_cast<FSoftObjectPath&>(ViewportInfo.Value.FollowActor.ToSoftObjectPath()).FixupForPIE(PIEWorldContext->PIEInstance);
			
			ApplyViewportSettings(ViewportInfo.Key, ViewportInfo.Value);
		}
	}
}

void USyncViewportSubsystem::OnPIEEnded(const bool bIsSimulating)
{
	PIEWorldContext = nullptr;

	for(auto& ViewportInfo : ViewportInfos)
	{
		RevertViewportSettings(ViewportInfo.Key, ViewportInfo.Value);

		ViewportInfo.Value.bIsPIEViewport = false;
	}

	// Clear our override so next PIE session they can choose if they want to override it again or not
	GlobalFollowActorOverride = nullptr;
}

//////////////////////////////////////////////
// Editor UI
//////////////////////////////////////////////

FLevelEditorViewportClient* USyncViewportSubsystem::GetActiveViewportClient()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(LevelEditorModuleName);
	TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetLevelEditorInstance().Pin();

	return static_cast<FLevelEditorViewportClient*>(&LevelEditor->GetActiveViewportInterface()->GetAssetViewportClient());
}

void USyncViewportSubsystem::RegisterCommands(TSharedRef<FUICommandList> CommandList)
{
	CommandList->MapAction(FViewportSyncEditorCommands::Get().ToggleViewportSync,
		FExecuteAction::CreateLambda([this]
		{
			if(FLevelEditorViewportClient* ViewportClient = GetActiveViewportClient())
			{
				SetViewportSyncState(ViewportClient, !IsViewportSyncing(ViewportClient));
			}		
		}),
		FCanExecuteAction::CreateLambda([]{ return true; }),
		FGetActionCheckState::CreateLambda([this]()
		{
			return IsViewportSyncing(GetActiveViewportClient()) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
	);

	CommandList->MapAction(FViewportSyncEditorCommands::Get().FollowActor,
		FExecuteAction::CreateLambda([this]
		{
			TArray<AActor*> SelectedActors;
			GEditor->GetSelectedActors()->GetSelectedObjects(SelectedActors);
			
			FLevelEditorViewportClient* ViewportClient = GetActiveViewportClient();
			
			if(SelectedActors.Num() > 0 && ViewportClient != nullptr)
			{
				SetViewportFollowActor(ViewportClient, SelectedActors[0]);
			}	
		})
	);
}

void USyncViewportSubsystem::UnRegisterCommands(TSharedRef<FUICommandList> CommandList)
{
	CommandList->UnmapAction(FViewportSyncEditorCommands::Get().ToggleViewportSync);
	CommandList->UnmapAction(FViewportSyncEditorCommands::Get().FollowActor);
}

TSharedRef<FExtender> USyncViewportSubsystem::OnExtendLevelViewportOptionMenu(const TSharedRef<FUICommandList> CommandList)
{
	FLevelEditorModule& LevelEditorModule			= FModuleManager::GetModuleChecked<FLevelEditorModule>(LevelEditorModuleName);
	TSharedPtr<ILevelEditor> LevelEditor			= LevelEditorModule.GetLevelEditorInstance().Pin();
	FLevelEditorViewportClient* ViewportClient		= static_cast<FLevelEditorViewportClient*>(&LevelEditor->GetActiveViewportInterface()->GetAssetViewportClient());

	static const FName ExtensionPoint("LevelViewportViewportOptions");
	
	TSharedRef<FExtender> Extender = MakeShared<FExtender>();
	Extender->AddMenuExtension(
		ExtensionPoint,
		EExtensionHook::After,
		nullptr,
		FMenuExtensionDelegate::CreateUObject(this, &USyncViewportSubsystem::BuildMenuListForViewport, ViewportClient)
	);
	return Extender;
}

void USyncViewportSubsystem::BuildMenuListForViewport(FMenuBuilder& MenuBuilder, FLevelEditorViewportClient* ViewportClient)
{	
	MenuBuilder.BeginSection(USyncViewportSubsystem::SectionExtensionPointName, USyncViewportSubsystem::SectionExtensionPointText);
	{		
		// Enable or Disable Viewport Syncing
		MenuBuilder.AddMenuEntry(FViewportSyncEditorCommands::Get().ToggleViewportSync);

		FUIAction FollowActorSubMenu;
		FollowActorSubMenu.CanExecuteAction.BindUObject(this, &USyncViewportSubsystem::IsViewportSyncing, ViewportClient);
		
		// Follow Actor SubMenu
		MenuBuilder.AddSubMenu(
			LOCTEXT("FollowActor", "Follow Actor"),
			LOCTEXT("FollowActorTooltip", "Select an actor for this Viewport to follow"),
			FNewMenuDelegate::CreateUObject(this, &USyncViewportSubsystem::CreateFollowActorMenuForViewport, ViewportClient),
			FollowActorSubMenu,
			USyncViewportSubsystem::FollowActorExtensionPointName,
			EUserInterfaceActionType::Button
		);

		BuildCurrentFollowActorWidgetForViewport(MenuBuilder, ViewportClient);
	}
	MenuBuilder.EndSection();
}

void USyncViewportSubsystem::CreateFollowActorMenuForViewport(FMenuBuilder& MenuBuilder, FLevelEditorViewportClient* ViewportClient)
{
	// Set up a menu entry to add the selected actor(s) to the sequencer
	TArray<AActor*> SelectedActors;
	GEditor->GetSelectedActors()->GetSelectedObjects(SelectedActors);

	AActor* SelectedActor = nullptr;

	FText SelectedActorDisplayName;
	FSlateIcon SelectedActorIcon;
	
	if (SelectedActors.Num() > 0)
	{
		SelectedActor = SelectedActors[0];
		
		SelectedActorDisplayName	= FText::Format(LOCTEXT("FollowSelectedActor", "Follow '{0}'"), FText::FromString(SelectedActor->GetHumanReadableName()));
		SelectedActorIcon			= FSlateIconFinder::FindIconForClass(SelectedActor->GetClass());
	}
	else
	{
		SelectedActorIcon = FSlateIconFinder::FindIconForClass(AActor::StaticClass());;
	}

	if (!SelectedActorDisplayName.IsEmpty())
	{
		MenuBuilder.AddMenuEntry(
			FViewportSyncEditorCommands::Get().FollowActor,
			NAME_None,
			SelectedActorDisplayName, 
			FText(), 
			SelectedActorIcon
		);
	}

	/* Scene outliner for picking a follow actor */
	MenuBuilder.BeginSection(USyncViewportSubsystem::SelectActorExtensionPointName, LOCTEXT("SelectFollowActor", "Select Actor to Follow:"));
	{
		using namespace SceneOutliner;

		FInitializationOptions InitOptions;
		{
			InitOptions.Mode = ESceneOutlinerMode::ActorPicker;

			// We hide the header row to keep the UI compact.
			InitOptions.bShowHeaderRow = false;
			InitOptions.bShowSearchBox = true;
			InitOptions.bShowCreateNewFolder = false;
			InitOptions.bFocusSearchBoxWhenOpened = true;

			// Only want the actor label column
			InitOptions.ColumnMap.Add(FBuiltInColumnTypes::Label(), FColumnInfo(EColumnVisibility::Visible, 0));
		}

		// actor selector to allow the user to choose an actor
		static const FName SceneOutlinerModuleName("SceneOutliner");
		
		FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>(SceneOutlinerModuleName);
		const TSharedRef< SWidget > MiniSceneOutliner =
			SNew(SBox)
			.MaxDesiredHeight(400.0f)
			.WidthOverride(300.0f)
			[
				SceneOutlinerModule.CreateSceneOutliner
				(
					InitOptions,
					FOnActorPicked::CreateLambda([this, ViewportClient](AActor* SelectedActor)
					{
						FSlateApplication::Get().DismissAllMenus();
						this->SetViewportFollowActor(ViewportClient, SelectedActor);
					})
				)
			];

		MenuBuilder.AddWidget(MiniSceneOutliner, FText::GetEmpty(), true);
	}
	
	MenuBuilder.EndSection();
}

void USyncViewportSubsystem::BuildCurrentFollowActorWidgetForViewport(FMenuBuilder& MenuBuilder, FLevelEditorViewportClient* ViewportClient)
{
	const FLiveViewportInfo* ViewportInfo = GetDataForViewport(ViewportClient);

	const auto ActorName = [ViewportInfo, &GlobalFollowActorOverride = GlobalFollowActorOverride]()
	{
		FString SelectedActorDetail;

		const TSoftObjectPtr<AActor> TargetActor = !GlobalFollowActorOverride.IsNull() ? GlobalFollowActorOverride : ViewportInfo->FollowActor;
		
		if (TargetActor.IsValid())
		{
			SelectedActorDetail = FString::Printf(TEXT("Following: '%s'"), *TargetActor->GetActorLabel());
		}
		else if (TargetActor.IsPending())
		{
			SelectedActorDetail = FString::Printf(TEXT("Waiting for: %s"), *TargetActor.ToSoftObjectPath().GetSubPathString());
		}
		else
		{
			SelectedActorDetail = TEXT("No follow Actor set");
		}

		return FText::Format(LOCTEXT("FollowSelectedActor", "{0} {1}"), (!GlobalFollowActorOverride.IsNull() ? FText::FromString("[Override]") : FText::GetEmpty()), FText::FromString(SelectedActorDetail));
	};
	
	MenuBuilder.AddWidget(
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Fill)
		[
			SNew(SButton)
				.HAlign(HAlign_Fill)
				.OnPressed_Lambda([FollowedActor = ViewportInfo->FollowActor]()
				{
					GEditor->SelectActor(FollowedActor.Get(), true, true);
				})
				.ForegroundColor(FSlateColor::UseForeground())
				.ButtonStyle(FEditorStyle::Get(), "NoBorder")
				.ContentPadding(0.0f)
				.Text_Lambda(ActorName)
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Right)
		[
				SNew(SButton)
				.OnPressed_Lambda([this, ViewportClient]()
				{
					this->SetViewportFollowActor(ViewportClient, nullptr);
				})
				.Visibility_Lambda([this, ViewportClient, ViewportInfo]()
				{
					if (IsViewportSyncing(ViewportClient) && !ViewportInfo->FollowActor.IsNull())
					{
						return EVisibility::Visible;
					}
					return EVisibility::Hidden;
				})
				.ForegroundColor(FSlateColor::UseForeground())
				.HAlign(HAlign_Fill)
				.ToolTipText(LOCTEXT("ClearFollow", "Remove followed Actor"))
				.ButtonStyle(FEditorStyle::Get(), "NoBorder")
				.Content()
				[			
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("PropertyWindow.Button_Clear"))
				]

		],
		FText::GetEmpty()
	);
}

void USyncViewportSubsystem::ExtendLevelEditorActorContextMenu()
{
	static const FName MenuName("LevelEditor.ActorContextMenu");
	
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(MenuName);
	FToolMenuSection& Section = Menu->AddSection(USyncViewportSubsystem::SectionExtensionPointName, USyncViewportSubsystem::SectionExtensionPointText);
	Section.AddDynamicEntry("ViewportSyncEditorCommands", FNewToolMenuSectionDelegate::CreateUObject(this, &USyncViewportSubsystem::OnExtendContextMenu));
}

void USyncViewportSubsystem::OnExtendContextMenu(FToolMenuSection& InSection)
{
	InSection.AddMenuEntry(FViewportSyncEditorCommands::Get().FollowActor);
}

#undef LOCTEXT_NAMESPACE