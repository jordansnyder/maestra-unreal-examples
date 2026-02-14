#include "RFDataProvider.h"

FRFSpectrumFrame URFDataProvider::GetNextFrame(float DeltaTime)
{
	FRFSpectrumFrame Frame;
	Frame.FrequencyMinMHz = FreqMinMHz;
	Frame.FrequencyMaxMHz = FreqMaxMHz;
	Frame.NumBins = NumBins;
	Frame.Timestamp = FPlatformTime::Seconds();
	Frame.Amplitudes.SetNumZeroed(NumBins);
	return Frame;
}

void URFDataProvider::Configure(float InFreqMin, float InFreqMax, int32 InNumBins, float InSampleRate)
{
	FreqMinMHz = InFreqMin;
	FreqMaxMHz = InFreqMax;
	NumBins = FMath::Max(InNumBins, 2);
	SampleRate = FMath::Max(InSampleRate, 1.0f);
}

bool URFDataProvider::IsReady() const
{
	return true;
}
