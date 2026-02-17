#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "MountainBackdropActor.generated.h"

/**
 * Procedural mountain silhouette backdrop for the aurora scene.
 * Generates a 360-degree ring of jagged mountain peaks using layered Perlin noise.
 * Designed to be a dark silhouette against the aurora-lit sky.
 */
UCLASS(BlueprintType, Blueprintable)
class MAESTRAUNREALCLEAN_API AMountainBackdropActor : public AActor
{
	GENERATED_BODY()

public:
	AMountainBackdropActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mountains")
	UProceduralMeshComponent* MountainMesh;

	/** Distance from center to the mountain ring. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mountains")
	float RingRadius = 15000.0f;

	/** Base height of mountains. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mountains")
	float BaseHeight = 800.0f;

	/** Maximum peak height above base. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mountains")
	float PeakHeight = 2500.0f;

	/** Horizontal segments around the ring (more = smoother silhouette). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mountains", meta=(ClampMin="32", ClampMax="512"))
	int32 Segments = 256;

	/** Vertical subdivisions of each mountain column. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mountains", meta=(ClampMin="2", ClampMax="16"))
	int32 VerticalDivisions = 6;

	/** How far below origin the mountain base extends (to ensure no gaps). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mountains")
	float BelowGroundDepth = 500.0f;

	/** Random seed for mountain shape. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mountains")
	int32 MountainSeed = 42;

	/** Material for the mountains — should be very dark / near-black. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mountains")
	UMaterialInterface* MountainMaterial;

protected:
	virtual void BeginPlay() override;

private:
	void GenerateMountains();
	float SampleMountainHeight(float Angle) const;
};
