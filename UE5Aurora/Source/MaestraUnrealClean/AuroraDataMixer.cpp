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

	if (bUseRFSpectrum || bUseUDP)
	{
		// Always initialize RF when UDP is on — SDRF binary packets may arrive
		InitializeRF();
		if (!bUseRFSpectrum && bUseUDP)
		{
			// Auto-enable RF spectrum when UDP is active so incoming SDR data is used
			bUseRFSpectrum = true;
			UE_LOG(LogTemp, Log, TEXT("AuroraDataMixer: Auto-enabled RF spectrum (UDP is active, SDR data expected)"));
		}
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

		// Fall back to HTTP polling if WebSocket isn't connected
		if (MaestraClient && !MaestraClient->IsWebSocketConnected() &&
			!MaestraEntitySlug.IsEmpty() &&
			(ElapsedTime - LastStatePollTime) >= StatePollInterval)
		{
			MaestraClient->GetEntityBySlug(MaestraEntitySlug);
			LastStatePollTime = ElapsedTime;
		}
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
		UDPReceiver->OnDataPacketReceived.AddDynamic(this, &UAuroraDataMixer::OnUDPDataPacketReceived);
		UE_LOG(LogTemp, Log, TEXT("AuroraDataMixer: UDP receiver started on port %d"), UDPPort);
	}
}

void UAuroraDataMixer::InitializeRF()
{
	if (bUseUDP)
	{
		// Use real RF data from UDP stream (supports JSON and SDRF binary formats)
		UDPRFProvider = NewObject<UUDPRFDataProvider>(this);
		// Configure with wide defaults — SDRF packets will override freq range per-packet
		UDPRFProvider->Configure(88.0f, 108.0f, RFNumBins, 30.0f);
		RFProvider = UDPRFProvider;
		UE_LOG(LogTemp, Log, TEXT("AuroraDataMixer: UDP RF spectrum provider initialized with %d bins (JSON + SDRF binary)"), RFNumBins);
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

void UAuroraDataMixer::OnUDPDataPacketReceived(const FDataPacket& Packet)
{
	// Handle SDRF binary packets from the RTL-SDR Python script
	if (UDPRFProvider && Packet.RawBytes.Num() >= 36 && UUDPRFDataProvider::IsSDRFPacket(Packet.RawBytes))
	{
		UDPRFProvider->IngestSDRFPacket(Packet.RawBytes);
	}
}

// ============================================================================
// Data Reading
// ============================================================================

void UAuroraDataMixer::ReadMaestraData()
{
	if (!TrackedEntity)
	{
		// Log once per second to avoid spam
		if (FMath::Fmod(ElapsedTime, 5.0f) < 0.02f)
		{
			UE_LOG(LogTemp, Warning, TEXT("AuroraDataMixer: TrackedEntity is null — waiting for Maestra entity '%s'"), *MaestraEntitySlug);
		}
		return;
	}

	RawMaestraValue = FMath::Clamp(
		TrackedEntity->GetStateFloat(MaestraDataKey, 0.0f),
		0.0f, 1.0f
	);

	// Read potentiometer values from entity state
	bool bFoundAnyPot = TrackedEntity->HasStateKey(TEXT("pot1")) ||
		TrackedEntity->HasStateKey(TEXT("pot2")) ||
		TrackedEntity->HasStateKey(TEXT("pot3")) ||
		TrackedEntity->HasStateKey(TEXT("pot4"));

	if (bFoundAnyPot)
	{
		float NewPot1 = FMath::Clamp(TrackedEntity->GetStateFloat(TEXT("pot1"), RawPot1), 0.0f, 1.0f);
		float NewPot2 = FMath::Clamp(TrackedEntity->GetStateFloat(TEXT("pot2"), RawPot2), 0.0f, 1.0f);
		float NewPot3 = FMath::Clamp(TrackedEntity->GetStateFloat(TEXT("pot3"), RawPot3), 0.0f, 1.0f);
		float NewPot4 = FMath::Clamp(TrackedEntity->GetStateFloat(TEXT("pot4"), RawPot4), 0.0f, 1.0f);

		// Log when values change
		if (!bHasPotData || FMath::Abs(NewPot1 - RawPot1) > 0.001f ||
			FMath::Abs(NewPot2 - RawPot2) > 0.001f ||
			FMath::Abs(NewPot3 - RawPot3) > 0.001f ||
			FMath::Abs(NewPot4 - RawPot4) > 0.001f)
		{
			UE_LOG(LogTemp, Log, TEXT("AuroraDataMixer: Pots from entity state — pot1=%.3f pot2=%.3f pot3=%.3f pot4=%.3f"),
				NewPot1, NewPot2, NewPot3, NewPot4);
		}

		RawPot1 = NewPot1;
		RawPot2 = NewPot2;
		RawPot3 = NewPot3;
		RawPot4 = NewPot4;
		bHasPotData = true;
	}
	else if (!bHasPotData && FMath::Fmod(ElapsedTime, 5.0f) < 0.02f)
	{
		// Periodic diagnostic: show what keys ARE in the state
		TArray<FString> Keys = TrackedEntity->GetStateKeys();
		FString KeyList = FString::Join(Keys, TEXT(", "));
		UE_LOG(LogTemp, Log, TEXT("AuroraDataMixer: No pot keys found in entity '%s'. Available keys: [%s]"),
			*MaestraEntitySlug, *KeyList);
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
	float PotSmooth = PotSmoothingFactor; // Pots react fast — ~3 frame response at 0.3

	// --- Smooth potentiometer values ---
	Pot1_Hue = FMath::Lerp(Pot1_Hue, RawPot1, PotSmooth);
	Pot2_Intensity = FMath::Lerp(Pot2_Intensity, RawPot2, PotSmooth);
	Pot3_Height = FMath::Lerp(Pot3_Height, RawPot3, PotSmooth);
	Pot4_Turbulence = FMath::Lerp(Pot4_Turbulence, RawPot4, PotSmooth);

	// --- Intensity ---
	// Pot2 directly overrides intensity when pot data is present
	float TargetIntensity = bHasPotData ? Pot2_Intensity : Aggregate;
	// Blend RF/data aggregate contribution even when pots are active
	if (bHasPotData)
	{
		// Pot controls the baseline, RF/data adds energy on top
		TargetIntensity = FMath::Clamp(Pot2_Intensity * 0.7f + Aggregate * 0.3f, 0.0f, 1.0f);
	}
	SmoothedIntensity = FMath::Lerp(SmoothedIntensity, TargetIntensity, bHasPotData ? PotSmooth : Smooth);

	// --- Audio pulse ---
	SmoothedAudio = FMath::Lerp(SmoothedAudio, RawAudioAmplitude, Smooth * 3.0f);

	// --- Fold count ---
	float TargetFolds;
	if (bHasPotData)
	{
		// Pot4 turbulence drives fold complexity: 0→2 folds (calm), 1→12 folds (chaotic)
		TargetFolds = FMath::Lerp(2.0f, 12.0f, Pot4_Turbulence);
	}
	else
	{
		TargetFolds = FMath::Lerp(2.0f, 10.0f, FMath::Clamp(CurrentEntropy * 1.5f, 0.0f, 1.0f));
	}
	SmoothedFoldCount = FMath::Lerp(SmoothedFoldCount, TargetFolds, bHasPotData ? PotSmooth : Smooth * 0.5f);

	// --- Vertical extent ---
	float TargetExtent;
	if (bHasPotData)
	{
		// Pot3: 0→stubby (0.15), 1→towering (1.0)
		TargetExtent = FMath::Lerp(0.15f, 1.0f, Pot3_Height);
		TargetExtent = FMath::Clamp(TargetExtent + SubstormEnergy * 0.2f, 0.15f, 1.0f);
	}
	else
	{
		TargetExtent = FMath::Clamp(Aggregate * 1.2f + SubstormEnergy * 0.3f, 0.1f, 1.0f);
	}
	SmoothedVerticalExtent = FMath::Lerp(SmoothedVerticalExtent, TargetExtent, bHasPotData ? PotSmooth : Smooth);

	// --- Wave speed ---
	float TargetWaveSpeed = 1.0f;
	if (bHasPotData)
	{
		// Pot4 also drives wave speed: 0→slow (0.2), 1→fast (4.0)
		TargetWaveSpeed = FMath::Lerp(0.2f, 4.0f, Pot4_Turbulence);
	}
	else if (bUseRFSpectrum && RawRFBins.Num() > 0)
	{
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
	SmoothedWaveSpeed = FMath::Lerp(SmoothedWaveSpeed, TargetWaveSpeed, bHasPotData ? PotSmooth : Smooth);

	// --- Per-bin RF spectrum smoothing ---
	// Use faster smoothing so SDR data shows up immediately on the curtain
	if (bUseRFSpectrum && RawRFBins.Num() > 0)
	{
		int32 NumBins = RawRFBins.Num();
		if (SmoothedRFBins.Num() != NumBins)
		{
			SmoothedRFBins.SetNumZeroed(NumBins);
		}

		// Fast RF smoothing: ~5 frame response for obvious visual feedback
		float RFSmooth = FMath::Min(Smooth * 6.0f, 0.5f);
		for (int32 i = 0; i < NumBins; ++i)
		{
			float Normalized = FMath::Clamp((RawRFBins[i] + 95.0f) / 85.0f, 0.0f, 1.0f);
			SmoothedRFBins[i] = FMath::Lerp(SmoothedRFBins[i], Normalized, RFSmooth);
		}
	}

	// --- Build output ---
	FAuroraParameters Params;

	Params.Intensity = SmoothedIntensity;

	// Hue: Pot1 directly controls hue when available, otherwise fall back to data
	float HueInput;
	if (bHasPotData)
	{
		HueInput = Pot1_Hue;
	}
	else if (bUseMaestra)
	{
		HueInput = RawMaestraValue;
	}
	else
	{
		HueInput = SmoothedIntensity;
	}
	// Map through aurora-realistic color stops:
	// 0.0 → deep green (0.33), 0.5 → cyan-blue (0.55), 1.0 → crimson-red (0.95)
	if (HueInput < 0.5f)
	{
		Params.Hue = FMath::Lerp(0.33f, 0.55f, HueInput * 2.0f);
	}
	else
	{
		Params.Hue = FMath::Lerp(0.55f, 0.95f, (HueInput - 0.5f) * 2.0f);
	}

	// Saturation: keep high when pots are active for vivid color response
	bool bHealthy = true;
	if (bUseMaestra && !TrackedEntity)
	{
		bHealthy = false;
	}
	if (bHasPotData)
	{
		// Vivid saturation that responds to hue position — more saturated in green/cyan
		Params.Saturation = FMath::Lerp(0.7f, 0.98f, SmoothedIntensity);
	}
	else
	{
		Params.Saturation = bHealthy ? FMath::Lerp(0.6f, 0.95f, SmoothedIntensity) : 0.3f;
	}

	Params.FoldCount = SmoothedFoldCount;
	Params.VerticalExtent = SmoothedVerticalExtent;
	Params.WaveSpeed = SmoothedWaveSpeed;

	// Noise complexity: pot4 turbulence also drives noise when active
	if (bHasPotData)
	{
		Params.NoiseOctaves = FMath::Lerp(1.5f, 5.5f, Pot4_Turbulence);
		Params.NoisePersistence = FMath::Lerp(0.25f, 0.7f, Pot4_Turbulence);
	}
	else
	{
		Params.NoiseOctaves = FMath::Lerp(1.5f, 5.0f, CurrentEntropy);
		Params.NoisePersistence = FMath::Lerp(0.3f, 0.65f, CurrentEntropy);
	}

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
