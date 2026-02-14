#include "FlowFieldActor.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"

AFlowFieldActor::AFlowFieldActor()
{
	ParticleComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ParticleComponent"));
	SetRootComponent(ParticleComponent);
	ParticleComponent->bAutoActivate = false;
}

// ============================================================================
// Lifecycle
// ============================================================================

void AFlowFieldActor::BeginPlay()
{
	Super::BeginPlay();

	FieldData.SetNum(FieldGridSize.X * FieldGridSize.Y * FieldGridSize.Z);
	RegenerateField();

	// Sync asset reference: if user assigned the system on the component directly, pick it up
	if (!ParticleSystemAsset && ParticleComponent && ParticleComponent->GetAsset())
	{
		ParticleSystemAsset = ParticleComponent->GetAsset();
		UE_LOG(LogTemp, Log, TEXT("FlowFieldActor: Picked up Niagara asset from component: %s"), *ParticleSystemAsset->GetName());
	}

	UE_LOG(LogTemp, Log, TEXT("FlowFieldActor BeginPlay: ParticleSystemAsset=%s, ParticleComponent=%s"),
		ParticleSystemAsset ? *ParticleSystemAsset->GetName() : TEXT("NULL"),
		ParticleComponent ? TEXT("Valid") : TEXT("NULL"));

	if (ParticleSystemAsset && ParticleComponent)
	{
		ParticleComponent->SetAsset(ParticleSystemAsset);
		ParticleComponent->Activate(true);
		UE_LOG(LogTemp, Log, TEXT("FlowFieldActor: Niagara activated. IsActive=%s"),
			ParticleComponent->IsActive() ? TEXT("YES") : TEXT("NO"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("FlowFieldActor: Niagara NOT activated - asset or component is null. Assign a Niagara System in Details panel."));
	}

	// Initialize Maestra connection if data source is set to Maestra
	if (DataSource == EDataSourceType::Maestra && !MaestraEntitySlug.IsEmpty())
	{
		InitializeMaestraConnection();
	}

	// Run initial mapping so Niagara gets valid params on first frame
	MapIntensityToVisualParams();
	PushAllNiagaraParameters();
}

void AFlowFieldActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (MaestraClientInstance)
	{
		MaestraClientInstance->DisconnectWebSocket();
	}

	Super::EndPlay(EndPlayReason);
}

// ============================================================================
// Main Tick
// ============================================================================

void AFlowFieldActor::UpdateGenerativeArt(float DeltaTime)
{
	// 1. Get raw input value based on data source
	float RawInput = MasterIntensity;

	if (DataSource == EDataSourceType::Maestra)
	{
		ReadMaestraData();
		RawInput = MasterIntensity;
	}
	else if (DataSource == EDataSourceType::Simulated)
	{
		RawInput = GenerateSimulatedPotentiometer();
	}
	// Manual and UDP: just use MasterIntensity as-is

	// 2. Smooth input
	SmoothedMasterIntensity = FMath::Lerp(SmoothedMasterIntensity, RawInput, InputSmoothingFactor);

	// 3. Map intensity to all visual + noise params
	MapIntensityToVisualParams();

	// 4. Regenerate CPU field (for debug arrows and non-Niagara consumers)
	RegenerateField();

	// 5. Push all params to Niagara
	PushAllNiagaraParameters();

	// 6. Debug visualization
	if (bDrawDebugArrows)
	{
		DrawDebugVisualization();
	}

	// 7. On-screen debug HUD
	if (bShowDebugHUD)
	{
		DrawDebugHUD();
	}
}

// ============================================================================
// Maestra Integration
// ============================================================================

void AFlowFieldActor::InitializeMaestraConnection()
{
	MaestraClientInstance = NewObject<UMaestraClient>(this);
	MaestraClientInstance->OnEntityReceived.AddDynamic(this, &AFlowFieldActor::OnMaestraEntityReceived);

	// Subscribe to this entity for WebSocket filtering
	MaestraClientInstance->SubscribeToEntity(MaestraEntitySlug);

	// Initialize with both REST + WebSocket
	MaestraClientInstance->InitializeWithWebSocket(MaestraApiUrl, MaestraWebSocketUrl);

	// Do an initial REST fetch to get current state immediately
	MaestraClientInstance->GetEntityBySlug(MaestraEntitySlug);

	UE_LOG(LogTemp, Log, TEXT("FlowFieldActor: Connected to Maestra entity '%s' via WebSocket"), *MaestraEntitySlug);
}

void AFlowFieldActor::OnMaestraEntityReceived(const FString& Slug, UMaestraEntity* Entity)
{
	if (Slug == MaestraEntitySlug)
	{
		TrackedEntity = Entity;
	}
}

void AFlowFieldActor::ReadMaestraData()
{
	if (!TrackedEntity)
	{
		return;
	}

	// Read the potentiometer value from entity state (WebSocket keeps this up-to-date in real-time)
	MasterIntensity = FMath::Clamp(
		TrackedEntity->GetStateFloat(PotentiometerDataKey, 0.5f),
		0.0f, 1.0f
	);
}

float AFlowFieldActor::GenerateSimulatedPotentiometer() const
{
	float Time = ElapsedTime;

	// Aurora: almost still, with very slow tidal breathing
	float Base = 0.15f;
	float Tide = FMath::Max(0.0f, FMath::Sin(Time * 0.012f)) * 0.2f;  // ~80s cycle, only positive half
	float Drift = FMath::Sin(Time * 0.005f + 1.7f) * 0.08f;            // ~200s glacial wander

	return FMath::Clamp(Base + Tide + Drift, 0.05f, 0.5f);
}

// ============================================================================
// Parameter Mapping
// ============================================================================

void AFlowFieldActor::MapIntensityToVisualParams()
{
	float T = SmoothedMasterIntensity;

	// Smooth cubic for gentle transitions
	float TSoft = T * T * (3.0f - 2.0f * T);

	// === AURORA VIA SOFT OVERLAPPING SPRITES ===
	// Many large, soft, transparent sprites that overlap additively = aurora glow

	// Initial velocity: very slow drift
	WindStrength = FMath::Lerp(3.0f, 12.0f, TSoft);
	DragAmount = FMath::Lerp(0.5f, 1.5f, TSoft);
	NoiseForceStrength = 0.0f;

	// CPU field (debug arrows only)
	NoiseScale = 0.0003f;
	NoiseSpeed = 0.01f;
	FieldStrength = 3.0f;

	// Large, long-lived sprites
	SpawnRate = FMath::Lerp(30.0f, 80.0f, TSoft);
	ParticleLifetime = FMath::Lerp(15.0f, 8.0f, TSoft);
	ParticleSize = FMath::Lerp(200.0f, 400.0f, TSoft);       // Used as fallback uniform size

	// Elongated sprites — wide along velocity, short perpendicular = aurora streaks
	SpriteSizeX = FMath::Lerp(400.0f, 800.0f, TSoft);        // Wide (along velocity)
	SpriteSizeY = FMath::Lerp(60.0f, 120.0f, TSoft);         // Short (perpendicular)

	// Not using ribbons
	RibbonWidth = 0.0f;

	// Aurora color: cycle through the palette on its own slow timer (independent of intensity)
	// This ensures color is always visibly changing even when intensity is low
	float ColorTime = ElapsedTime;
	float ColorCycle = FMath::Sin(ColorTime * 0.025f) * 0.5f + 0.5f;  // 0→1→0 over ~40 seconds
	ColorHue = FMath::Lerp(0.28f, 0.80f, ColorCycle);                  // Green → cyan → blue → violet → back
	ColorSaturation = FMath::Lerp(0.5f, 0.95f, ColorCycle);

	// Emissive: brighter base so colors are visible, intensity adds bloom
	EmissiveStrength = FMath::Lerp(0.8f, 2.5f, T);

	TurbulenceIntensity = 0.0f;
	TrailLength = 0.0f;
}

// HSV to RGB conversion (H in 0-1 range, S in 0-1, V in 0-1)
static FLinearColor HSVToLinearColor(float H, float S, float V)
{
	float C = V * S;
	float Hp = FMath::Fmod(H * 6.0f, 6.0f);
	float X = C * (1.0f - FMath::Abs(FMath::Fmod(Hp, 2.0f) - 1.0f));
	float M = V - C;

	float R = 0, G = 0, B = 0;
	if (Hp < 1.0f)      { R = C; G = X; B = 0; }
	else if (Hp < 2.0f) { R = X; G = C; B = 0; }
	else if (Hp < 3.0f) { R = 0; G = C; B = X; }
	else if (Hp < 4.0f) { R = 0; G = X; B = C; }
	else if (Hp < 5.0f) { R = X; G = 0; B = C; }
	else                 { R = C; G = 0; B = X; }

	return FLinearColor(R + M, G + M, B + M, 1.0f);
}

void AFlowFieldActor::PushAllNiagaraParameters()
{
	if (!ParticleComponent)
	{
		return;
	}

	// Field bounds
	ParticleComponent->SetVariableVec3(FName(TEXT("FieldBoundsMin")), FieldBoundsMin);
	ParticleComponent->SetVariableVec3(FName(TEXT("FieldBoundsMax")), FieldBoundsMax);

	// Noise params (for Niagara's GPU-side curl noise module)
	ParticleComponent->SetVariableFloat(FName(TEXT("NoiseScale")), NoiseScale);
	ParticleComponent->SetVariableFloat(FName(TEXT("NoiseSpeed")), NoiseSpeed);
	ParticleComponent->SetVariableFloat(FName(TEXT("FieldStrength")), FieldStrength);
	ParticleComponent->SetVariableFloat(FName(TEXT("NoiseOctaves")), static_cast<float>(NoiseOctaves));

	// Visual params
	ParticleComponent->SetVariableFloat(FName(TEXT("SpawnRate")), SpawnRate);
	ParticleComponent->SetVariableFloat(FName(TEXT("ParticleLifetime")), ParticleLifetime);
	ParticleComponent->SetVariableFloat(FName(TEXT("ParticleSize")), ParticleSize);
	ParticleComponent->SetVariableFloat(FName(TEXT("TrailLength")), TrailLength);
	ParticleComponent->SetVariableFloat(FName(TEXT("ColorHue")), ColorHue);
	ParticleComponent->SetVariableFloat(FName(TEXT("ColorSaturation")), ColorSaturation);
	ParticleComponent->SetVariableFloat(FName(TEXT("EmissiveStrength")), EmissiveStrength);
	ParticleComponent->SetVariableFloat(FName(TEXT("TurbulenceIntensity")), TurbulenceIntensity);

	// Ribbon width for aurora curtains
	ParticleComponent->SetVariableFloat(FName(TEXT("RibbonWidth")), RibbonWidth);

	// Sprite size as 2D vector (wide x short = aurora streak shape)
	ParticleComponent->SetVariableVec2(FName(TEXT("SpriteSize")), FVector2D(SpriteSizeX, SpriteSizeY));

	// Initial velocity for spawned particles (slowly rotating direction)
	float WindAngle = ElapsedTime * 0.015f;
	FVector WindDir = FVector(
		FMath::Cos(WindAngle),
		FMath::Sin(WindAngle),
		FMath::Sin(ElapsedTime * 0.03f) * 0.15f
	).GetSafeNormal();
	FVector SpawnVelocity = WindDir * WindStrength;
	ParticleComponent->SetVariableVec3(FName(TEXT("InitialVelocity")), SpawnVelocity);
	ParticleComponent->SetVariableFloat(FName(TEXT("DragAmount")), DragAmount);

	// Compute actual color from HSV and push as a LinearColor + emissive-boosted color
	FLinearColor BaseColor = HSVToLinearColor(ColorHue, ColorSaturation, 1.0f);
	FLinearColor EmissiveColor = BaseColor * EmissiveStrength;
	ParticleComponent->SetVariableLinearColor(FName(TEXT("ParticleColor")), EmissiveColor);

	// Master intensity (raw 0-1 for any Niagara logic that wants it)
	ParticleComponent->SetVariableFloat(FName(TEXT("MasterIntensity")), SmoothedMasterIntensity);
}

// ============================================================================
// Debug HUD
// ============================================================================

void AFlowFieldActor::DrawDebugHUD() const
{
	if (!GEngine) return;

	int32 Key = 100; // Starting key for on-screen messages

	// === Header ===
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::White,
		TEXT("=== FLOW FIELD DEBUG ==="));

	// === Data Source ===
	FString SourceName;
	switch (DataSource)
	{
		case EDataSourceType::Manual:    SourceName = TEXT("MANUAL"); break;
		case EDataSourceType::Maestra:   SourceName = TEXT("MAESTRA"); break;
		case EDataSourceType::UDP:       SourceName = TEXT("UDP"); break;
		case EDataSourceType::Simulated: SourceName = TEXT("SIMULATED"); break;
		default:                         SourceName = TEXT("UNKNOWN"); break;
	}
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Cyan,
		FString::Printf(TEXT("Data Source: %s"), *SourceName));

	// === Maestra Connection Status ===
	if (DataSource == EDataSourceType::Maestra)
	{
		bool bWsConnected = MaestraClientInstance && MaestraClientInstance->IsWebSocketConnected();
		FColor WsColor = bWsConnected ? FColor::Green : FColor::Red;
		GEngine->AddOnScreenDebugMessage(Key++, 0.0f, WsColor,
			FString::Printf(TEXT("WebSocket: %s"), bWsConnected ? TEXT("CONNECTED") : TEXT("DISCONNECTED")));

		GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Cyan,
			FString::Printf(TEXT("Entity Slug: %s"), *MaestraEntitySlug));

		GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Cyan,
			FString::Printf(TEXT("WS URL: %s"), *MaestraWebSocketUrl));

		GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Cyan,
			FString::Printf(TEXT("API URL: %s"), *MaestraApiUrl));

		GEngine->AddOnScreenDebugMessage(Key++, 0.0f, TrackedEntity ? FColor::Green : FColor::Yellow,
			FString::Printf(TEXT("Tracked Entity: %s"), TrackedEntity ? TEXT("YES (receiving data)") : TEXT("NO (waiting for first data)")));

		GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Cyan,
			FString::Printf(TEXT("Data Key: %s"), *PotentiometerDataKey));
	}

	// === Input Values ===
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::White, TEXT("---"));

	// Visual bar for intensity
	int32 BarLength = 20;
	int32 FilledLength = FMath::RoundToInt32(SmoothedMasterIntensity * BarLength);
	FString Bar = TEXT("[");
	for (int32 i = 0; i < BarLength; ++i)
	{
		Bar += (i < FilledLength) ? TEXT("|") : TEXT(".");
	}
	Bar += TEXT("]");

	FColor IntensityColor = FColor::MakeRedToGreenColorFromScalar(1.0f - SmoothedMasterIntensity);
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, IntensityColor,
		FString::Printf(TEXT("Intensity: %.3f  %s"), SmoothedMasterIntensity, *Bar));

	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Silver,
		FString::Printf(TEXT("Raw MasterIntensity: %.3f"), MasterIntensity));

	// === Mapped Parameters ===
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::White, TEXT("---"));
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Yellow, TEXT("Mapped Parameters:"));

	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Silver,
		FString::Printf(TEXT("  SpawnRate:     %.0f"), SpawnRate));
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Silver,
		FString::Printf(TEXT("  Lifetime:      %.2f s"), ParticleLifetime));
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Silver,
		FString::Printf(TEXT("  ParticleSize:  %.2f"), ParticleSize));
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Silver,
		FString::Printf(TEXT("  TrailLength:   %.2f"), TrailLength));
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Silver,
		FString::Printf(TEXT("  ColorHue:      %.3f"), ColorHue));
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Silver,
		FString::Printf(TEXT("  ColorSat:      %.3f"), ColorSaturation));
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Silver,
		FString::Printf(TEXT("  Emissive:      %.1f"), EmissiveStrength));
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Silver,
		FString::Printf(TEXT("  Turbulence:    %.2f"), TurbulenceIntensity));
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Silver,
		FString::Printf(TEXT("  NoiseScale:    %.4f"), NoiseScale));
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Silver,
		FString::Printf(TEXT("  NoiseSpeed:    %.3f"), NoiseSpeed));
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Silver,
		FString::Printf(TEXT("  FieldStrength: %.1f"), FieldStrength));
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Silver,
		FString::Printf(TEXT("  RibbonWidth:   %.1f"), RibbonWidth));
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Silver,
		FString::Printf(TEXT("  WindStrength:  %.1f"), WindStrength));
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Silver,
		FString::Printf(TEXT("  DragAmount:    %.1f"), DragAmount));
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::Silver,
		FString::Printf(TEXT("  NoiseForce:    %.2f"), NoiseForceStrength));

	// === Niagara Status ===
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::White, TEXT("---"));
	bool bNiagaraAsset = ParticleSystemAsset != nullptr;
	bool bComponentValid = ParticleComponent != nullptr;
	bool bComponentHasAsset = bComponentValid && ParticleComponent->GetAsset() != nullptr;
	bool bNiagaraActive = bComponentValid && ParticleComponent->IsActive();

	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, bNiagaraAsset ? FColor::Green : FColor::Red,
		FString::Printf(TEXT("Niagara UPROPERTY Asset: %s"), bNiagaraAsset ? *ParticleSystemAsset->GetName() : TEXT("NULL - Assign in Details!")));
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, bComponentHasAsset ? FColor::Green : FColor::Red,
		FString::Printf(TEXT("Component Asset: %s"), bComponentHasAsset ? *ParticleComponent->GetAsset()->GetName() : TEXT("NULL - SetAsset not called")));
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, bNiagaraActive ? FColor::Green : FColor::Red,
		FString::Printf(TEXT("Niagara Active: %s"), bNiagaraActive ? TEXT("YES") : TEXT("NO")));
	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, bComponentValid ? FColor::Green : FColor::Red,
		FString::Printf(TEXT("Component Valid: %s"), bComponentValid ? TEXT("YES") : TEXT("NO")));

	GEngine->AddOnScreenDebugMessage(Key++, 0.0f, FColor::White,
		FString::Printf(TEXT("Elapsed Time: %.1f s"), ElapsedTime));
}

// ============================================================================
// Field Generation (unchanged)
// ============================================================================

void AFlowFieldActor::RegenerateField()
{
	for (int32 Z = 0; Z < FieldGridSize.Z; ++Z)
	{
		for (int32 Y = 0; Y < FieldGridSize.Y; ++Y)
		{
			for (int32 X = 0; X < FieldGridSize.X; ++X)
			{
				FVector WorldPos = GridToWorld(X, Y, Z);
				FVector FlowVector;

				switch (NoiseType)
				{
				case EFlowFieldNoiseType::CurlNoise:
					FlowVector = ComputeCurlNoise(WorldPos, ElapsedTime);
					break;
				case EFlowFieldNoiseType::Perlin3D:
				default:
					FlowVector = ComputeNoiseVector(WorldPos, ElapsedTime);
					break;
				case EFlowFieldNoiseType::Simplex:
					FlowVector = ComputeNoiseVector(WorldPos, ElapsedTime);
					break;
				}

				FieldData[GridIndex(X, Y, Z)] = FlowVector * FieldStrength;
			}
		}
	}
}

float AFlowFieldActor::SampleNoise(FVector Position, float Time) const
{
	float Total = 0.0f;
	float Amplitude = 1.0f;
	float Frequency = 1.0f;
	float MaxValue = 0.0f;

	FVector P = Position * NoiseScale;

	for (int32 i = 0; i < NoiseOctaves; ++i)
	{
		FVector SamplePos = P * Frequency + FVector(Time * NoiseSpeed * 0.1f);
		Total += FMath::PerlinNoise3D(SamplePos) * Amplitude;
		MaxValue += Amplitude;
		Amplitude *= NoisePersistence;
		Frequency *= NoiseLacunarity;
	}

	return (MaxValue > 0.0f) ? Total / MaxValue : 0.0f;
}

FVector AFlowFieldActor::ComputeNoiseVector(FVector Position, float Time) const
{
	float X = SampleNoise(Position + FVector(31.7f, 0.0f, 0.0f), Time);
	float Y = SampleNoise(Position + FVector(0.0f, 47.3f, 0.0f), Time);
	float Z = SampleNoise(Position + FVector(0.0f, 0.0f, 73.1f), Time);
	return FVector(X, Y, Z);
}

FVector AFlowFieldActor::ComputeCurlNoise(FVector Position, float Time) const
{
	const float Eps = 1.0f / NoiseScale;

	float dNdy = SampleNoise(Position + FVector(0, Eps, 0), Time) - SampleNoise(Position - FVector(0, Eps, 0), Time);
	float dNdz = SampleNoise(Position + FVector(0, 0, Eps), Time) - SampleNoise(Position - FVector(0, 0, Eps), Time);

	FVector Offset(127.1f, 311.7f, 74.7f);
	float dN2dx = SampleNoise(Position + Offset + FVector(Eps, 0, 0), Time) - SampleNoise(Position + Offset - FVector(Eps, 0, 0), Time);
	float dN2dz = SampleNoise(Position + Offset + FVector(0, 0, Eps), Time) - SampleNoise(Position + Offset - FVector(0, 0, Eps), Time);

	FVector Offset2(269.5f, 183.3f, 246.1f);
	float dN3dx = SampleNoise(Position + Offset2 + FVector(Eps, 0, 0), Time) - SampleNoise(Position + Offset2 - FVector(Eps, 0, 0), Time);
	float dN3dy = SampleNoise(Position + Offset2 + FVector(0, Eps, 0), Time) - SampleNoise(Position + Offset2 - FVector(0, Eps, 0), Time);

	return FVector(
		dNdz - dN2dz,
		dN2dx - dN3dx,
		dN3dy - dNdy
	).GetSafeNormal();
}

FVector AFlowFieldActor::SampleField(FVector WorldPosition) const
{
	FVector FieldSize = FieldBoundsMax - FieldBoundsMin;
	FVector Normalized = (WorldPosition - FieldBoundsMin) / FieldSize;

	float GX = Normalized.X * (FieldGridSize.X - 1);
	float GY = Normalized.Y * (FieldGridSize.Y - 1);
	float GZ = Normalized.Z * (FieldGridSize.Z - 1);

	int32 X0 = FMath::Clamp(FMath::FloorToInt32(GX), 0, FieldGridSize.X - 1);
	int32 Y0 = FMath::Clamp(FMath::FloorToInt32(GY), 0, FieldGridSize.Y - 1);
	int32 Z0 = FMath::Clamp(FMath::FloorToInt32(GZ), 0, FieldGridSize.Z - 1);
	int32 X1 = FMath::Min(X0 + 1, FieldGridSize.X - 1);
	int32 Y1 = FMath::Min(Y0 + 1, FieldGridSize.Y - 1);
	int32 Z1 = FMath::Min(Z0 + 1, FieldGridSize.Z - 1);

	float XF = GX - X0;
	float YF = GY - Y0;
	float ZF = GZ - Z0;

	FVector C000 = FieldData[GridIndex(X0, Y0, Z0)];
	FVector C100 = FieldData[GridIndex(X1, Y0, Z0)];
	FVector C010 = FieldData[GridIndex(X0, Y1, Z0)];
	FVector C110 = FieldData[GridIndex(X1, Y1, Z0)];
	FVector C001 = FieldData[GridIndex(X0, Y0, Z1)];
	FVector C101 = FieldData[GridIndex(X1, Y0, Z1)];
	FVector C011 = FieldData[GridIndex(X0, Y1, Z1)];
	FVector C111 = FieldData[GridIndex(X1, Y1, Z1)];

	FVector C00 = FMath::Lerp(C000, C100, XF);
	FVector C10 = FMath::Lerp(C010, C110, XF);
	FVector C01 = FMath::Lerp(C001, C101, XF);
	FVector C11 = FMath::Lerp(C011, C111, XF);

	FVector C0 = FMath::Lerp(C00, C10, YF);
	FVector C1 = FMath::Lerp(C01, C11, YF);

	return FMath::Lerp(C0, C1, ZF);
}

int32 AFlowFieldActor::GridIndex(int32 X, int32 Y, int32 Z) const
{
	return X + Y * FieldGridSize.X + Z * FieldGridSize.X * FieldGridSize.Y;
}

FVector AFlowFieldActor::GridToWorld(int32 X, int32 Y, int32 Z) const
{
	FVector FieldSize = FieldBoundsMax - FieldBoundsMin;
	return FieldBoundsMin + FVector(
		(static_cast<float>(X) / FMath::Max(FieldGridSize.X - 1, 1)) * FieldSize.X,
		(static_cast<float>(Y) / FMath::Max(FieldGridSize.Y - 1, 1)) * FieldSize.Y,
		(static_cast<float>(Z) / FMath::Max(FieldGridSize.Z - 1, 1)) * FieldSize.Z
	);
}

void AFlowFieldActor::DrawDebugVisualization() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	int32 Step = FMath::Max(1, FMath::Max(FieldGridSize.X, FMath::Max(FieldGridSize.Y, FieldGridSize.Z)) / 8);

	for (int32 Z = 0; Z < FieldGridSize.Z; Z += Step)
	{
		for (int32 Y = 0; Y < FieldGridSize.Y; Y += Step)
		{
			for (int32 X = 0; X < FieldGridSize.X; X += Step)
			{
				FVector Pos = GetActorLocation() + GridToWorld(X, Y, Z);
				FVector Dir = FieldData[GridIndex(X, Y, Z)];
				float Magnitude = Dir.Size();
				FColor Color = FColor::MakeRedToGreenColorFromScalar(FMath::Clamp(Magnitude / FieldStrength, 0.0f, 1.0f));

				DrawDebugDirectionalArrow(World, Pos, Pos + Dir.GetSafeNormal() * 30.0f, 5.0f, Color, false, -1.0f, 0, 1.5f);
			}
		}
	}
}
