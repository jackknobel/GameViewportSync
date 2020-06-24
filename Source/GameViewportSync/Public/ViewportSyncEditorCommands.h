// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

/**
 * Editor commands used by the Viewport Syncing System
 */
class FViewportSyncEditorCommands : public TCommands<FViewportSyncEditorCommands>
{
public:
	FViewportSyncEditorCommands();

	// TCommands<> interface
	virtual void RegisterCommands() override;
	// End of TCommands<> interface

	// Command to Toggle PIE Viewport Syncing
	TSharedPtr<FUICommandInfo> ToggleViewportSync;

	// Command to tell a Viewport to follow an actor
	TSharedPtr<FUICommandInfo> FollowActor;
};