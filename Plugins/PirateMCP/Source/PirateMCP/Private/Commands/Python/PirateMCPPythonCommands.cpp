#include "PirateMCPPythonCommands.h"
#include "PirateMCPCommonUtils.h"
#include "PirateMCPModule.h"
#include "Editor.h"
#include "Misc/StringOutputDevice.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

// Python execution uses the `py` console command registered by PythonScriptPlugin.
// We avoid any compile/link dependency on PythonScriptPlugin — the `py` command
// is available at runtime as long as the plugin is enabled in the .uproject.
// Output is captured by wrapping user code in a temp script that redirects stdout.

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

static FString RunPythonWithCapture(const FString& Code)
{
	// Set up temp directory
	FString TempDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("PirateMCP"));
	IFileManager::Get().MakeDirectory(*TempDir, true);

	FString CodeFile = TempDir / TEXT("_pmcp_code.py");
	FString OutputFile = TempDir / TEXT("_pmcp_output.txt");
	FString WrapperFile = TempDir / TEXT("_pmcp_wrapper.py");

	// Normalize paths to forward slashes for Python
	CodeFile.ReplaceInline(TEXT("\\"), TEXT("/"));
	OutputFile.ReplaceInline(TEXT("\\"), TEXT("/"));
	WrapperFile.ReplaceInline(TEXT("\\"), TEXT("/"));

	// Write the user's raw code to a file (avoids escaping issues)
	FFileHelper::SaveStringToFile(Code, *CodeFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	// Delete old output file if it exists
	IFileManager::Get().Delete(*OutputFile, false, false, true);

	// Write wrapper script that executes user code with stdout/stderr capture
	FString WrapperCode = FString::Printf(TEXT(
		"import sys, io, traceback\n"
		"_pmcp_out = io.StringIO()\n"
		"_pmcp_err = io.StringIO()\n"
		"_pmcp_old_out, _pmcp_old_err = sys.stdout, sys.stderr\n"
		"sys.stdout, sys.stderr = _pmcp_out, _pmcp_err\n"
		"_pmcp_success = True\n"
		"try:\n"
		"    with open(r'%s', 'r', encoding='utf-8') as _pmcp_f:\n"
		"        _pmcp_code = _pmcp_f.read()\n"
		"    exec(compile(_pmcp_code, '<pirate_mcp>', 'exec'))\n"
		"except Exception:\n"
		"    _pmcp_success = False\n"
		"    traceback.print_exc()\n"
		"finally:\n"
		"    sys.stdout, sys.stderr = _pmcp_old_out, _pmcp_old_err\n"
		"_pmcp_result = _pmcp_out.getvalue() + _pmcp_err.getvalue()\n"
		"with open(r'%s', 'w', encoding='utf-8') as _pmcp_f:\n"
		"    _pmcp_f.write(('OK\\n' if _pmcp_success else 'ERROR\\n') + _pmcp_result)\n"
	), *CodeFile, *OutputFile);

	FFileHelper::SaveStringToFile(WrapperCode, *WrapperFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	// Execute via `py` console command
	if (!GEditor || !GEditor->GetEditorWorldContext().World())
	{
		return TEXT("ERROR\nNo editor world available");
	}

	FStringOutputDevice ConsoleOutput;
	GEditor->Exec(GEditor->GetEditorWorldContext().World(),
		*FString::Printf(TEXT("py \"%s\""), *WrapperFile), ConsoleOutput);

	// Read captured output
	FString Output;
	if (FFileHelper::LoadFileToString(Output, *OutputFile))
	{
		return Output;
	}

	// If output file wasn't created, the py command itself might have failed
	FString ConsoleStr = ConsoleOutput;
	if (!ConsoleStr.IsEmpty())
	{
		return FString::Printf(TEXT("ERROR\nPython execution failed. Console output: %s"), *ConsoleStr);
	}

	return TEXT("ERROR\nPython execution failed — no output captured. Is PythonScriptPlugin enabled?");
}

TSharedPtr<FJsonObject> FPirateMCPPythonCommands::HandleExecPython(const TSharedPtr<FJsonObject>& Params)
{
	FString Code;
	if (!Params->TryGetStringField(TEXT("code"), Code))
	{
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing required 'code' parameter"));
	}

	FString RawOutput = RunPythonWithCapture(Code);

	// Parse the output — first line is "OK" or "ERROR", rest is actual output
	bool bSuccess = true;
	FString Output;

	int32 NewlineIdx;
	if (RawOutput.FindChar(TEXT('\n'), NewlineIdx))
	{
		FString StatusLine = RawOutput.Left(NewlineIdx).TrimEnd();
		Output = RawOutput.Mid(NewlineIdx + 1);
		bSuccess = (StatusLine == TEXT("OK"));
	}
	else
	{
		Output = RawOutput;
		bSuccess = !RawOutput.StartsWith(TEXT("ERROR"));
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), bSuccess);
	Result->SetStringField(TEXT("output"), Output);

	if (!bSuccess)
	{
		Result->SetStringField(TEXT("error"), Output);
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

	FString HelpType;
	Params->TryGetStringField(TEXT("help_type"), HelpType);

	FString PythonCode;
	if (HelpType == TEXT("dir"))
	{
		PythonCode = FString::Printf(TEXT(
			"import unreal\n"
			"result = dir(%s)\n"
			"for item in result:\n"
			"    if not item.startswith('_'):\n"
			"        print(item)"
		), *Target);
	}
	else if (HelpType == TEXT("signature"))
	{
		PythonCode = FString::Printf(TEXT(
			"import unreal, inspect\n"
			"try:\n"
			"    sig = inspect.signature(%s)\n"
			"    print(str(sig))\n"
			"except:\n"
			"    print(type(%s).__name__)"
		), *Target, *Target);
	}
	else
	{
		PythonCode = FString::Printf(TEXT("import unreal\nhelp(%s)"), *Target);
	}

	FString RawOutput = RunPythonWithCapture(PythonCode);

	// Parse status
	bool bSuccess = true;
	FString Output;
	int32 NewlineIdx;
	if (RawOutput.FindChar(TEXT('\n'), NewlineIdx))
	{
		FString StatusLine = RawOutput.Left(NewlineIdx).TrimEnd();
		Output = RawOutput.Mid(NewlineIdx + 1);
		bSuccess = (StatusLine == TEXT("OK"));
	}
	else
	{
		Output = RawOutput;
		bSuccess = !RawOutput.StartsWith(TEXT("ERROR"));
	}

	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
	Result->SetBoolField(TEXT("success"), bSuccess);
	Result->SetStringField(TEXT("target"), Target);
	Result->SetStringField(TEXT("help_type"), HelpType.IsEmpty() ? TEXT("help") : HelpType);
	Result->SetStringField(TEXT("output"), Output);

	if (!bSuccess)
	{
		Result->SetStringField(TEXT("error"), Output);
	}

	return Result;
}
