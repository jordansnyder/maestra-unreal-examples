#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MaestraClient.h"
#include "MaestraEntity.h"
#include "DataDrivenParticleActor.h"
#include "PostProcessDataViz.generated.h"

UCLASS(BlueprintType, Blueprintable)
class MAESTRAUNREALCLEAN_API APostProcessDataViz : public AActor
{
	GENERATED_BODY()

public:
	APostProcessDataViz();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PostProcessViz")
	UPostProcessComponent* PostProcessComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcessViz|Material")
	UMaterialInterface* PostProcessMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcessViz|Data", meta=(ClampMin="0.0", ClampMax="1.0"))
	float SignalStrength = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcessViz|Data", meta=(ClampMin="0.0", ClampMax="1.0"))
	float DistortionAmount = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcessViz|Data")
	FLinearColor DataTintColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcessViz|Data", meta=(ClampMin="0.0", ClampMax="1.0"))
	float ChromaticAberration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcessViz|Data", meta=(ClampMin="0.0", ClampMax="1.0"))
	float VignetteIntensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcessViz|Data", meta=(ClampMin="0.0", ClampMax="1.0"))
	float ScanlineIntensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcessViz|Data", meta=(ClampMin="0.0", ClampMax="1.0"))
	float GlitchIntensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcessViz|Source")
	EDataSourceType DataSource = EDataSourceType::Simulated;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcessViz|Source")
	FString MaestraEntitySlug;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcessViz|Source")
	FString MaestraApiUrl = TEXT("http://localhost:8080");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PostProcessViz|Source")
	FString DataKeyForStrength = TEXT("signal_strength");

	UFUNCTION(BlueprintCallable, Category = "PostProcessViz")
	void SetSignalStrength(float NewStrength);

	UFUNCTION(BlueprintCallable, Category = "PostProcessViz")
	void OnJsonDataReceived(const FString& JsonString);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	UPROPERTY()
	UMaterialInstanceDynamic* PostProcessMID;

	UPROPERTY()
	UMaestraClient* MaestraClientInstance;

	void InitializePostProcess();
	void UpdateMaterialParameters();
	float SimulateSignalStrength() const;

	UFUNCTION()
	void OnMaestraEntityReceived(const FString& Slug, UMaestraEntity* Entity);
};
