#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "AudioCaptureComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "AudioSpectrumActor.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSpectrumUpdated, const TArray<float>&, FrequencyBands, float, OverallAmplitude);

UCLASS(BlueprintType, Blueprintable)
class MAESTRAUNREALCLEAN_API AAudioSpectrumActor : public AActor
{
	GENERATED_BODY()

public:
	AAudioSpectrumActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioSpectrum|Audio")
	int32 NumFrequencyBands = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioSpectrum|Audio")
	float AudioSmoothingFactor = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioSpectrum|Audio")
	float AudioAmplitudeMultiplier = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioSpectrum|Ring")
	int32 NumMeshesInRing = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioSpectrum|Ring")
	float RingRadius = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioSpectrum|Ring")
	FVector BaseBarScale = FVector(20.0f, 20.0f, 50.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioSpectrum|Ring")
	float MaxScaleMultiplier = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioSpectrum|Ring")
	float RotationSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioSpectrum|Ring")
	UStaticMesh* BarMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AudioSpectrum|Ring")
	UMaterialInterface* BarMaterial;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AudioSpectrum")
	USceneComponent* RingRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AudioSpectrum")
	UAudioCaptureComponent* AudioCapture;

	UPROPERTY(BlueprintAssignable, Category = "AudioSpectrum|Events")
	FOnSpectrumUpdated OnSpectrumUpdated;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	TArray<UStaticMeshComponent*> BarComponents;
	TArray<UMaterialInstanceDynamic*> BarMaterialInstances;
	TArray<float> SmoothedSpectrum;
	TArray<float> RawSpectrum;

	UStaticMesh* DefaultBarMesh = nullptr;

	void InitializeRing();
	void UpdateRingVisualization(float DeltaTime);
	FLinearColor FrequencyToColor(float NormalizedFrequency, float Amplitude) const;
};
