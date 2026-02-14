#pragma once

#include "CoreMinimal.h"
#include "RFDataProvider.h"
#include "SimulatedRFDataProvider.generated.h"

USTRUCT(BlueprintType)
struct MAESTRAUNREALCLEAN_API FSimulatedStation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF Simulation")
	float FrequencyMHz = 95.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF Simulation")
	float BandwidthMHz = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF Simulation")
	float PeakAmplitude = -20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF Simulation")
	float FadeRate = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF Simulation")
	float FadeDepth = 6.0f;
};

UCLASS(BlueprintType, Blueprintable)
class MAESTRAUNREALCLEAN_API USimulatedRFDataProvider : public URFDataProvider
{
	GENERATED_BODY()

public:
	USimulatedRFDataProvider();

	virtual FRFSpectrumFrame GetNextFrame(float DeltaTime) override;
	virtual void Configure(float InFreqMin, float InFreqMax, int32 InNumBins, float InSampleRate) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF Simulation")
	TArray<FSimulatedStation> Stations;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF Simulation")
	float NoiseFloorDbm = -90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF Simulation")
	float NoiseVariance = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF Simulation")
	float SweepSignalSpeed = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF Simulation")
	float SweepSignalAmplitude = -30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF Simulation")
	float SweepSignalBandwidth = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF Simulation")
	int32 RandomSeed = 12345;

private:
	FRandomStream RNG;
	float ElapsedTime = 0.0f;
	float SweepPosition = 0.0f;

	float GenerateStationSignal(float FreqMHz, const FSimulatedStation& Station) const;
	float GenerateNoise();
	float GenerateSweep(float FreqMHz) const;
	void SetupDefaultStations();
};
