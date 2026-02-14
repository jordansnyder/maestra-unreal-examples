#include "RFSpectrumVisualizer.h"
#include "SimulatedRFDataProvider.h"

ARFSpectrumVisualizer::ARFSpectrumVisualizer()
{
	PrimaryActorTick.bCanEverTick = true;

	SpectrumMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SpectrumMesh"));
	SetRootComponent(SpectrumMesh);

	PeakParticles = CreateDefaultSubobject<UNiagaraComponent>(TEXT("PeakParticles"));
	PeakParticles->SetupAttachment(SpectrumMesh);
	PeakParticles->bAutoActivate = false;
}

void ARFSpectrumVisualizer::BeginPlay()
{
	Super::BeginPlay();
	EnsureDataProvider();
	InitializeMesh();

	if (PeakParticleSystem && PeakParticles)
	{
		PeakParticles->SetAsset(PeakParticleSystem);
		PeakParticles->Activate();
	}
}

void ARFSpectrumVisualizer::EnsureDataProvider()
{
	if (!DataProvider && bUseSimulatedData)
	{
		USimulatedRFDataProvider* SimProvider = NewObject<USimulatedRFDataProvider>(this);
		SimProvider->Configure(FrequencyMinMHz, FrequencyMaxMHz, FrequencyResolution, SampleRate);
		DataProvider = SimProvider;
	}

	if (DataProvider)
	{
		DataProvider->Configure(FrequencyMinMHz, FrequencyMaxMHz, FrequencyResolution, SampleRate);
	}
}

void ARFSpectrumVisualizer::InitializeMesh()
{
	int32 NumVerts = FrequencyResolution * WaterfallDepth;

	WaterfallHistory.SetNum(WaterfallDepth);
	for (int32 Row = 0; Row < WaterfallDepth; ++Row)
	{
		WaterfallHistory[Row].SetNum(FrequencyResolution);
		for (int32 Bin = 0; Bin < FrequencyResolution; ++Bin)
		{
			WaterfallHistory[Row][Bin] = -95.0f;
		}
	}
	CurrentRow = 0;

	Vertices.SetNum(NumVerts);
	VertexColors.SetNum(NumVerts);
	Normals.SetNum(NumVerts);
	UVs.SetNum(NumVerts);

	int32 NumQuads = (FrequencyResolution - 1) * (WaterfallDepth - 1);
	Triangles.SetNum(NumQuads * 6);
	int32 TriIdx = 0;

	for (int32 Row = 0; Row < WaterfallDepth - 1; ++Row)
	{
		for (int32 Col = 0; Col < FrequencyResolution - 1; ++Col)
		{
			int32 TopLeft = Row * FrequencyResolution + Col;
			int32 TopRight = TopLeft + 1;
			int32 BottomLeft = (Row + 1) * FrequencyResolution + Col;
			int32 BottomRight = BottomLeft + 1;

			Triangles[TriIdx++] = TopLeft;
			Triangles[TriIdx++] = BottomLeft;
			Triangles[TriIdx++] = TopRight;

			Triangles[TriIdx++] = TopRight;
			Triangles[TriIdx++] = BottomLeft;
			Triangles[TriIdx++] = BottomRight;
		}
	}

	for (int32 Row = 0; Row < WaterfallDepth; ++Row)
	{
		for (int32 Col = 0; Col < FrequencyResolution; ++Col)
		{
			int32 Idx = Row * FrequencyResolution + Col;
			float U = static_cast<float>(Col) / static_cast<float>(FrequencyResolution - 1);
			float V = static_cast<float>(Row) / static_cast<float>(WaterfallDepth - 1);

			Vertices[Idx] = FVector(U * MeshWidth, 0.0f, V * MeshDepth);
			UVs[Idx] = FVector2D(U, V);
			Normals[Idx] = FVector(0.0f, 1.0f, 0.0f);
			VertexColors[Idx] = CoolColor;
		}
	}

	SpectrumMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, VertexColors, TArray<FProcMeshTangent>(), false);
}

void ARFSpectrumVisualizer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!DataProvider)
	{
		return;
	}

	TimeSinceLastFrame += DeltaTime;
	float FrameInterval = 1.0f / SampleRate;

	if (TimeSinceLastFrame < FrameInterval)
	{
		return;
	}
	TimeSinceLastFrame -= FrameInterval;

	FRFSpectrumFrame Frame = DataProvider->GetNextFrame(DeltaTime);

	if (Frame.Amplitudes.Num() > 0)
	{
		PushNewSpectrumRow(Frame);
		UpdateMeshFromWaterfall();
		UpdatePeakParticles(Frame);
	}
}

void ARFSpectrumVisualizer::PushNewSpectrumRow(const FRFSpectrumFrame& Frame)
{
	TArray<float>& Row = WaterfallHistory[CurrentRow];

	int32 CopyCount = FMath::Min(Frame.Amplitudes.Num(), FrequencyResolution);
	for (int32 i = 0; i < CopyCount; ++i)
	{
		Row[i] = Frame.Amplitudes[i];
	}
	for (int32 i = CopyCount; i < FrequencyResolution; ++i)
	{
		Row[i] = -95.0f;
	}

	CurrentRow = (CurrentRow + 1) % WaterfallDepth;
}

void ARFSpectrumVisualizer::UpdateMeshFromWaterfall()
{
	for (int32 DisplayRow = 0; DisplayRow < WaterfallDepth; ++DisplayRow)
	{
		int32 HistoryRow = (CurrentRow - 1 - DisplayRow + WaterfallDepth * 2) % WaterfallDepth;

		for (int32 Col = 0; Col < FrequencyResolution; ++Col)
		{
			int32 Idx = DisplayRow * FrequencyResolution + Col;
			float DbValue = WaterfallHistory[HistoryRow][Col];

			float NormalizedAmp = FMath::Clamp(
				(DbValue - DbMinForColor) / (DbMaxForColor - DbMinForColor), 0.0f, 1.0f);

			float U = static_cast<float>(Col) / static_cast<float>(FrequencyResolution - 1);
			float V = static_cast<float>(DisplayRow) / static_cast<float>(WaterfallDepth - 1);

			Vertices[Idx] = FVector(U * MeshWidth, NormalizedAmp * AmplitudeScale, V * MeshDepth);
			VertexColors[Idx] = AmplitudeToColor(DbValue);
		}
	}

	for (int32 Row = 0; Row < WaterfallDepth; ++Row)
	{
		for (int32 Col = 0; Col < FrequencyResolution; ++Col)
		{
			int32 Idx = Row * FrequencyResolution + Col;

			FVector Right = (Col < FrequencyResolution - 1)
				? Vertices[Idx + 1] - Vertices[Idx]
				: Vertices[Idx] - Vertices[Idx - 1];

			FVector Forward = (Row < WaterfallDepth - 1)
				? Vertices[Idx + FrequencyResolution] - Vertices[Idx]
				: Vertices[Idx] - Vertices[Idx - FrequencyResolution];

			Normals[Idx] = FVector::CrossProduct(Forward, Right).GetSafeNormal();
		}
	}

	SpectrumMesh->UpdateMeshSection_LinearColor(0, Vertices, Normals, UVs, VertexColors, TArray<FProcMeshTangent>());
}

FLinearColor ARFSpectrumVisualizer::AmplitudeToColor(float DbValue) const
{
	float T = FMath::Clamp((DbValue - DbMinForColor) / (DbMaxForColor - DbMinForColor), 0.0f, 1.0f);

	if (T < 0.5f)
	{
		float LocalT = T * 2.0f;
		return FLinearColor::LerpUsingHSV(CoolColor, WarmColor, LocalT);
	}
	else
	{
		float LocalT = (T - 0.5f) * 2.0f;
		return FLinearColor::LerpUsingHSV(WarmColor, HotColor, LocalT);
	}
}

void ARFSpectrumVisualizer::UpdatePeakParticles(const FRFSpectrumFrame& Frame)
{
	if (!PeakParticles || !PeakParticleSystem)
	{
		return;
	}

	for (int32 i = 0; i < Frame.Amplitudes.Num(); ++i)
	{
		if (Frame.Amplitudes[i] > PeakThresholdDb)
		{
			float U = static_cast<float>(i) / static_cast<float>(FMath::Max(Frame.Amplitudes.Num() - 1, 1));
			float NormalizedAmp = FMath::Clamp(
				(Frame.Amplitudes[i] - DbMinForColor) / (DbMaxForColor - DbMinForColor), 0.0f, 1.0f);

			FVector PeakWorldPos = GetActorLocation() + FVector(U * MeshWidth, NormalizedAmp * AmplitudeScale, 0.0f);

			PeakParticles->SetVariableVec3(FName(*FString::Printf(TEXT("PeakPosition_%d"), i % 16)), PeakWorldPos);
			PeakParticles->SetVariableFloat(FName(*FString::Printf(TEXT("PeakIntensity_%d"), i % 16)), NormalizedAmp);
		}
	}
}
