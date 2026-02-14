// Copyright Maestra Team. All Rights Reserved.

#include "MaestraClient.h"
#include "MaestraEntity.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"
#include "WebSocketsModule.h"
#include "TimerManager.h"

UMaestraClient::UMaestraClient()
    : ApiBaseUrl(TEXT("http://localhost:8080"))
{
}

// ============================================================================
// Initialization
// ============================================================================

void UMaestraClient::Initialize(const FString& ApiUrl)
{
    ApiBaseUrl = ApiUrl;
    UE_LOG(LogTemp, Log, TEXT("Maestra Client initialized with URL: %s"), *ApiBaseUrl);
    OnConnected.Broadcast(true);
}

void UMaestraClient::InitializeWithWebSocket(const FString& ApiUrl, const FString& WsUrl)
{
    ApiBaseUrl = ApiUrl;
    UE_LOG(LogTemp, Log, TEXT("Maestra Client initialized with REST: %s, WebSocket: %s"), *ApiBaseUrl, *WsUrl);
    OnConnected.Broadcast(true);
    ConnectWebSocket(WsUrl);
}

// ============================================================================
// REST API (unchanged)
// ============================================================================

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> UMaestraClient::CreateRequest(const FString& Endpoint, const FString& Verb)
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(ApiBaseUrl + Endpoint);
    Request->SetVerb(Verb);
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    return Request;
}

void UMaestraClient::GetEntityBySlug(const FString& Slug)
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateRequest(
        FString::Printf(TEXT("/entities/by-slug/%s"), *Slug),
        TEXT("GET")
    );

    Request->OnProcessRequestComplete().BindUObject(this, &UMaestraClient::HandleGetEntityResponse, Slug);
    Request->ProcessRequest();
}

void UMaestraClient::HandleGetEntityResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess, FString Slug)
{
    if (!bSuccess || !Response.IsValid())
    {
        OnError.Broadcast(TEXT("Failed to get entity"));
        return;
    }

    if (Response->GetResponseCode() != 200)
    {
        OnError.Broadcast(FString::Printf(TEXT("HTTP Error %d: %s"),
            Response->GetResponseCode(), *Response->GetContentAsString()));
        return;
    }

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        OnError.Broadcast(TEXT("Failed to parse entity JSON"));
        return;
    }

    UMaestraEntity* Entity = EntityCache.FindRef(Slug);
    if (!Entity)
    {
        Entity = NewObject<UMaestraEntity>(this);
        EntityCache.Add(Slug, Entity);
    }

    Entity->InitializeFromJson(JsonObject, this);
    OnEntityReceived.Broadcast(Slug, Entity);
}

void UMaestraClient::GetEntities(const FString& EntityType)
{
    FString Endpoint = TEXT("/entities");
    if (!EntityType.IsEmpty())
    {
        Endpoint += FString::Printf(TEXT("?type=%s"), *EntityType);
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateRequest(Endpoint, TEXT("GET"));
    Request->OnProcessRequestComplete().BindUObject(this, &UMaestraClient::HandleGetEntitiesResponse);
    Request->ProcessRequest();
}

void UMaestraClient::HandleGetEntitiesResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
{
    if (!bSuccess || !Response.IsValid())
    {
        OnError.Broadcast(TEXT("Failed to get entities"));
        return;
    }

    if (Response->GetResponseCode() != 200)
    {
        OnError.Broadcast(FString::Printf(TEXT("HTTP Error %d"), Response->GetResponseCode()));
        return;
    }

    TArray<TSharedPtr<FJsonValue>> JsonArray;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

    if (!FJsonSerializer::Deserialize(Reader, JsonArray))
    {
        OnError.Broadcast(TEXT("Failed to parse entities JSON"));
        return;
    }

    TArray<FMaestraEntityData> Entities;
    for (const TSharedPtr<FJsonValue>& JsonValue : JsonArray)
    {
        TSharedPtr<FJsonObject> JsonObj = JsonValue->AsObject();
        if (JsonObj.IsValid())
        {
            FMaestraEntityData EntityData;
            EntityData.Id = JsonObj->GetStringField(TEXT("id"));
            EntityData.Name = JsonObj->GetStringField(TEXT("name"));
            EntityData.Slug = JsonObj->GetStringField(TEXT("slug"));
            EntityData.EntityType = JsonObj->GetStringField(TEXT("entity_type"));
            EntityData.ParentId = JsonObj->GetStringField(TEXT("parent_id"));
            EntityData.Status = JsonObj->GetStringField(TEXT("status"));
            Entities.Add(EntityData);
        }
    }

    OnEntitiesReceived.Broadcast(Entities);
}

void UMaestraClient::UpdateEntityState(const FString& EntityId, const FString& StateJson)
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateRequest(
        FString::Printf(TEXT("/entities/%s/state"), *EntityId),
        TEXT("PATCH")
    );

    FString Body = FString::Printf(TEXT("{\"state\":%s,\"source\":\"unreal\"}"), *StateJson);
    Request->SetContentAsString(Body);
    Request->OnProcessRequestComplete().BindUObject(this, &UMaestraClient::HandleStateUpdateResponse, EntityId);
    Request->ProcessRequest();
}

void UMaestraClient::SetEntityState(const FString& EntityId, const FString& StateJson)
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateRequest(
        FString::Printf(TEXT("/entities/%s/state"), *EntityId),
        TEXT("PUT")
    );

    FString Body = FString::Printf(TEXT("{\"state\":%s,\"source\":\"unreal\"}"), *StateJson);
    Request->SetContentAsString(Body);
    Request->OnProcessRequestComplete().BindUObject(this, &UMaestraClient::HandleStateUpdateResponse, EntityId);
    Request->ProcessRequest();
}

void UMaestraClient::HandleStateUpdateResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess, FString EntityId)
{
    if (!bSuccess || !Response.IsValid())
    {
        OnError.Broadcast(TEXT("Failed to update state"));
        return;
    }

    if (Response->GetResponseCode() != 200)
    {
        OnError.Broadcast(FString::Printf(TEXT("HTTP Error %d: %s"),
            Response->GetResponseCode(), *Response->GetContentAsString()));
        return;
    }

    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        FString Slug = JsonObject->GetStringField(TEXT("slug"));
        UMaestraEntity* CachedEntity = EntityCache.FindRef(Slug);
        if (CachedEntity)
        {
            const TSharedPtr<FJsonObject>* StateObj;
            if (JsonObject->TryGetObjectField(TEXT("state"), StateObj))
            {
                CachedEntity->UpdateStateFromJson(*StateObj);
            }
        }
    }
}

UMaestraEntity* UMaestraClient::GetCachedEntity(const FString& Slug)
{
    return EntityCache.FindRef(Slug);
}

// ============================================================================
// WebSocket Gateway
// ============================================================================

void UMaestraClient::ConnectWebSocket(const FString& WsUrl)
{
    if (WebSocket.IsValid() && WebSocket->IsConnected())
    {
        UE_LOG(LogTemp, Warning, TEXT("Maestra WS: Already connected, disconnecting first"));
        DisconnectWebSocket();
    }

    WebSocketUrl = WsUrl;

    // Ensure the WebSockets module is loaded
    FModuleManager::Get().LoadModuleChecked<FWebSocketsModule>(TEXT("WebSockets"));

    WebSocket = FWebSocketsModule::Get().CreateWebSocket(WsUrl, TEXT("ws"));

    WebSocket->OnConnected().AddUObject(this, &UMaestraClient::OnWsConnected);
    WebSocket->OnConnectionError().AddUObject(this, &UMaestraClient::OnWsConnectionError);
    WebSocket->OnMessage().AddUObject(this, &UMaestraClient::OnWsMessage);
    WebSocket->OnClosed().AddUObject(this, &UMaestraClient::OnWsClosed);

    UE_LOG(LogTemp, Log, TEXT("Maestra WS: Connecting to %s"), *WsUrl);
    WebSocket->Connect();
}

void UMaestraClient::DisconnectWebSocket()
{
    // Clear timers
    if (UWorld* World = GEngine ? GEngine->GetWorldContexts()[0].World() : nullptr)
    {
        World->GetTimerManager().ClearTimer(PingTimerHandle);
        World->GetTimerManager().ClearTimer(ReconnectTimerHandle);
    }

    if (WebSocket.IsValid())
    {
        if (WebSocket->IsConnected())
        {
            WebSocket->Close();
        }
        WebSocket.Reset();
    }

    bWebSocketConnected = false;
    UE_LOG(LogTemp, Log, TEXT("Maestra WS: Disconnected"));
}

void UMaestraClient::SubscribeToEntity(const FString& Slug)
{
    SubscribedSlugs.Add(Slug);
    UE_LOG(LogTemp, Log, TEXT("Maestra WS: Subscribed to entity '%s' (%d total subscriptions)"),
        *Slug, SubscribedSlugs.Num());
}

void UMaestraClient::UnsubscribeFromEntity(const FString& Slug)
{
    SubscribedSlugs.Remove(Slug);
}

void UMaestraClient::PublishMessage(const FString& Subject, const FString& DataJson)
{
    if (!WebSocket.IsValid() || !WebSocket->IsConnected())
    {
        UE_LOG(LogTemp, Warning, TEXT("Maestra WS: Cannot publish, not connected"));
        return;
    }

    FString Message = FString::Printf(
        TEXT("{\"type\":\"publish\",\"subject\":\"%s\",\"data\":%s}"),
        *Subject, *DataJson);

    WebSocket->Send(Message);
}

void UMaestraClient::OnWsConnected()
{
    bWebSocketConnected = true;
    ReconnectDelay = 2.0f; // Reset backoff
    UE_LOG(LogTemp, Log, TEXT("Maestra WS: Connected to gateway"));

    // Start ping keep-alive timer (every 30 seconds)
    if (UWorld* World = GEngine ? GEngine->GetWorldContexts()[0].World() : nullptr)
    {
        World->GetTimerManager().SetTimer(PingTimerHandle, FTimerDelegate::CreateUObject(this, &UMaestraClient::SendPing), 30.0f, true);
    }
}

void UMaestraClient::OnWsConnectionError(const FString& Error)
{
    bWebSocketConnected = false;
    UE_LOG(LogTemp, Error, TEXT("Maestra WS: Connection error: %s"), *Error);
    OnError.Broadcast(FString::Printf(TEXT("WebSocket connection error: %s"), *Error));
    AttemptReconnect();
}

void UMaestraClient::OnWsMessage(const FString& Message)
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        return;
    }

    FString Type;
    if (!JsonObject->TryGetStringField(TEXT("type"), Type))
    {
        return;
    }

    if (Type == TEXT("message"))
    {
        // Real-time message from NATS via gateway
        const TSharedPtr<FJsonObject>* DataObject;
        if (JsonObject->TryGetObjectField(TEXT("data"), DataObject) && DataObject->IsValid())
        {
            FString DataType;
            if ((*DataObject)->TryGetStringField(TEXT("type"), DataType) && DataType == TEXT("state_changed"))
            {
                HandleStateChangeMessage(*DataObject);
            }
        }
    }
    else if (Type == TEXT("welcome"))
    {
        FString WelcomeMsg;
        JsonObject->TryGetStringField(TEXT("message"), WelcomeMsg);
        UE_LOG(LogTemp, Log, TEXT("Maestra WS: %s"), *WelcomeMsg);
    }
    else if (Type == TEXT("pong"))
    {
        // Keep-alive acknowledged
    }
    else if (Type == TEXT("error"))
    {
        FString ErrorMsg;
        JsonObject->TryGetStringField(TEXT("message"), ErrorMsg);
        UE_LOG(LogTemp, Warning, TEXT("Maestra WS: Server error: %s"), *ErrorMsg);
        OnError.Broadcast(FString::Printf(TEXT("WebSocket server error: %s"), *ErrorMsg));
    }
}

void UMaestraClient::HandleStateChangeMessage(TSharedPtr<FJsonObject> DataObject)
{
    if (!DataObject.IsValid())
    {
        return;
    }

    FString EntitySlug;
    if (!DataObject->TryGetStringField(TEXT("entity_slug"), EntitySlug))
    {
        return;
    }

    // Client-side filter: only process entities we've subscribed to
    if (!SubscribedSlugs.Contains(EntitySlug))
    {
        UE_LOG(LogTemp, Verbose, TEXT("Maestra WS: Ignoring state_changed for unsubscribed entity '%s'"), *EntitySlug);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Maestra WS: State changed for subscribed entity '%s'"), *EntitySlug);

    // Get or create cached entity
    UMaestraEntity* Entity = EntityCache.FindRef(EntitySlug);
    if (!Entity)
    {
        Entity = NewObject<UMaestraEntity>(this);
        EntityCache.Add(EntitySlug, Entity);

        // Set basic metadata from the event
        Entity->Slug = EntitySlug;
        FString EntityId;
        if (DataObject->TryGetStringField(TEXT("entity_id"), EntityId))
        {
            Entity->Id = EntityId;
        }
        FString EntityType;
        if (DataObject->TryGetStringField(TEXT("entity_type"), EntityType))
        {
            Entity->EntityType = EntityType;
        }
    }

    // Update state from the current_state field
    const TSharedPtr<FJsonObject>* CurrentStateObj;
    if (DataObject->TryGetObjectField(TEXT("current_state"), CurrentStateObj) && CurrentStateObj->IsValid())
    {
        Entity->UpdateStateFromJson(*CurrentStateObj);
    }

    // Broadcast entity received (same delegate as REST, consumers don't need to care about transport)
    OnEntityReceived.Broadcast(EntitySlug, Entity);

    // Build and broadcast state change event
    FMaestraStateChangeEvent Event;
    DataObject->TryGetStringField(TEXT("entity_id"), Event.EntityId);
    Event.EntitySlug = EntitySlug;
    DataObject->TryGetStringField(TEXT("entity_type"), Event.EntityType);
    DataObject->TryGetStringField(TEXT("source"), Event.Source);
    Event.Timestamp = FDateTime::UtcNow();

    const TArray<TSharedPtr<FJsonValue>>* ChangedKeysArray;
    if (DataObject->TryGetArrayField(TEXT("changed_keys"), ChangedKeysArray))
    {
        for (const TSharedPtr<FJsonValue>& Val : *ChangedKeysArray)
        {
            FString Key;
            if (Val->TryGetString(Key))
            {
                Event.ChangedKeys.Add(Key);
            }
        }
    }

    OnWebSocketStateChanged.Broadcast(Event);
}

void UMaestraClient::OnWsClosed(int32 StatusCode, const FString& Reason, bool bWasClean)
{
    bWebSocketConnected = false;

    if (UWorld* World = GEngine ? GEngine->GetWorldContexts()[0].World() : nullptr)
    {
        World->GetTimerManager().ClearTimer(PingTimerHandle);
    }

    UE_LOG(LogTemp, Warning, TEXT("Maestra WS: Connection closed (code=%d, reason=%s, clean=%s)"),
        StatusCode, *Reason, bWasClean ? TEXT("yes") : TEXT("no"));

    AttemptReconnect();
}

void UMaestraClient::SendPing()
{
    if (WebSocket.IsValid() && WebSocket->IsConnected())
    {
        WebSocket->Send(TEXT("{\"type\":\"ping\"}"));
    }
}

void UMaestraClient::AttemptReconnect()
{
    if (WebSocketUrl.IsEmpty())
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Maestra WS: Attempting reconnect in %.1f seconds"), ReconnectDelay);

    if (UWorld* World = GEngine ? GEngine->GetWorldContexts()[0].World() : nullptr)
    {
        World->GetTimerManager().SetTimer(
            ReconnectTimerHandle,
            FTimerDelegate::CreateLambda([this]()
            {
                if (!bWebSocketConnected && !WebSocketUrl.IsEmpty())
                {
                    ConnectWebSocket(WebSocketUrl);
                }
            }),
            ReconnectDelay,
            false
        );
    }

    // Exponential backoff: 2s, 4s, 8s, max 30s
    ReconnectDelay = FMath::Min(ReconnectDelay * 2.0f, 30.0f);
}
