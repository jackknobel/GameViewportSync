// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FGameViewportSyncModule"

class FGameViewportSyncModule : public IModuleInterface
{
	
};

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FGameViewportSyncModule, GameViewportSync)