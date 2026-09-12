#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

// ---------------------------------------------------------------------------------------------
// Helpers shared between command handlers.
//
// Keep this file free of knowledge about any particular asset type — a handler that needs to do
// something specific to materials, behavior trees, or animation should do it at the call site.
// Handlers were originally allowed to keep private copies of these; the copies are being retired
// as each handler is touched, so prefer these versions in new code.
// ---------------------------------------------------------------------------------------------

namespace HandlerUtils
{
	/** "/Game/Foo/M_Bar" -> "/Game/Foo/M_Bar.M_Bar" (already-qualified paths pass through). */
	FString NormalizeObjectPath(const FString& InPath);

	/** Reads a non-empty string argument, or throws CLI_USAGE carrying Usage. */
	FString RequireStringArg(const TSharedPtr<FJsonObject>& Args, const TCHAR* Field, const TCHAR* Usage);

	/** Converts a JSON value into a string ImportText can parse for the given property. */
	FString JsonToPropertyText(const FProperty* Property, const TSharedPtr<FJsonValue>& Value);

	/**
	 * Writes the package straight to disk.
	 * FEditorFileUtils / UEditorLoadingAndSavingUtils are not an option here: they pop a modal
	 * source-control dialog, which would hang a CLI call indefinitely.
	 */
	bool SaveAssetPackage(UObject* Asset);

	/**
	 * Called once per property before the default handling. Returning true means the property was
	 * dealt with and the default path is skipped.
	 */
	using FPropertyOverrideFn = TFunctionRef<bool(FProperty& Property, void* ValuePtr, const TSharedPtr<FJsonValue>& Value)>;

	/**
	 * Applies a {"PropertyName": value} object onto any UObject via reflection.
	 * Object-typed properties take a content path; everything else goes through ImportText.
	 * Throws NOT_FOUND for an unknown property or unloadable asset, OPERATION_FAILED for a value
	 * the property will not accept.
	 */
	void ApplyValues(UObject* Target, const TSharedPtr<FJsonObject>& Values);

	/** ApplyValues with a hook for property types that need more than ImportText can give. */
	void ApplyValues(UObject* Target, const TSharedPtr<FJsonObject>& Values, FPropertyOverrideFn Override);
}
