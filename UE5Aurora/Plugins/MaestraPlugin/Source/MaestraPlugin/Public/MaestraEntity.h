// Copyright Maestra Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "MaestraTypes.h"
#include "MaestraEntity.generated.h"

class UMaestraClient;

/**
 * Represents an entity in the Maestra platform.
 * Provides access to entity metadata and state management.
 */
UCLASS(BlueprintType)
class MAESTRAPLUGIN_API UMaestraEntity : public UObject
{
    GENERATED_BODY()

public:
    UMaestraEntity();

    void InitializeFromJson(TSharedPtr<FJsonObject> JsonObject, UMaestraClient* InClient);
    void UpdateStateFromJson(TSharedPtr<FJsonObject> StateJson);

    UPROPERTY(BlueprintReadOnly, Category = "Maestra|Entity")
    FString Id;

    UPROPERTY(BlueprintReadOnly, Category = "Maestra|Entity")
    FString Name;

    UPROPERTY(BlueprintReadOnly, Category = "Maestra|Entity")
    FString Slug;

    UPROPERTY(BlueprintReadOnly, Category = "Maestra|Entity")
    FString EntityType;

    UPROPERTY(BlueprintReadOnly, Category = "Maestra|Entity")
    FString ParentId;

    UPROPERTY(BlueprintReadOnly, Category = "Maestra|Entity")
    FString Status;

    UFUNCTION(BlueprintCallable, Category = "Maestra|Entity")
    FString GetStateString(const FString& Key, const FString& DefaultValue = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "Maestra|Entity")
    int32 GetStateInt(const FString& Key, int32 DefaultValue = 0);

    UFUNCTION(BlueprintCallable, Category = "Maestra|Entity")
    float GetStateFloat(const FString& Key, float DefaultValue = 0.0f);

    UFUNCTION(BlueprintCallable, Category = "Maestra|Entity")
    bool GetStateBool(const FString& Key, bool DefaultValue = false);

    UFUNCTION(BlueprintCallable, Category = "Maestra|Entity")
    bool HasStateKey(const FString& Key);

    UFUNCTION(BlueprintCallable, Category = "Maestra|Entity")
    TArray<FString> GetStateKeys();

    UFUNCTION(BlueprintCallable, Category = "Maestra|Entity")
    FString GetStateAsJson();

    UFUNCTION(BlueprintCallable, Category = "Maestra|Entity")
    void UpdateState(const FString& StateJson);

    UFUNCTION(BlueprintCallable, Category = "Maestra|Entity")
    void SetState(const FString& StateJson);

    UFUNCTION(BlueprintCallable, Category = "Maestra|Entity")
    void SetStateValue(const FString& Key, const FString& Value);

    UFUNCTION(BlueprintCallable, Category = "Maestra|Entity")
    void SetStateInt(const FString& Key, int32 Value);

    UFUNCTION(BlueprintCallable, Category = "Maestra|Entity")
    void SetStateFloat(const FString& Key, float Value);

    UFUNCTION(BlueprintCallable, Category = "Maestra|Entity")
    void SetStateBool(const FString& Key, bool Value);

    UPROPERTY(BlueprintAssignable, Category = "Maestra|Entity")
    FOnStateChanged OnStateChanged;

protected:
    UPROPERTY()
    UMaestraClient* Client;

    TSharedPtr<FJsonObject> StateObject;
};
