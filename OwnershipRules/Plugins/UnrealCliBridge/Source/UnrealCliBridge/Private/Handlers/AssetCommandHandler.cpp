#include "AssetCommandHandler.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "ObjectTools.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/TextureFactory.h"
#include "Factories/DataAssetFactory.h"
#include "Factories/BlueprintFactory.h"
#include "Materials/Material.h"
#include "FileHelpers.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "HAL/FileManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealCliBridgeAsset, Log, All);

static IAssetRegistry& GetAssetRegistry()
{
	return FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
}

static IAssetTools& GetAssetTools()
{
	return FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
}

static TSharedPtr<FJsonObject> AssetDataToJson(const FAssetData& AssetData)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
	Obj->SetStringField(TEXT("packageName"), AssetData.PackageName.ToString());
	Obj->SetStringField(TEXT("assetName"), AssetData.AssetName.ToString());
	Obj->SetStringField(TEXT("assetClass"), AssetData.AssetClassPath.ToString());
	Obj->SetBoolField(TEXT("isLoaded"), AssetData.IsAssetLoaded());
	return Obj;
}

void FAssetCommandHandler::RegisterAll(FCommandDispatcher& Dispatcher)
{
	Dispatcher.Register(TEXT("asset.find"), &HandleFind);
	Dispatcher.Register(TEXT("asset.info"), &HandleInfo);
	Dispatcher.Register(TEXT("asset.move"), &HandleMove);
	Dispatcher.Register(TEXT("asset.rename"), &HandleRename);
	Dispatcher.Register(TEXT("asset.delete"), &HandleDelete);
	Dispatcher.Register(TEXT("asset.create"), &HandleCreate);
	Dispatcher.Register(TEXT("asset.mkdir"), &HandleMkdir);
}

TSharedPtr<FJsonObject> FAssetCommandHandler::HandleFind(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	FString Name, TypeFilter, FolderFilter;
	int32 Limit = 100;
	if (Args.IsValid())
	{
		Args->TryGetStringField(TEXT("name"), Name);
		Args->TryGetStringField(TEXT("type"), TypeFilter);
		Args->TryGetStringField(TEXT("folder"), FolderFilter);
		Args->TryGetNumberField(TEXT("limit"), Limit);
	}

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.bRecursiveClasses = true;

	if (!FolderFilter.IsEmpty())
	{
		Filter.PackagePaths.Add(FName(*FolderFilter));
	}

	if (!TypeFilter.IsEmpty())
	{
		Filter.ClassPaths.Add(FTopLevelAssetPath(FName(*TypeFilter), NAME_None));
	}

	TArray<FAssetData> Assets;
	GetAssetRegistry().GetAssets(Filter, Assets);

	TArray<TSharedPtr<FJsonValue>> Results;
	for (const FAssetData& Asset : Assets)
	{
		if (!Name.IsEmpty() && !Asset.AssetName.ToString().Contains(Name, ESearchCase::IgnoreCase))
			continue;
		Results.Add(MakeShared<FJsonValueObject>(AssetDataToJson(Asset)));
		if (Results.Num() >= Limit) break;
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("assets"), Results);
	Data->SetNumberField(TEXT("count"), Results.Num());
	return Data;
}

TSharedPtr<FJsonObject> FAssetCommandHandler::HandleInfo(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	FString Path;
	if (!Args.IsValid() || !Args->TryGetStringField(TEXT("path"), Path) || Path.IsEmpty())
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("--path is required."));

	FAssetData AssetData = GetAssetRegistry().GetAssetByObjectPath(FSoftObjectPath(Path));
	if (!AssetData.IsValid())
		throw FCommandFailedException(TEXT("NOT_FOUND"), FString::Printf(TEXT("Asset not found: %s"), *Path));

	TSharedPtr<FJsonObject> Data = AssetDataToJson(AssetData);

	// Additional disk info
	FString PackageFilename;
	if (FPackageName::TryConvertLongPackageNameToFilename(AssetData.PackageName.ToString(), PackageFilename))
	{
		Data->SetStringField(TEXT("diskPath"), PackageFilename);
		Data->SetBoolField(TEXT("existsOnDisk"), FPaths::FileExists(PackageFilename));
	}

	return Data;
}

TSharedPtr<FJsonObject> FAssetCommandHandler::HandleMove(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	FString From, To;
	if (!Args.IsValid() || !Args->TryGetStringField(TEXT("from"), From) || !Args->TryGetStringField(TEXT("to"), To))
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("--from and --to are required."));

	FAssetData AssetData = GetAssetRegistry().GetAssetByObjectPath(FSoftObjectPath(From));
	if (!AssetData.IsValid())
		throw FCommandFailedException(TEXT("NOT_FOUND"), FString::Printf(TEXT("Source asset not found: %s"), *From));

	// Check if destination exists
	FString DestPackageName = FPackageName::ObjectPathToPackageName(To);
	FAssetData DestAsset = GetAssetRegistry().GetAssetByObjectPath(FSoftObjectPath(To));
	if (DestAsset.IsValid() && !bForce)
		throw FCommandFailedException(TEXT("FORCE_REQUIRED"),
			FString::Printf(TEXT("Destination already exists: %s. Use --force to overwrite."), *To));

	TArray<FAssetRenameData> RenameData;
	FAssetRenameData Rename(AssetData.GetAsset(), FPackageName::ObjectPathToPackageName(To), AssetData.AssetName.ToString());
	RenameData.Add(Rename);

	bool bSuccess = GetAssetTools().RenameAssets(RenameData);
	if (!bSuccess)
		throw FCommandFailedException(TEXT("OPERATION_FAILED"), TEXT("Failed to move asset."));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("from"), From);
	Data->SetStringField(TEXT("to"), To);
	return Data;
}

TSharedPtr<FJsonObject> FAssetCommandHandler::HandleRename(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	FString Path, NewName;
	if (!Args.IsValid() || !Args->TryGetStringField(TEXT("path"), Path) || !Args->TryGetStringField(TEXT("name"), NewName))
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("--path and --name are required."));

	FAssetData AssetData = GetAssetRegistry().GetAssetByObjectPath(FSoftObjectPath(Path));
	if (!AssetData.IsValid())
		throw FCommandFailedException(TEXT("NOT_FOUND"), FString::Printf(TEXT("Asset not found: %s"), *Path));

	FString PackageDir = FPackageName::GetLongPackagePath(AssetData.PackageName.ToString());
	FString NewPackageName = PackageDir / NewName;

	FAssetData DestAsset = GetAssetRegistry().GetAssetByObjectPath(FSoftObjectPath(NewPackageName + TEXT(".") + NewName));
	if (DestAsset.IsValid() && !bForce)
		throw FCommandFailedException(TEXT("FORCE_REQUIRED"),
			FString::Printf(TEXT("Asset with name '%s' already exists in this folder. Use --force to overwrite."), *NewName));

	TArray<FAssetRenameData> RenameData;
	RenameData.Add(FAssetRenameData(AssetData.GetAsset(), PackageDir, NewName));

	bool bSuccess = GetAssetTools().RenameAssets(RenameData);
	if (!bSuccess)
		throw FCommandFailedException(TEXT("OPERATION_FAILED"), TEXT("Failed to rename asset."));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("oldPath"), Path);
	Data->SetStringField(TEXT("newName"), NewName);
	return Data;
}

TSharedPtr<FJsonObject> FAssetCommandHandler::HandleDelete(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	if (!bForce) throw FCommandFailedException(TEXT("FORCE_REQUIRED"), TEXT("asset delete always requires --force."));

	FString Path;
	if (!Args.IsValid() || !Args->TryGetStringField(TEXT("path"), Path) || Path.IsEmpty())
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("--path is required."));

	FAssetData AssetData = GetAssetRegistry().GetAssetByObjectPath(FSoftObjectPath(Path));
	if (!AssetData.IsValid())
		throw FCommandFailedException(TEXT("NOT_FOUND"), FString::Printf(TEXT("Asset not found: %s"), *Path));

	TArray<FAssetData> AssetsToDelete = { AssetData };
	int32 NumDeleted = ObjectTools::DeleteAssets(AssetsToDelete, false);

	if (NumDeleted == 0)
		throw FCommandFailedException(TEXT("OPERATION_FAILED"), TEXT("Failed to delete asset."));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), Path);
	Data->SetStringField(TEXT("message"), TEXT("Asset deleted."));
	return Data;
}

TSharedPtr<FJsonObject> FAssetCommandHandler::HandleCreate(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	FString Type, Path;
	if (!Args.IsValid() || !Args->TryGetStringField(TEXT("type"), Type) || !Args->TryGetStringField(TEXT("path"), Path))
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("--type and --path are required."));

	// Check if destination already exists
	FAssetData Existing = GetAssetRegistry().GetAssetByObjectPath(FSoftObjectPath(Path));
	if (Existing.IsValid() && !bForce)
		throw FCommandFailedException(TEXT("FORCE_REQUIRED"),
			FString::Printf(TEXT("Asset already exists: %s. Use --force to overwrite."), *Path));

	FString PackageName = FPackageName::ObjectPathToPackageName(Path);
	FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
	FString PackagePath = FPackageName::GetLongPackagePath(PackageName);

	UObject* NewAsset = nullptr;

	Type = Type.ToLower();
	if (Type == TEXT("material"))
	{
		UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
		NewAsset = GetAssetTools().CreateAsset(AssetName, PackagePath, UMaterial::StaticClass(), Factory);
	}
	else if (Type == TEXT("blueprint"))
	{
		UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
		Factory->ParentClass = AActor::StaticClass();
		NewAsset = GetAssetTools().CreateAsset(AssetName, PackagePath, UBlueprint::StaticClass(), Factory);
	}
	else if (Type == TEXT("dataasset"))
	{
		UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
		NewAsset = GetAssetTools().CreateAsset(AssetName, PackagePath, UDataAsset::StaticClass(), Factory);
	}
	else
	{
		throw FCommandFailedException(TEXT("CLI_USAGE"),
			FString::Printf(TEXT("Unknown asset type: %s. Supported: material, blueprint, dataasset"), *Type));
	}

	if (!NewAsset)
		throw FCommandFailedException(TEXT("OPERATION_FAILED"), TEXT("Failed to create asset."));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), Path);
	Data->SetStringField(TEXT("type"), Type);
	Data->SetStringField(TEXT("message"), TEXT("Asset created."));
	return Data;
}

TSharedPtr<FJsonObject> FAssetCommandHandler::HandleMkdir(const TSharedPtr<FJsonObject>& Args, bool bForce)
{
	FString Path;
	if (!Args.IsValid() || !Args->TryGetStringField(TEXT("path"), Path) || Path.IsEmpty())
		throw FCommandFailedException(TEXT("CLI_USAGE"), TEXT("--path is required."));

	FString DiskPath;
	if (!FPackageName::TryConvertLongPackageNameToFilename(Path + TEXT("/"), DiskPath))
		throw FCommandFailedException(TEXT("OPERATION_FAILED"), FString::Printf(TEXT("Invalid package path: %s"), *Path));

	if (!IFileManager::Get().MakeDirectory(*DiskPath, true) && !IFileManager::Get().DirectoryExists(*DiskPath))
		throw FCommandFailedException(TEXT("OPERATION_FAILED"), FString::Printf(TEXT("Failed to create folder: %s"), *Path));

	GetAssetRegistry().AddPath(Path);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("path"), Path);
	Data->SetStringField(TEXT("message"), TEXT("Folder created."));
	return Data;
}
