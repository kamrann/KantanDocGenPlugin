#include "DocGenSettings.h"
#include "OutputFormats/DocGenOutputFormatFactoryBase.h"

UKantanDocGenSettingsObject* UKantanDocGenSettingsObject::Get()
{
	static bool bInitialized = false;

	// This is a singleton, use default object
	auto DefaultSettings = GetMutableDefault<UKantanDocGenSettingsObject>();

	if (!bInitialized)
	{
		DefaultSettings->LoadConfig();
		InitDefaults(DefaultSettings);
		bInitialized = true;
	}
	return DefaultSettings;
}

void UKantanDocGenSettingsObject::InitDefaults(UKantanDocGenSettingsObject* CDO)
{
	if (CDO->Settings.DocumentationTitle.IsEmpty())
	{
		CDO->Settings.DocumentationTitle = FApp::GetProjectName();
	}

	if (CDO->Settings.OutputDirectory.Path.IsEmpty())
	{
		CDO->Settings.OutputDirectory.Path = FPaths::ProjectSavedDir() / TEXT("KantanDocGen");
	}

	if (CDO->Settings.BlueprintContextClass == nullptr)
	{
		CDO->Settings.BlueprintContextClass = AActor::StaticClass();
	}
}

void UKantanDocGenSettingsObject::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	Settings.OutputFormatsSerializationData.Empty();
	for (const auto& OutputFormat : Settings.OutputFormats)
	{
		if (OutputFormat)
		{
			IDocGenOutputFormatFactory* FactoryInterface = Cast<IDocGenOutputFormatFactory>(OutputFormat);
			if (FactoryInterface)
			{
				Settings.OutputFormatsSerializationData.Add(FactoryInterface->SaveSettings());
			}
		}
	}
}

void UKantanDocGenSettingsObject::PostInitProperties()
{
	Super::PostInitProperties();
	Settings.OutputFormats.Empty();
	for (const auto& FormatData : Settings.OutputFormatsSerializationData)
	{
		UDocGenOutputFormatFactoryBase* RecreatedFactory =
			NewObject<UDocGenOutputFormatFactoryBase>(this, FormatData.FactoryClass);
		if (RecreatedFactory)
		{
			IDocGenOutputFormatFactory* FactoryInterface = Cast<IDocGenOutputFormatFactory>(RecreatedFactory);
			if (FactoryInterface)
			{
				FactoryInterface->LoadSettings(FormatData);
				Settings.OutputFormats.Add(RecreatedFactory);
			}
		}
	}
}
