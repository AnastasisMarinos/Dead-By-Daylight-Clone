// © Anastasis Marinos 2025 //

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SettingsSaveGame.generated.h"

// Audio mix channels stored in the save (index order must match TArray).
UENUM(BlueprintType)
enum class EAudioChannel : uint8
{
	Master,
	Music,
	Effects,
	Environment,
	Dialogue
};

// Game-wide persistent player settings.
//
// Responsibilities
// - Store audio volumes, input sensitivity, and toggleable visual features.
// - Provide safe getters/setters for indexed volume access.
//
// Notes
// - AudioVolumes array length must match number of EAudioChannel entries.
// - Values are expected as normalized [0..1].

UCLASS()
class GAMETEMPLATE_API USettingsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	USettingsSaveGame();
	
	// Normalized [0..1] per-channel volumes (indexed by EAudioChannel).
	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	TArray<float> AudioVolumes;

	// Mouse/controller look sensitivity (game-specific scale).
	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	float Sensitivity;

	// Optional retro rendering toggle read by your rendering/UI code.
	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	bool bRetroRendering;

	// Safe accessor for a channel volume (returns 1.0f if out of bounds).
	UFUNCTION(BlueprintCallable, Category = "Settings")
	float GetVolume(EAudioChannel Channel) const;

	// Safe setter with clamping to [0..1].
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetVolume(EAudioChannel Channel, float NewVolume);
};