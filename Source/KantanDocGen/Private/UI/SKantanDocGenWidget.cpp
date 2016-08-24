// Copyright (C) 2016 Cameron Angus. All Rights Reserved.

#include "KantanDocGenPCH.h"
#include "SKantanDocGenWidget.h"
#include "DocGenSettings.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "DocGenSettings.h"
#include "KantanDocGenModule.h"

#define LOCTEXT_NAMESPACE "KantanDocGen"


void SKantanDocGenWidget::Construct(const SKantanDocGenWidget::FArguments& InArgs)
{
	auto& PropertyEditorModule = FModuleManager::LoadModuleChecked< FPropertyEditorModule >("PropertyEditor");

	FDetailsViewArgs DetailArgs;
	DetailArgs.bUpdatesFromSelection = false;
	DetailArgs.bLockable = false;
	DetailArgs.NameAreaSettings = FDetailsViewArgs::ComponentsAndActorsUseNameArea;
	DetailArgs.bCustomNameAreaLocation = false;
	DetailArgs.bCustomFilterAreaLocation = true;
	DetailArgs.DefaultsOnlyVisibility = FDetailsViewArgs::EEditDefaultsOnlyNodeVisibility::Hide;

	auto DetailView = PropertyEditorModule.CreateDetailView(DetailArgs);

	ChildSlot
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				DetailView
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("GenButtonLabel", "Generate Docs"))
					.IsEnabled(this, &SKantanDocGenWidget::ValidateSettingsForGeneration)
					.OnClicked(this, &SKantanDocGenWidget::OnGenerateDocs)
				]
			]
		];

	DetailView->SetObject(UKantanDocGenSettingsObject::Get());
}

bool SKantanDocGenWidget::ValidateSettingsForGeneration() const
{
	auto const& Settings = UKantanDocGenSettingsObject::Get()->Settings;

	if(Settings.DocumentationTitle.IsEmpty())
	{
		return false;
	}

	if(!Settings.HasAnySources())
	{
		return false;
	}

	if(Settings.BlueprintContextClass == nullptr)
	{
		return false;
	}

	return true;
}

FReply SKantanDocGenWidget::OnGenerateDocs()
{
	auto& Module = FModuleManager::LoadModuleChecked< FKantanDocGenModule >(TEXT("KantanDocGen"));
	Module.GenerateDocs(UKantanDocGenSettingsObject::Get()->Settings);

	TSharedRef< SWindow > ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared()).ToSharedRef();
	FSlateApplication::Get().RequestDestroyWindow(ParentWindow);

	return FReply::Handled();
}


#undef LOCTEXT_NAMESPACE

