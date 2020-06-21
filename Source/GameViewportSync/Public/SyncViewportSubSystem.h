// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "EditorSubsystem.h"
#include "SyncViewportSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class GAMEVIEWPORTSYNC_API USyncViewportSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

	USyncViewportSubsystem()
		: PIEWorldContext(nullptr)
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

	struct FLiveViewportInfo
	{
		// Should we be live updating this viewport 
		bool bLiveUpdate;

		// The actor the user wants this viewport to follow
		TSoftObjectPtr<AActor> FollowActor;

		// Used for smoothing the follow
		FVector PreviousFollowLocation;
	};
	TMap<FLevelEditorViewportClient*, FLiveViewportInfo> ViewportInfos;
	
public:
	const FLiveViewportInfo* GetDataForViewport(FLevelEditorViewportClient* ViewportClient) const;

	void SaveInformationForViewport(FLevelEditorViewportClient* ViewportClient, const FLiveViewportInfo& InfoToSave);
	void LoadInformationForViewport(FLevelEditorViewportClient* ViewportClient, FLiveViewportInfo& LoadedInfo);
	
	virtual void SetViewportLiveTrackState(FLevelEditorViewportClient* ViewportClient, bool bState);
	virtual bool IsViewportTrackingLiveState(FLevelEditorViewportClient* ViewportClient) const;

	virtual void SetViewportFollowActor(FLevelEditorViewportClient* ViewportClient, AActor* Actor);
	virtual bool IsViewportFollowingActor(FLevelEditorViewportClient* ViewportClient, AActor* Actor) const;
	
protected:
	/* Called when there has been a change to the number of level viewports in the editor */
	virtual void OnLevelViewportClientListChanged();

	virtual void ApplyViewportSettings(FLevelEditorViewportClient* const Client, const FLiveViewportInfo& ViewportInfo);
	virtual void RevertViewportSettings(FLevelEditorViewportClient* const Client, const FLiveViewportInfo& ViewportInfo);
	
	void ApplyViewportLiveUpdate(FLevelEditorViewportClient* const ViewportClient);
	void RevertViewportLiveUpdate(FLevelEditorViewportClient* const ViewportClient);

	void ApplyViewportLockActor(FLevelEditorViewportClient* const ViewportClient, AActor* Actor);
	void RevertViewportLockedActor(FLevelEditorViewportClient* const ViewportClient);
	
	// Begin PIE Callbacks
	void OnPIEPostStarted(const bool bIsSimulating);
	void OnPIEEnded(const bool bIsSimulating);
	// End PIE Callbacks

	//////////////////////////////////////////////
	// Editor Extension
	//////////////////////////////////////////////
	
public:
	/* Extension point names */
	static const FName SectionExtensionPointName;
	static const FName FollowActorExtensionPointName;
	static const FName SelectActorExtensionPointName;

protected:
	
	virtual TSharedRef<FExtender> OnExtendLevelViewportOptionMenu(const TSharedRef<FUICommandList> CommandList);
	
	void BuildMenuListForViewport(FMenuBuilder& MenuBuilder, FLevelEditorViewportClient* ViewportClient);
	void CreateFollowActorMenuForViewport(FMenuBuilder& MenuBuilder, FLevelEditorViewportClient* ViewportClient);

	void BuildCurrentFollowActorWidgetForViewport(FMenuBuilder& MenuBuilder, FLevelEditorViewportClient* ViewportClient);
};

// INLINES

inline const USyncViewportSubsystem::FLiveViewportInfo* USyncViewportSubsystem::GetDataForViewport(FLevelEditorViewportClient* ViewportClient) const
{
	return ViewportInfos.Find(ViewportClient);
}

inline bool USyncViewportSubsystem::IsViewportTrackingLiveState(FLevelEditorViewportClient* ViewportClient) const
{
	if(const FLiveViewportInfo* ViewportInfo = ViewportInfos.Find(ViewportClient))
	{
		return ViewportInfo->bLiveUpdate;
	}
	return false;
}

inline bool USyncViewportSubsystem::IsViewportFollowingActor(FLevelEditorViewportClient* ViewportClient, AActor* Actor) const
{
	if(const FLiveViewportInfo* ViewportInfo = ViewportInfos.Find(ViewportClient))
	{
		return ViewportInfo->FollowActor == Actor;
	}
	return false;
}