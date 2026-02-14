#pragma once

#include "CoreMinimal.h"
#include "GenerativeArtActor.h"
#include "ProceduralMeshComponent.h"
#include "ParametricMeshActor.generated.h"

UENUM(BlueprintType)
enum class EParametricShape : uint8
{
	Superformula,
	TorusKnot,
	LorenzAttractor,
	RosslerAttractor
};

UCLASS(BlueprintType, Blueprintable)
class MAESTRAUNREALCLEAN_API AParametricMeshActor : public AGenerativeArtActor
{
	GENERATED_BODY()

public:
	AParametricMeshActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parametric|Shape")
	EParametricShape ShapeType = EParametricShape::TorusKnot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parametric|Superformula")
	float SF_A = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parametric|Superformula")
	float SF_B = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parametric|Superformula")
	float SF_M1 = 6.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parametric|Superformula")
	float SF_N1_1 = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parametric|Superformula")
	float SF_N2_1 = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parametric|Superformula")
	float SF_N3_1 = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parametric|Superformula")
	float SF_M2 = 6.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parametric|Superformula")
	float SF_N1_2 = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parametric|Superformula")
	float SF_N2_2 = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parametric|Superformula")
	float SF_N3_2 = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parametric|TorusKnot")
	int32 TK_P = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parametric|TorusKnot")
	int32 TK_Q = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parametric|TorusKnot")
	float TK_MajorRadius = 200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parametric|TorusKnot")
	float TK_MinorRadius = 50.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parametric|TorusKnot")
	float TK_TubeRadius = 10.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parametric|TorusKnot")
	int32 TK_TubeSides = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parametric|Attractor")
	float Attractor_Sigma = 10.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parametric|Attractor")
	float Attractor_Rho = 28.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parametric|Attractor")
	float Attractor_Beta = 2.667f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parametric|Attractor")
	int32 Attractor_Iterations = 10000;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parametric|Attractor")
	float Attractor_StepSize = 0.01f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parametric|Attractor")
	float Attractor_Scale = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parametric|Attractor")
	float Attractor_TubeRadius = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parametric|Attractor")
	int32 Attractor_TubeSides = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parametric|Display")
	float MeshScale = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parametric|Display")
	bool bAnimateParameters = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parametric|Display")
	float AnimationSpeed = 0.2f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Parametric")
	UProceduralMeshComponent* MeshComponent;

	UFUNCTION(BlueprintCallable, Category = "Parametric")
	void RegenerateMesh();

protected:
	virtual void BeginPlay() override;
	virtual void UpdateGenerativeArt(float DeltaTime) override;

private:
	void GenerateSuperformula(TArray<FVector>& OutVerts, TArray<int32>& OutTris,
		TArray<FVector>& OutNormals, TArray<FVector2D>& OutUVs, TArray<FLinearColor>& OutColors);
	void GenerateTorusKnot(TArray<FVector>& OutVerts, TArray<int32>& OutTris,
		TArray<FVector>& OutNormals, TArray<FVector2D>& OutUVs, TArray<FLinearColor>& OutColors);
	void GenerateAttractor(TArray<FVector>& OutVerts, TArray<int32>& OutTris,
		TArray<FVector>& OutNormals, TArray<FVector2D>& OutUVs, TArray<FLinearColor>& OutColors, bool bLorenz);

	float EvaluateSuperformula(float Angle, float A, float B, float M,
		float N1, float N2, float N3) const;
	FVector LorenzDerivative(FVector State) const;
	FVector RosslerDerivative(FVector State) const;

	void GenerateTubeAlongCurve(const TArray<FVector>& CurvePoints, float Radius, int32 Sides,
		TArray<FVector>& OutVerts, TArray<int32>& OutTris,
		TArray<FVector>& OutNormals, TArray<FVector2D>& OutUVs, TArray<FLinearColor>& OutColors);
};
