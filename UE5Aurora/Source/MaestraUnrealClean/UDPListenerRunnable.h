#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "DataPacket.h"

class FSocket;

class MAESTRAUNREALCLEAN_API FUDPListenerRunnable : public FRunnable
{
public:
	FUDPListenerRunnable(
		const FString& InBindAddress,
		int32 InPort,
		int32 InRecvBufferSize,
		TSharedPtr<TThreadSafeRingBuffer<FDataPacket>> InRingBuffer);

	virtual ~FUDPListenerRunnable() override;

	// FRunnable interface
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

private:
	FString BindAddress;
	int32 Port;
	int32 RecvBufferSize;
	TSharedPtr<TThreadSafeRingBuffer<FDataPacket>> RingBuffer;

	FSocket* Socket;
	TAtomic<bool> bStopping;
	TAtomic<int32> SequenceCounter;
};
