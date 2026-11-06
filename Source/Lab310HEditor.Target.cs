using UnrealBuildTool;

public class Lab310HEditorTarget : TargetRules
{
	public Lab310HEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4;
		ExtraModuleNames.Add("Lab310H");
		// Newer Xcode compilers flag UE 5.4 engine headers; silence those warnings.
		bOverrideBuildEnvironment = true;
		AdditionalCompilerArguments = "-Wno-deprecated-literal-operator -Wno-invalid-unevaluated-string -Wno-nontrivial-memcall -Wno-unknown-warning-option";
	}
}
