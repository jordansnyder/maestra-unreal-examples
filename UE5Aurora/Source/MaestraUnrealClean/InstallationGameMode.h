#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "InstallationGameMode.generated.h"

/**
 * Minimal game mode that spawns the autonomous installation camera.
 */
UCLASS()
class MAESTRAUNREALCLEAN_API AInstallationGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AInstallationGameMode();
};
