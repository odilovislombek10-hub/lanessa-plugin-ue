using UnrealBuildTool;

public class Lanessa : ModuleRules
{
	public Lanessa(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"UMG",
			"Slate",
			"SlateCore",
			// DeveloperSettings backs ULanessaDayNightSettings, which puts the day/night hours on a
			// Project Settings page saved to DefaultGame.ini - so changing when the lights come on
			// is an editor edit, not a C++ edit plus a rebuild.
			"DeveloperSettings"
		});

		// UnrealEd gives access to FStructureEditorUtils (adding/renaming UserDefinedStruct fields
		// from C++, not exposed to Python/Blueprint) - editor-only, must not leak into packaged/game
		// builds. See ULanessaV2Widget::AddIntFieldToStruct.
		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
