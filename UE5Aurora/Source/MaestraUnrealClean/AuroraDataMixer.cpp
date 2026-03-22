#include "AuroraDataMixer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"

UAuroraDataMixer::UAuroraDataMixer()
{
	PrimaryComponentTick.bCanEverTick = true;
}

// ============================================================================
// Lifecycle
// ============================================================================

void UAuroraDataMixer::BeginPlay()
{
	Super::BeginPlay();

	EntropyHistory.SetNumZeroed(EntropyWindowSize);

	if (bUseMaestra && !MaestraEntitySlug.IsEmpty())
	{
		InitializeMaestra();
	}

	if (bUseUDP)
	{
		InitializeUDP();
	}

	if (bUseRFSpectrum)
	{
		InitializeRF();
	}
}

void UAuroraDataMixer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (MaestraClient)
	{
		MaestraClient->DisconnectWebSocket();
	}

	Super::EndPlay(EndPlayReason);
}

void UAuroraDataMixer::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ElapsedTime += DeltaTime;

	// Poll data sources
	if (bUseMaestra)
	{
		ReadMaestraData();
	}

	if (bUseRFSpectrum)
	{
		ReadRFData(DeltaTime);
	}

	// Compute mixed parameters
	CurrentParameters = ComputeParameters(DeltaTime);
	OnParametersUpdated.Broadcast(CurrentParameters);
}

// ============================================================================
// Source Initialization
// ============================================================================

void UAuroraDataMixer::InitializeMaestra()
{
	MaestraClient = NewObject<UMaestraClient>(this);
	MaestraClient->OnEntityReceived.AddDynamic(this, &UAuroraDataMixer::OnMaestraEntityReceived);
	MaestraClient->SubscribeToEntity(MaestraEntitySlug);
	MaestraClient->InitializeWithWebSocket(MaestraApiUrl, MaestraWebSocketUrl);
	MaestraClient->GetEntityBySlug(MaestraEntitySlug);

	UE_LOG(LogTemp, Log, TEXT("AuroraDataMixer: Maestra connected for entity '%s'"), *MaestraEntitySlug);
}

void UAuroraDataMixer::InitializeUDP()
{
	UDPReceiver = NewObject<UUDPReceiverComponent>(GetOwner());
	if (UDPReceiver)
	{
		UDPReceiver->Port = UDPPort;
		UDPReceiver->bAutoStart = true;
		UDPReceiver->RegisterComponent();
		UDPReceiver->OnJsonMessageReceived.AddDynamic(this, &UAuroraDataMixer::OnUDPJsonReceived);
		UE_LOG(LogTemp, Log, TEXT("AuroraDataMixer: UDP receiver started on port %d"), UDPPort);
	}
}

void UAuroraDataMixer::InitializeRF()
{
	if (bUseUDP)
	{
		// Use real RF data from UDP stream
		UDPRFProvider = NewObject<UUDPRFDataProvider>(this);
		UDPRFProvider->Configure(88.0f, 108.0f, RFNumBins, 30.0f);
		RFProvider = UDPRFProvider;
		UE_LOG(LogTemp, Log, TEXT("AuroraDataMixer: UDP RF spectrum provider initialized with %d bins"), RFNumBins);
	}
	else
	{
		// Fall back to simulated FM radio
		USimulatedRFDataProvider* SimProvider = NewObject<USimulatedRFDataProvider>(this);
		SimProvider->Configure(88.0f, 108.0f, RFNumBins, 30.0f);
		RFProvider = SimProvider;
		UE_LOG(LogTemp, Log, TEXT("AuroraDataMixer: Simulated RF spectrum provider initialized with %d bins"), RFNumBins);
	}
}

// ============================================================================
// Source Callbacks
// ============================================================================

void UAuroraDataMixer::OnMaestraEntityReceived(const FString& Slug, UMaestraEntity* Entity)
{
	if (Slug == MaestraEntitySlug)
	{
		TrackedEntity = Entity;
	}
}

void UAuroraDataMixer::OnUDPJsonReceived(const FString& JsonString)
{
	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
	{
		// Route RF spectrum packets to the UDP RF provider
		if (JsonObj->HasField(TEXT("amplitudes")) && UDPRFProvider)
		{
			UDPRFProvider->IngestPacket(JsonString);
			return;
		}

		// Accept a generic "value" field (0-1) or "intensity"
		if (JsonObj->HasField(TEXT("value")))
		{
			RawUDPValue = FMath::Clamp(static_cast<float>(JsonObj->GetNumberField(TEXT("value"))), 0.0f, 1.0f);
		}
		else if (JsonObj->HasField(TEXT("intensity")))
		{
			RawUDPValue = FMath::Clamp(static_cast<float>(JsonObj->GetNumberField(TEXT("intensity"))), 0.0f, 1.0f);
		}
	}
}

// ============================================================================
// Data Reading
// ============================================================================

void UAuroraDataMixer::ReadMaestraData()
{
	if (TrackedEntity)
	{
		RawMaestraValue = FMath::Clamp(
			TrackedEntity->GetStateFloat(MaestraDataKey, 0.0f),
			0.0f, 1.0f
		);
	}
}

void UAuroraDataMixer::ReadRFData(float DeltaTime)
{
	if (RFProvider && RFProvider->IsReady())
	{
		FRFSpectrumFrame Frame = RFProvider->GetNextFrame(DeltaTime);
		RawRFBins = Frame.Amplitudes;
	}
}

// ============================================================================
// Manual / External Input
// ============================================================================

void UAuroraDataMixer::InjectManualValue(float Value)
{
	RawMaestraValue = FMath::Clamp(Value, 0.0f, 1.0f);
}

void UAuroraDataMixer::FeedAudioSpectrum(const TArray<float>& FrequencyBands, float OverallAmplitude)
{
	RawAudioBands = FrequencyBands;
	RawAudioAmplitude = FMath::Clamp(OverallAmplitude, 0.0f, 1.0f);
}

// ============================================================================
// Entropy Computation (Shannon entropy over recent aggregate values)
// ============================================================================

void UAuroraDataMixer::RecordEntropyValue(float Value)
{
	EntropyHistory[EntropyWriteIndex] = Value;
	EntropyWriteIndex = (EntropyWriteIndex + 1) % EntropyWindowSize;
}

float UAuroraDataMixer::ComputeEntropy() const
{
	// Bin the history into 16 buckets and compute Shannon entropy
	constexpr int32 NumBuckets = 16;
	int32 Buckets[NumBuckets] = {};
	int32 TotalSamples = 0;

	for (float V : EntropyHistory)
	{
		int32 Bucket = FMath::Clamp(FMath::FloorToInt32(V * NumBuckets), 0, NumBuckets - 1);
		Buckets[Bucket]++;
		TotalSamples++;
	}

	if (TotalSamples == 0) return 0.0f;

	float Entropy = 0.0f;
	for (int32 i = 0; i < NumBuckets; ++i)
	{
		if (Buckets[i] > 0)
		{
			float P = static_cast<float>(Buckets[i]) / static_cast<float>(TotalSamples);
			Entropy -= P * FMath::Loge(P);
		}
	}

	// Normalize to 0-1 (max entropy = ln(NumBuckets))
	float MaxEntropy = FMath::Loge(static_cast<float>(NumBuckets));
	return (MaxEntropy > 0.0f) ? FMath::Clamp(Entropy / MaxEntropy, 0.0f, 1.0f) : 0.0f;
}

// ============================================================================
// Substorm Detection (data spike → bright flare)
// ============================================================================

void UAuroraDataMixer::DetectSubstorm(float NewAggregate)
{
	float Delta = NewAggregate - PreviousAggregateValue;
	PreviousAggregateValue = NewAggregate;

	// A sudden positive spike exceeding threshold triggers a substorm
	constexpr float SpikeThreshold = 0.15f;
	if (Delta > SpikeThreshold)
	{
		// Inject energy proportional to the spike magnitude
		SubstormEnergy = FMath::Min(SubstormEnergy + Delta * 3.0f, 1.0f);
	}
}

// ============================================================================
// Aggregate Intensity
// ============================================================================

float UAuroraDataMixer::ComputeAggregateIntensity() const
{
	float Total = 0.0f;
	float WeightSum = 0.0f;

	if (bUseMaestra)
	{
		Total += RawMaestraValue * SourceWeights.MaestraWeight;
		WeightSum += SourceWeights.MaestraWeight;
	}

	if (bUseUDP)
	{
		Total += RawUDPValue * SourceWeights.UDPWeight;
		WeightSum += SourceWeights.UDPWeight;
	}

	if (bUseAudio)
	{
		Total += RawAudioAmplitude * SourceWeights.AudioWeight;
		WeightSum += SourceWeights.AudioWeight;
	}

	if (bUseRFSpectrum && RawRFBins.Num() > 0)
	{
		// Average RF energy as a 0-1 signal (amplitudes are in dBm, normalize from -95..-10)
		float RFAvg = 0.0f;
		for (float Amp : RawRFBins)
		{
			float Normalized = FMath::Clamp((Amp + 95.0f) / 85.0f, 0.0f, 1.0f);
			RFAvg += Normalized;
		}
		RFAvg /= static_cast<float>(RawRFBins.Num());
		Total += RFAvg * SourceWeights.RFSpectrumWeight;
		WeightSum += SourceWeights.RFSpectrumWeight;
	}

	// If no sources are active, return a gentle simulated baseline
	if (WeightSum <= 0.0f)
	{
		float T = ElapsedTime;
		float Base = 0.2f;
		float Tide = FMath::Max(0.0f, FMath::Sin(T * 0.012f)) * 0.15f;
		float Drift = FMath::Sin(T * 0.005f + 1.7f) * 0.08f;
		return FMath::Clamp(Base + Tide + Drift, 0.05f, 0.5f);
	}

	return FMath::Clamp(Total / WeightSum, 0.0f, 1.0f);
}

// ============================================================================
// Core Parameter Computation
// ============================================================================

FAuroraParameters UAuroraDataMixer::ComputeParameters(float DeltaTime)
{
	float Aggregate = ComputeAggregateIntensity();

	// Record for entropy analysis
	RecordEntropyValue(Aggregate);
	CurrentEntropy = ComputeEntropy();

	// Detect substorms
	DetectSubstorm(Aggregate);

	// Decay substorm energy
	SubstormEnergy = FMath::Max(0.0f, SubstormEnergy - SubstormDecayRate * DeltaTime);

	float Smooth = GlobalSmoothingFactor;

	// --- Intensity ---
	SmoothedIntensity = FMath::Lerp(SmoothedIntensity, Aggregate, Smooth);

	// --- Audio pulse ---
	SmoothedAudio = FMath::Lerp(SmoothedAudio, RawAudioAmplitude, Smooth * 3.0f); // Audio reacts faster

	// --- Fold count (data activity → more complex folds) ---
	// Use a combination of RF bin variance and aggregate change rate
	float TargetFolds = FMath::Lerp(2.0f, 10.0f, FMath::Clamp(CurrentEntropy * 1.5f, 0.0f, 1.0f));
	SmoothedFoldCount = FMath::Lerp(SmoothedFoldCount, TargetFolds, Smooth * 0.5f);

	// --- Vertical extent (aggregate signal strength) ---
	float TargetExtent = FMath::Clamp(Aggregate * 1.2f + SubstormEnergy * 0.3f, 0.1f, 1.0f);
	SmoothedVerticalExtent = FMath::Lerp(SmoothedVerticalExtent, TargetExtent, Smooth);

	// --- Wave speed (RF frequency encoding) ---
	float TargetWaveSpeed = 1.0f;
	if (bUseRFSpectrum && RawRFBins.Num() > 0)
	{
		// Find dominant frequency bin → map to wave speed
		int32 PeakBin = 0;
		float PeakVal = -999.0f;
		for (int32 i = 0; i < RawRFBins.Num(); ++i)
		{
			if (RawRFBins[i] > PeakVal)
			{
				PeakVal = RawRFBins[i];
				PeakBin = i;
			}
		}
		float NormalizedFreq = static_cast<float>(PeakBin) / FMath::Max(1.0f, static_cast<float>(RawRFBins.Num() - 1));
		TargetWaveSpeed = FMath::Lerp(0.3f, 3.0f, NormalizedFreq);
	}
	SmoothedWaveSpeed = FMath::Lerp(SmoothedWaveSpeed, TargetWaveSpeed, Smooth);

	// --- Per-bin RF spectrum smoothing ---
	if (bUseRFSpectrum && RawRFBins.Num() > 0)
	{
		int32 NumBins = RawRFBins.Num();
		if (SmoothedRFBins.Num() != NumBins)
		{
			SmoothedRFBins.SetNumZeroed(NumBins);
		}

		float RFSmooth = FMath::Min(Smooth * 2.0f, 1.0f);
		for (int32 i = 0; i < NumBins; ++i)
		{
			float Normalized = FMath::Clamp((RawRFBins[i] + 95.0f) / 85.0f, 0.0f, 1.0f);
			SmoothedRFBins[i] = FMath::Lerp(SmoothedRFBins[i], Normalized, RFSmooth);
		}
	}

	// --- Build output ---
	FAuroraParameters Params;

	Params.Intensity = SmoothedIntensity;

	// Hue: Maestra potentiometer → spectral range (green→cyan→purple→red)
	// Uses real aurora emission anchors
	float HueInput = bUseMaestra ? RawMaestraValue : SmoothedIntensity;
	// Map through aurora-realistic color stops:
	// 0.0 → deep green (0.33), 0.5 → cyan-blue (0.55), 1.0 → crimson-red (0.0)
	if (HueInput < 0.5f)
	{
		Params.Hue = FMath::Lerp(0.33f, 0.55f, HueInput * 2.0f);
	}
	else
	{
		// Wrap through purple (0.75) down toward red (0.95 in normalized hue)
		Params.Hue = FMath::Lerp(0.55f, 0.95f, (HueInput - 0.5f) * 2.0f);
	}

	// Saturation: desaturate if data sources are disconnected
	bool bHealthy = true;
	if (bUseMaestra && !TrackedEntity)
	{
		bHealthy = false;
	}
	Params.Saturation = bHealthy ? FMath::Lerp(0.6f, 0.95f, SmoothedIntensity) : 0.3f;

	Params.FoldCount = SmoothedFoldCount;
	Params.VerticalExtent = SmoothedVerticalExtent;
	Params.WaveSpeed = SmoothedWaveSpeed;

	// Noise complexity from entropy
	Params.NoiseOctaves = FMath::Lerp(1.5f, 5.0f, CurrentEntropy);
	Params.NoisePersistence = FMath::Lerp(0.3f, 0.65f, CurrentEntropy);

	// Audio breathing pulse
	Params.LuminancePulse = SmoothedAudio;

	// Substorm flare
	Params.SubstormFlare = SubstormEnergy;

	// Per-bin RF spectrum for spatial curtain modulation
	if (bUseRFSpectrum && SmoothedRFBins.Num() > 0)
	{
		Params.RFSpectrumBins = SmoothedRFBins;
	}

	return Params;
}
