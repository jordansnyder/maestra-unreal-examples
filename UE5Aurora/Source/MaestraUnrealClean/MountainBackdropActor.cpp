#include "MountainBackdropActor.h"

AMountainBackdropActor::AMountainBackdropActor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	MountainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("MountainMesh"));
	MountainMesh->SetupAttachment(Root);
	MountainMesh->SetCastShadow(false);
	MountainMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AMountainBackdropActor::BeginPlay()
{
	Super::BeginPlay();
	GenerateMountains();
}

float AMountainBackdropActor::SampleMountainHeight(float Angle) const
{
	// Layer multiple Perlin noise octaves for natural-looking mountain profiles
	// Use the angle around the ring as input, scaled to different frequencies

	float Seed = static_cast<float>(MountainSeed);

	// Large peaks — the dominant mountain shapes
	float N1 = FMath::PerlinNoise3D(FVector(
		FMath::Cos(Angle) * 2.0f + Seed,
		FMath::Sin(Angle) * 2.0f + Seed,
		Seed * 0.1f
	));

	// Medium ridgelines
	float N2 = FMath::PerlinNoise3D(FVector(
		FMath::Cos(Angle) * 5.0f + Seed + 100.0f,
		FMath::Sin(Angle) * 5.0f + Seed + 100.0f,
		Seed * 0.2f
	));

	// Small rocky details
	float N3 = FMath::PerlinNoise3D(FVector(
		FMath::Cos(Angle) * 12.0f + Seed + 200.0f,
		FMath::Sin(Angle) * 12.0f + Seed + 200.0f,
		Seed * 0.3f
	));

	// Very fine jagged peaks
	float N4 = FMath::PerlinNoise3D(FVector(
		FMath::Cos(Angle) * 25.0f + Seed + 300.0f,
		FMath::Sin(Angle) * 25.0f + Seed + 300.0f,
		Seed * 0.4f
	));

	// Combine: large shapes dominant, smaller detail adds roughness
	// Perlin returns [-1,1], remap to [0,1] and weight
	float Height = (N1 + 1.0f) * 0.5f * 1.0f    // big shapes
				 + (N2 + 1.0f) * 0.5f * 0.4f     // ridges
				 + (N3 + 1.0f) * 0.5f * 0.15f     // rocks
				 + (N4 + 1.0f) * 0.5f * 0.06f;    // jagged tips

	// Normalize to roughly [0,1]
	Height /= 1.61f;

	// Apply some power shaping to make peaks sharper
	Height = FMath::Pow(Height, 1.3f);

	return BaseHeight + Height * PeakHeight;
}

void AMountainBackdropActor::GenerateMountains()
{
	FVector ActorLoc = GetActorLocation();

	int32 VertRows = VerticalDivisions + 1;
	int32 VertCols = Segments + 1; // +1 to close the ring (duplicate first/last)

	int32 NumVerts = VertRows * VertCols;

	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	TArray<int32> Triangles;

	Vertices.SetNum(NumVerts);
	Normals.SetNum(NumVerts);
	UVs.SetNum(NumVerts);
	Colors.SetNum(NumVerts);

	// Generate vertices — a ring of mountain columns
	for (int32 Col = 0; Col < VertCols; ++Col)
	{
		float U = static_cast<float>(Col) / static_cast<float>(Segments);
		float Angle = U * PI * 2.0f;

		float DirX = FMath::Cos(Angle);
		float DirY = FMath::Sin(Angle);

		float TopHeight = SampleMountainHeight(Angle);

		for (int32 Row = 0; Row < VertRows; ++Row)
		{
			float V = static_cast<float>(Row) / static_cast<float>(VerticalDivisions);
			int32 Idx = Row * VertCols + Col;

			// V=0 is bottom (below ground), V=1 is peak
			float Z = FMath::Lerp(-BelowGroundDepth, TopHeight, V);

			Vertices[Idx] = FVector(DirX * RingRadius, DirY * RingRadius, Z);

			// Normal points inward (toward viewer at center)
			Normals[Idx] = FVector(-DirX, -DirY, 0.0f);

			UVs[Idx] = FVector2D(U, V);

			// Very dark color — almost black with slight variation for depth
			float Darkness = FMath::Lerp(0.005f, 0.03f, V * V);
			Colors[Idx] = FLinearColor(Darkness, Darkness, Darkness * 1.1f, 1.0f);
		}
	}

	// Triangles — facing inward
	int32 NumQuads = VerticalDivisions * Segments;
	Triangles.SetNum(NumQuads * 6);
	int32 TriIdx = 0;

	for (int32 Col = 0; Col < Segments; ++Col)
	{
		for (int32 Row = 0; Row < VerticalDivisions; ++Row)
		{
			int32 TL = Row * VertCols + Col;
			int32 TR = TL + 1;
			int32 BL = (Row + 1) * VertCols + Col;
			int32 BR = BL + 1;

			// Wind triangles to face inward (toward center)
			Triangles[TriIdx++] = TL;
			Triangles[TriIdx++] = TR;
			Triangles[TriIdx++] = BL;

			Triangles[TriIdx++] = TR;
			Triangles[TriIdx++] = BR;
			Triangles[TriIdx++] = BL;
		}
	}

	MountainMesh->CreateMeshSection_LinearColor(
		0,
		Vertices,
		Triangles,
		Normals,
		UVs,
		Colors,
		TArray<FProcMeshTangent>(),
		false
	);

	// Apply dark material if provided
	if (MountainMaterial)
	{
		MountainMesh->SetMaterial(0, MountainMaterial);
	}

	UE_LOG(LogTemp, Log, TEXT("MountainBackdrop: Generated %d verts, %d tris, radius %.0f"),
		NumVerts, Triangles.Num() / 3, RingRadius);
}
