#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UDPReceiverComponent.h"
#include "RFDataProvider.h"
#include "SimulatedRFDataProvider.h"
#include "UDPRFDataProvider.h"
#include "MaestraClient.h"
#include "MaestraEntity.h"
#include "AuroraDataMixer.generated.h"

// ============================================================================
// Enums for external signal routing
// ============================================================================

/** Which FAuroraParameters field an external signal binding targets. */
UENUM(BlueprintType)
enum class EAuroraTargetParam : uint8
{
	Intensity        UMETA(DisplayName = "Intensity"),
	Hue              UMETA(DisplayName = "Hue"),
	Saturation       UMETA(DisplayName = "Saturation"),
	FoldCount        UMETA(DisplayName = "Fold Count"),
	VerticalExtent   UMETA(DisplayName = "Vertical Extent"),
	WaveSpeed        UMETA(DisplayName = "Wave Speed"),
	NoiseOctaves     UMETA(DisplayName = "Noise Octaves"),
	NoisePersistence UMETA(DisplayName = "Noise Persistence"),
	LuminancePulse   UMETA(DisplayName = "Luminance Pulse"),
	SubstormFlare    UMETA(DisplayName = "Substorm Flare"),
};

/** How an external signal combines with the already-computed parameter value. */
UENUM(BlueprintType)
enum class ESignalBlendMode : uint8
{
	Override   UMETA(DisplayName = "Override",  ToolTip = "Replace the computed value entirely"),
	Additive   UMETA(DisplayName = "Additive",  ToolTip = "Add to the computed value (clamped)"),
	Multiply   UMETA(DisplayName = "Multiply",  ToolTip = "Multiply the computed value"),
};

// ============================================================================
// External signal binding — one routing from (entity + key) → aurora parameter
// ============================================================================

/**
 * Routes a single float value from a Maestra entity's state into one of the
 * aurora visual parameters. Configure input/output ranges to remap from the
 * publishing artist's value range to the aurora's expected range.
 */
USTRUCT(BlueprintType)
struct MAESTRAUNREALCLEAN_API FExternalSignalBinding
{
	GENERATED_BODY()

	/** Maestra entity slug to read from (e.g. "janes-light-sculpture"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Signal")
	FString EntitySlug;

	/** State key to read from that entity (e.g. "brightness", "colorWheel"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Signal")
	FString StateKey;

	/** Which aurora parameter this signal drives. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Signal")
	EAuroraTargetParam TargetParam = EAuroraTargetParam::Intensity;

	/** How to combine with the existing computed value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Signal")
	ESignalBlendMode BlendMode = ESignalBlendMode::Additive;

	/** Input range minimum (the publishing artist's value range). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Signal|Range")
	float InputMin = 0.0f;

	/** Input range maximum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Signal|Range")
	float InputMax = 1.0f;

	/** Output range minimum (after remapping, before blend). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Signal|Range")
	float OutputMin = 0.0f;

	/** Output range maximum. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Signal|Range")
	float OutputMax = 1.0f;

	/** Blend weight: 0 = no effect, 1 = full effect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Signal", meta=(ClampMin="0.0", ClampMax="1.0"))
	float Weight = 1.0f;

	/** Smoothing factor (higher = faster response, 0.15 ≈ ~7 frame lag at 60 FPS). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Signal", meta=(ClampMin="0.01", ClampMax="1.0"))
	float SmoothingFactor = 0.15f;

	/** Enable/disable this binding without removing it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Signal")
	bool bEnabled = true;
};

/**
 * Aggregated aurora parameters produced by the mixer each frame.
 * Each field maps to a specific visual dimension of the aurora.
 */
USTRUCT(BlueprintType)
struct MAESTRAUNREALCLEAN_API FAuroraParameters
{
	GENERATED_BODY()

	// Overall curtain brightness (0-1). Drives emissive strength and spawn rate.
	UPROPERTY(BlueprintReadOnly, Category = "Aurora")
	float Intensity = 0.2f;

	// Dominant hue in 0-1 range (maps to aurora emission colors: green/cyan/purple/red)
	UPROPERTY(BlueprintReadOnly, Category = "Aurora")
	float Hue = 0.35f;

	// Color saturation (0-1). Drops when data connections are unhealthy.
	UPROPERTY(BlueprintReadOnly, Category = "Aurora")
	float Saturation = 0.8f;

	// Number of ripple folds in the curtain (1-12). Driven by data activity.
	UPROPERTY(BlueprintReadOnly, Category = "Aurora")
	float FoldCount = 3.0f;

	// Vertical extent of the curtain (0-1). Aggregate signal strength.
	UPROPERTY(BlueprintReadOnly, Category = "Aurora")
	float VerticalExtent = 0.5f;

	// Curtain wave propagation speed. RF frequency encoding.
	UPROPERTY(BlueprintReadOnly, Category = "Aurora")
	float WaveSpeed = 1.0f;

	// Noise detail level (1-6 octaves). Driven by data entropy.
	UPROPERTY(BlueprintReadOnly, Category = "Aurora")
	float NoiseOctaves = 2.0f;

	// Noise turbulence persistence (0-1). High entropy = turbulent.
	UPROPERTY(BlueprintReadOnly, Category = "Aurora")
	float NoisePersistence = 0.4f;

	// Audio-driven luminance pulse (0-1). Breathing rhythm.
	UPROPERTY(BlueprintReadOnly, Category = "Aurora")
	float LuminancePulse = 0.0f;

	// Substorm flare intensity (0-1). Triggered by data spikes.
	UPROPERTY(BlueprintReadOnly, Category = "Aurora")
	float SubstormFlare = 0.0f;

	// Per-bin normalized RF spectrum (0-1), mapped spatially along curtain width.
	// Empty when RF is disabled. Length matches RFNumBins.
	UPROPERTY(BlueprintReadOnly, Category = "Aurora")
	TArray<float> RFSpectrumBins;
};

/**
 * Configuration for how strongly each data source influences the aurora.
 */
USTRUCT(BlueprintType)
struct MAESTRAUNREALCLEAN_API FAuroraSourceWeights
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Weights", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MaestraWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Weights", meta=(ClampMin="0.0", ClampMax="1.0"))
	float UDPWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Weights", meta=(ClampMin="0.0", ClampMax="1.0"))
	float RFSpectrumWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Weights", meta=(ClampMin="0.0", ClampMax="1.0"))
	float AudioWeight = 1.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAuroraParametersUpdated, const FAuroraParameters&, Params);

/**
 * UAuroraDataMixer aggregates multiple data sources into a single set of
 * aurora visual parameters. It acts as the "solar wind" processor — raw data
 * comes in from Maestra, UDP, RF spectrum, and audio, and smooth, perceptually-
 * mapped aurora parameters come out.
 *
 * Attach this component to your AuroraBorealisActor.
 */
UCLASS(ClassGroup=(Aurora), meta=(BlueprintSpawnableComponent))
class MAESTRAUNREALCLEAN_API UAuroraDataMixer : public UActorComponent
{
	GENERATED_BODY()

public:
	UAuroraDataMixer();

	// === Source Configuration ===

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Sources")
	FAuroraSourceWeights SourceWeights;

	// Maestra
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Maestra")
	bool bUseMaestra = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Maestra", meta=(EditCondition="bUseMaestra"))
	FString MaestraEntitySlug;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Maestra", meta=(EditCondition="bUseMaestra"))
	FString MaestraApiUrl = TEXT("http://localhost:8080");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Maestra", meta=(EditCondition="bUseMaestra"))
	FString MaestraWebSocketUrl = TEXT("ws://localhost:8765");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Maestra", meta=(EditCondition="bUseMaestra"))
	FString MaestraDataKey = TEXT("potentiometer");

	// UDP
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|UDP")
	bool bUseUDP = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|UDP", meta=(EditCondition="bUseUDP"))
	int32 UDPPort = 9001;

	// RF Spectrum
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|RF")
	bool bUseRFSpectrum = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|RF", meta=(EditCondition="bUseRFSpectrum"))
	int32 RFNumBins = 128;

	// Audio (expects an AudioSpectrumActor to broadcast OnSpectrumUpdated)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Audio")
	bool bUseAudio = false;

	// === Potentiometers (4 knobs, 0-1, received via UDP JSON) ===

	/** Pot 1: Color/Hue — sweeps through the full aurora color spectrum (green→cyan→purple→red). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aurora|Potentiometers")
	float Pot1_Hue = 0.35f;

	/** Pot 2: Intensity — controls overall brightness from dim to blazing. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aurora|Potentiometers")
	float Pot2_Intensity = 0.5f;

	/** Pot 3: Height — controls curtain vertical extent from stubby to towering. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aurora|Potentiometers")
	float Pot3_Height = 0.5f;

	/** Pot 4: Turbulence — controls wave speed and fold complexity (calm to chaotic). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aurora|Potentiometers")
	float Pot4_Turbulence = 0.3f;

	/** How fast potentiometer changes take effect (higher = faster, 0.3 = ~3 frame response). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Potentiometers", meta=(ClampMin="0.05", ClampMax="1.0"))
	float PotSmoothingFactor = 0.3f;

	/** Set to true when at least one potentiometer packet has been received. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aurora|Potentiometers")
	bool bHasPotData = false;

	// === External Signal Routing (other artists' Maestra entities) ===

	/** Routes signals from other artists' Maestra entities into aurora parameters.
	 *  Each binding reads one key from one entity and blends it into a target param. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|External Signals")
	TArray<FExternalSignalBinding> ExternalBindings;

	// === Smoothing ===

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Smoothing", meta=(ClampMin="0.005", ClampMax="1.0"))
	float GlobalSmoothingFactor = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Smoothing", meta=(ClampMin="0.0", ClampMax="30.0"))
	float SubstormDecayRate = 2.0f;

	// === Entropy Window ===

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Analysis", meta=(ClampMin="8", ClampMax="256"))
	int32 EntropyWindowSize = 64;

	// === Output ===

	UPROPERTY(BlueprintReadOnly, Category = "Aurora|Output")
	FAuroraParameters CurrentParameters;

	UPROPERTY(BlueprintAssignable, Category = "Aurora|Events")
	FOnAuroraParametersUpdated OnParametersUpdated;

	// === Blueprint API ===

	UFUNCTION(BlueprintCallable, Category = "Aurora")
	const FAuroraParameters& GetCurrentParameters() const { return CurrentParameters; }

	/** Manually inject a value (useful for testing or manual override). */
	UFUNCTION(BlueprintCallable, Category = "Aurora")
	void InjectManualValue(float Value);

	/** Feed audio spectrum data from an external AudioSpectrumActor. */
	UFUNCTION(BlueprintCallable, Category = "Aurora")
	void FeedAudioSpectrum(const TArray<float>& FrequencyBands, float OverallAmplitude);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	// --- Raw source values (pre-smoothing) ---
	float RawMaestraValue = 0.0f;
	float RawUDPValue = 0.0f;
	float RawAudioAmplitude = 0.0f;
	TArray<float> RawAudioBands;
	TArray<float> RawRFBins;
	TArray<float> SmoothedRFBins;

	// --- Potentiometer raw targets ---
	float RawPot1 = 0.35f;
	float RawPot2 = 0.5f;
	float RawPot3 = 0.5f;
	float RawPot4 = 0.3f;

	// --- Smoothed intermediates ---
	float SmoothedIntensity = 0.2f;
	float SmoothedAudio = 0.0f;
	float SmoothedFoldCount = 3.0f;
	float SmoothedVerticalExtent = 0.5f;
	float SmoothedWaveSpeed = 1.0f;
	float SmoothedNoisePersistence = 0.4f;
	float SubstormEnergy = 0.0f;

	// --- Entropy tracking ---
	TArray<float> EntropyHistory;
	int32 EntropyWriteIndex = 0;
	float CurrentEntropy = 0.0f;

	// --- Spike detection ---
	float PreviousAggregateValue = 0.0f;

	// --- Maestra ---
	UPROPERTY()
	UMaestraClient* MaestraClient;

	UPROPERTY()
	UMaestraEntity* TrackedEntity;

	// --- UDP ---
	UPROPERTY()
	UUDPReceiverComponent* UDPReceiver;

	// --- RF ---
	UPROPERTY()
	URFDataProvider* RFProvider;

	UPROPERTY()
	UUDPRFDataProvider* UDPRFProvider;

	// --- Elapsed time ---
	float ElapsedTime = 0.0f;

	// --- Periodic state polling ---
	float LastStatePollTime = 0.0f;
	float StatePollInterval = 0.5f; // Re-fetch entity state every 0.5s via HTTP

	// --- External signal binding state ---
	UPROPERTY()
	TMap<FString, UMaestraEntity*> ExternalEntityCache;

	TArray<float> SmoothedBindingValues;

	// --- Internal methods ---
	void InitializeMaestra();
	void InitializeUDP();
	void InitializeRF();
	void InitializeExternalBindings();

	void ReadMaestraData();
	void ReadRFData(float DeltaTime);
	void ReadExternalBindings(float DeltaTime);
	void ApplyExternalBindings(FAuroraParameters& Params) const;

	float ComputeAggregateIntensity() const;
	float ComputeEntropy() const;
	void RecordEntropyValue(float Value);
	void DetectSubstorm(float NewAggregate);

	FAuroraParameters ComputeParameters(float DeltaTime);

	/** Get a mutable reference to the target field in FAuroraParameters by enum. */
	static float& GetParamField(FAuroraParameters& Params, EAuroraTargetParam Target);

	/** Get the valid output range for a target parameter. */
	static void GetParamRange(EAuroraTargetParam Target, float& OutMin, float& OutMax);

	UFUNCTION()
	void OnMaestraEntityReceived(const FString& Slug, UMaestraEntity* Entity);

	UFUNCTION()
	void OnUDPJsonReceived(const FString& JsonString);

	UFUNCTION()
	void OnUDPDataPacketReceived(const FDataPacket& Packet);
};
