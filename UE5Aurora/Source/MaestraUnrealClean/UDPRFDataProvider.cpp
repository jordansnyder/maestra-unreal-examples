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
	// Try SDRF format first
	if (IngestSDRFPacket(Bytes))
	{
		return;
	}

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

bool UUDPRFDataProvider::IsSDRFPacket(const TArray<uint8>& Bytes)
{
	if (Bytes.Num() < 36)
	{
		return false;
	}
	uint32 Magic = 0;
	FMemory::Memcpy(&Magic, Bytes.GetData(), sizeof(uint32));
	return Magic == 0x53445246; // "SDRF"
}

bool UUDPRFDataProvider::IngestSDRFPacket(const TArray<uint8>& Bytes)
{
	if (!IsSDRFPacket(Bytes))
	{
		return false;
	}

	const uint8* Data = Bytes.GetData();

	// Parse header (all little-endian, matching x86/ARM defaults)
	// [0:4] magic (already validated)
	// [4:8] sequence
	// [8:16] center_freq (Hz, float64)
	// [16:24] sample_rate (Hz, float64)
	// [24:32] reserved
	// [32:36] fft_size (uint32)
	double CenterFreqHz = 0.0;
	double SampleRateHz = 0.0;
	uint32 FFTSize = 0;

	FMemory::Memcpy(&CenterFreqHz, Data + 8, sizeof(double));
	FMemory::Memcpy(&SampleRateHz, Data + 16, sizeof(double));
	FMemory::Memcpy(&FFTSize, Data + 32, sizeof(uint32));

	// Validate
	int32 ExpectedBodyBytes = static_cast<int32>(FFTSize) * sizeof(float);
	if (Bytes.Num() < 36 + ExpectedBodyBytes || FFTSize == 0 || FFTSize > 16384)
	{
		UE_LOG(LogTemp, Warning, TEXT("UDPRFDataProvider: SDRF packet invalid (fft=%u, bytes=%d, need=%d)"),
			FFTSize, Bytes.Num(), 36 + ExpectedBodyBytes);
		return false;
	}

	// Convert center_freq + sample_rate to freq_min/freq_max in MHz
	double FreqMinHz = CenterFreqHz - (SampleRateHz / 2.0);
	double FreqMaxHz = CenterFreqHz + (SampleRateHz / 2.0);

	FScopeLock Lock(&FrameLock);

	LatestFrame.Timestamp = FPlatformTime::Seconds();
	LatestFrame.FrequencyMinMHz = static_cast<float>(FreqMinHz / 1.0e6);
	LatestFrame.FrequencyMaxMHz = static_cast<float>(FreqMaxHz / 1.0e6);
	LatestFrame.NumBins = static_cast<int32>(FFTSize);
	LatestFrame.Amplitudes.SetNum(static_cast<int32>(FFTSize));

	FMemory::Memcpy(LatestFrame.Amplitudes.GetData(), Data + 36, ExpectedBodyBytes);

	bHasData = true;

	UE_LOG(LogTemp, Verbose, TEXT("UDPRFDataProvider: SDRF packet ingested — %u bins, %.3f-%.3f MHz"),
		FFTSize, LatestFrame.FrequencyMinMHz, LatestFrame.FrequencyMaxMHz);

	return true;
}
