#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "DataPacket.generated.h"

USTRUCT(BlueprintType)
struct MAESTRAUNREALCLEAN_API FDataPacket
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UDP")
	double Timestamp = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "UDP")
	TArray<uint8> RawBytes;

	UPROPERTY(BlueprintReadOnly, Category = "UDP")
	FString ParsedJson;

	UPROPERTY(BlueprintReadOnly, Category = "UDP")
	FString SenderAddress;

	UPROPERTY(BlueprintReadOnly, Category = "UDP")
	int32 SequenceNumber = 0;
};

/**
 * Thread-safe ring buffer for single-producer single-consumer usage.
 * Uses a critical section for safety at expected data rates.
 */
template<typename T>
class TThreadSafeRingBuffer
{
public:
	explicit TThreadSafeRingBuffer(int32 InCapacity)
		: MaxSize(FMath::Max(InCapacity, 2))
		, Head(0)
		, Tail(0)
		, Count(0)
	{
		Buffer.SetNum(MaxSize);
	}

	bool Enqueue(const T& Item)
	{
		FScopeLock Lock(&Mutex);
		if (Count >= MaxSize)
		{
			return false; // Full — drop
		}
		Buffer[Tail] = Item;
		Tail = (Tail + 1) % MaxSize;
		++Count;
		return true;
	}

	bool Dequeue(T& OutItem)
	{
		FScopeLock Lock(&Mutex);
		if (Count == 0)
		{
			return false;
		}
		OutItem = MoveTemp(Buffer[Head]);
		Head = (Head + 1) % MaxSize;
		--Count;
		return true;
	}

	int32 Num() const
	{
		FScopeLock Lock(&Mutex);
		return Count;
	}

	void Reset()
	{
		FScopeLock Lock(&Mutex);
		Head = 0;
		Tail = 0;
		Count = 0;
	}

private:
	TArray<T> Buffer;
	int32 MaxSize;
	int32 Head;
	int32 Tail;
	int32 Count;
	mutable FCriticalSection Mutex;
};
