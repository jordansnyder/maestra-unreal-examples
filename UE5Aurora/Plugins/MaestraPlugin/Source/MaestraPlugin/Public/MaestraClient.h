// Copyright Maestra Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "MaestraTypes.h"
#include "Interfaces/IHttpRequest.h"
#include "IWebSocket.h"
#include "MaestraClient.generated.h"

class UMaestraEntity;

/**
 * Main client for connecting to the Maestra platform.
 * Provides entity management, state synchronization via REST,
 * and real-time streaming via WebSocket Gateway.
 */
UCLASS(BlueprintType, Blueprintable)
class MAESTRAPLUGIN_API UMaestraClient : public UObject
{
    GENERATED_BODY()

public:
    UMaestraClient();

    // --- Initialization ---

    /** Initialize with REST API URL only (legacy mode, HTTP polling) */
    UFUNCTION(BlueprintCallable, Category = "Maestra")
    void Initialize(const FString& ApiUrl);

    /** Initialize with both REST and WebSocket URLs (real-time mode) */
    UFUNCTION(BlueprintCallable, Category = "Maestra")
    void InitializeWithWebSocket(const FString& ApiUrl, const FString& WsUrl);

    // --- REST API ---

    UFUNCTION(BlueprintCallable, Category = "Maestra")
    void GetEntityBySlug(const FString& Slug);

    UFUNCTION(BlueprintCallable, Category = "Maestra")
    void GetEntities(const FString& EntityType = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "Maestra")
    void UpdateEntityState(const FString& EntityId, const FString& StateJson);

    UFUNCTION(BlueprintCallable, Category = "Maestra")
    void SetEntityState(const FString& EntityId, const FString& StateJson);

    UFUNCTION(BlueprintCallable, Category = "Maestra")
    UMaestraEntity* GetCachedEntity(const FString& Slug);

    // --- WebSocket ---

    /** Connect to the Maestra WebSocket Gateway for real-time state updates */
    UFUNCTION(BlueprintCallable, Category = "Maestra|WebSocket")
    void ConnectWebSocket(const FString& WsUrl);

    /** Disconnect from the WebSocket Gateway */
    UFUNCTION(BlueprintCallable, Category = "Maestra|WebSocket")
    void DisconnectWebSocket();

    /** Register interest in an entity slug for client-side filtering of WS messages */
    UFUNCTION(BlueprintCallable, Category = "Maestra|WebSocket")
    void SubscribeToEntity(const FString& Slug);

    /** Unregister interest in an entity slug */
    UFUNCTION(BlueprintCallable, Category = "Maestra|WebSocket")
    void UnsubscribeFromEntity(const FString& Slug);

    /** Publish a message to a NATS subject via the WebSocket Gateway */
    UFUNCTION(BlueprintCallable, Category = "Maestra|WebSocket")
    void PublishMessage(const FString& Subject, const FString& DataJson);

    /** Check if the WebSocket is currently connected */
    UFUNCTION(BlueprintCallable, Category = "Maestra|WebSocket")
    bool IsWebSocketConnected() const { return bWebSocketConnected; }

    // --- Events ---

    UPROPERTY(BlueprintAssignable, Category = "Maestra")
    FOnConnected OnConnected;

    UPROPERTY(BlueprintAssignable, Category = "Maestra")
    FOnError OnError;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEntityReceived, const FString&, Slug, UMaestraEntity*, Entity);
    UPROPERTY(BlueprintAssignable, Category = "Maestra")
    FOnEntityReceived OnEntityReceived;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEntitiesReceived, const TArray<FMaestraEntityData>&, Entities);
    UPROPERTY(BlueprintAssignable, Category = "Maestra")
    FOnEntitiesReceived OnEntitiesReceived;

    /** Fired when a real-time state change is received via WebSocket */
    UPROPERTY(BlueprintAssignable, Category = "Maestra|WebSocket")
    FOnStateChanged OnWebSocketStateChanged;

protected:
    UPROPERTY()
    FString ApiBaseUrl;

    UPROPERTY()
    TMap<FString, UMaestraEntity*> EntityCache;

private:
    // --- REST internals ---
    void HandleGetEntityResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess, FString Slug);
    void HandleGetEntitiesResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess);
    void HandleStateUpdateResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess, FString EntityId);
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> CreateRequest(const FString& Endpoint, const FString& Verb);

    // --- WebSocket internals ---
    TSharedPtr<IWebSocket> WebSocket;
    FString WebSocketUrl;
    bool bWebSocketConnected = false;
    TSet<FString> SubscribedSlugs;
    FTimerHandle PingTimerHandle;
    FTimerHandle ReconnectTimerHandle;
    float ReconnectDelay = 2.0f;

    void OnWsConnected();
    void OnWsConnectionError(const FString& Error);
    void OnWsMessage(const FString& Message);
    void OnWsClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
    void HandleStateChangeMessage(TSharedPtr<FJsonObject> DataObject);
    void SendPing();
    void AttemptReconnect();
};
