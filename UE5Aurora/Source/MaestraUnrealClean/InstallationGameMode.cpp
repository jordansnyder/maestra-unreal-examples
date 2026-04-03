#include "InstallationGameMode.h"
#include "InstallationCameraPawn.h"

AInstallationGameMode::AInstallationGameMode()
{
	DefaultPawnClass = AInstallationCameraPawn::StaticClass();
}
