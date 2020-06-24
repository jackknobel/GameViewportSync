// Fill out your copyright notice in the Description page of Project Settings.

#include "GameViewportSyncStatics.h"
#include "SyncViewportSubsystem.h"

// UE Includes
#include "Editor.h"

void UGameViewportSyncStatics::SetGlobalViewportFollowTargetOverride(AActor* FollowTarget)
{
	if (USyncViewportSubsystem* SyncViewport = GEditor->GetEditorSubsystem<USyncViewportSubsystem>())
	{
		return SyncViewport->SetGlobalViewportFollowTargetOverride(FollowTarget);
	}
}

TSoftObjectPtr<AActor> UGameViewportSyncStatics::GetGlobalViewportFollowTargetOverride()
{
	if (USyncViewportSubsystem* SyncViewport = GEditor->GetEditorSubsystem<USyncViewportSubsystem>())
	{
		return SyncViewport->GetGlobalViewportFollowTargetOverride();
	}
	return nullptr;
}
