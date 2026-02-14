#include "UDPRFDataProvider.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FRFSpectrumFrame UUDPRFDataProvider::GetNextFrame(float DeltaTime)
{
	FScopeLock Lock(&FrameLock);
	if (bHasData)
	{
		return LatestFrame;
	}
	return Super::GetNextFrame(DeltaTime);
}

bool UUDPRFDataProvider::IsReady() const
{
	return bHasData;
}

void UUDPRFDataProvider::IngestPacket(const FString& JsonString)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UDPRFDataProvider: Failed to parse JSON packet"));
		return;
	}

	FScopeLock Lock(&FrameLock);

	LatestFrame.Timestamp = FPlatformTime::Seconds();

	if (JsonObject->HasField(TEXT("freq_min")))
	{
		LatestFrame.FrequencyMinMHz = static_cast<float>(JsonObject->GetNumberField(TEXT("freq_min")));
	}
	if (JsonObject->HasField(TEXT("freq_max")))
	{
		LatestFrame.FrequencyMaxMHz = static_cast<float>(JsonObject->GetNumberField(TEXT("freq_max")));
	}

	const TArray<TSharedPtr<FJsonValue>>* AmplitudesArray = nullptr;
	if (JsonObject->TryGetArrayField(TEXT("amplitudes"), AmplitudesArray))
	{
		LatestFrame.Amplitudes.SetNum(AmplitudesArray->Num());
		for (int32 i = 0; i < AmplitudesArray->Num(); ++i)
		{
			LatestFrame.Amplitudes[i] = static_cast<float>((*AmplitudesArray)[i]->AsNumber());
		}
		LatestFrame.NumBins = AmplitudesArray->Num();
	}

	bHasData = true;
}

void UUDPRFDataProvider::IngestRawBytes(const TArray<uint8>& Bytes)
{
	if (Bytes.Num() < static_cast<int32>(sizeof(float)))
	{
		return;
	}

	int32 FloatCount = Bytes.Num() / sizeof(float);

	FScopeLock Lock(&FrameLock);

	LatestFrame.Timestamp = FPlatformTime::Seconds();
	LatestFrame.Amplitudes.SetNum(FloatCount);
	LatestFrame.NumBins = FloatCount;
	LatestFrame.FrequencyMinMHz = FreqMinMHz;
	LatestFrame.FrequencyMaxMHz = FreqMaxMHz;

	FMemory::Memcpy(LatestFrame.Amplitudes.GetData(), Bytes.GetData(), FloatCount * sizeof(float));

	bHasData = true;
}
