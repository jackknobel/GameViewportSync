// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "EditorSubsystem.h"
#include "SyncViewportSubsystem.generated.h"

/**
 * Subsystem for syncing the PIE world with other Level Editor Viewports
 */
UCLASS()
class GAMEVIEWPORTSYNC_API USyncViewportSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

	USyncViewportSubsystem()
		: PIEWorldContext(nullptr)
		, GlobalFollowActorOverride(nullptr)
	{}

protected:
	// Begin Subsystems override
	void Initialize(FSubsystemCollectionBase& Collection) override;
	void Deinitialize() override;
	// End Subsystems override

	void OnPostEditorTick(float DeltaTime);
	
	//////////////////////////////////////////////
	// Live Viewport
	//////////////////////////////////////////////
protected:
	// Only valid during PIE
	FWorldContext* PIEWorldContext;

	// Override to force all viewports to follow this actor
	TSoftObjectPtr<AActor> GlobalFollowActorOverride;

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
	TMap<FLevelEditorViewportClient*, FLiveViewportInfo> ViewportInfos;
	
public:
	const FLiveViewportInfo* GetDataForViewport(FLevelEditorViewportClient* ViewportClient) const;

	void SaveInformationForViewport(FLevelEditorViewportClient* ViewportClient, const FLiveViewportInfo& InfoToSave);
	FLiveViewportInfo LoadInformationForViewport(FLevelEditorViewportClient* ViewportClient);
	
	virtual void SetViewportSyncState(FLevelEditorViewportClient* ViewportClient, bool bState);
	virtual bool IsViewportSyncing(FLevelEditorViewportClient* ViewportClient) const;

	virtual void SetViewportFollowActor(FLevelEditorViewportClient* ViewportClient, const AActor* Actor);
	virtual bool IsViewportFollowingActor(FLevelEditorViewportClient* ViewportClient, const AActor* Actor) const;

	/* Set the override for all viewports to follow */
	void SetGlobalViewportFollowTargetOverride(AActor* FollowTarget);
	
	/* Get the override for all viewports to follow */
	const TSoftObjectPtr<AActor>& GetGlobalViewportFollowTargetOverride() const;
	
protected:
	/* Called when there has been a change to the number of level viewports in the editor */
	virtual void OnLevelViewportClientListChanged();

	virtual void ApplyViewportSettings(FLevelEditorViewportClient* const Client, const FLiveViewportInfo& ViewportInfo);
	virtual void RevertViewportSettings(FLevelEditorViewportClient* const Client, const FLiveViewportInfo& ViewportInfo);
	
	void ApplyViewportSync(FLevelEditorViewportClient* const ViewportClient);
	void RevertViewportSync(FLevelEditorViewportClient* const ViewportClient);

	void ApplyViewportFollowActor(FLevelEditorViewportClient* const ViewportClient, const AActor* Actor);
	void RevertViewportFollowActor(FLevelEditorViewportClient* const ViewportClient);
	
	// Begin PIE Callbacks
	void OnPrePIEBegin(const bool bIsSimulating);
	void OnPIEPostStarted(const bool bIsSimulating);
	void OnPIEEnded(const bool bIsSimulating);
	// End PIE Callbacks

	//////////////////////////////////////////////
	// Editor Extension
	//////////////////////////////////////////////
	
public:

	static const FText SectionExtensionPointText;
	
	/* Extension point names */
	static const FName SectionExtensionPointName;
	static const FName FollowActorExtensionPointName;
	static const FName SelectActorExtensionPointName;

	void RegisterCommands(TSharedRef<FUICommandList> CommandList);
	void UnRegisterCommands(TSharedRef<FUICommandList> CommandList);

	static FLevelEditorViewportClient* GetActiveViewportClient();
	
	//////////////////////////////////////////////
	// Viewport Extending
	//////////////////////////////////////////////
protected:	
	virtual TSharedRef<FExtender> OnExtendLevelViewportOptionMenu(const TSharedRef<FUICommandList> CommandList);
	
	void BuildMenuListForViewport(FMenuBuilder& MenuBuilder, FLevelEditorViewportClient* ViewportClient);
	void CreateFollowActorMenuForViewport(FMenuBuilder& MenuBuilder, FLevelEditorViewportClient* ViewportClient);
	void BuildCurrentFollowActorWidgetForViewport(FMenuBuilder& MenuBuilder, FLevelEditorViewportClient* ViewportClient);

	//////////////////////////////////////////////
	// Context Menu Extending
	//////////////////////////////////////////////
protected:
	void ExtendLevelEditorActorContextMenu();
	
	virtual void OnExtendContextMenu(FToolMenuSection& InSection);
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