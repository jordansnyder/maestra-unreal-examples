#pragma once

#include "CoreMinimal.h"
#include "RFDataProvider.h"
#include "UDPRFDataProvider.generated.h"

UCLASS(BlueprintType, Blueprintable)
class MAESTRAUNREALCLEAN_API UUDPRFDataProvider : public URFDataProvider
{
	GENERATED_BODY()

public:
	virtual FRFSpectrumFrame GetNextFrame(float DeltaTime) override;
	virtual bool IsReady() const override;

	/** Feed a JSON-encoded spectrum frame from a UDP packet. Expected format:
	 *  {"freq_min": 88.0, "freq_max": 108.0, "amplitudes": [-90.0, -85.2, ...]}
	 */
	UFUNCTION(BlueprintCallable, Category = "RF Data")
	void IngestPacket(const FString& JsonString);

	/** Feed raw float array bytes (little-endian float32 per bin). */
	UFUNCTION(BlueprintCallable, Category = "RF Data")
	void IngestRawBytes(const TArray<uint8>& Bytes);

private:
	FRFSpectrumFrame LatestFrame;
	FCriticalSection FrameLock;
	bool bHasData = false;
};
