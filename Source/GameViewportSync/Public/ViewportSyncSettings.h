// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ViewportSyncSettings.generated.h"

/**
 * 
 */
UCLASS(config=EditorPerProjectUserSettings, meta = (DisplayName = "Viewports Sync Settings"))
class GAMEVIEWPORTSYNC_API UViewportSyncSettings : public UDeveloperSettings
{
	GENERATED_BODY()

	UViewportSyncSettings()
		: bSyncByDefault(true)
		, FollowActorSmoothSpeed(100.0f)
	{}

	virtual FName GetCategoryName() const override;
	
public:
	/* Should our viewports sync by default */
	UPROPERTY(config, EditAnywhere)
	bool bSyncByDefault;

	/*
	 * Speed at which our viewports should update to the desired actor target
	 * This shouldn't be modified unless you *really* need to
	 */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay)
	float FollowActorSmoothSpeed;

};

// INLINES
inline FName UViewportSyncSettings::GetCategoryName() const
{
	static const FName ViewportSyncCategory("LevelEditor");
	return ViewportSyncCategory;
}