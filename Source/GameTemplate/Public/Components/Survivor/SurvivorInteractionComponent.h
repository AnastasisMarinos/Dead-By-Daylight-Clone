// © Anastasis Marinos 2025 //

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SurvivorInteractionComponent.generated.h"

class ASurvivorCharacter;
class AGenerator;
class AExitGateLever;

// What the player can/does interact with right now.
UENUM(BlueprintType)
enum class EInteractionMode : uint8
{
	None      UMETA(DisplayName="None"),
	Repair    UMETA(DisplayName="Repair Generator"),
	Power     UMETA(DisplayName="Power Exit Gate"),
	HealOther UMETA(DisplayName="Heal Other"),
	HealSelf  UMETA(DisplayName="Heal Self"),
	Unhook    UMETA(DisplayName="Unhook")
};

// USurvivorInteractionComponent
// -----------------------------------------------------------------------------
// Server-owned interaction orchestrator that:
// - Tracks nearby interactables via overlaps.
// - Decides available & active interaction mode.
// - Starts/stops interaction timers and movement lock.
// - Pushes progress to the owning client (character Client RPC).
//
// Character focuses on replicated flags + UI RPCs.

UCLASS(ClassGroup=(Player), meta=(BlueprintSpawnableComponent))
class GAMETEMPLATE_API USurvivorInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USurvivorInteractionComponent();

	// Bind overlaps; safe to call multiple times (idempotent).
	UFUNCTION(BlueprintCallable, Category="Interaction")
	void Initialize();

	// Input entry points (from Character). Server only.
	UFUNCTION(Server, Reliable) void Server_BeginInteract();
	UFUNCTION(Server, Reliable) void Server_EndInteract();

	// Read access for Character/skill-check.
	FORCEINLINE AGenerator*         GetActiveGenerator()      const { return ActiveGenerator; }
	FORCEINLINE AExitGateLever*     GetActiveExitGateLever()  const { return ActiveExitGateLever; }
	FORCEINLINE ASurvivorCharacter* GetHealingTarget()        const { return HealingTarget.Get(); }
	FORCEINLINE ASurvivorCharacter* GetActiveHookedTarget()   const { return ActiveHookedTarget.Get(); }

	// Current active mode (server truth).
	FORCEINLINE EInteractionMode    GetActiveMode()           const { return ActiveMode; }

protected:
	virtual void BeginPlay() override;

	// Overlaps (server)
	UFUNCTION() void OnCapsuleBeginOverlap(UPrimitiveComponent* Overlapped, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION() void OnCapsuleEndOverlap  (UPrimitiveComponent* Overlapped, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
	// Owner & Timers
	TWeakObjectPtr<ASurvivorCharacter> OwnerSurvivor;

	FTimerHandle RepairTimerHandle;
	FTimerHandle PowerTimerHandle;
	FTimerHandle HealTimerHandle;
	FTimerHandle ActionProgressTimerHandle;

	// Nearby interactables (GC-safe; not replicated)
	UPROPERTY() TObjectPtr<AGenerator>     ActiveGenerator = nullptr;
	UPROPERTY() TObjectPtr<AExitGateLever> ActiveExitGateLever = nullptr;
	UPROPERTY() TWeakObjectPtr<ASurvivorCharacter> HealingTarget;
	UPROPERTY() TWeakObjectPtr<ASurvivorCharacter> ActiveHookedTarget;

	// Modes
	EInteractionMode ActiveMode = EInteractionMode::None;

	// Choose best available hover mode by proximity/state.
	EInteractionMode EvaluateAvailableMode() const;

	// Capability checks (server)
	bool IsCrawling() const;
	bool CanPowerGates() const;

	bool CanInteractWithGenerator() const;
	bool CanInteractWithExitGate()  const;
	bool CanHealOther()             const;
	bool CanUnhookOther()           const;
	bool CanSelfHeal()              const;

	// UI routing (server triggers client UI)
	void TryShowInteractionUI();
	void ClearInteractionUIAndTick();

	// Periodic server push of 0..1 progress to the owning client.
	UFUNCTION(Server, Unreliable) void Server_PushActionProgressUI();

	// Compute 0..1 progress for the current hover context.
	bool GetHoverProgress(float& OutValue) const;

	// Interaction flows (server only)
	void BeginRepair();    void TickRepair();    void EndRepair();
	void BeginPower();     void TickPower();     void EndPower();
	void BeginHealOther(); void TickHealOther(); void EndHealOther();
	void BeginHealSelf();  void TickHealSelf();  void EndHealSelf();

	// Centralized begin/end helpers
	void SetActiveMode(EInteractionMode NewMode);
	void EndCurrentMode();
};