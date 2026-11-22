#pragma once

#include "CoreMinimal.h"
#include "Character/DTCharacterBase.h"
#include "DTPoliceCharacter.generated.h"

class UDTWeaponComponent;

/** Police officer on foot. BP_PoliceOfficer sets mesh, StartingWeapons and DeathCrime (DA_Crime_CopKill). */
UCLASS()
class DOWNTOWN_API ADTPoliceCharacter : public ADTCharacterBase
{
	GENERATED_BODY()

public:
	ADTPoliceCharacter();

	UFUNCTION(BlueprintPure, Category = "Police")
	UDTWeaponComponent* GetWeaponComponent() const { return WeaponComponent; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDTWeaponComponent> WeaponComponent;
};
