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

	// === Smoothing ===

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Smoothing", meta=(ClampMin="0.005", ClampMax="1.0"))
	float GlobalSmoothingFactor = 0.03f;

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

	// --- Smoothed intermediates ---
	float SmoothedIntensity = 0.2f;
	float SmoothedAudio = 0.0f;
	float SmoothedFoldCount = 3.0f;
	float SmoothedVerticalExtent = 0.5f;
	float SmoothedWaveSpeed = 1.0f;
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

	// --- Internal methods ---
	void InitializeMaestra();
	void InitializeUDP();
	void InitializeRF();

	void ReadMaestraData();
	void ReadRFData(float DeltaTime);

	float ComputeAggregateIntensity() const;
	float ComputeEntropy() const;
	void RecordEntropyValue(float Value);
	void DetectSubstorm(float NewAggregate);

	FAuroraParameters ComputeParameters(float DeltaTime);

	UFUNCTION()
	void OnMaestraEntityReceived(const FString& Slug, UMaestraEntity* Entity);

	UFUNCTION()
	void OnUDPJsonReceived(const FString& JsonString);
};
