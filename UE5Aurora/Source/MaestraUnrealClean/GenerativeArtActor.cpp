#include "GenerativeArtActor.h"

AGenerativeArtActor::AGenerativeArtActor()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AGenerativeArtActor::BeginPlay()
{
	Super::BeginPlay();

	if (bRandomizeSeedOnBeginPlay)
	{
		RandomSeed = FMath::Rand();
	}
	RNG.Initialize(RandomSeed);
}

void AGenerativeArtActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bPaused)
	{
		float ScaledDelta = DeltaTime * TimeScale;
		ElapsedTime += ScaledDelta;
		TickParameterAnimations(ScaledDelta);
		UpdateGenerativeArt(ScaledDelta);
	}
}

void AGenerativeArtActor::UpdateGenerativeArt(float DeltaTime)
{
	// Override in subclasses
}

void AGenerativeArtActor::ResetTime()
{
	ElapsedTime = 0.0f;
}

void AGenerativeArtActor::SetSeed(int32 NewSeed)
{
	RandomSeed = NewSeed;
	RNG.Initialize(NewSeed);
}

void AGenerativeArtActor::AnimateParameter(FName ParameterName, float TargetValue,
	float Duration, EParameterBlendMode BlendMode)
{
	FAnimatedParam& Param = AnimatedParameters.FindOrAdd(ParameterName);
	Param.StartValue = Param.CurrentValue;
	Param.TargetValue = TargetValue;
	Param.Duration = (Duration < 0.0f) ? DefaultBlendDuration : Duration;
	Param.Elapsed = 0.0f;
	Param.BlendMode = (BlendMode == EParameterBlendMode::EaseInOut && Duration < 0.0f)
		? DefaultBlendMode : BlendMode;
}

float AGenerativeArtActor::GetAnimatedParameter(FName ParameterName, float DefaultValue) const
{
	const FAnimatedParam* Param = AnimatedParameters.Find(ParameterName);
	if (Param)
	{
		return Param->CurrentValue;
	}
	return DefaultValue;
}

void AGenerativeArtActor::TickParameterAnimations(float DeltaTime)
{
	for (auto& Pair : AnimatedParameters)
	{
		FAnimatedParam& Param = Pair.Value;

		if (Param.BlendMode == EParameterBlendMode::Instant)
		{
			Param.CurrentValue = Param.TargetValue;
			continue;
		}

		Param.Elapsed += DeltaTime;
		float T = (Param.Duration > 0.0f)
			? FMath::Clamp(Param.Elapsed / Param.Duration, 0.0f, 1.0f)
			: 1.0f;

		float BlendedT = ApplyBlend(T, Param.BlendMode);
		Param.CurrentValue = FMath::Lerp(Param.StartValue, Param.TargetValue, BlendedT);
	}
}

float AGenerativeArtActor::ApplyBlend(float T, EParameterBlendMode Mode)
{
	switch (Mode)
	{
	case EParameterBlendMode::Linear:
		return T;
	case EParameterBlendMode::EaseInOut:
		return T * T * (3.0f - 2.0f * T);
	case EParameterBlendMode::Spring:
	{
		float Decay = FMath::Exp(-4.0f * T);
		return 1.0f - Decay * FMath::Cos(T * PI * 3.0f);
	}
	default:
		return T;
	}
}
