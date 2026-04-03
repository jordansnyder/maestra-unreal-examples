#include "InstallationCameraPawn.h"
#include "AuroraBorealisActor.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

AInstallationCameraPawn::AInstallationCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);

	// No player input — this is a fully autonomous camera
	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

void AInstallationCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	// Randomize phase offsets so each session starts from a different place
	FRandomStream RNG(FMath::Rand());
	PositionPhaseOffset = FVector(
		RNG.FRandRange(0.0f, 1000.0f),
		RNG.FRandRange(0.0f, 1000.0f),
		RNG.FRandRange(0.0f, 1000.0f)
	);
	LookPhaseOffset = FVector(
		RNG.FRandRange(0.0f, 1000.0f),
		RNG.FRandRange(0.0f, 1000.0f),
		RNG.FRandRange(0.0f, 1000.0f)
	);

	// Auto-find aurora actor if not assigned
	if (!AuroraActor)
	{
		AActor* Found = UGameplayStatics::GetActorOfClass(GetWorld(), AAuroraBorealisActor::StaticClass());
		AuroraActor = Cast<AAuroraBorealisActor>(Found);
	}

	// Start at the initial wander position
	SetActorLocation(SampleWanderPosition(0.0f));
}

void AInstallationCameraPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ElapsedTime += DeltaTime;

	// --- Position: smooth noise-driven wandering ---
	FVector TargetPos = SampleWanderPosition(ElapsedTime);
	SetActorLocation(TargetPos);

	// --- Look target: aurora center + noise drift ---
	FVector LookCenter = FVector(0.0f, 0.0f, LookTargetAltitude);
	if (AuroraActor)
	{
		LookCenter = AuroraActor->GetActorLocation() + FVector(0.0f, 0.0f, LookTargetAltitude);
	}
	LookCenter += SampleLookOffset(ElapsedTime);

	// Compute desired rotation toward the look target
	FVector Direction = (LookCenter - TargetPos).GetSafeNormal();
	FRotator DesiredRotation = Direction.Rotation();

	// Smooth rotation interpolation
	FRotator CurrentRotation = GetActorRotation();
	FRotator SmoothedRotation = FMath::RInterpTo(CurrentRotation, DesiredRotation, DeltaTime, RotationSmoothSpeed);
	SetActorRotation(SmoothedRotation);
}

FVector AInstallationCameraPawn::SampleWanderPosition(float Time) const
{
	float T = Time * WanderSpeed;

	// Each axis uses Perlin noise at slightly different frequencies
	// so the path never repeats or feels periodic
	float NX = FMath::PerlinNoise3D(FVector(
		T * 0.7f + PositionPhaseOffset.X,
		PositionPhaseOffset.X * 0.3f,
		0.0f
	));
	// Layer a second octave for subtle detail
	NX += FMath::PerlinNoise3D(FVector(
		T * 1.9f + PositionPhaseOffset.X + 50.0f,
		PositionPhaseOffset.X * 0.7f,
		10.0f
	)) * 0.3f;

	float NY = FMath::PerlinNoise3D(FVector(
		T * 0.5f + PositionPhaseOffset.Y,
		PositionPhaseOffset.Y * 0.3f,
		100.0f
	));
	NY += FMath::PerlinNoise3D(FVector(
		T * 1.3f + PositionPhaseOffset.Y + 50.0f,
		PositionPhaseOffset.Y * 0.7f,
		110.0f
	)) * 0.3f;

	float NZ = FMath::PerlinNoise3D(FVector(
		T * 0.4f + PositionPhaseOffset.Z,
		PositionPhaseOffset.Z * 0.3f,
		200.0f
	));
	NZ += FMath::PerlinNoise3D(FVector(
		T * 1.1f + PositionPhaseOffset.Z + 50.0f,
		PositionPhaseOffset.Z * 0.7f,
		210.0f
	)) * 0.3f;

	FVector BaseCenter = FVector::ZeroVector;
	if (AuroraActor)
	{
		BaseCenter = AuroraActor->GetActorLocation();
	}

	return FVector(
		BaseCenter.X + NX * WanderRangeX,
		BaseCenter.Y + NY * WanderRangeY,
		CenterAltitude + NZ * WanderRangeZ
	);
}

FVector AInstallationCameraPawn::SampleLookOffset(float Time) const
{
	// Very slow drift for the gaze point — even slower than position wander
	float T = Time * WanderSpeed * 0.4f;

	float DX = FMath::PerlinNoise3D(FVector(T * 0.6f + LookPhaseOffset.X, 0.0f, 300.0f));
	float DY = FMath::PerlinNoise3D(FVector(T * 0.5f + LookPhaseOffset.Y, 0.0f, 400.0f));
	float DZ = FMath::PerlinNoise3D(FVector(T * 0.3f + LookPhaseOffset.Z, 0.0f, 500.0f));

	return FVector(DX, DY, DZ) * LookDriftAmount;
}
