// © Anastasis Marinos 2025 //

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SurvivorHealthComponent.generated.h"

class AHook;
class AKillerCharacter;
class ASurvivorCharacter;

// High-level health state of the survivor (replicated).
UENUM(BlueprintType)
enum class EHealthState : uint8
{
	Healthy,
	Injured,
	Crawling,
	Carried,
	Hooked
};

// Sub-state while hooked (replicated).
UENUM(BlueprintType)
enum class EHookState : uint8
{
	Unhook,
	Hooked,
	Struggling
};

// Event used by the Character to react visually (attach to killer/hook, enable/disable movement, etc.).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHealthStateChanged, EHealthState, NewState);

// USurvivorHealthComponent
// -----------------------------------------------------------------------------
// Authoritative health/afflictions component for Survivors.
//
// Responsibilities
// - Replicates health/hook-related state and progress (healing, wiggle, hook timer).
// - Starts/stops timers for hook damage and UI pushes.
// - Notifies owning character to create/clear/update action progress UI.
//
// Notes
// - UI mirrors USurvivorInteractionComponent: server decides when to show/clear,
//   and periodically pushes a float progress to the owning client.
// - Character handles visuals/attachments via OnHealthStateChanged.

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class GAMETEMPLATE_API USurvivorHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USurvivorHealthComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Returns current 0..1 progress based on HealthState (hook/wiggle/heal).
	float GetCurrentProgressValue() const;

	// Accessors
	UFUNCTION(BlueprintPure, Category="Health") EHealthState      GetHealthState()    const { return HealthState; }
	UFUNCTION(BlueprintPure, Category="Health") EHookState        GetHookState()      const { return HookedState; }
	UFUNCTION(BlueprintPure, Category="Health") float             GetHealingProgress()const { return HealingProgress; }
	UFUNCTION(BlueprintPure, Category="Health") float             GetWiggleProgress() const { return WiggleProgress; }
	UFUNCTION(BlueprintPure, Category="Health") float             GetHookProgress()   const { return HookProgress; }
	UFUNCTION(BlueprintPure, Category="Health") int32             GetHookCount()      const { return HookCount; }
	UFUNCTION(BlueprintPure, Category="Health") AKillerCharacter* GetActiveKiller()  const { return ActiveKiller; }
	UFUNCTION(BlueprintPure, Category="Health") AHook*            GetActiveHook()    const { return ActiveHook; }

	// Gameplay API (Server)
	UFUNCTION(Server, Reliable) void Server_SurvivorGetHit();

	UFUNCTION(Server, Reliable) void Server_StartInjured();
	UFUNCTION(Server, Reliable) void Server_StartCrawling();
	UFUNCTION(Server, Reliable) void Server_StopCrawling();

	UFUNCTION(Server, Reliable) void Server_StartCarried(AKillerCharacter* Killer);
	UFUNCTION(Server, Reliable) void Server_StopCarried();
	UFUNCTION(Server, Reliable) void Server_Wiggle();

	UFUNCTION(Server, Reliable) void Server_StartHooked(AHook* Hook);
	UFUNCTION(Server, Reliable) void Server_StopHooked();

	UFUNCTION(Server, Reliable) void Server_HealTarget(ASurvivorCharacter* Target);
	UFUNCTION(Server, Reliable) void Server_HealSelf();

	// Server asks the owner to sync the current progress UI immediately.
	void PushProgressToOwnerUI();

	// Character binds to this for visual reactions.
	UPROPERTY(BlueprintAssignable) FOnHealthStateChanged OnHealthStateChanged;

protected:
	virtual void BeginPlay() override;

	// RepNotifies
	UFUNCTION() void OnRep_HealthState();
	UFUNCTION() void OnRep_HookState();
	UFUNCTION() void OnRep_HealingProgress();
	UFUNCTION() void OnRep_WiggleProgress();
	UFUNCTION() void OnRep_HookProgress();

private:
	// Internal state setters (server)
	void SetHealthState(EHealthState NewState);
	void SetHookState(EHookState NewHook);

	// UI helpers (server drives creation/clearing and periodic pushes)
	void EmitProgressValue();         // calls PushProgressToOwnerUI()
	void UpdateHealthUIForState();    // decides show/hide
	void ShowHealthUI();              // create UI + start tick
	void HideHealthUI();              // clear UI + stop tick

	UFUNCTION(Server, Unreliable) void Server_PushHealthProgressUI();

	// Hook damage timer (server)
	void StartHookDamageTimer();
	void StopHookDamageTimer();
	void TickHookDamage();

	// Shared
	bool HasAuthority() const { return GetOwner() && GetOwner()->HasAuthority(); }
	void HandleDeath();

	// Replicated State
	UPROPERTY(ReplicatedUsing=OnRep_HealthState, VisibleInstanceOnly, Category="Health")
	EHealthState HealthState = EHealthState::Healthy;

	UPROPERTY(ReplicatedUsing=OnRep_HookState, VisibleInstanceOnly, Category="Health")
	EHookState HookedState = EHookState::Unhook;

	UPROPERTY(ReplicatedUsing=OnRep_HealingProgress, VisibleInstanceOnly, Category="Health")
	float HealingProgress = 0.0f;

	UPROPERTY(ReplicatedUsing=OnRep_WiggleProgress, VisibleInstanceOnly, Category="Health")
	float WiggleProgress = 0.0f;

	UPROPERTY(ReplicatedUsing=OnRep_HookProgress, VisibleInstanceOnly, Category="Health")
	float HookProgress = 1.0f;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category="Health")
	int32 HookCount = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category="Health")
	AKillerCharacter* ActiveKiller = nullptr;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category="Health")
	AHook* ActiveHook = nullptr;

	// Timers (server)
	FTimerHandle HookHealthTimer;
	FTimerHandle HealthProgressTimerHandle;

	// Cached owner
	UPROPERTY() ASurvivorCharacter* OwnerSurvivor = nullptr;
};