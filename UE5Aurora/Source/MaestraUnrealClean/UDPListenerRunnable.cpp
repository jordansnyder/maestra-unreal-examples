#include "UDPListenerRunnable.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "HAL/PlatformTime.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FUDPListenerRunnable::FUDPListenerRunnable(
	const FString& InBindAddress,
	int32 InPort,
	int32 InRecvBufferSize,
	TSharedPtr<TThreadSafeRingBuffer<FDataPacket>> InRingBuffer)
	: BindAddress(InBindAddress)
	, Port(InPort)
	, RecvBufferSize(InRecvBufferSize)
	, RingBuffer(InRingBuffer)
	, Socket(nullptr)
	, bStopping(false)
	, SequenceCounter(0)
{
}

FUDPListenerRunnable::~FUDPListenerRunnable()
{
	if (Socket)
	{
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
	}
}

bool FUDPListenerRunnable::Init()
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("UDPListener: Failed to get socket subsystem"));
		return false;
	}

	Socket = SocketSubsystem->CreateSocket(NAME_DGram, TEXT("UDPListener"), false);
	if (!Socket)
	{
		UE_LOG(LogTemp, Error, TEXT("UDPListener: Failed to create UDP socket"));
		return false;
	}

	Socket->SetReuseAddr(true);
	Socket->SetNonBlocking(true);
	Socket->SetReceiveBufferSize(RecvBufferSize, RecvBufferSize);

	TSharedRef<FInternetAddr> Addr = SocketSubsystem->CreateInternetAddr();
	bool bIsValid = false;
	Addr->SetIp(*BindAddress, bIsValid);
	if (!bIsValid)
	{
		UE_LOG(LogTemp, Error, TEXT("UDPListener: Invalid bind address: %s"), *BindAddress);
		return false;
	}
	Addr->SetPort(Port);

	if (!Socket->Bind(*Addr))
	{
		UE_LOG(LogTemp, Error, TEXT("UDPListener: Failed to bind to %s:%d"), *BindAddress, Port);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("UDPListener: Bound to %s:%d"), *BindAddress, Port);
	return true;
}

uint32 FUDPListenerRunnable::Run()
{
	TArray<uint8> RecvBuffer;
	RecvBuffer.SetNumUninitialized(65536);

	while (!bStopping)
	{
		if (!Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromMilliseconds(10)))
		{
			continue;
		}

		TSharedRef<FInternetAddr> SenderAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
		int32 BytesRead = 0;

		if (Socket->RecvFrom(RecvBuffer.GetData(), RecvBuffer.Num(), BytesRead, *SenderAddr))
		{
			if (BytesRead > 0)
			{
				FDataPacket Packet;
				Packet.Timestamp = FPlatformTime::Seconds();
				Packet.SequenceNumber = ++SequenceCounter;
				Packet.SenderAddress = SenderAddr->ToString(true);
				Packet.RawBytes.Append(RecvBuffer.GetData(), BytesRead);

				// Attempt UTF-8 JSON parse
				FString PayloadString;
				const FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(RecvBuffer.GetData()), BytesRead);
				PayloadString = FString(Converter.Length(), Converter.Get());

				TSharedPtr<FJsonObject> JsonObject;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PayloadString);
				if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
				{
					Packet.ParsedJson = PayloadString;
				}

				RingBuffer->Enqueue(MoveTemp(Packet));
			}
		}
	}

	return 0;
}

void FUDPListenerRunnable::Stop()
{
	bStopping = true;
}

void FUDPListenerRunnable::Exit()
{
	if (Socket)
	{
		Socket->Close();
	}
	UE_LOG(LogTemp, Log, TEXT("UDPListener: Exited"));
}
