// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameViewportSyncStatics.generated.h"

/**
 * 
 */
UCLASS()
class UGameViewportSyncStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	/* Set a global follow target override for all PIE sync'd viewports to follow */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Viewport", meta=(DevelopmentOnly))
	static void SetGlobalViewportFollowTargetOverride(AActor* FollowTarget);

	/* Get the current follow target override from the Viewport Sync Subsystem */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Viewport", meta=(DevelopmentOnly))
	static TSoftObjectPtr<AActor> GetGlobalViewportFollowTargetOverride();
};
