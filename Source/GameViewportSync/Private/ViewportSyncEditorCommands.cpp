// Fill out your copyright notice in the Description page of Project Settings.

#include "ViewportSyncEditorCommands.h"

// UE Includes
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "ViewportSyncEditorCommands"

FViewportSyncEditorCommands::FViewportSyncEditorCommands() : TCommands<FViewportSyncEditorCommands>(
	"ViewportSyncEditorCommands",
	NSLOCTEXT("Contexts", "ViewportSyncEditorCommands", "Viewport Sync"),
	NAME_None, 
	FEditorStyle::GetStyleSetName())
{}

void FViewportSyncEditorCommands::RegisterCommands()
{
	UI_COMMAND(ToggleViewportSync, "Viewport Sync", "If enabled, this viewport will be set to same world as the primary PIE session", EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Shift | EModifierKey::Alt, EKeys::S));
	UI_COMMAND(FollowActor, "Follow Actor...", "Follow the selected actor", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift | EModifierKey::Alt, EKeys::F));
}

#undef LOCTEXT_NAMESPACE
