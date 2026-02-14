#pragma once

#include "CoreMinimal.h"
#include "GenerativeArtActor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Engine/VolumeTexture.h"
#include "DataDrivenParticleActor.h"
#include "MaestraClient.h"
#include "MaestraEntity.h"
#include "FlowFieldActor.generated.h"

UENUM(BlueprintType)
enum class EFlowFieldNoiseType : uint8
{
	Perlin3D,
	CurlNoise,
	Simplex
};

UCLASS(BlueprintType, Blueprintable)
class MAESTRAUNREALCLEAN_API AFlowFieldActor : public AGenerativeArtActor
{
	GENERATED_BODY()

public:
	AFlowFieldActor();

	// --- Noise Parameters ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Noise")
	EFlowFieldNoiseType NoiseType = EFlowFieldNoiseType::CurlNoise;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Noise")
	float NoiseScale = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Noise")
	float NoiseSpeed = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Noise")
	int32 NoiseOctaves = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Noise")
	float NoiseLacunarity = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Noise")
	float NoisePersistence = 0.5f;

	// --- Field Parameters ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Field")
	FVector FieldBoundsMin = FVector(-1500.0f, -1500.0f, -200.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Field")
	FVector FieldBoundsMax = FVector(1500.0f, 1500.0f, 200.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Field")
	FIntVector FieldGridSize = FIntVector(32, 32, 32);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Field")
	float FieldStrength = 100.0f;

	// --- Niagara ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlowField")
	UNiagaraComponent* ParticleComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Particles")
	UNiagaraSystem* ParticleSystemAsset;

	// --- Data Source ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|DataSource")
	EDataSourceType DataSource = EDataSourceType::Simulated;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|DataSource")
	FString MaestraEntitySlug;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|DataSource")
	FString MaestraApiUrl = TEXT("http://localhost:8080");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|DataSource")
	FString MaestraWebSocketUrl = TEXT("ws://localhost:8765");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|DataSource")
	FString PotentiometerDataKey = TEXT("potentiometer");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|DataSource", meta=(ClampMin="0.005", ClampMax="1.0"))
	float InputSmoothingFactor = 0.02f;

	// --- Visual Parameters (driven by MasterIntensity) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Visual", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MasterIntensity = 0.5f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlowField|Visual")
	float SpawnRate = 3000.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlowField|Visual")
	float ParticleLifetime = 3.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlowField|Visual")
	float ParticleSize = 4.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlowField|Visual")
	float TrailLength = 0.5f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlowField|Visual")
	float ColorHue = 0.6f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlowField|Visual")
	float ColorSaturation = 0.8f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlowField|Visual")
	float EmissiveStrength = 5.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlowField|Visual")
	float TurbulenceIntensity = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlowField|Visual")
	float RibbonWidth = 15.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlowField|Visual")
	float WindStrength = 5.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlowField|Visual")
	float DragAmount = 3.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlowField|Visual")
	float NoiseForceStrength = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlowField|Visual")
	float SpriteSizeX = 400.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FlowField|Visual")
	float SpriteSizeY = 80.0f;

	// --- Debug ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Debug")
	bool bDrawDebugArrows = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlowField|Debug")
	bool bShowDebugHUD = true;

	// --- Blueprint API ---

	UFUNCTION(BlueprintCallable, Category = "FlowField")
	FVector SampleField(FVector WorldPosition) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void UpdateGenerativeArt(float DeltaTime) override;

private:
	TArray<FVector> FieldData;

	// Maestra runtime
	UPROPERTY()
	UMaestraClient* MaestraClientInstance;

	UPROPERTY()
	UMaestraEntity* TrackedEntity;

	float SmoothedMasterIntensity = 0.5f;

	// Field generation
	void RegenerateField();
	FVector ComputeNoiseVector(FVector Position, float Time) const;
	FVector ComputeCurlNoise(FVector Position, float Time) const;
	float SampleNoise(FVector Position, float Time) const;
	int32 GridIndex(int32 X, int32 Y, int32 Z) const;
	FVector GridToWorld(int32 X, int32 Y, int32 Z) const;
	void DrawDebugVisualization() const;

	// Maestra connection
	void InitializeMaestraConnection();
	void ReadMaestraData();
	float GenerateSimulatedPotentiometer() const;
	void MapIntensityToVisualParams();
	void PushAllNiagaraParameters();
	void DrawDebugHUD() const;

	UFUNCTION()
	void OnMaestraEntityReceived(const FString& Slug, UMaestraEntity* Entity);
};
