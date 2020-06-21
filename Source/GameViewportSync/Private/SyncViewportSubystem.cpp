// Fill out your copyright notice in the Description page of Project Settings.

#include "SyncViewportSubsystem.h"
#include "ViewportSyncSettings.h"

// UE Includes
#include "Editor.h"
#include "EditorSupportDelegates.h"
#include "IAssetViewport.h"
#include "LevelEditor.h"
#include "LevelEditorViewport.h"
#include "SceneOutlinerModule.h"
#include "Engine/Selection.h"
#include "Styling/SlateIconFinder.h"
#include "SceneOutlinerPublicTypes.h"
#include "SEditorViewport.h"

#define LOCTEXT_NAMESPACE "SyncViewportSubsystem"

static const FName LevelEditorModuleName("LevelEditor");

const FName USyncViewportSubsystem::SectionExtensionPointName("ViewportSync");
const FName USyncViewportSubsystem::FollowActorExtensionPointName("ViewportSync_FollowActor");
const FName USyncViewportSubsystem::SelectActorExtensionPointName("ViewportSync_SelectActor");

void USyncViewportSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{	
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(LevelEditorModuleName);

	FLevelEditorModule::FLevelEditorMenuExtender Extender = FLevelEditorModule::FLevelEditorMenuExtender::CreateUObject(this, &USyncViewportSubsystem::OnExtendLevelViewportOptionMenu);
	LevelEditorModule.GetAllLevelViewportOptionsMenuExtenders().Add(Extender);

	GEditor->OnLevelViewportClientListChanged().AddUObject(this, &USyncViewportSubsystem::OnLevelViewportClientListChanged);
	OnLevelViewportClientListChanged();
	
	FEditorDelegates::PostPIEStarted.AddUObject(this, &USyncViewportSubsystem::OnPIEPostStarted);
	FEditorDelegates::EndPIE.AddUObject(this, &USyncViewportSubsystem::OnPIEEnded);

	GEditor->OnPostEditorTick().AddUObject(this, &USyncViewportSubsystem::OnPostEditorTick);
}

void USyncViewportSubsystem::OnPostEditorTick(float DeltaTime)
{
	if(PIEWorldContext == nullptr)
	{
		// Exit we don't need to do any processing
		return;
	}
	
	const UViewportSyncSettings* ViewportDefault = GetDefault<UViewportSyncSettings>();
	const float FollowActorSmoothSpeed = ViewportDefault->FollowActorSmoothSpeed;
	
	for(auto& ViewportInfo : ViewportInfos)
	{
		if(ViewportInfo.Value.bLiveUpdate)
		{
			AActor* FollowActor = ViewportInfo.Value.FollowActor.Get();
			if(FollowActor != nullptr)
			{
				/*
				 * This could happen if the Actor we intended to follow wasn't first available when we hit play
				 * Such as a player pawn or some other object
				 */ 
				if(ViewportInfo.Key->bUsingOrbitCamera == false)
				{
					ApplyViewportLockActor(ViewportInfo.Key, FollowActor);
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
	
	FEditorDelegates::PostPIEStarted.RemoveAll(this);
	FEditorDelegates::EndPIE.RemoveAll(this);
}

void USyncViewportSubsystem::SaveInformationForViewport(FLevelEditorViewportClient* ViewportClient, const FLiveViewportInfo& InfoToSave)
{
	// TODO: Saving	
}

void USyncViewportSubsystem::LoadInformationForViewport(FLevelEditorViewportClient* ViewportClient, FLiveViewportInfo& LoadedInfo)
{
	// TODO: Real Loading
	
	const UViewportSyncSettings* ViewportDefault = GetDefault<UViewportSyncSettings>();
	LoadedInfo = { ViewportDefault->bSyncByDefault, nullptr, FVector::ZeroVector };
}


void USyncViewportSubsystem::OnLevelViewportClientListChanged()
{
	const TArray<FLevelEditorViewportClient*> LevelViewportClients = GEditor->GetLevelViewportClients();

	const FString& IniSection = FLayoutSaveRestore::GetAdditionalLayoutConfigIni();
	
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
			
			ViewportInfos.Add(LevelViewportClient, MoveTemp(LoadedInfo));
		}
	}
}

void USyncViewportSubsystem::ApplyViewportSettings(FLevelEditorViewportClient* const Client, const FLiveViewportInfo& ViewportInfo)
{
	checkf(PIEWorldContext, TEXT("Tried to enable viewport settings but we're currently not in a PIE session"));

	// Don't apply our PIE viewport settings
	if(GEditor->GetPIEViewport() == Client->Viewport )
	{
		return;
	}
	
	if (ViewportInfo.bLiveUpdate)
	{
		ApplyViewportLiveUpdate(Client);
	}
	
	if(ViewportInfo.FollowActor.IsValid())
	{
		ApplyViewportLockActor(Client, ViewportInfo.FollowActor.Get());
	}
}

void USyncViewportSubsystem::RevertViewportSettings(FLevelEditorViewportClient* const Client, const FLiveViewportInfo& ViewportInfo)
{	
	if (ViewportInfo.bLiveUpdate)
	{
		RevertViewportLiveUpdate(Client);
	}

	if (ViewportInfo.FollowActor.IsValid())
	{
		RevertViewportLockedActor(Client);
	}
}

//////////////////////////////////////////////
// Live Update
//////////////////////////////////////////////

void USyncViewportSubsystem::SetViewportLiveTrackState(FLevelEditorViewportClient* const ViewportClient, bool bState)
{
	if(FLiveViewportInfo* ViewportInfo = ViewportInfos.Find(ViewportClient))
	{
		ViewportInfo->bLiveUpdate = bState;
		if(PIEWorldContext != nullptr)
		{
			if(bState)
			{
				// We enabled it so also enable tracking
				ApplyViewportSettings(ViewportClient, *ViewportInfo);
			}
			else
			{
				RevertViewportLiveUpdate(ViewportClient);

				// Force it to redraw so we return back to normal and our viewport doesn't have game elements
				ViewportClient->Viewport->Invalidate();
			}
		}
	}
}

void USyncViewportSubsystem::ApplyViewportLiveUpdate(FLevelEditorViewportClient* const ViewportClient)
{
	ViewportClient->SetReferenceToWorldContext(*PIEWorldContext);
#if ENGINE_MAJOR_VERSION <= 4 && ENGINE_MINOR_VERSION <= 24
	ViewportClient->SetRealtime(true, true);
#else
	ViewportClient->SetRealtimeOverride(true, LOCTEXT("ViewportSync", "Viewport Sync"));
#endif
}

void USyncViewportSubsystem::RevertViewportLiveUpdate(FLevelEditorViewportClient* const ViewportClient)
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

void USyncViewportSubsystem::SetViewportFollowActor(FLevelEditorViewportClient* const ViewportClient, AActor* Actor)
{
	if (FLiveViewportInfo* ViewportInfo = ViewportInfos.Find(ViewportClient))
	{
		ViewportInfo->FollowActor = Actor;
	
		if (ViewportInfo->FollowActor.IsValid())
		{
			// We check that the world is valid here because i don't want to take away user control while PIE is not active
			if (PIEWorldContext)
			{
				ApplyViewportLockActor(ViewportClient, ViewportInfo->FollowActor.Get());
			}
		}
		else
		{
			RevertViewportLockedActor(ViewportClient);
		}
	}
}

void USyncViewportSubsystem::ApplyViewportLockActor(FLevelEditorViewportClient* const ViewportClient, AActor* Actor)
{
	// This will be modified during the camera lock and we want to restore it
	const FRotator ViewportViewPreLock = ViewportClient->GetViewRotation();
	const FVector PreViewportLockLoc = ViewportClient->GetViewLocation();
	
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

void USyncViewportSubsystem::RevertViewportLockedActor(FLevelEditorViewportClient* const ViewportClient)
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

void USyncViewportSubsystem::OnPIEPostStarted(const bool bIsSimulating)
{
	PIEWorldContext = GEditor->GetPIEWorldContext();
	
	if(PIEWorldContext != nullptr)
	{		
		for(const auto& ViewportInfo : ViewportInfos)
		{
			ApplyViewportSettings(ViewportInfo.Key, ViewportInfo.Value);
		}
	}
}

void USyncViewportSubsystem::OnPIEEnded(const bool bIsSimulating)
{
	PIEWorldContext = nullptr;
	
	const TArray<class FLevelEditorViewportClient*>& ViewportClients = GEditor->GetLevelViewportClients();

	for(const auto& ViewportInfo : ViewportInfos)
	{
		RevertViewportSettings(ViewportInfo.Key, ViewportInfo.Value);
	}
}


//////////////////////////////////////////////
// Editor UI
//////////////////////////////////////////////

TSharedRef<FExtender> USyncViewportSubsystem::OnExtendLevelViewportOptionMenu(const TSharedRef<FUICommandList> CommandList)
{
	FLevelEditorModule& LevelEditorModule			= FModuleManager::LoadModuleChecked<FLevelEditorModule>(LevelEditorModuleName);
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
	MenuBuilder.BeginSection(SectionExtensionPointName, LOCTEXT("ViewportSync", "Viewport Sync"));
	{		
		FUIAction ToggleLivePIEView;
		ToggleLivePIEView.ExecuteAction.BindLambda([this, ViewportClient]()
		{
			SetViewportLiveTrackState(ViewportClient, !IsViewportTrackingLiveState(ViewportClient));
		});
		
		ToggleLivePIEView.GetActionCheckState.BindLambda([this, ViewportClient]()
		{
			return IsViewportTrackingLiveState(ViewportClient) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		});

		// Enable or Disable Viewport Syncing
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ViewportSyncEnable", "Enable"),
			LOCTEXT("ViewportSyncEnableTooltip", "If enabled, this viewport will track the primary PIE session"),
			FSlateIcon(),
			ToggleLivePIEView,
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);

		FUIAction FollowActorSubMenu;
		FollowActorSubMenu.CanExecuteAction.BindUObject(this, &USyncViewportSubsystem::IsViewportTrackingLiveState, ViewportClient);
		
		// Follow Actor SubMenu
		MenuBuilder.AddSubMenu(
			LOCTEXT("FollowActor", "Follow Actor"),
			LOCTEXT("FollowActorTooltip", "Select an actor for this Viewport to follow"),
			FNewMenuDelegate::CreateUObject(this, &USyncViewportSubsystem::CreateFollowActorMenuForViewport, ViewportClient),
			FollowActorSubMenu,
			FollowActorExtensionPointName,
			EUserInterfaceActionType::Button
		);

		BuildCurrentFollowActorWidgetForViewport(MenuBuilder, ViewportClient);
	}
	MenuBuilder.EndSection();
}

struct FollowActorDetails
{
	FText DisplayName;
	FSlateIcon Icon;	
};

void USyncViewportSubsystem::CreateFollowActorMenuForViewport(FMenuBuilder& MenuBuilder, FLevelEditorViewportClient* ViewportClient)
{
	// Set up a menu entry to add the selected actor(s) to the sequencer
	TArray<AActor*> SelectedActors;
	GEditor->GetSelectedActors()->GetSelectedObjects(SelectedActors);

	AActor* SelectedActor = nullptr;

	FollowActorDetails ActorDetails;
	
	if (SelectedActors.Num() > 0)
	{
		SelectedActor = SelectedActors[0];
		
		ActorDetails.DisplayName	= FText::Format(LOCTEXT("FollowSelectedActor", "Follow '{0}'"), FText::FromString(SelectedActor->GetActorLabel()));
		ActorDetails.Icon			= FSlateIconFinder::FindIconForClass(SelectedActor->GetClass());
	}
	else
	{
		ActorDetails.Icon = FSlateIconFinder::FindIconForClass(AActor::StaticClass());;
	}

	// Select Actor Lambda we use both in the "select selected actor" menu and in the scene outliner selector below
	auto OnSelectedActor = [this, ViewportClient](AActor* SelectedActor)
	{
		this->SetViewportFollowActor(ViewportClient, SelectedActor);
	};

	if (!ActorDetails.DisplayName.IsEmpty())
	{
		MenuBuilder.AddMenuEntry
		(
			ActorDetails.DisplayName, 
			FText(), 
			ActorDetails.Icon, 
			FExecuteAction::CreateLambda(OnSelectedActor, SelectedActor)
		);
	}

	/* Scene outliner for picking a follow actor */
	MenuBuilder.BeginSection(SelectActorExtensionPointName, LOCTEXT("SelectFollowActor", "Select Actor to Follow:"));
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
					FOnActorPicked::CreateLambda(OnSelectedActor)
				)
			];

		MenuBuilder.AddWidget(MiniSceneOutliner, FText::GetEmpty(), true);
	}
	
	MenuBuilder.EndSection();
}

void USyncViewportSubsystem::BuildCurrentFollowActorWidgetForViewport(FMenuBuilder& MenuBuilder, FLevelEditorViewportClient* ViewportClient)
{
	const FLiveViewportInfo* ViewportInfo = GetDataForViewport(ViewportClient);

	// Gather the details we need for the actor
	const FollowActorDetails ActorDetails = [ViewportInfo]() -> FollowActorDetails
	{
		FString SelectedActorDetail;
		FSlateIcon ActorIcon;

		if (ViewportInfo->FollowActor.IsValid())
		{
			SelectedActorDetail = FString::Printf(TEXT("Following: '%s'"), *ViewportInfo->FollowActor->GetActorLabel());
			ActorIcon = FSlateIconFinder::FindIconForClass(ViewportInfo->FollowActor->GetClass());
		}
		else if (ViewportInfo->FollowActor.IsPending())
		{
			SelectedActorDetail = FString::Printf(TEXT("Waiting for: %s"), *ViewportInfo->FollowActor.ToSoftObjectPath().GetSubPathString());
			ActorIcon = FSlateIconFinder::FindIconForClass(AActor::StaticClass());
		}
		else
		{
			SelectedActorDetail = TEXT("No follow Actor set");
			ActorIcon = FSlateIconFinder::FindIconForClass(AActor::StaticClass());
		}
		return { FText::Format(LOCTEXT("FollowSelectedActor", "{0}"), FText::FromString(SelectedActorDetail)) };
	}();
	
	auto OnPressed = [FollowedActor = ViewportInfo->FollowActor]()
	{
		GEditor->SelectActor(FollowedActor.Get(), true, true);
	};

	auto GetVisibility = [this, ViewportClient, ViewportInfo]()
	{
		if (IsViewportTrackingLiveState(ViewportClient) && !ViewportInfo->FollowActor.IsNull())
		{
			return EVisibility::Visible;
		}
		return EVisibility::Hidden;
	};

	auto ClearFollowActor = [this, ViewportClient]()
	{
		this->SetViewportFollowActor(ViewportClient, nullptr);
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
				.OnPressed_Lambda(OnPressed)
				.ForegroundColor(FSlateColor::UseForeground())
				.ButtonStyle(FEditorStyle::Get(), "NoBorder")
				.ContentPadding(0.0f)
				.Text(ActorDetails.DisplayName)
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Right)
		[
				SNew(SButton)
				.OnPressed_Lambda(ClearFollowActor)
				.Visibility_Lambda(GetVisibility)
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

#undef LOCTEXT_NAMESPACE