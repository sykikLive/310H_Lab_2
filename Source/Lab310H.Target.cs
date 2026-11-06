using UnrealBuildTool;

public class Lab310HTarget : TargetRules
{
	public Lab310HTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4;
		ExtraModuleNames.Add("Lab310H");
		// Newer Xcode compilers flag UE 5.4 engine headers; silence those warnings.
		bOverrideBuildEnvironment = true;
		AdditionalCompilerArguments = "-Wno-deprecated-literal-operator -Wno-invalid-unevaluated-string -Wno-nontrivial-memcall -Wno-unknown-warning-option";
	}
}
