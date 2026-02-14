#include "UDPReceiverComponent.h"
#include "UDPListenerRunnable.h"
#include "HAL/RunnableThread.h"

UUDPReceiverComponent::UUDPReceiverComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UUDPReceiverComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoStart)
	{
		StartListening();
	}
}

void UUDPReceiverComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopListening();
	Super::EndPlay(EndPlayReason);
}

void UUDPReceiverComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsListening || !RingBuffer.IsValid())
	{
		return;
	}

	FDataPacket Packet;
	int32 ProcessedCount = 0;

	while (ProcessedCount < MaxPacketsPerTick && RingBuffer->Dequeue(Packet))
	{
		OnDataPacketReceived.Broadcast(Packet);

		if (!Packet.ParsedJson.IsEmpty())
		{
			OnJsonMessageReceived.Broadcast(Packet.ParsedJson);
		}

		++ProcessedCount;
	}
}

void UUDPReceiverComponent::StartListening()
{
	if (bIsListening)
	{
		return;
	}

	RingBuffer = MakeShared<TThreadSafeRingBuffer<FDataPacket>>(RingBufferCapacity);

	ListenerRunnable = new FUDPListenerRunnable(BindAddress, Port, BufferSize, RingBuffer);
	ListenerThread = FRunnableThread::Create(
		ListenerRunnable,
		TEXT("UDPListenerThread"),
		0,
		TPri_Normal);

	if (ListenerThread)
	{
		bIsListening = true;
		UE_LOG(LogTemp, Log, TEXT("UDPReceiver: Started listening on %s:%d"), *BindAddress, Port);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("UDPReceiver: Failed to create listener thread"));
		delete ListenerRunnable;
		ListenerRunnable = nullptr;
		RingBuffer.Reset();
	}
}

void UUDPReceiverComponent::StopListening()
{
	if (!bIsListening)
	{
		return;
	}

	if (ListenerRunnable)
	{
		ListenerRunnable->Stop();
	}

	if (ListenerThread)
	{
		ListenerThread->WaitForCompletion();
		delete ListenerThread;
		ListenerThread = nullptr;
	}

	if (ListenerRunnable)
	{
		delete ListenerRunnable;
		ListenerRunnable = nullptr;
	}

	RingBuffer.Reset();
	bIsListening = false;

	UE_LOG(LogTemp, Log, TEXT("UDPReceiver: Stopped listening"));
}

bool UUDPReceiverComponent::IsListening() const
{
	return bIsListening;
}

int32 UUDPReceiverComponent::GetPendingPacketCount() const
{
	if (RingBuffer.IsValid())
	{
		return RingBuffer->Num();
	}
	return 0;
}
