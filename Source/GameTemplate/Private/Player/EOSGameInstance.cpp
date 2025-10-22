// © Anastasis Marinos 2025 //

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

// Login & identity hookup (EOS account portal).
#pragma region Login & Identity

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

#pragma endregion

// Session creation on host; opens lobby as a listen server on success.
#pragma region Host | Join | Destroy Session

void UEOSGameInstance::CreateSession()
{
    if (!OnlineSubsystem) return;

    IOnlineSessionPtr SessionPtr = OnlineSubsystem->GetSessionInterface();
    if (!SessionPtr.IsValid()) return;

    SessionPtr->ClearOnCreateSessionCompleteDelegates(this);
    SessionPtr->OnCreateSessionCompleteDelegates.AddUObject(this, &UEOSGameInstance::OnCreateSessionComplete);

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

    if (!SessionPtr->CreateSession(0, MatchSessionName, SessionSettings))
    {
        //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to start session creation!"));
        SessionPtr->ClearOnCreateSessionCompleteDelegates(this);
    }
}

void UEOSGameInstance::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
    if (OnlineSubsystem)
    {
        if (IOnlineSessionPtr SessionPtr = OnlineSubsystem->GetSessionInterface())
        {
            SessionPtr->ClearOnCreateSessionCompleteDelegates(this);
        }
    }

    //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("Session Created: %d"), bWasSuccessful));

    if (!bWasSuccessful)
    {
        //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Session creation failed."));
        return;
    }

    if (OnlineSubsystem)
    {
        if (IOnlineSessionPtr SessionPtr = OnlineSubsystem->GetSessionInterface())
        {
            FNamedOnlineSession* CreatedSession = SessionPtr->GetNamedSession(SessionName);
            if (!CreatedSession)
            {
                //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Session not fully ready yet!"));
                return;
            }
        }
    }

    if (UWorld* World = GetWorld())
    {
        //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, TEXT("Traveling to lobby..."));
        UGameplayStatics::OpenLevel(World, FName("/Game/Maps/M_Lobby"), true, "listen");
    }
    else
    {
        //GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("World context invalid! Cannot travel."));
    }
}

// Session teardown; host returns to main menu after destroy completes.
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
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Session Destroyed."));
	
	if(OnlineSubsystem)
	{
		if(IOnlineSessionPtr SessionPtr = OnlineSubsystem->GetSessionInterface())
		{
			SessionPtr->ClearOnDestroySessionCompleteDelegates(this);
		}
	}
	UGameplayStatics::OpenLevel(GetWorld(), FName("M_MainMenu"));
}

void UEOSGameInstance::LeaveSession()
{
	if (OnlineSubsystem)
	{
		if (IOnlineSessionPtr SessionPtr = OnlineSubsystem->GetSessionInterface())
		{
			SessionPtr->OnDestroySessionCompleteDelegates.AddUObject(this, &UEOSGameInstance::OnDestroySessionComplete);
			SessionPtr->DestroySession(MatchSessionName);
		}
	}
}

// Client-side discovery and join flow; travels to server address on success.
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
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("Session Found: %d"), bWasSuccessful));
	
	if(bWasSuccessful)
	{
		//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("Found %d Sessions"), SearchSettings->SearchResults.Num()));

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
		//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("Joined Session")));
		
		if(IOnlineSessionPtr SessionPtr = OnlineSubsystem->GetSessionInterface())
		{
			FString ConnectionInfo;
			SessionPtr->GetResolvedConnectString(MatchSessionName, ConnectionInfo);
			
			if(!ConnectionInfo.IsEmpty())
			{
				if(APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GetWorld(), 0))
				{
					//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("Traveling To Session")));
					PlayerController->ClientTravel(ConnectionInfo, TRAVEL_Absolute);
				}
			}
		}
	}
}

#pragma endregion

// Persistent player settings save/load helpers.
#pragma region SettingsSave

bool UEOSGameInstance::SettingsSaveExists() const
{
	return UGameplayStatics::DoesSaveGameExist(SettingsSlotName, UserIndex);
}

void UEOSGameInstance::CreateNewSettingsSave()
{
	CurrentSettingsSave = Cast<USettingsSaveGame>(UGameplayStatics::CreateSaveGameObject(USettingsSaveGame::StaticClass()));
	if (CurrentSettingsSave)
		UGameplayStatics::SaveGameToSlot(CurrentSettingsSave, SettingsSlotName, UserIndex);
}

void UEOSGameInstance::LoadSettingsSave()
{
	if (SettingsSaveExists())
		CurrentSettingsSave = Cast<USettingsSaveGame>(UGameplayStatics::LoadGameFromSlot(SettingsSlotName, UserIndex));
	else
		CurrentSettingsSave = nullptr;
}

void UEOSGameInstance::SaveSettingsToSlot()
{
	if (CurrentSettingsSave)
		UGameplayStatics::SaveGameToSlot(CurrentSettingsSave, SettingsSlotName, UserIndex);
}

#pragma endregion