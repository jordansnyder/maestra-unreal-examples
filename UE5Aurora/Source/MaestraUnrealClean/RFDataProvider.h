#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "RFDataProvider.generated.h"

USTRUCT(BlueprintType)
struct MAESTRAUNREALCLEAN_API FRFSpectrumFrame
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "RF")
	TArray<float> Amplitudes;

	UPROPERTY(BlueprintReadOnly, Category = "RF")
	float FrequencyMinMHz = 88.0f;

	UPROPERTY(BlueprintReadOnly, Category = "RF")
	float FrequencyMaxMHz = 108.0f;

	UPROPERTY(BlueprintReadOnly, Category = "RF")
	double Timestamp = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "RF")
	int32 NumBins = 0;
};

UCLASS(Abstract, BlueprintType, Blueprintable)
class MAESTRAUNREALCLEAN_API URFDataProvider : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "RF Data")
	virtual FRFSpectrumFrame GetNextFrame(float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = "RF Data")
	virtual void Configure(float InFreqMin, float InFreqMax, int32 InNumBins, float InSampleRate);

	UFUNCTION(BlueprintCallable, Category = "RF Data")
	virtual bool IsReady() const;

protected:
	float FreqMinMHz = 88.0f;
	float FreqMaxMHz = 108.0f;
	int32 NumBins = 256;
	float SampleRate = 30.0f;
};
