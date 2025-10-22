// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/EOSGameInstance.h"

#include "Interfaces/OnlineSessionInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Online/OnlineSessionNames.h"
#include "Player/SettingsSaveGame.h"

const FName MatchSessionName = FName("BBDOnlineSession");

const FString UEOSGameInstance::SettingsSlotName = TEXT("SettingsSave");
const uint32 UEOSGameInstance::UserIndex = 0;

UEOSGameInstance::UEOSGameInstance()
{
	CurrentSettingsSave = nullptr;
}

void UEOSGameInstance::Init()
{
	Super::Init();

	OnlineSubsystem = IOnlineSubsystem::Get();
	Login();
}

void UEOSGameInstance::Login()
{
	if(OnlineSubsystem)
	{
		if(IOnlineIdentityPtr Identity = OnlineSubsystem->GetIdentityInterface())
		{
			FOnlineAccountCredentials Credentials;
			Credentials.Id = FString();
			Credentials.Token = FString();
			Credentials.Type = FString("accountportal");

			Identity->OnLoginCompleteDelegates->AddUObject(this, &UEOSGameInstance::OnLoginComplete);
			Identity->Login(0 , Credentials);
		}
	}
}

void UEOSGameInstance::OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId,
	const FString& Error)
{
	if(OnlineSubsystem)
	{
		if(IOnlineIdentityPtr Identity = OnlineSubsystem->GetIdentityInterface())
		{
			Identity->ClearOnLoginCompleteDelegates(0, this);
			
			if (bWasSuccessful && Identity->GetLoginStatus(LocalUserNum) == ELoginStatus::LoggedIn)
			{
				TSharedPtr<const FUniqueNetId> UniqueId = Identity->GetUniquePlayerId(LocalUserNum);
				DisplayName = Identity->GetPlayerNickname(*UniqueId);

				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("Logged in as: %s"), *DisplayName));
			}
		}
	}
}

void UEOSGameInstance::CreateSession()
{
    if (!OnlineSubsystem) return;

    IOnlineSessionPtr SessionPtr = OnlineSubsystem->GetSessionInterface();
    if (!SessionPtr.IsValid()) return;

    // Clear any previous delegate to prevent duplicates
    SessionPtr->ClearOnCreateSessionCompleteDelegates(this);

    // Bind our delegate
    SessionPtr->OnCreateSessionCompleteDelegates.AddUObject(this, &UEOSGameInstance::OnCreateSessionComplete);

    // Setup session settings
    FOnlineSessionSettings SessionSettings;
    SessionSettings.bIsDedicated = false;
    SessionSettings.bShouldAdvertise = true;
    SessionSettings.bIsLANMatch = false;
    SessionSettings.NumPublicConnections = 5;
    SessionSettings.bAllowJoinInProgress = true;
    SessionSettings.bUsesPresence = true;
    SessionSettings.bAllowJoinViaPresence = true;
    SessionSettings.bAllowInvites = true;
    SessionSettings.bUseLobbiesIfAvailable = true;

    SessionSettings.Set(SEARCH_KEYWORDS, FString("BBDOnlineSession"), EOnlineDataAdvertisementType::ViaOnlineService);

    // Attempt to create the session
    if (!SessionPtr->CreateSession(0, MatchSessionName, SessionSettings))
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to start session creation!"));
        SessionPtr->ClearOnCreateSessionCompleteDelegates(this);
    }
}

void UEOSGameInstance::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
    // Clear delegate immediately to prevent double firing
    if (OnlineSubsystem)
    {
        if (IOnlineSessionPtr SessionPtr = OnlineSubsystem->GetSessionInterface())
        {
            SessionPtr->ClearOnCreateSessionCompleteDelegates(this);
        }
    }

    GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("Session Created: %d"), bWasSuccessful));

    if (!bWasSuccessful)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Session creation failed."));
        return;
    }

    // Confirm the session exists before traveling
    if (OnlineSubsystem)
    {
        if (IOnlineSessionPtr SessionPtr = OnlineSubsystem->GetSessionInterface())
        {
            FNamedOnlineSession* CreatedSession = SessionPtr->GetNamedSession(SessionName);
            if (!CreatedSession)
            {
                GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Session not fully ready yet!"));
                return;
            }
        }
    }

    // Travel to the lobby
    if (UWorld* World = GetWorld())
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, TEXT("Traveling to lobby..."));
        UGameplayStatics::OpenLevel(World, FName("/Game/Maps/M_Lobby"), true, "listen");
    }
    else
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("World context invalid! Cannot travel."));
    }
}

void UEOSGameInstance::DestroySession()
{
	if(OnlineSubsystem)
	{
		if(IOnlineSessionPtr SessionPtr = OnlineSubsystem->GetSessionInterface())
		{
			SessionPtr->OnDestroySessionCompleteDelegates.AddUObject(this, &UEOSGameInstance::OnDestroySessionComplete);
			SessionPtr->DestroySession(MatchSessionName);
		}
	}
}

void UEOSGameInstance::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Session Destroyed."));
	
	if(OnlineSubsystem)
	{
		if(IOnlineSessionPtr SessionPtr = OnlineSubsystem->GetSessionInterface())
		{
			SessionPtr->ClearOnDestroySessionCompleteDelegates(this);
		}
	}
	UGameplayStatics::OpenLevel(GetWorld(), FName("M_MainMenu"));
}

void UEOSGameInstance::FindSessions()
{
	if(OnlineSubsystem)
	{
		if(IOnlineSessionPtr SessionPtr = OnlineSubsystem->GetSessionInterface())
		{
			SessionPtr->OnFindSessionsCompleteDelegates.AddUObject(this, &UEOSGameInstance::OnFindSessionsComplete);

			SearchSettings = MakeShareable(new FOnlineSessionSearch());
			SearchSettings->MaxSearchResults = 5000;
			SearchSettings->bIsLanQuery = false;
			SearchSettings->QuerySettings.Set(SEARCH_KEYWORDS, FString("BBDOnlineSession"), EOnlineComparisonOp::Equals);
			SearchSettings->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
			
			SessionPtr->FindSessions(0, SearchSettings.ToSharedRef());
		}
	}
}

void UEOSGameInstance::OnFindSessionsComplete(bool bWasSuccessful)
{
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("Session Found: %d"), bWasSuccessful));
	
	if(bWasSuccessful)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("Found %d Sessions"), SearchSettings->SearchResults.Num()));

		if(OnlineSubsystem)
		{
			if(IOnlineSessionPtr SessionPtr = OnlineSubsystem->GetSessionInterface())
			{
				SessionPtr->ClearOnFindSessionsCompleteDelegates(this);
				if(SearchSettings->SearchResults.Num())
				{
					SessionPtr->OnJoinSessionCompleteDelegates.AddUObject(this, &UEOSGameInstance::OnJoinSessionComplete);
					SessionPtr->JoinSession(0, MatchSessionName, SearchSettings->SearchResults[0]);
				}
			}
		}
	}
}

void UEOSGameInstance::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type JoinResult)
{
	if(OnlineSubsystem && JoinResult == EOnJoinSessionCompleteResult::Success)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("Joined Session")));
		
		if(IOnlineSessionPtr SessionPtr = OnlineSubsystem->GetSessionInterface())
		{
			FString ConnectionInfo;
			SessionPtr->GetResolvedConnectString(MatchSessionName, ConnectionInfo);
			
			if(!ConnectionInfo.IsEmpty())
			{
				if(APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0))
				{
					GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("Traveling To Session")));
					PlayerController->ClientTravel(ConnectionInfo, TRAVEL_Absolute);
				}
			}
		}
	}
}

void UEOSGameInstance::LeaveSession()
{
	if (OnlineSubsystem)
	{
		if (IOnlineSessionPtr SessionPtr = OnlineSubsystem->GetSessionInterface())
		{
			// Bind the delegate for session destruction completion
			SessionPtr->OnDestroySessionCompleteDelegates.AddUObject(this, &UEOSGameInstance::OnDestroySessionComplete);
            
			// Initiate session destruction
			SessionPtr->DestroySession(MatchSessionName);
		}
	}
}

#pragma region SettingsSave

bool UEOSGameInstance::SettingsSaveExists() const
{
	return UGameplayStatics::DoesSaveGameExist(SettingsSlotName, UserIndex);
}

// Creates a new save game object and writes it to disk.
void UEOSGameInstance::CreateNewSettingsSave()
{
	CurrentSettingsSave = Cast<USettingsSaveGame>(UGameplayStatics::CreateSaveGameObject(USettingsSaveGame::StaticClass()));
	if (CurrentSettingsSave)
		UGameplayStatics::SaveGameToSlot(CurrentSettingsSave, SettingsSlotName, UserIndex);
}

// Loads an existing save, if available.
void UEOSGameInstance::LoadSettingsSave()
{
	if (SettingsSaveExists())
		CurrentSettingsSave = Cast<USettingsSaveGame>(UGameplayStatics::LoadGameFromSlot(SettingsSlotName, UserIndex));
	else
		CurrentSettingsSave = nullptr;
}

// Writes current save data back to slot.
void UEOSGameInstance::SaveSettingsToSlot()
{
	if (CurrentSettingsSave)
		UGameplayStatics::SaveGameToSlot(CurrentSettingsSave, SettingsSlotName, UserIndex);
}

#pragma endregion

