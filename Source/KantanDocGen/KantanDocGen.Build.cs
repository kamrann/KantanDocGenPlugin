// Copyright (C) 2016 Cameron Angus. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class KantanDocGen : ModuleRules
{
	public KantanDocGen(TargetInfo Target)
	{
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

		// Super hacky - copied from EnvironmentQueryEditor module!
		PrivateIncludePaths.AddRange(
		   new string[] {
				"Editor/GraphEditor/Private",
		   }
		   );

		PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "Slate",
				"SlateCore",
                "UnrealEd",
                "PropertyEditor",
                "EditorStyle",
				"BlueprintGraph",
				"GraphEditor",
				"MainFrame",
				"LevelEditor",
				"XmlParser",
				"UMG",
				"Projects",
            }
        );
	}
}
