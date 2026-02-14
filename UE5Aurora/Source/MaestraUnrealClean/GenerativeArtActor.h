#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GenerativeArtActor.generated.h"

UENUM(BlueprintType)
enum class EParameterBlendMode : uint8
{
	Instant,
	Linear,
	EaseInOut,
	Spring
};

UCLASS(Abstract, BlueprintType, Blueprintable)
class MAESTRAUNREALCLEAN_API AGenerativeArtActor : public AActor
{
	GENERATED_BODY()

public:
	AGenerativeArtActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generative|Time")
	float TimeScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generative|Time")
	bool bPaused = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generative|Seed")
	int32 RandomSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generative|Seed")
	bool bRandomizeSeedOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generative|Quality")
	int32 Resolution = 128;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generative|Animation")
	EParameterBlendMode DefaultBlendMode = EParameterBlendMode::EaseInOut;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Generative|Animation")
	float DefaultBlendDuration = 1.0f;

	UFUNCTION(BlueprintCallable, Category = "Generative")
	float GetElapsedTime() const { return ElapsedTime; }

	UFUNCTION(BlueprintCallable, Category = "Generative")
	void ResetTime();

	UFUNCTION(BlueprintCallable, Category = "Generative")
	void SetSeed(int32 NewSeed);

	UFUNCTION(BlueprintCallable, Category = "Generative")
	void AnimateParameter(FName ParameterName, float TargetValue,
		float Duration = -1.0f,
		EParameterBlendMode BlendMode = EParameterBlendMode::EaseInOut);

	UFUNCTION(BlueprintCallable, Category = "Generative")
	float GetAnimatedParameter(FName ParameterName, float DefaultValue = 0.0f) const;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void UpdateGenerativeArt(float DeltaTime);

	float ElapsedTime = 0.0f;
	FRandomStream RNG;

private:
	struct FAnimatedParam
	{
		float CurrentValue = 0.0f;
		float StartValue = 0.0f;
		float TargetValue = 0.0f;
		float Duration = 1.0f;
		float Elapsed = 0.0f;
		EParameterBlendMode BlendMode = EParameterBlendMode::EaseInOut;
	};

	TMap<FName, FAnimatedParam> AnimatedParameters;
	void TickParameterAnimations(float DeltaTime);
	static float ApplyBlend(float T, EParameterBlendMode Mode);
};
