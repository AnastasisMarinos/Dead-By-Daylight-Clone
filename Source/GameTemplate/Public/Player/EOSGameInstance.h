// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "EOSGameInstance.generated.h"


class USettingsSaveGame;
class FOnlineSessionSearch;
class IOnlineSubsystem;

UCLASS()
class GAMETEMPLATE_API UEOSGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UEOSGameInstance();
	
	virtual void Init() override;

	void Login();
	void OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);

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

	TSharedPtr<FOnlineSessionSearch> SearchSettings;

	FString DisplayName;

	// SETTINGS SAVE FUNCTIONS //
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
