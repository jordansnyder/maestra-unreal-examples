#pragma once

#include "CoreMinimal.h"
#include "GenerativeArtActor.h"
#include "AuroraDataMixer.h"
#include "ProceduralMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Components/PostProcessComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "AuroraBorealisActor.generated.h"

/**
 * A single curtain band configuration.
 */
USTRUCT(BlueprintType)
struct MAESTRAUNREALCLEAN_API FAuroraFieldLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|FieldLine")
	float LShell = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|FieldLine")
	float ThetaMin = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|FieldLine")
	float ThetaMax = 2.84f;

	// Depth offset to layer multiple curtain bands
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|FieldLine")
	float DepthOffset = 0.0f;

	// Per-line phase offset for fold animation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|FieldLine")
	float PhaseOffset = 0.0f;

	// Per-line additive hue shift
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|FieldLine")
	float HueOffset = 0.0f;

	// Per-line intensity multiplier (0-1)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|FieldLine")
	float IntensityScale = 1.0f;
};

/**
 * AuroraBorealisActor — a data-driven aurora borealis visualization.
 *
 * Renders aurora curtains as procedural mesh ribbons with:
 *   - Vertical ray structure (shimmer along height via Perlin noise)
 *   - Multiple overlapping translucent curtain layers
 *   - Brightness concentrated at the lower edge, fading upward
 *   - Color gradient from green (bottom) to purple/red (top)
 *   - Traveling fold waves and organic drift
 *   - Data-driven parameters from UAuroraDataMixer
 */
UCLASS(BlueprintType, Blueprintable)
class MAESTRAUNREALCLEAN_API AAuroraBorealisActor : public AGenerativeArtActor
{
	GENERATED_BODY()

public:
	AAuroraBorealisActor();

	// =======================================================================
	// Components
	// =======================================================================

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aurora")
	UAuroraDataMixer* DataMixer;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aurora|Rendering")
	UProceduralMeshComponent* CurtainMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aurora|Rendering")
	UNiagaraComponent* HighlightParticles;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aurora|Rendering")
	UPostProcessComponent* AtmosphericGlow;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aurora|Rendering")
	UExponentialHeightFogComponent* AuroraFog;

	// =======================================================================
	// Asset References
	// =======================================================================

	/** Translucent emissive material — should use Additive blend, Unlit, Two-Sided.
	 *  Must read VertexColor for RGB emissive and Alpha for opacity.
	 *  If left empty, a basic runtime fallback is used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Assets")
	UMaterialInterface* CurtainMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Assets")
	UNiagaraSystem* HighlightNiagaraSystem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Assets")
	UMaterialInterface* PostProcessMaterial;

	// =======================================================================
	// Curtain Band Configuration
	// =======================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Field")
	TArray<FAuroraFieldLine> FieldLines;

	/** Samples along each curtain spine. Higher = smoother horizontal detail. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Field", meta=(ClampMin="16", ClampMax="512"))
	int32 FieldLineSamples = 256;

	/** Number of auto-generated curtain bands if FieldLines is empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Field", meta=(ClampMin="1", ClampMax="30"))
	int32 AutoFieldLineCount = 18;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Field")
	float BaseLShell = 2000.0f;

	/** Depth spacing between curtain layers (Y axis). Larger = more spread out in depth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Field")
	float CurtainDepthSpacing = 350.0f;

	// =======================================================================
	// Curtain Shape
	// =======================================================================

	/** Total horizontal span of each curtain band. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Curtain")
	float CurtainSpan = 80000.0f;

	/** Altitude of the curtain's lower (brightest) edge above the actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Curtain")
	float CurtainAltitude = 6000.0f;

	/** Gentle arc curvature — center is this much higher than edges. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Curtain")
	float CurtainArcHeight = 2000.0f;

	/** Base amplitude of fold ripple. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Curtain")
	float BaseFoldAmplitude = 1200.0f;

	// =======================================================================
	// Curtain Mesh / Vertical Ray Parameters
	// =======================================================================

	/** Vertical subdivisions of each curtain. More = smoother vertical gradient. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Mesh", meta=(ClampMin="4", ClampMax="128"))
	int32 CurtainVerticalSegments = 64;

	/** Total height of the curtain drape (world units). Auroras are TALL. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Mesh")
	float CurtainHeight = 14000.0f;

	/** Speed of vertical shimmer (the fast brightness pulse along rays). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Mesh")
	float RayShimmerSpeed = 2.5f;

	/** Frequency of vertical ray structure (higher = more/thinner rays). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Mesh")
	float RayFrequency = 18.0f;

	/** Contrast of individual rays (0 = uniform, 1 = sharp rays). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Mesh", meta=(ClampMin="0.0", ClampMax="1.0"))
	float RayContrast = 0.7f;

	// =======================================================================
	// RF Spectrum Collision
	// =======================================================================

	/** How strongly RF spectrum data modulates curtain brightness spatially (0=off, 1=full). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|RF Collision", meta=(ClampMin="0.0", ClampMax="1.0"))
	float RFCollisionStrength = 0.7f;

	/** Minimum brightness floor so dim RF regions don't completely vanish. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|RF Collision", meta=(ClampMin="0.0", ClampMax="1.0"))
	float RFMinBrightness = 0.1f;

	// =======================================================================
	// Rendering Parameters
	// =======================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Rendering")
	float BaseEmissive = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Rendering")
	float MaxEmissive = 8.0f;

	// =======================================================================
	// Atmospheric Glow
	// =======================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Atmosphere")
	float FogBaseOpacity = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Atmosphere")
	float FogMaxOpacity = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Atmosphere")
	float FogHeightFalloff = 0.005f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Atmosphere", meta=(ClampMin="0.0", ClampMax="10.0"))
	float GlowLagSeconds = 3.0f;

	// =======================================================================
	// Memory / Trailing Echo
	// =======================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Memory", meta=(ClampMin="0.0", ClampMax="30.0"))
	float EchoDecayTime = 8.0f;

	// =======================================================================
	// Debug
	// =======================================================================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Debug")
	bool bDrawFieldLines = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aurora|Debug")
	bool bShowDebugHUD = false;

	// =======================================================================
	// Blueprint API
	// =======================================================================

	UFUNCTION(BlueprintCallable, Category = "Aurora")
	FVector SampleFieldLinePoint(int32 LineIndex, float T) const;

	UFUNCTION(BlueprintCallable, Category = "Aurora")
	TArray<FVector> GetCurtainPoints(int32 LineIndex) const;

protected:
	virtual void BeginPlay() override;
	virtual void UpdateGenerativeArt(float DeltaTime) override;

private:
	// --- Cached spine geometry ---
	TArray<TArray<FVector>> CachedCurtainPoints;

	// --- Mesh buffers per field line ---
	struct FCurtainMeshData
	{
		TArray<FVector> Vertices;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FLinearColor> Colors;
		TArray<int32> Triangles;
		bool bInitialized = false;
	};
	TArray<FCurtainMeshData> CurtainMeshSections;

	float LaggedIntensity = 0.0f;
	float EchoIntensity = 0.0f;
	float AccumulatedWavePhase = 0.0f;

	UPROPERTY()
	UMaterialInstanceDynamic* CurtainMID;

	UPROPERTY()
	UMaterialInstanceDynamic* PostProcessMID;

	// --- Methods ---
	void AutoGenerateFieldLines();
	void ComputeAllCurtainGeometry(const FAuroraParameters& Params);
	FVector ComputeSpinePoint(const FAuroraFieldLine& Line, float T,
		const FAuroraParameters& Params) const;

	void InitializeCurtainMesh();
	void UpdateCurtainMesh(const FAuroraParameters& Params);
	void PushHighlightParameters(const FAuroraParameters& Params);
	void UpdateAtmosphericGlow(const FAuroraParameters& Params, float DeltaTime);
	void UpdateEcho(const FAuroraParameters& Params, float DeltaTime);

	void DrawDebugFieldLines() const;
	void DrawDebugHUD(const FAuroraParameters& Params) const;

	static FLinearColor AuroraHSVToColor(float H, float S, float V);
	static FLinearColor EmissionColorFromAltitude(float NormalizedAltitude);
};
