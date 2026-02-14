#include "ParametricMeshActor.h"

AParametricMeshActor::AParametricMeshActor()
{
	MeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("MeshComponent"));
	SetRootComponent(MeshComponent);
}

void AParametricMeshActor::BeginPlay()
{
	Super::BeginPlay();
	RegenerateMesh();
}

void AParametricMeshActor::UpdateGenerativeArt(float DeltaTime)
{
	if (bAnimateParameters)
	{
		float T = ElapsedTime * AnimationSpeed;

		switch (ShapeType)
		{
		case EParametricShape::Superformula:
			SF_M1 = 3.0f + FMath::Sin(T * 0.7f) * 4.0f;
			SF_M2 = 3.0f + FMath::Cos(T * 0.5f) * 4.0f;
			SF_N1_1 = 0.5f + FMath::Sin(T * 0.3f) * 0.5f;
			SF_N2_1 = 0.5f + FMath::Cos(T * 0.4f) * 0.5f;
			break;
		case EParametricShape::TorusKnot:
			TK_MinorRadius = 30.0f + FMath::Sin(T) * 20.0f;
			TK_TubeRadius = 5.0f + FMath::Sin(T * 1.3f) * 4.0f;
			break;
		case EParametricShape::LorenzAttractor:
		case EParametricShape::RosslerAttractor:
			Attractor_Sigma = 10.0f + FMath::Sin(T * 0.2f) * 2.0f;
			Attractor_Rho = 28.0f + FMath::Cos(T * 0.15f) * 4.0f;
			break;
		}

		RegenerateMesh();
	}
}

void AParametricMeshActor::RegenerateMesh()
{
	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Norms;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;

	switch (ShapeType)
	{
	case EParametricShape::Superformula:
		GenerateSuperformula(Verts, Tris, Norms, UVs, Colors);
		break;
	case EParametricShape::TorusKnot:
		GenerateTorusKnot(Verts, Tris, Norms, UVs, Colors);
		break;
	case EParametricShape::LorenzAttractor:
		GenerateAttractor(Verts, Tris, Norms, UVs, Colors, true);
		break;
	case EParametricShape::RosslerAttractor:
		GenerateAttractor(Verts, Tris, Norms, UVs, Colors, false);
		break;
	}

	MeshComponent->ClearAllMeshSections();
	if (Verts.Num() > 0 && Tris.Num() > 0)
	{
		MeshComponent->CreateMeshSection_LinearColor(0, Verts, Tris, Norms, UVs, Colors, TArray<FProcMeshTangent>(), true);
	}
}

float AParametricMeshActor::EvaluateSuperformula(float Angle, float A, float B, float M,
	float N1, float N2, float N3) const
{
	float T1 = FMath::Abs(FMath::Cos(M * Angle / 4.0f) / A);
	float T2 = FMath::Abs(FMath::Sin(M * Angle / 4.0f) / B);
	float R = FMath::Pow(FMath::Pow(T1, N2) + FMath::Pow(T2, N3), -1.0f / N1);
	return R;
}

void AParametricMeshActor::GenerateSuperformula(TArray<FVector>& OutVerts, TArray<int32>& OutTris,
	TArray<FVector>& OutNormals, TArray<FVector2D>& OutUVs, TArray<FLinearColor>& OutColors)
{
	int32 StepsTheta = Resolution;
	int32 StepsPhi = Resolution * 2;

	OutVerts.Reserve(StepsTheta * StepsPhi);
	OutUVs.Reserve(StepsTheta * StepsPhi);
	OutColors.Reserve(StepsTheta * StepsPhi);
	OutNormals.Reserve(StepsTheta * StepsPhi);

	for (int32 i = 0; i <= StepsTheta; ++i)
	{
		float Theta = -PI / 2.0f + PI * static_cast<float>(i) / StepsTheta;
		float R1 = EvaluateSuperformula(Theta, SF_A, SF_B, SF_M1, SF_N1_1, SF_N2_1, SF_N3_1);

		for (int32 j = 0; j <= StepsPhi; ++j)
		{
			float Phi = -PI + 2.0f * PI * static_cast<float>(j) / StepsPhi;
			float R2 = EvaluateSuperformula(Phi, SF_A, SF_B, SF_M2, SF_N1_2, SF_N2_2, SF_N3_2);

			float X = R1 * FMath::Cos(Theta) * R2 * FMath::Cos(Phi);
			float Y = R1 * FMath::Cos(Theta) * R2 * FMath::Sin(Phi);
			float Z = R1 * FMath::Sin(Theta);

			OutVerts.Add(FVector(X, Y, Z) * MeshScale);
			OutUVs.Add(FVector2D(
				static_cast<float>(j) / StepsPhi,
				static_cast<float>(i) / StepsTheta));

			float ColorT = (Z + 1.0f) * 0.5f;
			OutColors.Add(FLinearColor::LerpUsingHSV(
				FLinearColor(0.1f, 0.3f, 0.8f),
				FLinearColor(0.9f, 0.2f, 0.5f), ColorT));
			OutNormals.Add(FVector(X, Y, Z).GetSafeNormal());
		}
	}

	int32 Cols = StepsPhi + 1;
	for (int32 i = 0; i < StepsTheta; ++i)
	{
		for (int32 j = 0; j < StepsPhi; ++j)
		{
			int32 TL = i * Cols + j;
			int32 TR = TL + 1;
			int32 BL = (i + 1) * Cols + j;
			int32 BR = BL + 1;

			OutTris.Add(TL);
			OutTris.Add(BL);
			OutTris.Add(TR);

			OutTris.Add(TR);
			OutTris.Add(BL);
			OutTris.Add(BR);
		}
	}
}

void AParametricMeshActor::GenerateTorusKnot(TArray<FVector>& OutVerts, TArray<int32>& OutTris,
	TArray<FVector>& OutNormals, TArray<FVector2D>& OutUVs, TArray<FLinearColor>& OutColors)
{
	int32 CurveSteps = Resolution * 4;
	TArray<FVector> CurvePoints;
	CurvePoints.Reserve(CurveSteps);

	for (int32 i = 0; i < CurveSteps; ++i)
	{
		float T = 2.0f * PI * static_cast<float>(i) / CurveSteps;
		float R = TK_MajorRadius + TK_MinorRadius * FMath::Cos(TK_Q * T);
		float X = R * FMath::Cos(TK_P * T);
		float Y = R * FMath::Sin(TK_P * T);
		float Z = TK_MinorRadius * FMath::Sin(TK_Q * T);
		CurvePoints.Add(FVector(X, Y, Z));
	}

	GenerateTubeAlongCurve(CurvePoints, TK_TubeRadius, TK_TubeSides, OutVerts, OutTris, OutNormals, OutUVs, OutColors);
}

FVector AParametricMeshActor::LorenzDerivative(FVector State) const
{
	return FVector(
		Attractor_Sigma * (State.Y - State.X),
		State.X * (Attractor_Rho - State.Z) - State.Y,
		State.X * State.Y - Attractor_Beta * State.Z
	);
}

FVector AParametricMeshActor::RosslerDerivative(FVector State) const
{
	float A = 0.2f;
	float B = 0.2f;
	float C = 5.7f;
	return FVector(
		-State.Y - State.Z,
		State.X + A * State.Y,
		B + State.Z * (State.X - C)
	);
}

void AParametricMeshActor::GenerateAttractor(TArray<FVector>& OutVerts, TArray<int32>& OutTris,
	TArray<FVector>& OutNormals, TArray<FVector2D>& OutUVs, TArray<FLinearColor>& OutColors, bool bLorenz)
{
	TArray<FVector> CurvePoints;
	CurvePoints.Reserve(Attractor_Iterations);

	FVector State = bLorenz ? FVector(1.0f, 1.0f, 1.0f) : FVector(0.1f, 0.0f, 0.0f);
	float Dt = Attractor_StepSize;

	for (int32 i = 0; i < Attractor_Iterations; ++i)
	{
		CurvePoints.Add(State * Attractor_Scale);

		FVector K1 = bLorenz ? LorenzDerivative(State) : RosslerDerivative(State);
		FVector K2 = bLorenz ? LorenzDerivative(State + K1 * Dt * 0.5f) : RosslerDerivative(State + K1 * Dt * 0.5f);
		FVector K3 = bLorenz ? LorenzDerivative(State + K2 * Dt * 0.5f) : RosslerDerivative(State + K2 * Dt * 0.5f);
		FVector K4 = bLorenz ? LorenzDerivative(State + K3 * Dt) : RosslerDerivative(State + K3 * Dt);

		State += (K1 + K2 * 2.0f + K3 * 2.0f + K4) * (Dt / 6.0f);
	}

	GenerateTubeAlongCurve(CurvePoints, Attractor_TubeRadius, Attractor_TubeSides, OutVerts, OutTris, OutNormals, OutUVs, OutColors);
}

void AParametricMeshActor::GenerateTubeAlongCurve(const TArray<FVector>& CurvePoints, float Radius, int32 Sides,
	TArray<FVector>& OutVerts, TArray<int32>& OutTris,
	TArray<FVector>& OutNormals, TArray<FVector2D>& OutUVs, TArray<FLinearColor>& OutColors)
{
	if (CurvePoints.Num() < 2)
	{
		return;
	}

	int32 NumSegments = CurvePoints.Num();
	OutVerts.Reserve(NumSegments * (Sides + 1));
	OutNormals.Reserve(NumSegments * (Sides + 1));
	OutUVs.Reserve(NumSegments * (Sides + 1));
	OutColors.Reserve(NumSegments * (Sides + 1));

	for (int32 Seg = 0; Seg < NumSegments; ++Seg)
	{
		FVector Tangent;
		if (Seg == 0)
		{
			Tangent = (CurvePoints[1] - CurvePoints[0]).GetSafeNormal();
		}
		else if (Seg == NumSegments - 1)
		{
			Tangent = (CurvePoints[Seg] - CurvePoints[Seg - 1]).GetSafeNormal();
		}
		else
		{
			Tangent = (CurvePoints[Seg + 1] - CurvePoints[Seg - 1]).GetSafeNormal();
		}

		FVector Up = FVector::UpVector;
		if (FMath::Abs(FVector::DotProduct(Tangent, Up)) > 0.99f)
		{
			Up = FVector::RightVector;
		}
		FVector Normal = FVector::CrossProduct(Tangent, Up).GetSafeNormal();
		FVector Binormal = FVector::CrossProduct(Tangent, Normal).GetSafeNormal();

		float ColorT = static_cast<float>(Seg) / FMath::Max(NumSegments - 1, 1);

		for (int32 S = 0; S <= Sides; ++S)
		{
			float Angle = 2.0f * PI * static_cast<float>(S) / Sides;
			FVector CirclePos = Normal * FMath::Cos(Angle) + Binormal * FMath::Sin(Angle);

			OutVerts.Add(CurvePoints[Seg] + CirclePos * Radius);
			OutNormals.Add(CirclePos);
			OutUVs.Add(FVector2D(static_cast<float>(S) / Sides, ColorT));

			OutColors.Add(FLinearColor::LerpUsingHSV(
				FLinearColor(0.0f, 0.7f, 1.0f),
				FLinearColor(1.0f, 0.3f, 0.8f), ColorT));
		}
	}

	int32 RingVerts = Sides + 1;
	for (int32 Seg = 0; Seg < NumSegments - 1; ++Seg)
	{
		for (int32 S = 0; S < Sides; ++S)
		{
			int32 Current = Seg * RingVerts + S;
			int32 Next = Current + RingVerts;

			OutTris.Add(Current);
			OutTris.Add(Next);
			OutTris.Add(Current + 1);

			OutTris.Add(Current + 1);
			OutTris.Add(Next);
			OutTris.Add(Next + 1);
		}
	}
}
