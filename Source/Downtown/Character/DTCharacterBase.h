#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DTCharacterBase.generated.h"

class UDTHealthComponent;
class UDTCrimeData;

/** Shared base for the player, pedestrians and police: health, damage, death and ragdoll. */
UCLASS(Abstract)
class DOWNTOWN_API ADTCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	ADTCharacterBase();

	UFUNCTION(BlueprintPure, Category = "Character")
	UDTHealthComponent* GetHealthComponent() const { return HealthComponent; }

	UFUNCTION(BlueprintPure, Category = "Character")
	bool IsDead() const;

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	virtual void HandleDeath(AActor* Killer);

	/** Blueprint hook for death sounds, dropping loot, etc. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Character")
	void OnDied(AActor* Killer);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDTHealthComponent> HealthComponent;

	/** Reported to the wanted system when the player kills this character (DA_Crime_Murder, DA_Crime_CopKill). */
	UPROPERTY(EditDefaultsOnly, Category = "Wanted")
	TObjectPtr<UDTCrimeData> DeathCrime;

	/** Seconds before the corpse is removed. 0 = never. */
	UPROPERTY(EditDefaultsOnly, Category = "Character")
	float CorpseLifeSpan = 30.f;
};
