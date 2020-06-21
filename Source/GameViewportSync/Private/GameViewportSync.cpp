// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FGameViewportSyncModule"

class FGameViewportSyncModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{

	}
	virtual void ShutdownModule() override
	{

	}
};

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FGameViewportSyncModule, GameViewportSync)