#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DataPacket.h"
#include "UDPReceiverComponent.generated.h"

class FUDPListenerRunnable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDataPacketReceived, const FDataPacket&, Packet);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnJsonMessageReceived, const FString&, JsonString);

UCLASS(ClassGroup=(Networking), meta=(BlueprintSpawnableComponent))
class MAESTRAUNREALCLEAN_API UUDPReceiverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UUDPReceiverComponent();

	// Configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Config")
	FString BindAddress = TEXT("0.0.0.0");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Config")
	int32 Port = 9000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Config")
	int32 BufferSize = 65536;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Config")
	int32 RingBufferCapacity = 1024;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Config")
	int32 MaxPacketsPerTick = 64;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDP Config")
	bool bAutoStart = true;

	// Events
	UPROPERTY(BlueprintAssignable, Category = "UDP Events")
	FOnDataPacketReceived OnDataPacketReceived;

	UPROPERTY(BlueprintAssignable, Category = "UDP Events")
	FOnJsonMessageReceived OnJsonMessageReceived;

	// Blueprint API
	UFUNCTION(BlueprintCallable, Category = "UDP")
	void StartListening();

	UFUNCTION(BlueprintCallable, Category = "UDP")
	void StopListening();

	UFUNCTION(BlueprintCallable, Category = "UDP")
	bool IsListening() const;

	UFUNCTION(BlueprintCallable, Category = "UDP")
	int32 GetPendingPacketCount() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	TSharedPtr<TThreadSafeRingBuffer<FDataPacket>> RingBuffer;
	FUDPListenerRunnable* ListenerRunnable = nullptr;
	FRunnableThread* ListenerThread = nullptr;
	bool bIsListening = false;
};
