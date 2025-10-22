// (C) Anastasis Marinos 2025

#include "Components/Survivor/SurvivorInteractionComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/Survivor/SkillCheckComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/Characters/SurvivorCharacter.h"
#include "World/Generator.h"
#include "World/ExitGateLever.h"

USurvivorInteractionComponent::USurvivorInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void USurvivorInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
	Initialize();
}

void USurvivorInteractionComponent::Initialize()
{
	OwnerSurvivor = Cast<ASurvivorCharacter>(GetOwner());
	if (!OwnerSurvivor.IsValid()) return;

	if (UCapsuleComponent* Capsule = OwnerSurvivor->FindComponentByClass<UCapsuleComponent>())
	{
		Capsule->OnComponentBeginOverlap.RemoveDynamic(this, &USurvivorInteractionComponent::OnCapsuleBeginOverlap);
		Capsule->OnComponentEndOverlap.RemoveDynamic(this,   &USurvivorInteractionComponent::OnCapsuleEndOverlap);

		Capsule->OnComponentBeginOverlap.AddDynamic(this,    &USurvivorInteractionComponent::OnCapsuleBeginOverlap);
		Capsule->OnComponentEndOverlap.AddDynamic(this,      &USurvivorInteractionComponent::OnCapsuleEndOverlap);
	}
}

// ============================================================================
// Overlaps (server)
// ============================================================================

void USurvivorInteractionComponent::OnCapsuleBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (!OwnerSurvivor.IsValid() || !OwnerSurvivor->HasAuthority()) return;

	if (AGenerator* Gen = Cast<AGenerator>(OtherActor))
	{
		ActiveGenerator = Gen;
	}
	else if (AExitGateLever* Lever = Cast<AExitGateLever>(OtherActor))
	{
		ActiveExitGateLever = Lever;
	}
	else if (ASurvivorCharacter* Other = Cast<ASurvivorCharacter>(OtherActor))
	{
		if (Other != OwnerSurvivor.Get())
		{
			if (Other->GetHealthState() != EHealthState::Healthy
			 && Other->GetHealthState() != EHealthState::Hooked)
			{
				HealingTarget = Other;
			}
			if (Other->GetHealthState() == EHealthState::Hooked)
			{
				ActiveHookedTarget = Other;
			}
		}
	}

	TryShowInteractionUI();
}

void USurvivorInteractionComponent::OnCapsuleEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32)
{
	if (!OwnerSurvivor.IsValid()) return;

	if (OtherActor == ActiveGenerator)         ActiveGenerator = nullptr;
	if (OtherActor == ActiveExitGateLever)     ActiveExitGateLever = nullptr;
	if (OtherActor == HealingTarget.Get())     HealingTarget.Reset();
	if (OtherActor == ActiveHookedTarget.Get())ActiveHookedTarget.Reset();

	const bool bHasAny =
		ActiveGenerator || ActiveExitGateLever || HealingTarget.IsValid() || ActiveHookedTarget.IsValid();

	if (!bHasAny && ActiveMode == EInteractionMode::None && !IsCrawling())
	{
		ClearInteractionUIAndTick();
	}
}

// ============================================================================
// Mode evaluation
// ============================================================================

EInteractionMode USurvivorInteractionComponent::EvaluateAvailableMode() const
{
	// Priority: Repair > Power > HealOther > Unhook > HealSelf
	if (CanInteractWithGenerator()) return EInteractionMode::Repair;
	if (CanInteractWithExitGate())  return EInteractionMode::Power;
	if (CanHealOther())             return EInteractionMode::HealOther;
	if (CanUnhookOther())           return EInteractionMode::Unhook;
	if (CanSelfHeal())              return EInteractionMode::HealSelf;
	return EInteractionMode::None;
}

// ============================================================================
// UI helpers (server -> client)
// ============================================================================

void USurvivorInteractionComponent::TryShowInteractionUI()
{
	if (!OwnerSurvivor.IsValid() || !OwnerSurvivor->HasAuthority()) return;

	const EInteractionMode HoverMode = EvaluateAvailableMode();
	if (HoverMode == EInteractionMode::None) return;

	// Create UI on the owning client (works for listen-server too).
	OwnerSurvivor->CreateActionProgressUI();

	// (Re)start the periodic progress push.
	auto& TM = GetWorld()->GetTimerManager();
	TM.ClearTimer(ActionProgressTimerHandle);
	TM.SetTimer(ActionProgressTimerHandle, this,
		&USurvivorInteractionComponent::Server_PushActionProgressUI,
		0.1f, /*bLoop=*/true);

	// Kick an immediate push.
	Server_PushActionProgressUI();
}

void USurvivorInteractionComponent::ClearInteractionUIAndTick()
{
	if (!OwnerSurvivor.IsValid()) return;

	GetWorld()->GetTimerManager().ClearTimer(ActionProgressTimerHandle);
	OwnerSurvivor->ClearActionProgressUI();
}

void USurvivorInteractionComponent::Server_PushActionProgressUI_Implementation()
{
	if (!OwnerSurvivor.IsValid()) return;

	float Value = 0.f;
	if (GetHoverProgress(Value))
	{
		// One-way push to the owning client (Character Client RPC).
		OwnerSurvivor->SetUIProgressValue(Value);
	}
}

bool USurvivorInteractionComponent::GetHoverProgress(float& OutValue) const
{
	OutValue = 0.f;

	switch (EvaluateAvailableMode())
	{
	case EInteractionMode::Repair:
		if (ActiveGenerator) { OutValue = ActiveGenerator->GetRepairPercent(); return true; }
		break;
	case EInteractionMode::Power:
		if (ActiveExitGateLever) { OutValue = ActiveExitGateLever->GetRepairPercent(); return true; }
		break;
	case EInteractionMode::HealOther:
		{
			if (HealingTarget.IsValid())
			{
				if (USurvivorHealthComponent* HC = HealingTarget->FindComponentByClass<USurvivorHealthComponent>())
				{
					OutValue = HC->GetCurrentProgressValue(); // shows their healing progress
					return true;
				}
			}
			break;
		}
	case EInteractionMode::Unhook:
		// Optional: you could push the hooked player's HookProgress here if desired.
		return false;
	case EInteractionMode::HealSelf:
		// Self-heal progress bar is handled by Character (self-state). No hover value here.
		return false;
	default: break;
	}

	return false;
}

// ============================================================================
// Interact routing (server)
// ============================================================================

void USurvivorInteractionComponent::Server_BeginInteract_Implementation()
{
	if (!OwnerSurvivor.IsValid()) return;

	// Don’t start two interactions at once.
	if (ActiveMode != EInteractionMode::None) return;

	switch (EvaluateAvailableMode())
	{
	case EInteractionMode::Repair:    BeginRepair();    break;
	case EInteractionMode::Power:     BeginPower();     break;
	case EInteractionMode::HealOther: BeginHealOther(); break;
	case EInteractionMode::Unhook:
		{
			if (ASurvivorCharacter* Hooked = ActiveHookedTarget.Get())
			{
				if (USurvivorHealthComponent* HC = Hooked->FindComponentByClass<USurvivorHealthComponent>())
				{
					HC->Server_StopHooked();
				}
				ActiveHookedTarget.Reset();
			}
			break;
		}
	case EInteractionMode::HealSelf:  BeginHealSelf();  break;
	default: break;
	}
}

void USurvivorInteractionComponent::Server_EndInteract_Implementation()
{
	if (!OwnerSurvivor.IsValid()) return;
	EndCurrentMode();
}

// ============================================================================
// Centralized begin/end helpers
// ============================================================================

void USurvivorInteractionComponent::SetActiveMode(EInteractionMode NewMode)
{
	ActiveMode = NewMode;

	// Lock movement for hold-type interactions.
	const bool bLockMove =
		ActiveMode == EInteractionMode::Repair ||
		ActiveMode == EInteractionMode::Power  ||
		ActiveMode == EInteractionMode::HealOther ||
		ActiveMode == EInteractionMode::HealSelf;

	if (bLockMove)
	{
		if (auto* CMC = OwnerSurvivor->GetCharacterMovement())
			CMC->SetMovementMode(MOVE_None);
	}
	else
	{
		if (auto* CMC = OwnerSurvivor->GetCharacterMovement())
			CMC->SetMovementMode(MOVE_Walking);
	}
}

void USurvivorInteractionComponent::EndCurrentMode()
{
	switch (ActiveMode)
	{
	case EInteractionMode::Repair:    EndRepair();    break;
	case EInteractionMode::Power:     EndPower();     break;
	case EInteractionMode::HealOther: EndHealOther(); break;
	case EInteractionMode::HealSelf:  EndHealSelf();  break;
	default: break;
	}

	SetActiveMode(EInteractionMode::None);
}

// ============================================================================
// Repair
// ============================================================================

void USurvivorInteractionComponent::BeginRepair()
{
	if (!OwnerSurvivor.IsValid() || !ActiveGenerator) return;

	OwnerSurvivor->bIsRepairing = true;
	if (OwnerSurvivor->SkillCheckComponent)
		OwnerSurvivor->SkillCheckComponent->SetSkillCheckTimer();

	OwnerSurvivor->CreateActionProgressUI();
	SetActiveMode(EInteractionMode::Repair);

	GetWorld()->GetTimerManager().SetTimer(RepairTimerHandle, this,
		&USurvivorInteractionComponent::TickRepair, 0.1f, true);
}

void USurvivorInteractionComponent::TickRepair()
{
	if (!OwnerSurvivor.IsValid()) return;

	if (AGenerator* Gen = ActiveGenerator)
	{
		if (!Gen->GetIsRepaired())
		{
			Gen->Repair(1.0f);
		}
		else
		{
			EndRepair();
		}
	}
}

void USurvivorInteractionComponent::EndRepair()
{
	if (!OwnerSurvivor.IsValid()) return;

	OwnerSurvivor->bIsRepairing = false;

	GetWorld()->GetTimerManager().ClearTimer(RepairTimerHandle);

	if (OwnerSurvivor->SkillCheckComponent)
	{
		OwnerSurvivor->SkillCheckComponent->ClearFailTimer();
		OwnerSurvivor->SkillCheckComponent->ClearSkillCheckTimer();
	}
	OwnerSurvivor->ClearSkillCheckUI();

	// Unlock movement
	if (auto* CMC = OwnerSurvivor->GetCharacterMovement())
		CMC->SetMovementMode(MOVE_Walking);
}

// ============================================================================
// Power Gates
// ============================================================================

void USurvivorInteractionComponent::BeginPower()
{
	if (!OwnerSurvivor.IsValid() || !ActiveExitGateLever) return;

	OwnerSurvivor->bIsPoweringGates = true;
	OwnerSurvivor->CreateActionProgressUI();
	SetActiveMode(EInteractionMode::Power);

	GetWorld()->GetTimerManager().SetTimer(PowerTimerHandle, this,
		&USurvivorInteractionComponent::TickPower, 0.1f, true);
}

void USurvivorInteractionComponent::TickPower()
{
	if (!OwnerSurvivor.IsValid()) return;

	if (AExitGateLever* Gate = ActiveExitGateLever)
	{
		if (!Gate->GetIsPowered())
		{
			Gate->Power(1.0f);
		}
		else
		{
			EndPower();
		}
	}
}

void USurvivorInteractionComponent::EndPower()
{
	if (!OwnerSurvivor.IsValid()) return;

	OwnerSurvivor->bIsPoweringGates = false;

	GetWorld()->GetTimerManager().ClearTimer(PowerTimerHandle);

	// Unlock movement
	if (auto* CMC = OwnerSurvivor->GetCharacterMovement())
		CMC->SetMovementMode(MOVE_Walking);
}

// ============================================================================
// Heal Other
// ============================================================================

void USurvivorInteractionComponent::BeginHealOther()
{
	if (!OwnerSurvivor.IsValid() || !HealingTarget.IsValid()) return;

	OwnerSurvivor->bIsHealingOther = true;

	if (ASurvivorCharacter* T = HealingTarget.Get())
	{
		if (T->GetHealthState() != EHealthState::Crawling)
			T->bIsBeingHealed = true;
	}

	OwnerSurvivor->CreateActionProgressUI();
	SetActiveMode(EInteractionMode::HealOther);

	GetWorld()->GetTimerManager().SetTimer(HealTimerHandle, this,
		&USurvivorInteractionComponent::TickHealOther, 0.25f, true);
}

void USurvivorInteractionComponent::TickHealOther()
{
	if (!OwnerSurvivor.IsValid()) return;

	if (ASurvivorCharacter* T = HealingTarget.Get())
	{
		if (OwnerSurvivor.IsValid() && OwnerSurvivor->HealthComponent)
		{
			OwnerSurvivor->HealthComponent->Server_HealTarget(T);
		}
	}
	else
	{
		EndHealOther();
	}
}

void USurvivorInteractionComponent::EndHealOther()
{
	if (!OwnerSurvivor.IsValid()) return;

	OwnerSurvivor->bIsHealingOther = false;

	if (ASurvivorCharacter* T = HealingTarget.Get())
	{
		T->bIsBeingHealed = false;
	}

	GetWorld()->GetTimerManager().ClearTimer(HealTimerHandle);

	// Unlock movement
	if (auto* CMC = OwnerSurvivor->GetCharacterMovement())
		CMC->SetMovementMode(MOVE_Walking);
}

// ============================================================================
// Heal Self
// ============================================================================

void USurvivorInteractionComponent::BeginHealSelf()
{
	if (!OwnerSurvivor.IsValid()) return;

	OwnerSurvivor->bIsBeingHealed = true;
	OwnerSurvivor->CreateActionProgressUI(); // optional: self-state could also drive UI
	SetActiveMode(EInteractionMode::HealSelf);

	GetWorld()->GetTimerManager().SetTimer(HealTimerHandle, this,
		&USurvivorInteractionComponent::TickHealSelf, 0.25f, true);
}

void USurvivorInteractionComponent::TickHealSelf()
{
	if (!OwnerSurvivor.IsValid()) return;
	if (OwnerSurvivor.IsValid() && OwnerSurvivor->HealthComponent)
	{
		OwnerSurvivor->HealthComponent->Server_HealSelf();
	}
}

void USurvivorInteractionComponent::EndHealSelf()
{
	if (!OwnerSurvivor.IsValid()) return;

	OwnerSurvivor->bIsBeingHealed = false;

	GetWorld()->GetTimerManager().ClearTimer(HealTimerHandle);

	// Unlock movement
	if (auto* CMC = OwnerSurvivor->GetCharacterMovement())
		CMC->SetMovementMode(MOVE_Walking);
}

// ============================================================================
// Capability checks
// ============================================================================

bool USurvivorInteractionComponent::IsCrawling() const
{
	return OwnerSurvivor.IsValid() && OwnerSurvivor->GetHealthState() == EHealthState::Crawling;
}

bool USurvivorInteractionComponent::CanPowerGates() const
{
	return OwnerSurvivor.IsValid() && OwnerSurvivor->GetCanPowerGates();
}

bool USurvivorInteractionComponent::CanInteractWithGenerator() const
{
	return OwnerSurvivor.IsValid()
	    && ActiveGenerator
	    && OwnerSurvivor->GetHealthState() != EHealthState::Crawling
	    && !ActiveGenerator->GetIsRepaired();
}

bool USurvivorInteractionComponent::CanInteractWithExitGate() const
{
	return OwnerSurvivor.IsValid()
	    && ActiveExitGateLever
	    && OwnerSurvivor->GetHealthState() != EHealthState::Crawling
	    && CanPowerGates()
	    && !ActiveExitGateLever->GetIsPowered();
}

bool USurvivorInteractionComponent::CanHealOther() const
{
	return OwnerSurvivor.IsValid()
	    && HealingTarget.IsValid()
	    && OwnerSurvivor->GetHealthState() != EHealthState::Crawling;
}

bool USurvivorInteractionComponent::CanUnhookOther() const
{
	return OwnerSurvivor.IsValid()
	    && ActiveHookedTarget.IsValid()
	    && ActiveHookedTarget->GetHealthState() == EHealthState::Hooked;
}

bool USurvivorInteractionComponent::CanSelfHeal() const
{
	return OwnerSurvivor.IsValid()
	    && OwnerSurvivor->GetHealthState() == EHealthState::Crawling;
}