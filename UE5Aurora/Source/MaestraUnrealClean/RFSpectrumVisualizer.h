#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "RFDataProvider.h"
#include "RFSpectrumVisualizer.generated.h"

UCLASS(BlueprintType, Blueprintable)
class MAESTRAUNREALCLEAN_API ARFSpectrumVisualizer : public AActor
{
	GENERATED_BODY()

public:
	ARFSpectrumVisualizer();

	// --- Spectrum config ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF Visualizer|Spectrum")
	float FrequencyMinMHz = 88.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF Visualizer|Spectrum")
	float FrequencyMaxMHz = 108.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF Visualizer|Spectrum")
	float SampleRate = 30.0f;

	// --- Mesh config ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF Visualizer|Mesh")
	int32 FrequencyResolution = 256;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF Visualizer|Mesh")
	int32 WaterfallDepth = 128;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF Visualizer|Mesh")
	float WaterfallScrollSpeed = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF Visualizer|Mesh")
	float MeshWidth = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF Visualizer|Mesh")
	float AmplitudeScale = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF Visualizer|Mesh")
	float MeshDepth = 300.0f;

	// --- Color config ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF Visualizer|Color")
	FLinearColor CoolColor = FLinearColor(0.0f, 0.05f, 0.4f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF Visualizer|Color")
	FLinearColor WarmColor = FLinearColor(1.0f, 1.0f, 0.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF Visualizer|Color")
	FLinearColor HotColor = FLinearColor(1.0f, 0.1f, 0.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF Visualizer|Color")
	float DbMinForColor = -95.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF Visualizer|Color")
	float DbMaxForColor = -10.0f;

	// --- Particle config ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF Visualizer|Particles")
	float PeakThresholdDb = -40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF Visualizer|Particles")
	UNiagaraSystem* PeakParticleSystem;

	// --- Data provider ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "RF Visualizer|Data")
	URFDataProvider* DataProvider;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RF Visualizer|Data")
	bool bUseSimulatedData = true;

	// --- Components ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RF Visualizer")
	UProceduralMeshComponent* SpectrumMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RF Visualizer")
	UNiagaraComponent* PeakParticles;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	TArray<TArray<float>> WaterfallHistory;
	int32 CurrentRow = 0;

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FLinearColor> VertexColors;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FProcMeshTangent> Tangents;

	float TimeSinceLastFrame = 0.0f;

	void InitializeMesh();
	void PushNewSpectrumRow(const FRFSpectrumFrame& Frame);
	void UpdateMeshFromWaterfall();
	FLinearColor AmplitudeToColor(float DbValue) const;
	void UpdatePeakParticles(const FRFSpectrumFrame& Frame);
	void EnsureDataProvider();
};
