#include "DataDrivenParticleActor.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

ADataDrivenParticleActor::ADataDrivenParticleActor()
{
	PrimaryActorTick.bCanEverTick = true;

	ParticleComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ParticleComponent"));
	SetRootComponent(ParticleComponent);
	ParticleComponent->bAutoActivate = false;
}

void ADataDrivenParticleActor::BeginPlay()
{
	Super::BeginPlay();

	if (ParticleSystem && ParticleComponent)
	{
		ParticleComponent->SetAsset(ParticleSystem);
		ParticleComponent->Activate();
	}

	switch (DataSource)
	{
	case EDataSourceType::Maestra:
		InitializeMaestraConnection();
		break;
	case EDataSourceType::Simulated:
		break;
	default:
		break;
	}
}

void ADataDrivenParticleActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (DataSource == EDataSourceType::Simulated)
	{
		GenerateSimulatedData();
	}

	if (DataSource == EDataSourceType::Maestra && TrackedEntity)
	{
		for (FDataParameterMapping& Mapping : ParameterMappings)
		{
			float Value = TrackedEntity->GetStateFloat(*Mapping.DataKey, 0.0f);
			CurrentData.Add(Mapping.DataKey, Value);
		}

		MaestraPollTimer += DeltaTime;
		if (MaestraPollTimer >= MaestraPollInterval)
		{
			MaestraPollTimer = 0.0f;
			if (MaestraClientInstance)
			{
				MaestraClientInstance->GetEntityBySlug(MaestraEntitySlug);
			}
		}
	}

	UpdateNiagaraParameters();
}

void ADataDrivenParticleActor::SetDataValue(const FString& Key, float Value)
{
	CurrentData.Add(Key, Value);
}

void ADataDrivenParticleActor::OnJsonDataReceived(const FString& JsonString)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return;
	}

	for (const FDataParameterMapping& Mapping : ParameterMappings)
	{
		double Value = 0.0;
		if (JsonObject->TryGetNumberField(Mapping.DataKey, Value))
		{
			CurrentData.Add(Mapping.DataKey, static_cast<float>(Value));
		}
	}
}

void ADataDrivenParticleActor::InitializeMaestraConnection()
{
	MaestraClientInstance = NewObject<UMaestraClient>(this);
	MaestraClientInstance->Initialize(MaestraApiUrl);
	MaestraClientInstance->OnEntityReceived.AddDynamic(this, &ADataDrivenParticleActor::OnMaestraEntityReceived);
	MaestraClientInstance->GetEntityBySlug(MaestraEntitySlug);
}

void ADataDrivenParticleActor::OnMaestraEntityReceived(const FString& Slug, UMaestraEntity* Entity)
{
	if (Slug == MaestraEntitySlug)
	{
		TrackedEntity = Entity;
	}
}

void ADataDrivenParticleActor::UpdateNiagaraParameters()
{
	if (!ParticleComponent)
	{
		return;
	}

	for (FDataParameterMapping& Mapping : ParameterMappings)
	{
		float* RawValue = CurrentData.Find(Mapping.DataKey);
		if (!RawValue)
		{
			continue;
		}

		float Remapped = RemapValue(*RawValue, Mapping);
		Mapping.CurrentSmoothedValue = FMath::Lerp(Mapping.CurrentSmoothedValue, Remapped, Mapping.SmoothingFactor);
		ParticleComponent->SetVariableFloat(Mapping.NiagaraParameterName, Mapping.CurrentSmoothedValue);
	}
}

float ADataDrivenParticleActor::RemapValue(float Value, const FDataParameterMapping& Mapping) const
{
	float InputRange = Mapping.InputMax - Mapping.InputMin;
	if (FMath::IsNearlyZero(InputRange))
	{
		return Mapping.OutputMin;
	}

	float Normalized = FMath::Clamp((Value - Mapping.InputMin) / InputRange, 0.0f, 1.0f);
	return Mapping.OutputMin + Normalized * (Mapping.OutputMax - Mapping.OutputMin);
}

void ADataDrivenParticleActor::GenerateSimulatedData()
{
	float Time = GetGameTimeSinceCreation();

	for (const FDataParameterMapping& Mapping : ParameterMappings)
	{
		uint32 Hash = GetTypeHash(Mapping.DataKey);
		float Phase = static_cast<float>(Hash % 1000) * 0.001f * 2.0f * PI;
		float Freq = 0.5f + static_cast<float>(Hash % 500) * 0.001f;

		float Value = Mapping.InputMin + (Mapping.InputMax - Mapping.InputMin) *
			(FMath::Sin(Time * Freq + Phase) * 0.5f + 0.5f);

		CurrentData.Add(Mapping.DataKey, Value);
	}
}
