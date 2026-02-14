#include "PostProcessDataViz.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

APostProcessDataViz::APostProcessDataViz()
{
	PrimaryActorTick.bCanEverTick = true;

	PostProcessComponent = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcessComponent"));
	SetRootComponent(PostProcessComponent);
	PostProcessComponent->bUnbound = true;
}

void APostProcessDataViz::BeginPlay()
{
	Super::BeginPlay();
	InitializePostProcess();

	if (DataSource == EDataSourceType::Maestra && !MaestraEntitySlug.IsEmpty())
	{
		MaestraClientInstance = NewObject<UMaestraClient>(this);
		MaestraClientInstance->Initialize(MaestraApiUrl);
		MaestraClientInstance->OnEntityReceived.AddDynamic(this, &APostProcessDataViz::OnMaestraEntityReceived);
		MaestraClientInstance->GetEntityBySlug(MaestraEntitySlug);
	}
}

void APostProcessDataViz::InitializePostProcess()
{
	if (!PostProcessMaterial)
	{
		return;
	}

	PostProcessMID = UMaterialInstanceDynamic::Create(PostProcessMaterial, this);

	if (PostProcessMID)
	{
		FWeightedBlendable Blendable;
		Blendable.Weight = 1.0f;
		Blendable.Object = PostProcessMID;
		PostProcessComponent->Settings.WeightedBlendables.Array.Add(Blendable);
	}
}

void APostProcessDataViz::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (DataSource == EDataSourceType::Simulated)
	{
		SignalStrength = SimulateSignalStrength();

		DistortionAmount = SignalStrength * 0.3f;
		ChromaticAberration = SignalStrength * 0.5f;
		VignetteIntensity = 0.2f + SignalStrength * 0.4f;
		ScanlineIntensity = SignalStrength * 0.6f;
		GlitchIntensity = FMath::Max(0.0f, SignalStrength - 0.7f) * 3.0f;

		float Hue = SignalStrength * 0.3f;
		DataTintColor = FLinearColor(
			0.8f + Hue * 0.2f,
			1.0f - SignalStrength * 0.3f,
			0.7f + FMath::Sin(GetGameTimeSinceCreation() * 2.0f) * 0.3f,
			1.0f);
	}

	UpdateMaterialParameters();
}

void APostProcessDataViz::SetSignalStrength(float NewStrength)
{
	SignalStrength = FMath::Clamp(NewStrength, 0.0f, 1.0f);
}

void APostProcessDataViz::OnJsonDataReceived(const FString& JsonString)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return;
	}

	double Value = 0.0;
	if (JsonObject->TryGetNumberField(DataKeyForStrength, Value))
	{
		SignalStrength = FMath::Clamp(static_cast<float>(Value), 0.0f, 1.0f);
	}
}

void APostProcessDataViz::OnMaestraEntityReceived(const FString& Slug, UMaestraEntity* Entity)
{
	if (Entity && Slug == MaestraEntitySlug)
	{
		SignalStrength = FMath::Clamp(Entity->GetStateFloat(*DataKeyForStrength, 0.0f), 0.0f, 1.0f);
	}
}

void APostProcessDataViz::UpdateMaterialParameters()
{
	if (!PostProcessMID)
	{
		return;
	}

	PostProcessMID->SetScalarParameterValue(TEXT("SignalStrength"), SignalStrength);
	PostProcessMID->SetScalarParameterValue(TEXT("DistortionAmount"), DistortionAmount);
	PostProcessMID->SetScalarParameterValue(TEXT("ChromaticAberration"), ChromaticAberration);
	PostProcessMID->SetScalarParameterValue(TEXT("VignetteIntensity"), VignetteIntensity);
	PostProcessMID->SetScalarParameterValue(TEXT("ScanlineIntensity"), ScanlineIntensity);
	PostProcessMID->SetScalarParameterValue(TEXT("GlitchIntensity"), GlitchIntensity);
	PostProcessMID->SetScalarParameterValue(TEXT("Time"), GetGameTimeSinceCreation());
	PostProcessMID->SetVectorParameterValue(TEXT("DataTintColor"), DataTintColor);
}

float APostProcessDataViz::SimulateSignalStrength() const
{
	float Time = GetGameTimeSinceCreation();

	float Slow = FMath::Sin(Time * 0.1f) * 0.3f + 0.5f;
	float Medium = FMath::Sin(Time * 0.7f + 1.3f) * 0.15f;
	float Fast = FMath::Sin(Time * 4.3f) * FMath::Sin(Time * 5.7f) * 0.1f;

	return FMath::Clamp(Slow + Medium + Fast, 0.0f, 1.0f);
}
