// © Anastasis Marinos //

#include "Player/SettingsSaveGame.h"

USettingsSaveGame::USettingsSaveGame()
{
	// Initialize volumes to default (0.8)
	AudioVolumes.Init(0.8f, 5); // 5 entries: Master, Music, Effects, Environment, Dialogue

	Sensitivity = 0.4f;
	bRetroRendering = true;
}

float USettingsSaveGame::GetVolume(EAudioChannel Channel) const
{
	int32 Index = static_cast<int32>(Channel);
	if (AudioVolumes.IsValidIndex(Index))
	{
		return AudioVolumes[Index];
	}
	return 1.f; // fallback
}

void USettingsSaveGame::SetVolume(EAudioChannel Channel, float NewVolume)
{
	int32 Index = static_cast<int32>(Channel);
	if (AudioVolumes.IsValidIndex(Index))
	{
		AudioVolumes[Index] = FMath::Clamp(NewVolume, 0.f, 1.f);
	}
}