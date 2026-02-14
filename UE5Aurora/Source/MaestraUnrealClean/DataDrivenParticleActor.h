#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "MaestraClient.h"
#include "MaestraEntity.h"
#include "DataDrivenParticleActor.generated.h"

UENUM(BlueprintType)
enum class EDataSourceType : uint8
{
	Manual,
	Maestra,
	UDP,
	Simulated
};

USTRUCT(BlueprintType)
struct MAESTRAUNREALCLEAN_API FDataParameterMapping
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataParticle")
	FString DataKey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataParticle")
	FName NiagaraParameterName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataParticle")
	float InputMin = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataParticle")
	float InputMax = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataParticle")
	float OutputMin = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataParticle")
	float OutputMax = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataParticle")
	float SmoothingFactor = 0.1f;

	// Runtime
	float CurrentSmoothedValue = 0.0f;
};

UCLASS(BlueprintType, Blueprintable)
class MAESTRAUNREALCLEAN_API ADataDrivenParticleActor : public AActor
{
	GENERATED_BODY()

public:
	ADataDrivenParticleActor();

	// --- Data source ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataParticle|Source")
	EDataSourceType DataSource = EDataSourceType::Simulated;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataParticle|Maestra")
	FString MaestraEntitySlug;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataParticle|Maestra")
	FString MaestraApiUrl = TEXT("http://localhost:8080");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataParticle|Maestra")
	float MaestraPollInterval = 1.0f;

	// --- Mappings ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataParticle|Mapping")
	TArray<FDataParameterMapping> ParameterMappings;

	// --- Niagara ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DataParticle")
	UNiagaraComponent* ParticleComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DataParticle")
	UNiagaraSystem* ParticleSystem;

	// --- Blueprint API ---
	UFUNCTION(BlueprintCallable, Category = "DataParticle")
	void SetDataValue(const FString& Key, float Value);

	UFUNCTION(BlueprintCallable, Category = "DataParticle")
	void OnJsonDataReceived(const FString& JsonString);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	TMap<FString, float> CurrentData;

	UPROPERTY()
	UMaestraClient* MaestraClientInstance;

	UPROPERTY()
	UMaestraEntity* TrackedEntity;

	float MaestraPollTimer = 0.0f;

	void InitializeMaestraConnection();
	void UpdateNiagaraParameters();
	float RemapValue(float Value, const FDataParameterMapping& Mapping) const;
	void GenerateSimulatedData();

	UFUNCTION()
	void OnMaestraEntityReceived(const FString& Slug, UMaestraEntity* Entity);
};
