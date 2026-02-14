#include "AudioSpectrumActor.h"
#include "AudioCaptureComponent.h"

AAudioSpectrumActor::AAudioSpectrumActor()
{
	PrimaryActorTick.bCanEverTick = true;

	RingRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RingRoot"));
	SetRootComponent(RingRoot);

	AudioCapture = CreateDefaultSubobject<UAudioCaptureComponent>(TEXT("AudioCapture"));
	AudioCapture->SetupAttachment(RingRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube"));
	if (CubeMesh.Succeeded())
	{
		DefaultBarMesh = CubeMesh.Object;
	}
}

void AAudioSpectrumActor::BeginPlay()
{
	Super::BeginPlay();

	SmoothedSpectrum.SetNumZeroed(NumFrequencyBands);
	RawSpectrum.SetNumZeroed(NumFrequencyBands);

	InitializeRing();

	if (AudioCapture)
	{
		AudioCapture->Start();
	}
}

void AAudioSpectrumActor::InitializeRing()
{
	UStaticMesh* MeshToUse = BarMesh ? BarMesh : DefaultBarMesh;

	for (int32 i = 0; i < NumMeshesInRing; ++i)
	{
		float Angle = 2.0f * PI * static_cast<float>(i) / NumMeshesInRing;
		FVector Position(
			FMath::Cos(Angle) * RingRadius,
			FMath::Sin(Angle) * RingRadius,
			0.0f);

		FString CompName = FString::Printf(TEXT("Bar_%d"), i);
		UStaticMeshComponent* Bar = NewObject<UStaticMeshComponent>(this, *CompName);
		Bar->RegisterComponent();
		Bar->AttachToComponent(RingRoot, FAttachmentTransformRules::KeepRelativeTransform);
		Bar->SetRelativeLocation(Position);
		Bar->SetRelativeScale3D(BaseBarScale / 100.0f);
		Bar->SetRelativeRotation(FRotator(0.0f, FMath::RadiansToDegrees(Angle), 0.0f));

		if (MeshToUse)
		{
			Bar->SetStaticMesh(MeshToUse);
		}

		UMaterialInstanceDynamic* MatInst = nullptr;
		if (BarMaterial)
		{
			MatInst = UMaterialInstanceDynamic::Create(BarMaterial, this);
			Bar->SetMaterial(0, MatInst);
		}
		else if (MeshToUse && MeshToUse->GetStaticMaterials().Num() > 0)
		{
			UMaterialInterface* DefaultMat = MeshToUse->GetStaticMaterials()[0].MaterialInterface;
			if (DefaultMat)
			{
				MatInst = UMaterialInstanceDynamic::Create(DefaultMat, this);
				Bar->SetMaterial(0, MatInst);
			}
		}

		BarComponents.Add(Bar);
		BarMaterialInstances.Add(MatInst);
	}
}

void AAudioSpectrumActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	float Time = GetGameTimeSinceCreation();
	for (int32 i = 0; i < NumFrequencyBands; ++i)
	{
		float FreqNorm = static_cast<float>(i) / FMath::Max(NumFrequencyBands - 1, 1);
		float BassBias = FMath::Exp(-FreqNorm * 3.0f);
		float Wave1 = FMath::Sin(Time * (2.0f + i * 0.3f)) * 0.5f + 0.5f;
		float Wave2 = FMath::Sin(Time * (1.1f + i * 0.7f) + PI * 0.3f) * 0.3f + 0.3f;
		RawSpectrum[i] = (BassBias * Wave1 + Wave2 * 0.3f) * AudioAmplitudeMultiplier;
	}

	for (int32 i = 0; i < NumFrequencyBands; ++i)
	{
		SmoothedSpectrum[i] = FMath::Lerp(SmoothedSpectrum[i], RawSpectrum[i], AudioSmoothingFactor);
	}

	float OverallAmplitude = 0.0f;
	for (float Val : SmoothedSpectrum)
	{
		OverallAmplitude += Val;
	}
	OverallAmplitude /= FMath::Max(NumFrequencyBands, 1);

	UpdateRingVisualization(DeltaTime);

	OnSpectrumUpdated.Broadcast(SmoothedSpectrum, OverallAmplitude);
}

void AAudioSpectrumActor::UpdateRingVisualization(float DeltaTime)
{
	FRotator CurrentRotation = RingRoot->GetRelativeRotation();
	CurrentRotation.Yaw += RotationSpeed * DeltaTime;
	RingRoot->SetRelativeRotation(CurrentRotation);

	int32 BandsPerBar = FMath::Max(1, NumFrequencyBands / FMath::Max(NumMeshesInRing, 1));

	for (int32 i = 0; i < BarComponents.Num(); ++i)
	{
		int32 BandIndex = FMath::Min(i * BandsPerBar, NumFrequencyBands - 1);
		float Amplitude = (BandIndex < SmoothedSpectrum.Num()) ? SmoothedSpectrum[BandIndex] : 0.0f;
		float NormalizedAmplitude = FMath::Clamp(Amplitude / AudioAmplitudeMultiplier, 0.0f, 1.0f);

		float HeightMultiplier = 1.0f + NormalizedAmplitude * (MaxScaleMultiplier - 1.0f);
		FVector NewScale = BaseBarScale / 100.0f;
		NewScale.Z *= HeightMultiplier;
		BarComponents[i]->SetRelativeScale3D(NewScale);

		float FreqNorm = static_cast<float>(i) / FMath::Max(BarComponents.Num() - 1, 1);
		if (BarMaterialInstances[i])
		{
			FLinearColor Color = FrequencyToColor(FreqNorm, NormalizedAmplitude);
			BarMaterialInstances[i]->SetVectorParameterValue(TEXT("BaseColor"), Color);
			BarMaterialInstances[i]->SetVectorParameterValue(TEXT("EmissiveColor"), Color * (1.0f + NormalizedAmplitude * 5.0f));
			BarMaterialInstances[i]->SetScalarParameterValue(TEXT("EmissiveStrength"), NormalizedAmplitude * 10.0f);
		}
	}
}

FLinearColor AAudioSpectrumActor::FrequencyToColor(float NormalizedFrequency, float Amplitude) const
{
	float Hue = NormalizedFrequency * 270.0f;
	float Saturation = 0.8f + Amplitude * 0.2f;
	float Value = 0.3f + Amplitude * 0.7f;

	return FLinearColor::MakeFromHSV8(
		static_cast<uint8>(Hue / 360.0f * 255.0f),
		static_cast<uint8>(Saturation * 255.0f),
		static_cast<uint8>(Value * 255.0f));
}
