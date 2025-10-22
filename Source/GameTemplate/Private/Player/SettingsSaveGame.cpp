// © Anastasis Marinos 2025 //

#include "Player/SettingsSaveGame.h"

USettingsSaveGame::USettingsSaveGame()
{
	AudioVolumes.Init(0.8f, 5);
	Sensitivity = 0.4f;
	bRetroRendering = true;
}

// Accessors & mutators for audio channels.

float USettingsSaveGame::GetVolume(EAudioChannel Channel) const
{
	int32 Index = static_cast<int32>(Channel);
	if (AudioVolumes.IsValidIndex(Index))
	{
		return AudioVolumes[Index];
	}
	return 1.f;
}

void USettingsSaveGame::SetVolume(EAudioChannel Channel, float NewVolume)
{
	int32 Index = static_cast<int32>(Channel);
	if (AudioVolumes.IsValidIndex(Index))
	{
		AudioVolumes[Index] = FMath::Clamp(NewVolume, 0.f, 1.f);
	}
}