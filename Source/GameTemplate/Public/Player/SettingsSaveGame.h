// © Anastasis Marinos //

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SettingsSaveGame.generated.h"

UENUM(BlueprintType)
enum class EAudioChannel : uint8
{
	Master,
	Music,
	Effects,
	Environment,
	Dialogue
};

UCLASS()
class GAMETEMPLATE_API USettingsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	USettingsSaveGame();
	
	// PROPERTIES & VARIABLES //
	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	TArray<float> AudioVolumes;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	float Sensitivity;

	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	bool bRetroRendering;

	// Helper function to get or set volume safely
	UFUNCTION(BlueprintCallable, Category = "Settings")
	float GetVolume(EAudioChannel Channel) const;

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void SetVolume(EAudioChannel Channel, float NewVolume);
};