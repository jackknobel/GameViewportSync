
#include "ViewportSyncEditorExtender.h"
#include "SyncViewportSubsystem.h"

// UE Includes
#include "Editor.h"
#include "IAssetViewport.h"
#include "LevelEditor.h"
#include "LevelEditorViewport.h"
#include "SceneOutlinerModule.h"
#include "Engine/Selection.h"
#include "Styling/SlateIconFinder.h"
#include "SceneOutlinerPublicTypes.h"
#include "SEditorViewport.h"
#include "SLevelViewport.h"
#include "ViewportSyncEditorCommands.h"
#include "ToolMenus.h"
#include "Slate/SceneViewport.h"

#define LOCTEXT_NAMESPACE "ViewportSync"

const FName FViewportSyncEditorExtender::LevelEditorModuleName("LevelEditor");

const FName FViewportSyncEditorExtender::SectionExtensionPointName("ViewportSync");
const FText FViewportSyncEditorExtender::SectionExtensionPointText(LOCTEXT("ViewportSync", "Viewport Sync"));

const FName FViewportSyncEditorExtender::FollowActorExtensionPointName("ViewportSync_FollowActor");
const FName FViewportSyncEditorExtender::SelectActorExtensionPointName("ViewportSync_SelectActor");

FViewportSyncEditorExtender::FViewportSyncEditorExtender(USyncViewportSubsystem& SyncViewportSubsystem)
	: OwningSubsystem(&SyncViewportSubsystem)
{	
	FViewportSyncEditorCommands::Register();

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(LevelEditorModuleName);

	RegisterCommands(LevelEditorModule.GetGlobalLevelEditorActions());

	// Register our viewport drop down extension
	auto& ViewportOptionExtenders = LevelEditorModule.GetAllLevelViewportOptionsMenuExtenders();
	ViewportOptionExtenders.Add(FLevelEditorModule::FLevelEditorMenuExtender::CreateRaw(this, &FViewportSyncEditorExtender::OnExtendLevelViewportOptionMenu));
	LevelViewportOptionsExtenderHandle = ViewportOptionExtenders.Last().GetHandle();

	// Register our right click menu
	auto& LevelViewportContextExtenderss = LevelEditorModule.GetAllLevelViewportContextMenuExtenders();
	LevelViewportContextExtenderss.Add(FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors::CreateRaw(this, &FViewportSyncEditorExtender::GetLevelViewportContextMenuExtender));
	LevelViewportContextExtenderHandle = LevelViewportContextExtenderss.Last().GetHandle();
}

FViewportSyncEditorExtender::~FViewportSyncEditorExtender()
{
	if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(LevelEditorModuleName))
	{
		UnRegisterCommands(LevelEditorModule->GetGlobalLevelEditorActions());

		typedef FLevelEditorModule::FLevelEditorMenuExtender MenuDelegateType;
		LevelEditorModule->GetAllLevelViewportOptionsMenuExtenders().RemoveAll([&](const MenuDelegateType& In) { return In.GetHandle() == LevelViewportOptionsExtenderHandle; });

		typedef FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors MenuDelegateType_SelectedActors;
		LevelEditorModule->GetAllLevelViewportContextMenuExtenders().RemoveAll([&](const MenuDelegateType_SelectedActors& In) { return In.GetHandle() == LevelViewportContextExtenderHandle; });
	}
}


void FViewportSyncEditorExtender::RegisterCommands(TSharedRef<FUICommandList> CommandList)
{
	CommandList->MapAction(FViewportSyncEditorCommands::Get().ToggleViewportSync,
		FExecuteAction::CreateLambda([OwningSubsystem = this->OwningSubsystem]
		{
			if(FLevelEditorViewportClient* ViewportClient = GetActiveViewportClient())
			{
				OwningSubsystem->SetViewportSyncState(ViewportClient, !OwningSubsystem->IsViewportSyncing(ViewportClient));
			}		
		}),
		FCanExecuteAction::CreateLambda([]{ return true; }),
		FGetActionCheckState::CreateLambda([OwningSubsystem = this->OwningSubsystem]()
		{
			return OwningSubsystem->IsViewportSyncing(GetActiveViewportClient()) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
	);

	CommandList->MapAction(FViewportSyncEditorCommands::Get().FollowActor,
		FExecuteAction::CreateLambda([OwningSubsystem = this->OwningSubsystem]
		{
			TArray<AActor*> SelectedActors;
			GEditor->GetSelectedActors()->GetSelectedObjects(SelectedActors);
			
			FLevelEditorViewportClient* ViewportClient = GetActiveViewportClient();
			
			if(SelectedActors.Num() > 0 && ViewportClient != nullptr)
			{
				OwningSubsystem->SetViewportFollowActor(ViewportClient, SelectedActors[0]);
			}	
		})
	);
}

void FViewportSyncEditorExtender::UnRegisterCommands(TSharedRef<FUICommandList> CommandList)
{
	CommandList->UnmapAction(FViewportSyncEditorCommands::Get().ToggleViewportSync);
	CommandList->UnmapAction(FViewportSyncEditorCommands::Get().FollowActor);
}

FLevelEditorViewportClient* FViewportSyncEditorExtender::GetActiveViewportClient()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(LevelEditorModuleName);
	TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetLevelEditorInstance().Pin();

	return static_cast<FLevelEditorViewportClient*>(&LevelEditor->GetActiveViewportInterface()->GetAssetViewportClient());
}

TSharedRef<FExtender> FViewportSyncEditorExtender::OnExtendLevelViewportOptionMenu(const TSharedRef<FUICommandList> CommandList)
{
	FLevelEditorModule& LevelEditorModule			= FModuleManager::GetModuleChecked<FLevelEditorModule>(LevelEditorModuleName);
	TSharedPtr<ILevelEditor> LevelEditor			= LevelEditorModule.GetLevelEditorInstance().Pin();
	FLevelEditorViewportClient* ViewportClient		= static_cast<FLevelEditorViewportClient*>(&LevelEditor->GetActiveViewportInterface()->GetAssetViewportClient());

	static const FName ExtensionPoint("LevelViewportViewportOptions");
	
	TSharedRef<FExtender> Extender = MakeShared<FExtender>();
	Extender->AddMenuExtension(
		ExtensionPoint,
		EExtensionHook::After,
		nullptr,
		FMenuExtensionDelegate::CreateSP(this, &FViewportSyncEditorExtender::BuildMenuListForViewport, ViewportClient)
	);
	return Extender;
}

void FViewportSyncEditorExtender::BuildMenuListForViewport(FMenuBuilder& MenuBuilder, FLevelEditorViewportClient* ViewportClient)
{	
	MenuBuilder.BeginSection(FViewportSyncEditorExtender::SectionExtensionPointName, FViewportSyncEditorExtender::SectionExtensionPointText);
	{		
		// Enable or Disable Viewport Syncing
		MenuBuilder.AddMenuEntry(FViewportSyncEditorCommands::Get().ToggleViewportSync);

		FUIAction FollowActorSubMenu;
		FollowActorSubMenu.CanExecuteAction.BindUObject(OwningSubsystem.Get(), &USyncViewportSubsystem::IsViewportSyncing, ViewportClient);
		
		// Follow Actor SubMenu
		MenuBuilder.AddSubMenu(
			LOCTEXT("FollowActor", "Follow Actor"),
			LOCTEXT("FollowActorTooltip", "Select an actor for this Viewport to follow"),
			FNewMenuDelegate::CreateSP(this, &FViewportSyncEditorExtender::CreateFollowActorMenuForViewport, ViewportClient),
			FollowActorSubMenu,
			FViewportSyncEditorExtender::FollowActorExtensionPointName,
			EUserInterfaceActionType::Button
		);

		BuildCurrentFollowActorWidgetForViewport(MenuBuilder, ViewportClient);
	}
	MenuBuilder.EndSection();
}

void FViewportSyncEditorExtender::CreateFollowActorMenuForViewport(FMenuBuilder& MenuBuilder, FLevelEditorViewportClient* ViewportClient)
{
	// Set up a menu entry to add the selected actor(s) to the sequencer
	TArray<AActor*> SelectedActors;
	GEditor->GetSelectedActors()->GetSelectedObjects(SelectedActors);

	AActor* SelectedActor = nullptr;

	FText SelectedActorDisplayName;
	FSlateIcon SelectedActorIcon;
	
	if (SelectedActors.Num() > 0)
	{
		SelectedActor = SelectedActors[0];
		
		SelectedActorDisplayName	= FText::Format(LOCTEXT("FollowSelectedActor", "Follow '{0}'"), FText::FromString(SelectedActor->GetActorLabel()));
		SelectedActorIcon			= FSlateIconFinder::FindIconForClass(SelectedActor->GetClass());
	}
	else
	{
		SelectedActorIcon = FSlateIconFinder::FindIconForClass(AActor::StaticClass());;
	}

	if (!SelectedActorDisplayName.IsEmpty())
	{
		MenuBuilder.AddMenuEntry(
			FViewportSyncEditorCommands::Get().FollowActor,
			NAME_None,
			SelectedActorDisplayName, 
			FText(), 
			SelectedActorIcon
		);
	}

	/* Scene outliner for picking a follow actor */
	MenuBuilder.BeginSection(FViewportSyncEditorExtender::SelectActorExtensionPointName, LOCTEXT("SelectFollowActor", "Select Actor to Follow:"));
	{
		using namespace SceneOutliner;

		FInitializationOptions InitOptions;
		{
			InitOptions.Mode = ESceneOutlinerMode::ActorPicker;

			// We hide the header row to keep the UI compact.
			InitOptions.bShowHeaderRow = false;
			InitOptions.bShowSearchBox = true;
			InitOptions.bShowCreateNewFolder = false;
			InitOptions.bFocusSearchBoxWhenOpened = true;

			// Only want the actor label column
			InitOptions.ColumnMap.Add(FBuiltInColumnTypes::Label(), FColumnInfo(EColumnVisibility::Visible, 0));
		}

		// actor selector to allow the user to choose an actor
		static const FName SceneOutlinerModuleName("SceneOutliner");
		
		FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>(SceneOutlinerModuleName);
		const TSharedRef< SWidget > MiniSceneOutliner =
			SNew(SBox)
			.MaxDesiredHeight(400.0f)
			.WidthOverride(300.0f)
			[
				SceneOutlinerModule.CreateSceneOutliner
				(
					InitOptions,
					FOnActorPicked::CreateLambda([OwningSubsystem = this->OwningSubsystem, ViewportClient](AActor* SelectedActor)
					{
						FSlateApplication::Get().DismissAllMenus();
						OwningSubsystem->SetViewportFollowActor(ViewportClient, SelectedActor);
					})
				)
			];

		MenuBuilder.AddWidget(MiniSceneOutliner, FText::GetEmpty(), true);
	}
	
	MenuBuilder.EndSection();
}

void FViewportSyncEditorExtender::BuildCurrentFollowActorWidgetForViewport(FMenuBuilder& MenuBuilder, FLevelEditorViewportClient* ViewportClient)
{
	const USyncViewportSubsystem::FLiveViewportInfo* ViewportInfo = OwningSubsystem->GetDataForViewport(ViewportClient);

	const auto ActorName = [ViewportInfo, &GlobalFollowActorOverride = OwningSubsystem->GetGlobalViewportFollowTargetOverride()]()
	{
		FString SelectedActorDetail;

		const TSoftObjectPtr<AActor> TargetActor = !GlobalFollowActorOverride.IsNull() ? GlobalFollowActorOverride : ViewportInfo->FollowActor;
		
		if (TargetActor.IsValid())
		{
			SelectedActorDetail = FString::Printf(TEXT("Following: '%s'"), *TargetActor->GetActorLabel());
		}
		else if (TargetActor.IsPending())
		{
			SelectedActorDetail = FString::Printf(TEXT("Waiting for: %s"), *TargetActor.ToSoftObjectPath().GetSubPathString());
		}
		else
		{
			SelectedActorDetail = TEXT("No follow Actor set");
		}

		return FText::Format(LOCTEXT("FollowSelectedActor", "{0} {1}"), (!GlobalFollowActorOverride.IsNull() ? FText::FromString("[Override]") : FText::GetEmpty()), FText::FromString(SelectedActorDetail));
	};
	
	MenuBuilder.AddWidget(
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Fill)
		[
			SNew(SButton)
				.HAlign(HAlign_Fill)
				.OnPressed_Lambda([FollowedActor = ViewportInfo->FollowActor]()
				{
					GEditor->SelectActor(FollowedActor.Get(), true, true);
				})
				.ForegroundColor(FSlateColor::UseForeground())
				.ButtonStyle(FEditorStyle::Get(), "NoBorder")
				.ContentPadding(0.0f)
				.Text_Lambda(ActorName)
		]
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Right)
		[
				SNew(SButton)
				.OnPressed_Lambda([OwningSubsystem = this->OwningSubsystem, ViewportClient]()
				{
					OwningSubsystem->SetViewportFollowActor(ViewportClient, nullptr);
				})
				.Visibility_Lambda([OwningSubsystem = this->OwningSubsystem, ViewportClient, ViewportInfo]()
				{
					if (OwningSubsystem->IsViewportSyncing(ViewportClient) && !ViewportInfo->FollowActor.IsNull())
					{
						return EVisibility::Visible;
					}
					return EVisibility::Hidden;
				})
				.ForegroundColor(FSlateColor::UseForeground())
				.HAlign(HAlign_Fill)
				.ToolTipText(LOCTEXT("ClearFollow", "Remove followed Actor"))
				.ButtonStyle(FEditorStyle::Get(), "NoBorder")
				.Content()
				[			
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("PropertyWindow.Button_Clear"))
				]

		],
		FText::GetEmpty()
	);
}

TSharedRef<FExtender> FViewportSyncEditorExtender::GetLevelViewportContextMenuExtender(const TSharedRef<FUICommandList> CommandList, const TArray<AActor*> InActors)
{
	TSharedRef<FExtender> Extender = MakeShareable(new FExtender);

	/* We only want to show this if we've selected one actor */
	if (InActors.Num() == 1)
	{
		Extender->AddMenuExtension(
			"ActorAsset",
			EExtensionHook::Before,
			CommandList,
			FMenuExtensionDelegate::CreateLambda([](FMenuBuilder& MenuBuilder)
			{
				MenuBuilder.BeginSection(SectionExtensionPointName, SectionExtensionPointText);
				{
					MenuBuilder.AddMenuEntry(FViewportSyncEditorCommands::Get().FollowActor);
				}
				MenuBuilder.EndSection();
				
			}));
	}

	return Extender;
	
	
}

#undef LOCTEXT_NAMESPACE