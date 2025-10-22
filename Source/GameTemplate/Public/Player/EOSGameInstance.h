// © Anastasis Marinos 2025 //

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "EOSGameInstance.generated.h"

class USettingsSaveGame;
class FOnlineSessionSearch;
class IOnlineSubsystem;

// Game-wide instance that owns EOS login, session lifecycle, and player settings.
//
// Responsibilities
// - Log in to the Online Subsystem (EOS) on startup.
// - Create/find/join/destroy sessions and handle seamless travel.
// - Manage a small settings save (audio/sensitivity/etc.).
//
// Networking
// - Session creation opens the lobby on the host in listen mode.
// - Destroying a session returns the host to the main menu map.

UCLASS()
class GAMETEMPLATE_API UEOSGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UEOSGameInstance();
	
	virtual void Init() override;

	// Identity
	void Login();
	void OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);

	// Sessions
	UFUNCTION(BlueprintCallable)
	void CreateSession();
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);

	UFUNCTION(BlueprintCallable)
	void DestroySession();
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);

	UFUNCTION(BlueprintCallable)
	void FindSessions();
	void OnFindSessionsComplete(bool bWasSuccessful);

	UFUNCTION(BlueprintCallable)
	void LeaveSession();
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type JoinResult);

	// Search results and display name convenience fields.
	TSharedPtr<FOnlineSessionSearch> SearchSettings;
	FString DisplayName;

	// Settings Save
	UFUNCTION(BlueprintCallable, Category = "Save|Settings")
	bool SettingsSaveExists() const;

	UFUNCTION(BlueprintCallable, Category = "Save|Settings")
	void CreateNewSettingsSave();

	UFUNCTION(BlueprintCallable, Category = "Save|Settings")
	void LoadSettingsSave();

	UFUNCTION(BlueprintCallable, Category = "Save|Settings")
	void SaveSettingsToSlot();

	UFUNCTION(BlueprintPure, Category = "Save|Settings")
	USettingsSaveGame* GetCurrentSettingsSave() const { return CurrentSettingsSave; }

private:
	IOnlineSubsystem* OnlineSubsystem;

	UPROPERTY()
	USettingsSaveGame* CurrentSettingsSave;
	
	static const FString SettingsSlotName;
	static const uint32 UserIndex;
};