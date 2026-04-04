#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Camera/CameraComponent.h"
#include "InstallationCameraPawn.generated.h"

class AAuroraBorealisActor;

/**
 * A noise-driven wandering camera for long-running installations.
 * Uses layered Perlin noise to produce smooth, never-repeating motion
 * through and around an aurora structure.
 */
UCLASS(BlueprintType, Blueprintable)
class MAESTRAUNREALCLEAN_API AInstallationCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	AInstallationCameraPawn();

	// =======================================================================
	// Components
	// =======================================================================

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* Camera;

	// =======================================================================
	// Wander Parameters
	// =======================================================================

	/** Overall speed of the wandering motion. Lower = slower, more meditative. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Wander", meta=(ClampMin="0.001", ClampMax="0.1"))
	float WanderSpeed = 0.015f;

	/** Horizontal wander range (X axis — along the curtain span). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Wander")
	float WanderRangeX = 30000.0f;

	/** Depth wander range (Y axis — through the curtain layers). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Wander")
	float WanderRangeY = 3000.0f;

	/** Vertical wander range (Z axis — altitude). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Wander")
	float WanderRangeZ = 6000.0f;

	/** Center altitude of the wander zone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Wander")
	float CenterAltitude = 9000.0f;

	// =======================================================================
	// Look Target
	// =======================================================================

	/** How much the gaze drifts away from the aurora center (world units). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Look", meta=(ClampMin="0.0"))
	float LookDriftAmount = 4000.0f;

	/** How quickly the camera reorients toward its look target (degrees/sec). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Look", meta=(ClampMin="0.1"))
	float RotationSmoothSpeed = 0.8f;

	/** Altitude offset for the look target above the aurora actor origin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Look")
	float LookTargetAltitude = 8000.0f;

	// =======================================================================
	// Aurora Reference
	// =======================================================================

	/** Optional reference to the aurora actor. If not set, auto-finds one in the level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Reference")
	AAuroraBorealisActor* AuroraActor;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	float ElapsedTime = 0.0f;

	// Phase offsets for each noise channel (randomized at start)
	FVector PositionPhaseOffset;
	FVector LookPhaseOffset;

	FVector SampleWanderPosition(float Time) const;
	FVector SampleLookOffset(float Time) const;
};
