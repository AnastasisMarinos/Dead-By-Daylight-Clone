// (C) Anastasis Marinos 2025 //

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractionInterface.generated.h"

class APlayerCharacter;


UENUM()
enum class EInteractableType : uint8
{
	Item UMETA(DisplayName = "Item"),
	NPC UMETA(DisplayName = "NPC"),
	Vehicle UMETA(DisplayName = "Vehicle"),
	Storage UMETA(DisplayName = "Storage"),
	Environment UMETA(DisplayName = "Environment")
};

USTRUCT()
struct FInteractableData
{
	GENERATED_USTRUCT_BODY()

	FInteractableData() :
	InteractableType(EInteractableType::Item),
	Name(FText::GetEmpty()),
	Action(FText::GetEmpty()),
	Quantity(0),
	InteractionDuration(0.0f) { };
	
	UPROPERTY(EditInstanceOnly)
	EInteractableType InteractableType;

	UPROPERTY(EditInstanceOnly)
	FText Name;

	UPROPERTY(EditInstanceOnly)
	FText Action;

	// Used only for pickups.
	UPROPERTY(EditInstanceOnly)
	int32 Quantity; // int32 so that it can be converted to blueprints

	// Used for actions that require an action timer.
	UPROPERTY(EditInstanceOnly)
	float InteractionDuration;
};


// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UInteractionInterface : public UInterface
{
	GENERATED_BODY()
};

class GAMETEMPLATE_API IInteractionInterface
{
	GENERATED_BODY()
	
public:
	// FUNCTIONS //
	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
	virtual void BeginFocus();
	virtual void EndFocus();
	virtual void BeginInteract();
	virtual void EndInteract();
	virtual void Interact(APlayerCharacter* PlayerCharacter);

	FInteractableData InteractableData;
};
