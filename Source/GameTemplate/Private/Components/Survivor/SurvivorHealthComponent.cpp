// © Anastasis Marinos 2025 //

#include "Components/Survivor/SurvivorHealthComponent.h"

#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Player/Characters/SurvivorCharacter.h"
#include "Player/Characters/KillerCharacter.h"
#include "World/Hook.h"

USurvivorHealthComponent::USurvivorHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void USurvivorHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerSurvivor = Cast<ASurvivorCharacter>(GetOwner());
}

void USurvivorHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USurvivorHealthComponent, HealthState);
	DOREPLIFETIME(USurvivorHealthComponent, HookedState);
	DOREPLIFETIME(USurvivorHealthComponent, HealingProgress);
	DOREPLIFETIME(USurvivorHealthComponent, WiggleProgress);
	DOREPLIFETIME(USurvivorHealthComponent, HookProgress);
	DOREPLIFETIME(USurvivorHealthComponent, HookCount);
	DOREPLIFETIME(USurvivorHealthComponent, ActiveKiller);
	DOREPLIFETIME(USurvivorHealthComponent, ActiveHook);
}

void USurvivorHealthComponent::SetHealthState(EHealthState NewState)
{
	if (!HasAuthority() || HealthState == NewState) return;
	HealthState = NewState;
	OnRep_HealthState();
}

void USurvivorHealthComponent::SetHookState(EHookState NewHook)
{
	if (!HasAuthority() || HookedState == NewHook) return;
	HookedState = NewHook;
	OnRep_HookState();
}

void USurvivorHealthComponent::HandleDeath()
{
	if (!HasAuthority()) return;

	if (OwnerSurvivor)
	{
		OwnerSurvivor->ClearActionProgressUI();
		OwnerSurvivor->ClearSkillCheckUI();
		OwnerSurvivor->Die();
	}
}

#pragma region UI

float USurvivorHealthComponent::GetCurrentProgressValue() const
{
	switch (HealthState)
	{
	case EHealthState::Hooked:   return HookProgress;     // how close to death
	case EHealthState::Carried:  return WiggleProgress;   // wiggle-to-freedom
	case EHealthState::Crawling: return HealingProgress;  // self-heal
	case EHealthState::Injured:  return HealingProgress;  // being healed by others
	default:                     return 0.f;
	}
}

void USurvivorHealthComponent::EmitProgressValue()
{
	PushProgressToOwnerUI();
}

void USurvivorHealthComponent::PushProgressToOwnerUI()
{
	if (OwnerSurvivor && HasAuthority())
	{
		OwnerSurvivor->SetUIProgressValue(GetCurrentProgressValue());
	}
}

void USurvivorHealthComponent::UpdateHealthUIForState()
{
	if (!HasAuthority() || !OwnerSurvivor) return;

	const bool bAlwaysShow =
		HealthState == EHealthState::Hooked ||
		HealthState == EHealthState::Carried ||
		HealthState == EHealthState::Crawling;

	const bool bHealeeWantsBar =
		(HealthState == EHealthState::Injured) && OwnerSurvivor->bIsBeingHealed;

	const bool bShow = bAlwaysShow || bHealeeWantsBar;

	if (bShow) ShowHealthUI();
	else       HideHealthUI();
}

void USurvivorHealthComponent::ShowHealthUI()
{
	if (!HasAuthority() || !OwnerSurvivor) return;

	OwnerSurvivor->CreateActionProgressUI();

	auto& TM = GetWorld()->GetTimerManager();
	TM.ClearTimer(HealthProgressTimerHandle);
	TM.SetTimer(HealthProgressTimerHandle, this, &USurvivorHealthComponent::Server_PushHealthProgressUI, 0.1f, true);

	Server_PushHealthProgressUI();
}

void USurvivorHealthComponent::HideHealthUI()
{
	if (!HasAuthority() || !OwnerSurvivor) return;

	GetWorld()->GetTimerManager().ClearTimer(HealthProgressTimerHandle);
	OwnerSurvivor->ClearActionProgressUI();
}

void USurvivorHealthComponent::Server_PushHealthProgressUI_Implementation()
{
	if (!OwnerSurvivor || !HasAuthority()) return;

	const bool bAlwaysShow =
		HealthState == EHealthState::Hooked ||
		HealthState == EHealthState::Carried ||
		HealthState == EHealthState::Crawling;

	const bool bHealeeWantsBar =
		(HealthState == EHealthState::Injured) && OwnerSurvivor && OwnerSurvivor->bIsBeingHealed;

	if (!(bAlwaysShow || bHealeeWantsBar))
	{
		HideHealthUI();
		return;
	}

	OwnerSurvivor->SetUIProgressValue(GetCurrentProgressValue());
}
#pragma endregion

#pragma region Handle Damage

void USurvivorHealthComponent::Server_SurvivorGetHit_Implementation()
{
	if (HealthState == EHealthState::Healthy)
	{
		Server_StartInjured();
	}
	else if (HealthState == EHealthState::Injured)
	{
		Server_StartCrawling();
	}
}

void USurvivorHealthComponent::Server_StartInjured_Implementation()
{
	SetHealthState(EHealthState::Injured);
}

void USurvivorHealthComponent::Server_StartCrawling_Implementation()
{
	SetHealthState(EHealthState::Crawling);
}

void USurvivorHealthComponent::Server_StopCrawling_Implementation()
{
	if (HealthState == EHealthState::Crawling)
	{
		SetHealthState(EHealthState::Injured);
	}
}

void USurvivorHealthComponent::Server_StartCarried_Implementation(AKillerCharacter* Killer)
{
	ActiveKiller = Killer;
	SetHealthState(EHealthState::Carried);
	WiggleProgress = 0.f;
	OnRep_WiggleProgress();
}

void USurvivorHealthComponent::Server_StopCarried_Implementation()
{
	AKillerCharacter* PrevKiller = ActiveKiller;

	ActiveKiller   = nullptr;
	SetHealthState(EHealthState::Injured);
	WiggleProgress = 0.f;
	OnRep_WiggleProgress();

	if (PrevKiller)
	{
		PrevKiller->bIsCarryingSurvivor = false;
		PrevKiller->SurvivorGettingCarried = nullptr;
	}

	UpdateHealthUIForState();
}

void USurvivorHealthComponent::Server_Wiggle_Implementation()
{
	if (HealthState != EHealthState::Carried) return;

	WiggleProgress += 0.11f;
	if (WiggleProgress >= 1.0f)
	{
		WiggleProgress = 0.0f;
		Server_StopCarried();
	}
	OnRep_WiggleProgress();
}

void USurvivorHealthComponent::Server_StartHooked_Implementation(AHook* Hook)
{
	if (!Hook) return;

	if (ActiveKiller)
	{
		ActiveKiller->bIsCarryingSurvivor = false;
		ActiveKiller->SurvivorGettingCarried = nullptr;
	}

	ActiveHook   = Hook;
	ActiveKiller = nullptr;

	HookCount++;
	if (HookCount >= 3)
	{
		HandleDeath();
		return;
	}

	HookProgress = (HookCount == 1) ? 1.0f : 0.5f;
	SetHookState((HookCount == 1) ? EHookState::Hooked : EHookState::Struggling);
	SetHealthState(EHealthState::Hooked);

	StartHookDamageTimer();
	OnRep_HookProgress();
}

void USurvivorHealthComponent::Server_StopHooked_Implementation()
{
	StopHookDamageTimer();
	SetHookState(EHookState::Unhook);
	SetHealthState(EHealthState::Injured);
	HookProgress = 1.0f;
	OnRep_HookProgress();
}

void USurvivorHealthComponent::Server_HealTarget_Implementation(ASurvivorCharacter* Target)
{
	if (!Target) return;

	if (USurvivorHealthComponent* HC = Target->FindComponentByClass<USurvivorHealthComponent>())
	{
		if (HC->HealthState == EHealthState::Injured)
		{
			HC->HealingProgress += 0.01f;
			if (HC->HealingProgress >= 1.0f)
			{
				HC->HealingProgress = 0.0f;
				HC->SetHealthState(EHealthState::Healthy);
			}
			HC->OnRep_HealingProgress();
		}
		else if (HC->HealthState == EHealthState::Crawling)
		{
			HC->HealingProgress += 0.025f;
			if (HC->HealingProgress >= 1.0f)
			{
				HC->HealingProgress = 0.0f;
				HC->Server_StopCrawling();
			}
			HC->OnRep_HealingProgress();
		}

		HC->PushProgressToOwnerUI();
	}
}

void USurvivorHealthComponent::Server_HealSelf_Implementation()
{
	if (HealthState == EHealthState::Crawling)
	{
		HealingProgress += 0.025f;
		HealingProgress = FMath::Clamp(HealingProgress, 0.0f, 0.95f);
		OnRep_HealingProgress();
	}
}
#pragma endregion

#pragma region Hook Damage Timer (Server)

void USurvivorHealthComponent::StartHookDamageTimer()
{
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().SetTimer(HookHealthTimer, this, &USurvivorHealthComponent::TickHookDamage, 0.5f, true);
	}
}

void USurvivorHealthComponent::StopHookDamageTimer()
{
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(HookHealthTimer);
	}
}

void USurvivorHealthComponent::TickHookDamage()
{
	if (HookedState == EHookState::Hooked || HookedState == EHookState::Struggling)
	{
		HookProgress -= 0.01f;

		if (HookProgress <= 0.5f && HookedState == EHookState::Hooked)
		{
			SetHookState(EHookState::Struggling);
		}
		if (HookProgress <= 0.0f)
		{
			HookProgress = 0.0f;
			StopHookDamageTimer();
			HandleDeath();
			return;
		}
		OnRep_HookProgress();
	}
	Server_PushHealthProgressUI();
}
#pragma endregion

#pragma region Replication & RepNotifies

// Propagate gameplay-facing state changes to character and UI.
void USurvivorHealthComponent::OnRep_HealthState()
{
	OnHealthStateChanged.Broadcast(HealthState);

	if (HasAuthority())
	{
		UpdateHealthUIForState();
		Server_PushHealthProgressUI();
	}
}

void USurvivorHealthComponent::OnRep_HookState()
{
	EmitProgressValue();
}

void USurvivorHealthComponent::OnRep_HealingProgress()
{
	if (HasAuthority())
	{
		UpdateHealthUIForState();
		Server_PushHealthProgressUI();
	}
}

void USurvivorHealthComponent::OnRep_WiggleProgress()
{
	if (HasAuthority())
	{
		Server_PushHealthProgressUI();
	}
}

void USurvivorHealthComponent::OnRep_HookProgress()
{
	if (HasAuthority())
	{
		Server_PushHealthProgressUI();
	}
}

#pragma endregion