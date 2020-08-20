// Fill out your copyright notice in the Description page of Project Settings.

#include "SyncViewportSubsystem.h"
#include "ViewportSyncSettings.h"
#include "ViewportSyncEditorExtender.h"

// UE Includes
#include "Editor.h"
#include "IAssetViewport.h"
#include "LevelEditor.h"
#include "LevelEditorViewport.h"
#include "Engine/Selection.h"
#include "SEditorViewport.h"
#include "SLevelViewport.h"
#include "ToolMenus.h"
#include "Slate/SceneViewport.h"

#define LOCTEXT_NAMESPACE "ViewportSync"

DEFINE_LOG_CATEGORY_STATIC(LogViewportSync, Log, All);

void USyncViewportSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	GEditor->OnLevelViewportClientListChanged().AddUObject(this, &USyncViewportSubsystem::OnLevelViewportClientListChanged);
	OnLevelViewportClientListChanged();

	FEditorDelegates::PreBeginPIE.AddUObject(this, &USyncViewportSubsystem::OnPrePIEBegin);
	FEditorDelegates::PostPIEStarted.AddUObject(this, &USyncViewportSubsystem::OnPIEPostStarted);
	FEditorDelegates::EndPIE.AddUObject(this, &USyncViewportSubsystem::OnPIEEnded);

	EditorExtender = MakeShared<FViewportSyncEditorExtender>(*this);
}

void USyncViewportSubsystem::OnPostEditorTick(float DeltaTime)
{
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
	EditorExtender.Reset();
	
	GEditor->OnPostEditorTick().RemoveAll(this);
	GEditor->OnLevelViewportClientListChanged().RemoveAll(this);

	FEditorDelegates::PreBeginPIE.RemoveAll(this);
	FEditorDelegates::PostPIEStarted.RemoveAll(this);
	FEditorDelegates::EndPIE.RemoveAll(this);
}

void USyncViewportSubsystem::SaveInformationForViewport(FLevelEditorViewportClient* ViewportClient, const FLiveViewportInfo& InfoToSave)
{
	// TODO: Saving	
}

USyncViewportSubsystem::FLiveViewportInfo USyncViewportSubsystem::LoadInformationForViewport(FLevelEditorViewportClient* ViewportClient)
{
	// TODO: Real Loading
	
	const UViewportSyncSettings* ViewportDefault = GetDefault<UViewportSyncSettings>();
	return { ViewportDefault->bSyncByDefault, nullptr };
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

			RevertViewportSettings(It.Key(), It.Value());

			ViewportInfos.Remove(It.Key());
		}
	}
	
	// Add recently added viewports that we don't know about yet.
	for (FLevelEditorViewportClient* LevelViewportClient : LevelViewportClients)
	{
		if (!ViewportInfos.Contains(LevelViewportClient))
		{
			// TODO: Load
			FLiveViewportInfo LoadedInfo = LoadInformationForViewport(LevelViewportClient);
			
			ViewportInfos.Emplace(LevelViewportClient, MoveTemp(LoadedInfo));

			// We're playing, update all settings
			if(PIEWorldContext != nullptr)
			{
				ApplyViewportSettings(LevelViewportClient, ViewportInfos.FindChecked(LevelViewportClient));
			}
		}
	}
}

USyncViewportSubsystem::FLiveViewportInfo::FLiveViewportInfo(bool bShouldSync, const TSoftObjectPtr<AActor>& ActorToFollow)
	: bIsPIEViewport(false)
	, bSync(bShouldSync)
	, FollowActor(ActorToFollow)
	, PreviousFollowLocation(FVector::ZeroVector)
{}

TSharedRef<SWidget> USyncViewportSubsystem::FLiveViewportInfo::GetOverlayWidget() const
{
	// Create on demand
	if(!OverlayWidget.IsValid())
	{
		const USyncViewportSubsystem* ViewportSyncSubSystem = GEditor->GetEditorSubsystem<USyncViewportSubsystem>();
			
		SAssignNew(OverlayWidget, SVerticalBox)
		.Visibility_Lambda([]
		{
			if (GetDefault<UViewportSyncSettings>()->bShowOverlay)
			{
				return EVisibility::HitTestInvisible;
			}
			return EVisibility::Hidden;

		})
		+ SVerticalBox::Slot()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Center)
		.AutoHeight()
		.Padding(2.0f, 1.0f, 2.0f, 1.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SyncingViewportLablel", "Syncing Viewport"))
			.Font(FEditorStyle::GetFontStyle(TEXT("MenuItem.Font")))
			.ColorAndOpacity(FLinearColor(0.4f, 1.0f, 1.0f))
			.ShadowOffset(FVector2D(1, 1))
		]
		+ SVerticalBox::Slot()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Center)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([this, ViewportSyncSubSystem]
			{
				const TSoftObjectPtr<AActor> FollowActorOverride = ViewportSyncSubSystem->GetGlobalViewportFollowTargetOverride();
				const TSoftObjectPtr<AActor> TargetActor = !FollowActorOverride.IsNull() ? FollowActorOverride : FollowActor;

				if (TargetActor.IsValid() || TargetActor.IsPending())
				{
					return EVisibility::HitTestInvisible;
				}
				return EVisibility::Hidden;
			})
			+ SHorizontalBox::Slot()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("FollowingActor", "Following Actor:"))
				.Font(FEditorStyle::GetFontStyle(TEXT("MenuItem.Font")))
				.ShadowOffset(FVector2D(1, 1))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.0f, 1.0f, 2.0f, 1.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([this, ViewportSyncSubSystem]
				{
					const TSoftObjectPtr<AActor> FollowActorOverride = ViewportSyncSubSystem->GetGlobalViewportFollowTargetOverride();
					const TSoftObjectPtr<AActor> TargetActor = !FollowActorOverride.IsNull() ? FollowActorOverride : FollowActor;
					
					FString FollowActorName;

					if (TargetActor.IsValid())
					{
						FollowActorName = FString::Printf(TEXT("Following: '%s'"), *TargetActor->GetActorLabel());
					}
					else if (TargetActor.IsPending())
					{
						FollowActorName = FString::Printf(TEXT("Waiting for: %s"), *TargetActor.ToSoftObjectPath().GetSubPathString());
					}
					else
					{
						FollowActorName = TEXT("No follow Actor set");
					}

					return FText::Format(LOCTEXT("FollowSelectedActor", "{0} {1}"),
					(
							!ViewportSyncSubSystem->GetGlobalViewportFollowTargetOverride().IsNull() ? FText::FromString("[Override]") : FText::GetEmpty())
							, FText::FromString(FollowActorName)
					);
				})
				.Font(FEditorStyle::GetFontStyle(TEXT("MenuItem.Font")))
						.ColorAndOpacity(FLinearColor(0.4f, 1.0f, 1.0f))
						.ShadowOffset(FVector2D(1, 1))
			]
		];
	}

	return OverlayWidget.ToSharedRef();
}

//////////////////////////////////////////////
// PIE Notifications
//////////////////////////////////////////////

void USyncViewportSubsystem::OnPrePIEBegin(const bool bIsSimulating)
{
	/*
	 * Find and mark our PIE Viewport
	 */

	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(FViewportSyncEditorExtender::LevelEditorModuleName);
	
	const TSharedPtr<SLevelViewport> ActiveLevelViewport = LevelEditorModule.GetFirstActiveLevelViewport();	
	const TSharedPtr<FSceneViewport> SharedActiveViewport = ActiveLevelViewport->GetSharedActiveViewport();

	for (auto& ViewportInfo : ViewportInfos)
	{
		if(ViewportInfo.Key->Viewport == SharedActiveViewport.Get())
		{
			ViewportInfo.Value.bIsPIEViewport = true;
			break;
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
			if (!ViewportInfo.Value.FollowActor.IsNull())
			{
				// We reset this so it resolves to the correct PIE instance
				ViewportInfo.Value.FollowActor.ResetWeakPtr();
				const_cast<FSoftObjectPath&>(ViewportInfo.Value.FollowActor.ToSoftObjectPath()).FixupForPIE(PIEWorldContext->PIEInstance);
			}
		
			ApplyViewportSettings(ViewportInfo.Key, ViewportInfo.Value);
		}
	}

	GEditor->OnPostEditorTick().AddUObject(this, &USyncViewportSubsystem::OnPostEditorTick);
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

	GEditor->OnPostEditorTick().RemoveAll(this);
}

void USyncViewportSubsystem::ApplyViewportSettings(FLevelEditorViewportClient* const Client, const FLiveViewportInfo& ViewportInfo)
{
	checkf(PIEWorldContext, TEXT("Tried to enable viewport settings but we're currently not in a PIE session"));

	// Don't apply our PIE viewport settings
	if(ViewportInfo.bIsPIEViewport)
	{
		return;
	}

	if(GetDefault<UViewportSyncSettings>()->bShowOverlay)
	{
		TSharedPtr<SLevelViewport> Viewport = StaticCastSharedPtr<SLevelViewport>(Client->GetEditorViewportWidget());
		if(Viewport.IsValid())
		{
			Viewport->AddOverlayWidget(ViewportInfo.GetOverlayWidget());
		}
		else
		{
			UE_LOG(LogViewportSync, Warning, TEXT("Failed to add viewport overlay"));
		}
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
	TSharedPtr<SLevelViewport> Viewport = StaticCastSharedPtr<SLevelViewport>(Client->GetEditorViewportWidget());
	if(Viewport.IsValid())
	{
		Viewport->RemoveOverlayWidget(ViewportInfo.GetOverlayWidget());
	}
	
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

#undef LOCTEXT_NAMESPACE