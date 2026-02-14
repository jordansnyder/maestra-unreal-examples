#include "ReactionDiffusionActor.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"

AReactionDiffusionActor::AReactionDiffusionActor()
{
	DisplayPlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisplayPlane"));
	SetRootComponent(DisplayPlane);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane"));
	if (PlaneMesh.Succeeded())
	{
		DisplayPlane->SetStaticMesh(PlaneMesh.Object);
	}
	DisplayPlane->SetRelativeScale3D(FVector(5.0f, 5.0f, 1.0f));
}

void AReactionDiffusionActor::BeginPlay()
{
	Super::BeginPlay();
	InitializeRenderTargets();
	ResetSimulation();
}

void AReactionDiffusionActor::InitializeRenderTargets()
{
	if (SimulationMaterial)
	{
		RenderTargetA = NewObject<UTextureRenderTarget2D>(this);
		RenderTargetA->InitCustomFormat(TextureResolution, TextureResolution, PF_FloatRGBA, false);
		RenderTargetA->ClearColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
		RenderTargetA->UpdateResourceImmediate();

		RenderTargetB = NewObject<UTextureRenderTarget2D>(this);
		RenderTargetB->InitCustomFormat(TextureResolution, TextureResolution, PF_FloatRGBA, false);
		RenderTargetB->ClearColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
		RenderTargetB->UpdateResourceImmediate();

		SimMaterialInstance = UMaterialInstanceDynamic::Create(SimulationMaterial, this);

		bUsingCPUFallback = false;
	}
	else
	{
		bUsingCPUFallback = true;
		InitializeCPUFallback();
	}

	if (DisplayMaterial)
	{
		DisplayMaterialInstance = UMaterialInstanceDynamic::Create(DisplayMaterial, this);
		DisplayPlane->SetMaterial(0, DisplayMaterialInstance);
	}
	else if (bUsingCPUFallback)
	{
		UMaterialInterface* DefaultMat = DisplayPlane->GetMaterial(0);
		if (DefaultMat)
		{
			DisplayMaterialInstance = UMaterialInstanceDynamic::Create(DefaultMat, this);
			DisplayPlane->SetMaterial(0, DisplayMaterialInstance);
		}
	}
}

void AReactionDiffusionActor::InitializeCPUFallback()
{
	int32 Total = TextureResolution * TextureResolution;
	ChemicalA.SetNum(Total);
	ChemicalB.SetNum(Total);
	ChemicalA_Next.SetNum(Total);
	ChemicalB_Next.SetNum(Total);

	CPUTexture = UTexture2D::CreateTransient(TextureResolution, TextureResolution, PF_B8G8R8A8);
	if (CPUTexture)
	{
		CPUTexture->UpdateResource();
	}
}

void AReactionDiffusionActor::ResetSimulation()
{
	if (!bUsingCPUFallback)
	{
		UKismetRenderingLibrary::ClearRenderTarget2D(this, RenderTargetA, FLinearColor(1.0f, 0.0f, 0.0f, 1.0f));
		UKismetRenderingLibrary::ClearRenderTarget2D(this, RenderTargetB, FLinearColor(1.0f, 0.0f, 0.0f, 1.0f));
		bPingPong = false;
	}
	else
	{
		for (int32 i = 0; i < ChemicalA.Num(); ++i)
		{
			ChemicalA[i] = 1.0f;
			ChemicalB[i] = 0.0f;
		}
	}

	AddRandomSeeds(5);
}

void AReactionDiffusionActor::AddSeedPoint(FVector2D UV, float Radius)
{
	if (bUsingCPUFallback)
	{
		int32 CX = FMath::Clamp(FMath::RoundToInt32(UV.X * TextureResolution), 0, TextureResolution - 1);
		int32 CY = FMath::Clamp(FMath::RoundToInt32(UV.Y * TextureResolution), 0, TextureResolution - 1);
		int32 RadiusPixels = FMath::Max(1, FMath::RoundToInt32(Radius * TextureResolution));

		for (int32 Y = -RadiusPixels; Y <= RadiusPixels; ++Y)
		{
			for (int32 X = -RadiusPixels; X <= RadiusPixels; ++X)
			{
				if (X * X + Y * Y <= RadiusPixels * RadiusPixels)
				{
					int32 PX = (CX + X + TextureResolution) % TextureResolution;
					int32 PY = (CY + Y + TextureResolution) % TextureResolution;
					int32 Idx = PY * TextureResolution + PX;
					ChemicalB[Idx] = 1.0f;
				}
			}
		}
	}
}

void AReactionDiffusionActor::AddRandomSeeds(int32 Count)
{
	for (int32 i = 0; i < Count; ++i)
	{
		FVector2D UV(RNG.FRandRange(0.2f, 0.8f), RNG.FRandRange(0.2f, 0.8f));
		float Radius = RNG.FRandRange(0.02f, 0.06f);
		AddSeedPoint(UV, Radius);
	}
}

UTextureRenderTarget2D* AReactionDiffusionActor::GetCurrentRenderTarget() const
{
	return bPingPong ? RenderTargetB : RenderTargetA;
}

void AReactionDiffusionActor::UpdateGenerativeArt(float DeltaTime)
{
	for (int32 Step = 0; Step < StepsPerFrame; ++Step)
	{
		if (bUsingCPUFallback)
		{
			StepCPUSimulation();
		}
		else
		{
			StepSimulation();
		}
	}

	if (bUsingCPUFallback)
	{
		UpdateDisplayFromCPU();
	}
	else if (DisplayMaterialInstance)
	{
		DisplayMaterialInstance->SetTextureParameterValue(TEXT("SimulationState"), GetCurrentRenderTarget());
	}
}

void AReactionDiffusionActor::StepSimulation()
{
	if (!SimMaterialInstance || !RenderTargetA || !RenderTargetB)
	{
		return;
	}

	UTextureRenderTarget2D* Source = bPingPong ? RenderTargetB : RenderTargetA;
	UTextureRenderTarget2D* Dest = bPingPong ? RenderTargetA : RenderTargetB;

	SimMaterialInstance->SetTextureParameterValue(TEXT("PreviousState"), Source);
	SimMaterialInstance->SetScalarParameterValue(TEXT("FeedRate"), FeedRate);
	SimMaterialInstance->SetScalarParameterValue(TEXT("KillRate"), KillRate);
	SimMaterialInstance->SetScalarParameterValue(TEXT("DiffusionRateA"), DiffusionRateA);
	SimMaterialInstance->SetScalarParameterValue(TEXT("DiffusionRateB"), DiffusionRateB);
	SimMaterialInstance->SetScalarParameterValue(TEXT("DeltaT"), DeltaT);
	SimMaterialInstance->SetScalarParameterValue(TEXT("TexelSize"), 1.0f / static_cast<float>(TextureResolution));

	UKismetRenderingLibrary::DrawMaterialToRenderTarget(this, Dest, SimMaterialInstance);

	SwapBuffers();
}

void AReactionDiffusionActor::SwapBuffers()
{
	bPingPong = !bPingPong;
}

void AReactionDiffusionActor::StepCPUSimulation()
{
	int32 W = TextureResolution;
	int32 H = TextureResolution;

	for (int32 Y = 0; Y < H; ++Y)
	{
		for (int32 X = 0; X < W; ++X)
		{
			int32 Idx = Y * W + X;

			float A = ChemicalA[Idx];
			float B = ChemicalB[Idx];

			int32 Left = Y * W + ((X - 1 + W) % W);
			int32 Right = Y * W + ((X + 1) % W);
			int32 Up = ((Y - 1 + H) % H) * W + X;
			int32 Down = ((Y + 1) % H) * W + X;

			float LaplacianA = ChemicalA[Left] + ChemicalA[Right] + ChemicalA[Up] + ChemicalA[Down] - 4.0f * A;
			float LaplacianB = ChemicalB[Left] + ChemicalB[Right] + ChemicalB[Up] + ChemicalB[Down] - 4.0f * B;

			float Reaction = A * B * B;
			ChemicalA_Next[Idx] = A + (DiffusionRateA * LaplacianA - Reaction + FeedRate * (1.0f - A)) * DeltaT;
			ChemicalB_Next[Idx] = B + (DiffusionRateB * LaplacianB + Reaction - (KillRate + FeedRate) * B) * DeltaT;

			ChemicalA_Next[Idx] = FMath::Clamp(ChemicalA_Next[Idx], 0.0f, 1.0f);
			ChemicalB_Next[Idx] = FMath::Clamp(ChemicalB_Next[Idx], 0.0f, 1.0f);
		}
	}

	Swap(ChemicalA, ChemicalA_Next);
	Swap(ChemicalB, ChemicalB_Next);
}

void AReactionDiffusionActor::UpdateDisplayFromCPU()
{
	if (!CPUTexture)
	{
		return;
	}

	FTexture2DMipMap& Mip = CPUTexture->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	if (Data)
	{
		uint8* Pixels = static_cast<uint8*>(Data);
		int32 Total = TextureResolution * TextureResolution;

		for (int32 i = 0; i < Total; ++i)
		{
			float B = ChemicalB[i];

			uint8 R = static_cast<uint8>(FMath::Clamp(B * 200.0f, 0.0f, 255.0f));
			uint8 G = static_cast<uint8>(FMath::Clamp(B * 100.0f, 0.0f, 255.0f));
			uint8 Bl = static_cast<uint8>(FMath::Clamp((1.0f - B) * 255.0f, 0.0f, 255.0f));

			Pixels[i * 4 + 0] = Bl;
			Pixels[i * 4 + 1] = G;
			Pixels[i * 4 + 2] = R;
			Pixels[i * 4 + 3] = 255;
		}

		Mip.BulkData.Unlock();
		CPUTexture->UpdateResource();
	}

	if (DisplayMaterialInstance)
	{
		DisplayMaterialInstance->SetTextureParameterValue(TEXT("SimulationState"), CPUTexture);
	}
}
