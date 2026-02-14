#pragma once

#include "CoreMinimal.h"
#include "GenerativeArtActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ReactionDiffusionActor.generated.h"

UCLASS(BlueprintType, Blueprintable)
class MAESTRAUNREALCLEAN_API AReactionDiffusionActor : public AGenerativeArtActor
{
	GENERATED_BODY()

public:
	AReactionDiffusionActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ReactionDiffusion|GrayScott",
		meta=(ClampMin="0.0", ClampMax="0.1"))
	float FeedRate = 0.055f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ReactionDiffusion|GrayScott",
		meta=(ClampMin="0.0", ClampMax="0.1"))
	float KillRate = 0.062f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ReactionDiffusion|GrayScott")
	float DiffusionRateA = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ReactionDiffusion|GrayScott")
	float DiffusionRateB = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ReactionDiffusion|Simulation")
	int32 StepsPerFrame = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ReactionDiffusion|Simulation")
	int32 TextureResolution = 512;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ReactionDiffusion|Simulation")
	float DeltaT = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ReactionDiffusion|Display")
	UMaterialInterface* SimulationMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ReactionDiffusion|Display")
	UMaterialInterface* DisplayMaterial;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ReactionDiffusion")
	UStaticMeshComponent* DisplayPlane;

	UFUNCTION(BlueprintCallable, Category = "ReactionDiffusion")
	void ResetSimulation();

	UFUNCTION(BlueprintCallable, Category = "ReactionDiffusion")
	void AddSeedPoint(FVector2D UV, float Radius = 0.05f);

	UFUNCTION(BlueprintCallable, Category = "ReactionDiffusion")
	void AddRandomSeeds(int32 Count = 5);

	UFUNCTION(BlueprintCallable, Category = "ReactionDiffusion")
	UTextureRenderTarget2D* GetCurrentRenderTarget() const;

protected:
	virtual void BeginPlay() override;
	virtual void UpdateGenerativeArt(float DeltaTime) override;

private:
	UPROPERTY()
	UTextureRenderTarget2D* RenderTargetA;

	UPROPERTY()
	UTextureRenderTarget2D* RenderTargetB;

	UPROPERTY()
	UMaterialInstanceDynamic* SimMaterialInstance;

	UPROPERTY()
	UMaterialInstanceDynamic* DisplayMaterialInstance;

	bool bPingPong = false;

	void InitializeRenderTargets();
	void StepSimulation();
	void SwapBuffers();
	void InitializeCPUFallback();
	void StepCPUSimulation();
	void UpdateDisplayFromCPU();

	TArray<float> ChemicalA;
	TArray<float> ChemicalB;
	TArray<float> ChemicalA_Next;
	TArray<float> ChemicalB_Next;
	bool bUsingCPUFallback = false;

	UPROPERTY()
	UTexture2D* CPUTexture;
};
