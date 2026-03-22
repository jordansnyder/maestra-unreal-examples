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

	/** Feed an SDRF binary packet from the Python RTL-SDR script.
	 *  Header (36 bytes, little-endian):
	 *    [0:4]   uint32  magic       0x53445246 ("SDRF")
	 *    [4:8]   uint32  sequence
	 *    [8:16]  float64 center_freq Hz
	 *    [16:24] float64 sample_rate Hz
	 *    [24:32] float64 reserved
	 *    [32:36] uint32  fft_size
	 *    [36:]   float32[] power_db
	 *  Returns true if the packet was valid SDRF format. */
	UFUNCTION(BlueprintCallable, Category = "RF Data")
	bool IngestSDRFPacket(const TArray<uint8>& Bytes);

	/** Check if raw bytes begin with the SDRF magic number. */
	UFUNCTION(BlueprintCallable, Category = "RF Data")
	static bool IsSDRFPacket(const TArray<uint8>& Bytes);

private:
	FRFSpectrumFrame LatestFrame;
	FCriticalSection FrameLock;
	bool bHasData = false;
};
