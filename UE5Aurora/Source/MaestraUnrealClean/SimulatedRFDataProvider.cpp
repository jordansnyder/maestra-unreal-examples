#include "SimulatedRFDataProvider.h"

USimulatedRFDataProvider::USimulatedRFDataProvider()
{
	SetupDefaultStations();
}

void USimulatedRFDataProvider::SetupDefaultStations()
{
	Stations.Empty();

	FSimulatedStation Station1;
	Station1.FrequencyMHz = 89.3f;
	Station1.BandwidthMHz = 0.15f;
	Station1.PeakAmplitude = -25.0f;
	Station1.FadeRate = 0.3f;
	Station1.FadeDepth = 4.0f;
	Stations.Add(Station1);

	FSimulatedStation Station2;
	Station2.FrequencyMHz = 93.7f;
	Station2.BandwidthMHz = 0.2f;
	Station2.PeakAmplitude = -15.0f;
	Station2.FadeRate = 0.7f;
	Station2.FadeDepth = 8.0f;
	Stations.Add(Station2);

	FSimulatedStation Station3;
	Station3.FrequencyMHz = 97.1f;
	Station3.BandwidthMHz = 0.18f;
	Station3.PeakAmplitude = -20.0f;
	Station3.FadeRate = 0.5f;
	Station3.FadeDepth = 5.0f;
	Stations.Add(Station3);

	FSimulatedStation Station4;
	Station4.FrequencyMHz = 101.5f;
	Station4.BandwidthMHz = 0.22f;
	Station4.PeakAmplitude = -18.0f;
	Station4.FadeRate = 1.1f;
	Station4.FadeDepth = 10.0f;
	Stations.Add(Station4);

	FSimulatedStation Station5;
	Station5.FrequencyMHz = 105.9f;
	Station5.BandwidthMHz = 0.16f;
	Station5.PeakAmplitude = -22.0f;
	Station5.FadeRate = 0.4f;
	Station5.FadeDepth = 3.0f;
	Stations.Add(Station5);
}

void USimulatedRFDataProvider::Configure(float InFreqMin, float InFreqMax, int32 InNumBins, float InSampleRate)
{
	Super::Configure(InFreqMin, InFreqMax, InNumBins, InSampleRate);
	RNG.Initialize(RandomSeed);
}

float USimulatedRFDataProvider::GenerateStationSignal(float FreqMHz, const FSimulatedStation& Station) const
{
	float Offset = FreqMHz - Station.FrequencyMHz;
	float Sigma = Station.BandwidthMHz * 0.5f;
	float Envelope = FMath::Exp(-(Offset * Offset) / (2.0f * Sigma * Sigma));
	float Fade = Station.FadeDepth * FMath::Sin(ElapsedTime * Station.FadeRate * 2.0f * PI);
	float AmplitudeDbm = Station.PeakAmplitude + Fade;
	return AmplitudeDbm * Envelope;
}

float USimulatedRFDataProvider::GenerateNoise()
{
	float Noise = NoiseFloorDbm + RNG.FRandRange(-NoiseVariance, NoiseVariance);
	return Noise;
}

float USimulatedRFDataProvider::GenerateSweep(float FreqMHz) const
{
	float Offset = FreqMHz - SweepPosition;
	float Sigma = SweepSignalBandwidth * 0.5f;
	float Envelope = FMath::Exp(-(Offset * Offset) / (2.0f * Sigma * Sigma));
	return SweepSignalAmplitude * Envelope;
}

FRFSpectrumFrame USimulatedRFDataProvider::GetNextFrame(float DeltaTime)
{
	ElapsedTime += DeltaTime;

	SweepPosition += SweepSignalSpeed * DeltaTime;
	if (SweepPosition > FreqMaxMHz)
	{
		SweepPosition = FreqMinMHz;
	}

	FRFSpectrumFrame Frame;
	Frame.FrequencyMinMHz = FreqMinMHz;
	Frame.FrequencyMaxMHz = FreqMaxMHz;
	Frame.NumBins = NumBins;
	Frame.Timestamp = FPlatformTime::Seconds();
	Frame.Amplitudes.SetNum(NumBins);

	float BinWidth = (FreqMaxMHz - FreqMinMHz) / static_cast<float>(NumBins);

	for (int32 i = 0; i < NumBins; ++i)
	{
		float BinFreq = FreqMinMHz + (static_cast<float>(i) + 0.5f) * BinWidth;
		float Value = GenerateNoise();

		for (const FSimulatedStation& Station : Stations)
		{
			float StationContribution = GenerateStationSignal(BinFreq, Station);
			Value = FMath::Max(Value, StationContribution);
		}

		float SweepContribution = GenerateSweep(BinFreq);
		Value = FMath::Max(Value, SweepContribution);

		Frame.Amplitudes[i] = Value;
	}

	return Frame;
}
