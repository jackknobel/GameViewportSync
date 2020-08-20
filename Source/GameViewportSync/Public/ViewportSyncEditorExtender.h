// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class AActor;
class FExtender;
class FMenuBuilder;
class FUICommandList;
class USyncViewportSubsystem;
class FLevelEditorViewportClient;

class FViewportSyncEditorExtender : public TSharedFromThis<FViewportSyncEditorExtender>
{
public:

	static const FText SectionExtensionPointText;
	
	/* Extension point names */
	static const FName LevelEditorModuleName; // UE4 Level Editor
	
	static const FName SectionExtensionPointName;
	static const FName FollowActorExtensionPointName;
	static const FName SelectActorExtensionPointName;

protected:
	TWeakObjectPtr<USyncViewportSubsystem> OwningSubsystem;

	FDelegateHandle LevelViewportOptionsExtenderHandle;
	FDelegateHandle LevelViewportContextExtenderHandle;

public:
	FViewportSyncEditorExtender(USyncViewportSubsystem& ViewportSyncSubsystem);
	~FViewportSyncEditorExtender();
	
	void RegisterCommands(TSharedRef<FUICommandList> CommandList);
	void UnRegisterCommands(TSharedRef<FUICommandList> CommandList);

	static FLevelEditorViewportClient* GetActiveViewportClient();
	
	//////////////////////////////////////////////
	// Viewport Extending
	//////////////////////////////////////////////
protected:	
	TSharedRef<FExtender> OnExtendLevelViewportOptionMenu(const TSharedRef<FUICommandList> CommandList);
	
	void BuildMenuListForViewport(FMenuBuilder& MenuBuilder, FLevelEditorViewportClient* ViewportClient);
	void CreateFollowActorMenuForViewport(FMenuBuilder& MenuBuilder, FLevelEditorViewportClient* ViewportClient);
	void BuildCurrentFollowActorWidgetForViewport(FMenuBuilder& MenuBuilder, FLevelEditorViewportClient* ViewportClient);

	//////////////////////////////////////////////
	// Context Menu Extending
	//////////////////////////////////////////////
protected:
	
	TSharedRef<FExtender> GetLevelViewportContextMenuExtender(const TSharedRef<FUICommandList> CommandList, const TArray<AActor*> InActors);
};