#include "PirateMCPDataTableCommands.h"
#include "PirateMCPCommonUtils.h"
#include "PirateMCPModule.h"
#include "Engine/DataTable.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "UObject/Class.h"
#include "UObject/PropertyIterator.h"

TSharedPtr<FJsonObject> FPirateMCPDataTableCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("datatable_list"))          return HandleListTables(Params);
	if (CommandType == TEXT("datatable_get_row_names")) return HandleGetRowNames(Params);
	if (CommandType == TEXT("datatable_get_row"))       return HandleGetRow(Params);
	if (CommandType == TEXT("datatable_get_all_rows"))  return HandleGetAllRows(Params);
	if (CommandType == TEXT("datatable_set_row"))       return HandleSetRow(Params);
	if (CommandType == TEXT("datatable_delete_row"))    return HandleDeleteRow(Params);
	return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown datatable command: %s"), *CommandType));
}

static UDataTable* LoadDataTable(const FString& AssetPath)
{
	return Cast<UDataTable>(UEditorAssetLibrary::LoadAsset(AssetPath));
}

// Helper: convert a row struct to JSON
static TSharedPtr<FJsonObject> RowToJson(UDataTable* DT, const FName& RowName)
{
	if (!DT) return nullptr;
	uint8* RowPtr = DT->FindRowUnchecked(RowName);
	if (!RowPtr) return nullptr;

	TSharedPtr<FJsonObject> RowObj = MakeShared<FJsonObject>();
	const UScriptStruct* Struct = DT->GetRowStruct();
	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		FProperty* Prop = *It;
		FString ValStr;
		Prop->ExportTextItem_Direct(ValStr, RowPtr + Prop->GetOffset_ForInternal(), nullptr, nullptr, PPF_None);
		RowObj->SetStringField(Prop->GetName(), ValStr);
	}
	return RowObj;
}

TSharedPtr<FJsonObject> FPirateMCPDataTableCommands::HandleListTables(const TSharedPtr<FJsonObject>& Params)
{
	FAssetRegistryModule& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FARFilter Filter;
	Filter.ClassPaths.Add(UDataTable::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(TEXT("/Game"));

	TArray<FAssetData> Assets;
	AR.Get().GetAssets(Filter, Assets);

	TArray<TSharedPtr<FJsonValue>> TableList;
	for (const FAssetData& Asset : Assets)
	{
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), Asset.AssetName.ToString());
		Entry->SetStringField(TEXT("path"), Asset.GetObjectPathString());
		TableList.Add(MakeShared<FJsonValueObject>(Entry));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("tables"), TableList);
	return Result;
}

TSharedPtr<FJsonObject> FPirateMCPDataTableCommands::HandleGetRowNames(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing asset_path"));

	UDataTable* DT = LoadDataTable(AssetPath);
	if (!DT)
		return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Could not load DataTable: %s"), *AssetPath));

	TArray<TSharedPtr<FJsonValue>> Names;
	for (const FName& Name : DT->GetRowNames())
		Names.Add(MakeShared<FJsonValueString>(Name.ToString()));

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("row_names"), Names);
	return Result;
}

TSharedPtr<FJsonObject> FPirateMCPDataTableCommands::HandleGetRow(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath, RowName;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing asset_path"));
	if (!Params->TryGetStringField(TEXT("row_name"), RowName))
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing row_name"));

	UDataTable* DT = LoadDataTable(AssetPath);
	if (!DT)
		return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Could not load DataTable: %s"), *AssetPath));

	TSharedPtr<FJsonObject> Row = RowToJson(DT, FName(*RowName));
	if (!Row)
		return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Row not found: %s"), *RowName));

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("row_name"), RowName);
	Result->SetObjectField(TEXT("row"), Row);
	return Result;
}

TSharedPtr<FJsonObject> FPirateMCPDataTableCommands::HandleGetAllRows(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing asset_path"));

	UDataTable* DT = LoadDataTable(AssetPath);
	if (!DT)
		return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Could not load DataTable: %s"), *AssetPath));

	TArray<TSharedPtr<FJsonValue>> Rows;
	for (const FName& Name : DT->GetRowNames())
	{
		TSharedPtr<FJsonObject> RowEntry = MakeShared<FJsonObject>();
		RowEntry->SetStringField(TEXT("row_name"), Name.ToString());
		TSharedPtr<FJsonObject> RowData = RowToJson(DT, Name);
		if (RowData) RowEntry->SetObjectField(TEXT("row"), RowData);
		Rows.Add(MakeShared<FJsonValueObject>(RowEntry));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("rows"), Rows);
	return Result;
}

TSharedPtr<FJsonObject> FPirateMCPDataTableCommands::HandleSetRow(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath, RowName;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing asset_path"));
	if (!Params->TryGetStringField(TEXT("row_name"), RowName))
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing row_name"));

	const TSharedPtr<FJsonObject>* RowDataPtr;
	if (!Params->TryGetObjectField(TEXT("row_data"), RowDataPtr))
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing row_data"));

	UDataTable* DT = LoadDataTable(AssetPath);
	if (!DT)
		return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Could not load DataTable: %s"), *AssetPath));

	const UScriptStruct* Struct = DT->GetRowStruct();
	FName Name(*RowName);

	// Find or add row — create zeroed default struct for new rows
	uint8* RowPtr = DT->FindRowUnchecked(Name);
	bool bNewRow = false;
	if (!RowPtr)
	{
		TArray<uint8> DefaultData;
		DefaultData.SetNumZeroed(Struct->GetStructureSize());
		Struct->InitializeStruct(DefaultData.GetData());
		DT->AddRow(Name, *reinterpret_cast<FTableRowBase*>(DefaultData.GetData()));
		Struct->DestroyStruct(DefaultData.GetData());
		RowPtr = DT->FindRowUnchecked(Name);
		bNewRow = true;
	}
	if (!RowPtr)
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Failed to add/find row"));

	// Apply JSON fields to the struct
	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		FProperty* Prop = *It;
		FString PropName = Prop->GetName();
		FString ValStr;
		if ((*RowDataPtr)->TryGetStringField(PropName, ValStr))
		{
			Prop->ImportText_Direct(*ValStr, RowPtr + Prop->GetOffset_ForInternal(), nullptr, PPF_None);
		}
	}

	DT->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("row_name"), RowName);
	Result->SetBoolField(TEXT("new_row"), bNewRow);
	return Result;
}

TSharedPtr<FJsonObject> FPirateMCPDataTableCommands::HandleDeleteRow(const TSharedPtr<FJsonObject>& Params)
{
	FString AssetPath, RowName;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing asset_path"));
	if (!Params->TryGetStringField(TEXT("row_name"), RowName))
		return FPirateMCPCommonUtils::CreateErrorResponse(TEXT("Missing row_name"));

	UDataTable* DT = LoadDataTable(AssetPath);
	if (!DT)
		return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Could not load DataTable: %s"), *AssetPath));

	FName Name(*RowName);
	if (!DT->FindRowUnchecked(Name))
		return FPirateMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Row not found: %s"), *RowName));

	DT->RemoveRow(Name);
	DT->MarkPackageDirty();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("deleted_row"), RowName);
	return Result;
}
