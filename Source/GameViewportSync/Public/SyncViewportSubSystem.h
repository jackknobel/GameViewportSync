// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "EditorSubsystem.h"
#include "SyncViewportSubsystem.generated.h"

class SWidget;
struct FWorldContext;
class FLevelEditorViewportClient;
class FViewportSyncEditorExtender;

/**
 * Subsystem for syncing the PIE world with other Level Editor Viewports
 */
UCLASS()
class GAMEVIEWPORTSYNC_API USyncViewportSubsystem final: public UEditorSubsystem
{
	GENERATED_BODY()

	USyncViewportSubsystem()
		: PIEWorldContext(nullptr)
		, GlobalFollowActorOverride(nullptr)
	{}

protected:
	
	TSharedPtr<FViewportSyncEditorExtender> EditorExtender;

protected:
	// Begin Subsystems override
	void Initialize(FSubsystemCollectionBase& Collection) override;
	void Deinitialize() override;
	// End Subsystems override

	void OnPostEditorTick(float DeltaTime);
	
	//////////////////////////////////////////////
	// Live Viewport
	//////////////////////////////////////////////
public:
	struct FLiveViewportInfo
	{
		// Is this the PIE viewport? If it is, we skip applying settings
		bool bIsPIEViewport;
		
		// Should this viewport be syncing with the PIE session
		bool bSync;

		// The actor the user wants this viewport to follow
		TSoftObjectPtr<AActor> FollowActor;

		// Used for smoothing the follow
		FVector PreviousFollowLocation;

	private:
		// Overlay Widget
		mutable TSharedPtr<SWidget> OverlayWidget;

	public:
		FLiveViewportInfo(bool bShouldSync, const TSoftObjectPtr<AActor>& ActorToFollow);
		
		TSharedRef<SWidget> GetOverlayWidget() const;
	};
	

protected:

	TMap<FLevelEditorViewportClient*, FLiveViewportInfo> ViewportInfos;
	
	// Only valid during PIE
	FWorldContext* PIEWorldContext;

	// Override to force all viewports to follow this actor
	TSoftObjectPtr<AActor> GlobalFollowActorOverride;
	
public:
	const FLiveViewportInfo* GetDataForViewport(FLevelEditorViewportClient* ViewportClient) const;

	void SaveInformationForViewport(FLevelEditorViewportClient* ViewportClient, const FLiveViewportInfo& InfoToSave);
	FLiveViewportInfo LoadInformationForViewport(FLevelEditorViewportClient* ViewportClient);
	
	void SetViewportSyncState(FLevelEditorViewportClient* ViewportClient, bool bState);
	bool IsViewportSyncing(FLevelEditorViewportClient* ViewportClient) const;

	void SetViewportFollowActor(FLevelEditorViewportClient* ViewportClient, const AActor* Actor);
	bool IsViewportFollowingActor(FLevelEditorViewportClient* ViewportClient, const AActor* Actor) const;

	/* Set the override for all viewports to follow */
	void SetGlobalViewportFollowTargetOverride(AActor* FollowTarget);
	
	/* Get the override for all viewports to follow */
	const TSoftObjectPtr<AActor>& GetGlobalViewportFollowTargetOverride() const;
	
protected:
	/* Called when there has been a change to the number of level viewports in the editor */
	void OnLevelViewportClientListChanged();

	void ApplyViewportSettings(FLevelEditorViewportClient* const Client, const FLiveViewportInfo& ViewportInfo);
	void RevertViewportSettings(FLevelEditorViewportClient* const Client, const FLiveViewportInfo& ViewportInfo);
	
	void ApplyViewportSync(FLevelEditorViewportClient* const ViewportClient);
	void RevertViewportSync(FLevelEditorViewportClient* const ViewportClient);

	void ApplyViewportFollowActor(FLevelEditorViewportClient* const ViewportClient, const AActor* Actor);
	void RevertViewportFollowActor(FLevelEditorViewportClient* const ViewportClient);
	
	// Begin PIE Callbacks
	void OnPrePIEBegin(const bool bIsSimulating);
	void OnPIEPostStarted(const bool bIsSimulating);
	void OnPIEEnded(const bool bIsSimulating);
	// End PIE Callbacks
};

// INLINES

inline const USyncViewportSubsystem::FLiveViewportInfo* USyncViewportSubsystem::GetDataForViewport(FLevelEditorViewportClient* ViewportClient) const
{
	return ViewportInfos.Find(ViewportClient);
}

inline bool USyncViewportSubsystem::IsViewportSyncing(FLevelEditorViewportClient* ViewportClient) const
{
	if(const FLiveViewportInfo* ViewportInfo = ViewportInfos.Find(ViewportClient))
	{
		return ViewportInfo->bSync;
	}
	return false;
}

inline bool USyncViewportSubsystem::IsViewportFollowingActor(FLevelEditorViewportClient* ViewportClient, const AActor* Actor) const
{
	if(const FLiveViewportInfo* ViewportInfo = ViewportInfos.Find(ViewportClient))
	{
		return ViewportInfo->FollowActor == Actor;
	}
	return false;
}

inline const TSoftObjectPtr<AActor>& USyncViewportSubsystem::GetGlobalViewportFollowTargetOverride() const
{
	return GlobalFollowActorOverride;
}