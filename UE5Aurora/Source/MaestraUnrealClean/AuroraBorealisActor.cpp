#include "AuroraBorealisActor.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"

AAuroraBorealisActor::AAuroraBorealisActor()
{
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	DataMixer = CreateDefaultSubobject<UAuroraDataMixer>(TEXT("DataMixer"));

	CurtainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("CurtainMesh"));
	CurtainMesh->SetupAttachment(Root);
	CurtainMesh->bUseAsyncCooking = true;
	CurtainMesh->SetCastShadow(false);
	CurtainMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	HighlightParticles = CreateDefaultSubobject<UNiagaraComponent>(TEXT("HighlightParticles"));
	HighlightParticles->SetupAttachment(Root);
	HighlightParticles->bAutoActivate = false;

	AtmosphericGlow = CreateDefaultSubobject<UPostProcessComponent>(TEXT("AtmosphericGlow"));
	AtmosphericGlow->SetupAttachment(Root);
	AtmosphericGlow->bUnbound = true;

	AuroraFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("AuroraFog"));
	AuroraFog->SetupAttachment(Root);
	AuroraFog->SetFogDensity(0.0f);
}

// ============================================================================
// BeginPlay
// ============================================================================

void AAuroraBorealisActor::BeginPlay()
{
	Super::BeginPlay();

	if (FieldLines.Num() == 0)
	{
		AutoGenerateFieldLines();
	}

	CachedCurtainPoints.SetNum(FieldLines.Num());
	for (auto& Points : CachedCurtainPoints)
	{
		Points.SetNum(FieldLineSamples);
	}

	if (CurtainMaterial)
	{
		CurtainMID = UMaterialInstanceDynamic::Create(CurtainMaterial, this);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AuroraBorealis: No CurtainMaterial assigned!"));
	}

	InitializeCurtainMesh();

	if (HighlightNiagaraSystem && HighlightParticles)
	{
		HighlightParticles->SetAsset(HighlightNiagaraSystem);
		HighlightParticles->Activate(true);
	}

	if (PostProcessMaterial)
	{
		PostProcessMID = UMaterialInstanceDynamic::Create(PostProcessMaterial, this);
		AtmosphericGlow->Settings.WeightedBlendables.Array.Empty();
		FWeightedBlendable Blendable;
		Blendable.Weight = 1.0f;
		Blendable.Object = PostProcessMID;
		AtmosphericGlow->Settings.WeightedBlendables.Array.Add(Blendable);
	}

	AuroraFog->SetFogDensity(FogBaseOpacity);
	AuroraFog->SetFogHeightFalloff(FogHeightFalloff);

	int32 TotalVerts = FieldLines.Num() * FieldLineSamples * (CurtainVerticalSegments + 1);
	UE_LOG(LogTemp, Log, TEXT("AuroraBorealis: %d bands, %d samples, %d vseg, %d total verts"),
		FieldLines.Num(), FieldLineSamples, CurtainVerticalSegments, TotalVerts);
}

// ============================================================================
// Auto-generate Field Lines — spread wide across the sky
// ============================================================================

void AAuroraBorealisActor::AutoGenerateFieldLines()
{
	FieldLines.Empty();

	for (int32 i = 0; i < AutoFieldLineCount; ++i)
	{
		FAuroraFieldLine Line;
		float T = static_cast<float>(i) / FMath::Max(1.0f, static_cast<float>(AutoFieldLineCount - 1));

		Line.LShell = BaseLShell;
		Line.ThetaMin = 0.3f;
		Line.ThetaMax = 2.84f;

		// Spread curtains in depth — close enough to overlap when viewed at angle
		Line.DepthOffset = (T - 0.5f) * CurtainDepthSpacing * static_cast<float>(AutoFieldLineCount);

		// Stagger phases — each band ripples independently
		Line.PhaseOffset = T * PI * 2.0f + FMath::Sin(T * 7.3f) * 1.5f;

		// Hue variation between layers
		Line.HueOffset = (T - 0.5f) * 0.1f;

		// Center bands brightest, outer bands dimmer for natural depth
		float DistFromCenter = FMath::Abs(T - 0.5f) * 2.0f;
		Line.IntensityScale = FMath::Lerp(1.0f, 0.4f, DistFromCenter * DistFromCenter);

		FieldLines.Add(Line);
	}
}

// ============================================================================
// Mesh Initialization
// ============================================================================

void AAuroraBorealisActor::InitializeCurtainMesh()
{
	int32 NumLines = FieldLines.Num();
	int32 VertRows = CurtainVerticalSegments + 1;
	int32 VertCols = FieldLineSamples;

	CurtainMeshSections.SetNum(NumLines);

	for (int32 LineIdx = 0; LineIdx < NumLines; ++LineIdx)
	{
		FCurtainMeshData& Section = CurtainMeshSections[LineIdx];

		int32 NumVerts = VertRows * VertCols;
		Section.Vertices.SetNumZeroed(NumVerts);
		Section.Normals.SetNumZeroed(NumVerts);
		Section.UVs.SetNum(NumVerts);
		Section.Colors.SetNum(NumVerts);

		for (int32 Row = 0; Row < VertRows; ++Row)
		{
			float V = static_cast<float>(Row) / static_cast<float>(CurtainVerticalSegments);
			for (int32 Col = 0; Col < VertCols; ++Col)
			{
				float U = static_cast<float>(Col) / static_cast<float>(VertCols - 1);
				int32 Idx = Row * VertCols + Col;
				Section.UVs[Idx] = FVector2D(U, V);
				Section.Colors[Idx] = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
				Section.Normals[Idx] = FVector(0.0f, -1.0f, 0.0f);
			}
		}

		int32 NumQuads = (VertRows - 1) * (VertCols - 1);
		Section.Triangles.SetNum(NumQuads * 6);
		int32 TriIdx = 0;

		for (int32 Row = 0; Row < VertRows - 1; ++Row)
		{
			for (int32 Col = 0; Col < VertCols - 1; ++Col)
			{
				int32 TL = Row * VertCols + Col;
				int32 TR = TL + 1;
				int32 BL = (Row + 1) * VertCols + Col;
				int32 BR = BL + 1;

				Section.Triangles[TriIdx++] = TL;
				Section.Triangles[TriIdx++] = BL;
				Section.Triangles[TriIdx++] = TR;

				Section.Triangles[TriIdx++] = TR;
				Section.Triangles[TriIdx++] = BL;
				Section.Triangles[TriIdx++] = BR;
			}
		}

		CurtainMesh->CreateMeshSection_LinearColor(
			LineIdx,
			Section.Vertices,
			Section.Triangles,
			Section.Normals,
			Section.UVs,
			Section.Colors,
			TArray<FProcMeshTangent>(),
			false
		);

		if (CurtainMID)
		{
			CurtainMesh->SetMaterial(LineIdx, CurtainMID);
		}

		Section.bInitialized = true;
	}
}

// ============================================================================
// Main Update
// ============================================================================

void AAuroraBorealisActor::UpdateGenerativeArt(float DeltaTime)
{
	const FAuroraParameters& Params = DataMixer->GetCurrentParameters();

	ComputeAllCurtainGeometry(Params);
	UpdateCurtainMesh(Params);
	PushHighlightParameters(Params);
	UpdateAtmosphericGlow(Params, DeltaTime);
	UpdateEcho(Params, DeltaTime);

	if (bDrawFieldLines) DrawDebugFieldLines();
	if (bShowDebugHUD) DrawDebugHUD(Params);
}

// ============================================================================
// Curtain Mesh Update
//
// DESIGN: The mesh provides SHAPE and CONTROL DATA.
// The material shader provides ALL visual beauty (color, rays, shimmer).
//
// Vertex Color encoding:
//   R = height fraction (V: 0=spine/top, 1=lower edge) — shader uses for color gradient
//   G = horizontal position (U: 0-1 along curtain) — shader uses for edge fade + rays
//   B = per-band phase (unique per curtain band) — shader uses to desync shimmer
//   A = opacity (intensity × edge fade × height falloff × band scaling)
//
// Material parameters pushed per frame:
//   AuroraTime, AuroraIntensity, AuroraHue, AuroraSaturation,
//   EmissiveStrength, SubstormFlare, CurtainBaseZ, CurtainTopZ
// ============================================================================

void AAuroraBorealisActor::UpdateCurtainMesh(const FAuroraParameters& Params)
{
	float EffectiveIntensity = FMath::Max(Params.Intensity, EchoIntensity * 0.4f);
	float ISoft = EffectiveIntensity * EffectiveIntensity * (3.0f - 2.0f * EffectiveIntensity);

	float Emissive = FMath::Lerp(BaseEmissive, MaxEmissive, ISoft);
	Emissive += Params.LuminancePulse * 4.0f;
	Emissive += Params.SubstormFlare * 15.0f;

	// Push emissive strength to material
	if (CurtainMID)
	{
		CurtainMID->SetScalarParameterValue(FName(TEXT("EmissiveStrength")), Emissive);
	}

	int32 VertRows = CurtainVerticalSegments + 1;
	int32 VertCols = FieldLineSamples;
	float DynamicHeight = CurtainHeight * Params.VerticalExtent;
	float Time = ElapsedTime;

	for (int32 LineIdx = 0; LineIdx < FieldLines.Num(); ++LineIdx)
	{
		if (!CurtainMeshSections.IsValidIndex(LineIdx) || !CurtainMeshSections[LineIdx].bInitialized)
			continue;

		FCurtainMeshData& Section = CurtainMeshSections[LineIdx];
		const TArray<FVector>& SpinePoints = CachedCurtainPoints[LineIdx];
		const FAuroraFieldLine& Line = FieldLines[LineIdx];

		float LineIntensity = EffectiveIntensity * Line.IntensityScale;
		float BandPhase = FMath::Fmod(Line.PhaseOffset / (PI * 2.0f), 1.0f); // normalize to 0-1

		for (int32 Col = 0; Col < VertCols; ++Col)
		{
			float U = static_cast<float>(Col) / static_cast<float>(VertCols - 1);
			FVector SpinePos = SpinePoints[Col];

			// Spine tangent for face normal
			FVector Tangent;
			if (Col == 0)
				Tangent = (SpinePoints[1] - SpinePoints[0]).GetSafeNormal();
			else if (Col == VertCols - 1)
				Tangent = (SpinePoints[Col] - SpinePoints[Col - 1]).GetSafeNormal();
			else
				Tangent = (SpinePoints[Col + 1] - SpinePoints[Col - 1]).GetSafeNormal();

			FVector DownDir(0.0f, 0.0f, -1.0f);
			FVector FaceNormal = FVector::CrossProduct(Tangent, DownDir).GetSafeNormal();
			if (FaceNormal.IsNearlyZero())
				FaceNormal = FVector(0.0f, -1.0f, 0.0f);

			// Horizontal edge fade
			float EdgeFade = FMath::Sin(U * PI);
			EdgeFade = FMath::Pow(FMath::Max(EdgeFade, 0.0f), 0.6f);

			// ---- VERTICAL RAY STRUCTURE (per-column) ----
			float RayNoise = FMath::PerlinNoise3D(FVector(
				U * RayFrequency + BandPhase * 3.0f,
				Time * 0.15f,
				static_cast<float>(LineIdx) * 3.7f
			));
			float RayBrightness = FMath::Lerp(1.0f, (RayNoise + 1.0f) * 0.5f, RayContrast);

			// Shimmer base for this column
			float ShimmerBase = FMath::PerlinNoise3D(FVector(
				U * 4.0f + BandPhase * 6.0f,
				Time * RayShimmerSpeed,
				static_cast<float>(LineIdx) * 2.1f
			));

			// Data-driven hue for this band
			float DataHue = FMath::Fmod(Params.Hue + Line.HueOffset + 1.0f, 1.0f);

			for (int32 Row = 0; Row < VertRows; ++Row)
			{
				float V = static_cast<float>(Row) / static_cast<float>(CurtainVerticalSegments);
				int32 Idx = Row * VertCols + Col;

				// ---- VERTEX POSITION ----
				float DropDistance = V * DynamicHeight;

				// Gentle cloth-like sway — low frequency, smooth curves
				float SwayPhase = U * 3.0f + Time * Params.WaveSpeed * 0.3f + Line.PhaseOffset;
				float Sway = FMath::Sin(SwayPhase + V * 1.5f) * V * V * 60.0f * Params.NoisePersistence;

				// Broad organic drift — low-frequency noise for gentle undulation
				float VertNoise = FMath::PerlinNoise3D(FVector(
					U * 2.0f + Line.PhaseOffset,
					V * 1.5f,
					Time * 0.12f + static_cast<float>(LineIdx) * 1.3f
				));

				Section.Vertices[Idx] = SpinePos
					+ DownDir * DropDistance
					+ FaceNormal * (Sway + VertNoise * V * 80.0f);

				// ---- BRIGHTNESS ----
				// Real aurora brightness profile:
				//   - Brightest band is in the lower-middle portion (~20-40% from bottom)
				//   - Gradual fade toward the top (upper colors still clearly visible)
				//   - Quick taper at the very bottom edge
				//
				// Use a bell-curve-like profile centered at V=0.7 (bottom 30%)
				// with a wide upper tail so purple/red zones are still visible
				float BrightCenter = 0.7f;  // peak brightness at 70% of way down
				float DistFromPeak = V - BrightCenter;
				// Asymmetric: tight below peak, wide above peak
				float Sigma = (V < BrightCenter) ? 0.25f : 0.55f;
				float HeightProfile = FMath::Exp(-0.5f * (DistFromPeak * DistFromPeak) / (Sigma * Sigma));
				// Add a floor so upper regions don't vanish entirely
				HeightProfile = FMath::Max(HeightProfile, 0.08f);

				// Vertical shimmer wave traveling upward
				float ShimmerPhase = V * 8.0f - Time * RayShimmerSpeed + U * 4.0f + BandPhase * 6.28f;
				float Shimmer = FMath::Lerp(0.65f, 1.0f, (FMath::Sin(ShimmerPhase + ShimmerBase * 3.0f) + 1.0f) * 0.5f);

				float Alpha = HeightProfile * RayBrightness * Shimmer * EdgeFade * LineIntensity;
				Alpha += Params.SubstormFlare * 0.3f * V;
				Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

				// ---- COLOR ----
				// Height-based: green(bottom) → cyan → purple → magenta/red(top)
				// V=0 is spine (top of curtain), V=1 is lower edge
				float AltFrac = 1.0f - V; // 0=bottom, 1=top
				FLinearColor AltColor = EmissionColorFromAltitude(AltFrac);

				// Only a subtle tint from data-driven hue — keep natural aurora colors dominant
				FLinearColor DataColor = AuroraHSVToColor(DataHue, Params.Saturation, 1.0f);
				FLinearColor FinalColor = FMath::Lerp(AltColor, DataColor, 0.15f);

				// Boost color saturation — prevent washed-out appearance
				float Luma = FinalColor.R * 0.299f + FinalColor.G * 0.587f + FinalColor.B * 0.114f;
				FinalColor.R = FMath::Lerp(Luma, FinalColor.R, 1.4f);
				FinalColor.G = FMath::Lerp(Luma, FinalColor.G, 1.4f);
				FinalColor.B = FMath::Lerp(Luma, FinalColor.B, 1.4f);
				FinalColor.R = FMath::Clamp(FinalColor.R, 0.0f, 1.0f);
				FinalColor.G = FMath::Clamp(FinalColor.G, 0.0f, 1.0f);
				FinalColor.B = FMath::Clamp(FinalColor.B, 0.0f, 1.0f);

				// Write LDR color in 0-1 range — the material multiplies by
				// EmissiveStrength to produce HDR bloom. Vertex color buffers
				// are 8-bit per channel so values > 1 get clamped to white.
				Section.Colors[Idx] = FLinearColor(
					FinalColor.R,
					FinalColor.G,
					FinalColor.B,
					Alpha
				);
			}
		}

		// Normals
		for (int32 Row = 0; Row < VertRows; ++Row)
		{
			for (int32 Col = 0; Col < VertCols; ++Col)
			{
				int32 Idx = Row * VertCols + Col;
				FVector Right = (Col < VertCols - 1)
					? Section.Vertices[Idx + 1] - Section.Vertices[Idx]
					: Section.Vertices[Idx] - Section.Vertices[Idx - 1];
				FVector Down = (Row < VertRows - 1)
					? Section.Vertices[Idx + VertCols] - Section.Vertices[Idx]
					: Section.Vertices[Idx] - Section.Vertices[Idx - VertCols];
				Section.Normals[Idx] = FVector::CrossProduct(Down, Right).GetSafeNormal();
			}
		}

		CurtainMesh->UpdateMeshSection_LinearColor(
			LineIdx, Section.Vertices, Section.Normals,
			Section.UVs, Section.Colors, TArray<FProcMeshTangent>()
		);
	}
}

// ============================================================================
// Spine Point — horizontal arc at altitude with fold ripple + drift
// ============================================================================

FVector AAuroraBorealisActor::ComputeSpinePoint(const FAuroraFieldLine& Line, float T,
	const FAuroraParameters& Params) const
{
	float X = (T - 0.5f) * CurtainSpan;
	float Y = Line.DepthOffset;

	float ArcT = (T - 0.5f) * 2.0f;
	float ArcZ = CurtainAltitude + CurtainArcHeight * (1.0f - ArcT * ArcT);

	float FoldFreq = Params.FoldCount;
	float Phase = Line.PhaseOffset + ElapsedTime * Params.WaveSpeed * 0.5f;

	float PrimaryFold = FMath::Sin(FoldFreq * T * PI * 2.0f + Phase);
	float DetailFold = FMath::Sin(FoldFreq * 2.5f * T * PI * 2.0f + Phase * 1.7f + 3.14f) * 0.3f;

	float Envelope = FMath::Sin(T * PI);
	float FoldMagnitude = (PrimaryFold + DetailFold * Params.NoisePersistence) * Envelope;
	float Amplitude = BaseFoldAmplitude * Params.VerticalExtent;

	Y += FoldMagnitude * Amplitude * 0.5f;

	float NoiseY = FMath::PerlinNoise3D(FVector(T * 3.0f, ElapsedTime * 0.1f, Line.PhaseOffset));
	float NoiseZ = FMath::PerlinNoise3D(FVector(T * 3.0f + 100.0f, ElapsedTime * 0.08f, Line.PhaseOffset + 50.0f));
	float NoiseX = FMath::PerlinNoise3D(FVector(T * 2.0f + 200.0f, ElapsedTime * 0.06f, Line.PhaseOffset + 100.0f));

	Y += NoiseY * Amplitude * 0.2f;
	ArcZ += NoiseZ * Amplitude * 0.25f;
	X += NoiseX * Amplitude * 0.1f;

	return FVector(X, Y, ArcZ);
}

void AAuroraBorealisActor::ComputeAllCurtainGeometry(const FAuroraParameters& Params)
{
	FVector ActorLoc = GetActorLocation();
	CachedCurtainPoints.SetNum(FieldLines.Num());

	for (int32 LineIdx = 0; LineIdx < FieldLines.Num(); ++LineIdx)
	{
		const FAuroraFieldLine& Line = FieldLines[LineIdx];
		TArray<FVector>& Points = CachedCurtainPoints[LineIdx];
		Points.SetNum(FieldLineSamples);

		for (int32 i = 0; i < FieldLineSamples; ++i)
		{
			float T = static_cast<float>(i) / static_cast<float>(FieldLineSamples - 1);
			Points[i] = ActorLoc + ComputeSpinePoint(Line, T, Params);
		}
	}
}

// ============================================================================
// Blueprint API
// ============================================================================

FVector AAuroraBorealisActor::SampleFieldLinePoint(int32 LineIndex, float T) const
{
	if (!FieldLines.IsValidIndex(LineIndex)) return GetActorLocation();
	FAuroraParameters DefaultParams;
	return GetActorLocation() + ComputeSpinePoint(FieldLines[LineIndex], FMath::Clamp(T, 0.0f, 1.0f), DefaultParams);
}

TArray<FVector> AAuroraBorealisActor::GetCurtainPoints(int32 LineIndex) const
{
	if (CachedCurtainPoints.IsValidIndex(LineIndex)) return CachedCurtainPoints[LineIndex];
	return TArray<FVector>();
}

// ============================================================================
// Niagara Highlights
// ============================================================================

void AAuroraBorealisActor::PushHighlightParameters(const FAuroraParameters& Params)
{
	if (!HighlightParticles || !HighlightParticles->IsActive()) return;

	HighlightParticles->SetVariableFloat(FName(TEXT("SpawnRate")),
		Params.LuminancePulse * 20.0f + Params.SubstormFlare * 50.0f);

	float HighlightSize = FMath::Lerp(100.0f, 300.0f, Params.LuminancePulse);
	HighlightParticles->SetVariableFloat(FName(TEXT("ParticleSize")), HighlightSize);
	HighlightParticles->SetVariableVec2(FName(TEXT("SpriteSize")), FVector2D(HighlightSize, HighlightSize * 0.6f));

	FLinearColor HighColor = AuroraHSVToColor(FMath::Fmod(Params.Hue + 0.05f, 1.0f),
		FMath::Min(Params.Saturation * 1.1f, 1.0f), 1.0f);
	HighlightParticles->SetVariableLinearColor(FName(TEXT("ParticleColor")),
		HighColor * (Params.Intensity * 8.0f + 2.0f));

	float HalfSpan = CurtainSpan * 0.5f;
	HighlightParticles->SetVariableVec3(FName(TEXT("FieldBoundsMin")),
		FVector(-HalfSpan, -800.0f, CurtainAltitude - CurtainHeight));
	HighlightParticles->SetVariableVec3(FName(TEXT("FieldBoundsMax")),
		FVector(HalfSpan, 800.0f, CurtainAltitude + CurtainArcHeight));
}

// ============================================================================
// Atmospheric Glow
// ============================================================================

void AAuroraBorealisActor::UpdateAtmosphericGlow(const FAuroraParameters& Params, float DeltaTime)
{
	float LagRate = (GlowLagSeconds > 0.0f) ? (DeltaTime / GlowLagSeconds) : 1.0f;
	LaggedIntensity = FMath::Lerp(LaggedIntensity, Params.Intensity, FMath::Clamp(LagRate, 0.0f, 1.0f));

	AuroraFog->SetFogDensity(FMath::Lerp(FogBaseOpacity, FogMaxOpacity, LaggedIntensity));

	FLinearColor FogColor = AuroraHSVToColor(Params.Hue, Params.Saturation * 0.5f, 0.8f);
	AuroraFog->SetFogInscatteringColor(FogColor);

	if (PostProcessMID)
	{
		PostProcessMID->SetScalarParameterValue(FName(TEXT("AuroraIntensity")), LaggedIntensity);
		PostProcessMID->SetScalarParameterValue(FName(TEXT("BloomStrength")),
			LaggedIntensity * 2.0f + Params.SubstormFlare * 5.0f);
		PostProcessMID->SetVectorParameterValue(FName(TEXT("AuroraTint")), FogColor);
	}
}

// ============================================================================
// Echo
// ============================================================================

void AAuroraBorealisActor::UpdateEcho(const FAuroraParameters& Params, float DeltaTime)
{
	if (Params.Intensity > EchoIntensity)
		EchoIntensity = Params.Intensity;
	else
	{
		float DecayRate = (EchoDecayTime > 0.0f) ? (DeltaTime / EchoDecayTime) : 1.0f;
		EchoIntensity = FMath::Lerp(EchoIntensity, Params.Intensity, FMath::Clamp(DecayRate, 0.0f, 1.0f));
	}
}

// ============================================================================
// Color Utilities (still used for fog/highlights, not for curtain mesh)
// ============================================================================

FLinearColor AAuroraBorealisActor::AuroraHSVToColor(float H, float S, float V)
{
	float C = V * S;
	float Hp = FMath::Fmod(H * 6.0f, 6.0f);
	if (Hp < 0.0f) Hp += 6.0f;
	float X = C * (1.0f - FMath::Abs(FMath::Fmod(Hp, 2.0f) - 1.0f));
	float M = V - C;
	float R = 0, G = 0, B = 0;
	if (Hp < 1.0f)      { R = C; G = X; }
	else if (Hp < 2.0f) { R = X; G = C; }
	else if (Hp < 3.0f) { G = C; B = X; }
	else if (Hp < 4.0f) { G = X; B = C; }
	else if (Hp < 5.0f) { R = X; B = C; }
	else                 { R = C; B = X; }
	return FLinearColor(R + M, G + M, B + M, 1.0f);
}

FLinearColor AAuroraBorealisActor::EmissionColorFromAltitude(float NormalizedAltitude)
{
	// NormalizedAltitude: 0 = bottom of curtain (lower edge), 1 = top of curtain (spine)
	// Real aurora: bright green at bottom → cyan/teal → deep blue-purple → magenta/red at top
	float A = FMath::Clamp(NormalizedAltitude, 0.0f, 1.0f);

	// 5-stop gradient for richer color transitions
	FLinearColor BrightGreen(0.05f, 1.0f, 0.15f, 1.0f);  // bottom — vivid oxygen green
	FLinearColor Teal(0.0f, 0.85f, 0.55f, 1.0f);          // lower-mid — teal/seafoam
	FLinearColor DeepBlue(0.1f, 0.2f, 0.9f, 1.0f);        // mid — deep blue transition
	FLinearColor Purple(0.6f, 0.05f, 0.95f, 1.0f);        // upper-mid — vivid purple
	FLinearColor Magenta(0.9f, 0.05f, 0.4f, 1.0f);        // top — hot magenta/red

	if (A < 0.2f)       return FMath::Lerp(BrightGreen, Teal, A / 0.2f);
	else if (A < 0.45f) return FMath::Lerp(Teal, DeepBlue, (A - 0.2f) / 0.25f);
	else if (A < 0.7f)  return FMath::Lerp(DeepBlue, Purple, (A - 0.45f) / 0.25f);
	else                 return FMath::Lerp(Purple, Magenta, (A - 0.7f) / 0.3f);
}

// ============================================================================
// Debug
// ============================================================================

void AAuroraBorealisActor::DrawDebugFieldLines() const
{
	UWorld* World = GetWorld();
	if (!World) return;
	for (int32 LineIdx = 0; LineIdx < CachedCurtainPoints.Num(); ++LineIdx)
	{
		const TArray<FVector>& Points = CachedCurtainPoints[LineIdx];
		FColor LineColor = FLinearColor::MakeFromHSV8(
			static_cast<uint8>(FMath::Fmod(static_cast<float>(LineIdx) * 0.08f, 1.0f) * 255.0f),
			200, 255).ToFColor(true);
		for (int32 i = 0; i < Points.Num() - 1; ++i)
			DrawDebugLine(World, Points[i], Points[i + 1], LineColor, false, -1.0f, 0, 2.0f);
	}
}

void AAuroraBorealisActor::DrawDebugHUD(const FAuroraParameters& Params) const
{
	if (!GEngine) return;
	int32 Key = 200;
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Cyan, TEXT("=== AURORA BOREALIS ==="));

	int32 TV = FieldLines.Num() * FieldLineSamples * (CurtainVerticalSegments + 1);
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::White,
		FString::Printf(TEXT("Bands:%d Samp:%d VSeg:%d Verts:%d"), FieldLines.Num(), FieldLineSamples, CurtainVerticalSegments, TV));
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Green,
		FString::Printf(TEXT("Int:%.3f Hue:%.2f Sat:%.2f"), Params.Intensity, Params.Hue, Params.Saturation));
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Yellow,
		FString::Printf(TEXT("Span:%.0f Alt:%.0f Height:%.0f Fold:%.0f"), CurtainSpan, CurtainAltitude, CurtainHeight, BaseFoldAmplitude));
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f,
		CurtainMID ? FColor::Green : FColor::Red,
		FString::Printf(TEXT("Material: %s"), CurtainMID ? TEXT("OK") : TEXT("NONE!")));
}
