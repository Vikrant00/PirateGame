#include "PirateMCPPythonCommands.h"
#include "PirateMCPCommonUtils.h"
#include "PirateMCPModule.h"
#include "IPythonScriptPlugin.h"
#include "Editor.h"
#include "Misc/StringOutputDevice.h"

TSharedPtr<FJsonObject> FPirateMCPPythonCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("exec_python"))
	{
		return HandleExecPython(Params);
	}
	else if (CommandType == TEXT("exec_console_command"))
	{
		return HandleExecConsoleCommand(Params);
	}
	else if (CommandType == TEXT("get_python_help"))
	{
		return HandleGetPythonHelp(Params);
	}

	return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown python command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FPirateMCPPythonCommands::HandleExecPython(const TSharedPtr<FJsonObject>& Params)
{
	FString Code;
	if (!Params->TryGetStringField(TEXT("code"), Code))
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'code' parameter"));
	}

	// Determine execution mode
	FString ModeStr;
	Params->TryGetStringField(TEXT("execution_mode"), ModeStr);

	EPythonCommandExecutionMode ExecMode = EPythonCommandExecutionMode::ExecuteFile;
	if (ModeStr == TEXT("execute_statement"))
	{
		ExecMode = EPythonCommandExecutionMode::ExecuteStatement;
	}
	else if (ModeStr == TEXT("evaluate_statement"))
	{
		ExecMode = EPythonCommandExecutionMode::EvaluateStatement;
	}

	// Get Python plugin
	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (!PythonPlugin)
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("PythonScriptPlugin is not loaded. Enable it in Project Settings > Plugins."));
	}

	if (!PythonPlugin->IsPythonAvailable())
	{
		// Try to force-enable Python
		PythonPlugin->ForceEnablePythonAtRuntime();
		if (!PythonPlugin->IsPythonAvailable())
		{
			return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Python is not available. The PythonScriptPlugin may not have initialized correctly."));
		}
	}

	// Execute using ExecPythonCommandEx for full output capture
	FPythonCommandEx PythonCommand;
	PythonCommand.Command = Code;
	PythonCommand.ExecutionMode = ExecMode;
	PythonCommand.FileExecutionScope = EPythonFileExecutionScope::Public;
	PythonCommand.Flags = EPythonCommandFlags::Unattended;

	bool bSuccess = PythonPlugin->ExecPythonCommandEx(PythonCommand);

	// Build response
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), bSuccess);
	Result->SetStringField(TEXT("result"), PythonCommand.CommandResult);

	// Capture log output
	TArray<TSharedPtr<FJsonValue>> LogEntries;
	for (const FPythonLogOutputEntry& Entry : PythonCommand.LogOutput)
	{
		TSharedPtr<FJsonObject> LogEntry = MakeShareable(new FJsonObject);
		LogEntry->SetStringField(TEXT("type"), LexToString(Entry.Type));
		LogEntry->SetStringField(TEXT("output"), Entry.Output);
		LogEntries.Add(MakeShareable(new FJsonValueObject(LogEntry)));
	}
	Result->SetArrayField(TEXT("log_output"), LogEntries);

	// Combine all log output into a single string for easy reading
	FString CombinedOutput;
	for (const FPythonLogOutputEntry& Entry : PythonCommand.LogOutput)
	{
		if (!CombinedOutput.IsEmpty()) CombinedOutput += TEXT("\n");
		CombinedOutput += Entry.Output;
	}
	Result->SetStringField(TEXT("output"), CombinedOutput);

	if (!bSuccess && PythonCommand.CommandResult.Len() > 0)
	{
		Result->SetStringField(TEXT("error"), PythonCommand.CommandResult);
	}

	return Result;
}

TSharedPtr<FJsonObject> FPirateMCPPythonCommands::HandleExecConsoleCommand(const TSharedPtr<FJsonObject>& Params)
{
	FString Command;
	if (!Params->TryGetStringField(TEXT("command"), Command))
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'command' parameter"));
	}

	if (!GEditor || !GEditor->GetEditorWorldContext().World())
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("No editor world available"));
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();

	// Execute the console command using FStringOutputDevice to capture output
	FStringOutputDevice OutputDevice;
	GEditor->Exec(World, *Command, OutputDevice);

	FString OutputStr = OutputDevice;

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("command"), Command);
	Result->SetStringField(TEXT("output"), OutputStr);

	return Result;
}

TSharedPtr<FJsonObject> FPirateMCPPythonCommands::HandleGetPythonHelp(const TSharedPtr<FJsonObject>& Params)
{
	FString Target;
	if (!Params->TryGetStringField(TEXT("target"), Target))
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'target' parameter (e.g. 'unreal.EditorAssetLibrary')"));
	}

	// Determine what kind of help
	FString HelpType;
	Params->TryGetStringField(TEXT("help_type"), HelpType);

	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (!PythonPlugin || !PythonPlugin->IsPythonAvailable())
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Python is not available"));
	}

	FString PythonCode;
	if (HelpType == TEXT("dir"))
	{
		// List all members
		PythonCode = FString::Printf(TEXT("import unreal\nresult = dir(%s)\nfor item in result:\n    if not item.startswith('_'):\n        print(item)"), *Target);
	}
	else if (HelpType == TEXT("signature"))
	{
		// Get function signature
		PythonCode = FString::Printf(TEXT("import unreal, inspect\ntry:\n    sig = inspect.signature(%s)\n    print(str(sig))\nexcept:\n    print(type(%s).__name__)"), *Target, *Target);
	}
	else
	{
		// Default: help() — full docstring
		PythonCode = FString::Printf(TEXT("import unreal\nhelp(%s)"), *Target);
	}

	FPythonCommandEx PythonCommand;
	PythonCommand.Command = PythonCode;
	PythonCommand.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
	PythonCommand.FileExecutionScope = EPythonFileExecutionScope::Public;
	PythonCommand.Flags = EPythonCommandFlags::Unattended;

	bool bSuccess = PythonPlugin->ExecPythonCommandEx(PythonCommand);

	FString CombinedOutput;
	for (const FPythonLogOutputEntry& Entry : PythonCommand.LogOutput)
	{
		if (!CombinedOutput.IsEmpty()) CombinedOutput += TEXT("\n");
		CombinedOutput += Entry.Output;
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), bSuccess);
	Result->SetStringField(TEXT("target"), Target);
	Result->SetStringField(TEXT("help_type"), HelpType.IsEmpty() ? TEXT("help") : HelpType);
	Result->SetStringField(TEXT("output"), CombinedOutput);

	if (!bSuccess)
	{
		Result->SetStringField(TEXT("error"), PythonCommand.CommandResult);
	}

	return Result;
}
