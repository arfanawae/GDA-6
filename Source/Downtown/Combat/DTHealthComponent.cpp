#include "Combat/DTHealthComponent.h"

UDTHealthComponent::UDTHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDTHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = MaxHealth;
}

void UDTHealthComponent::ApplyDamage(float Amount, AActor* DamageInstigator)
{
	if (Amount <= 0.f || IsDead())
	{
		return;
	}

	const float OldHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth - Amount, 0.f, MaxHealth);
	OnHealthChanged.Broadcast(this, CurrentHealth, CurrentHealth - OldHealth);

	if (IsDead())
	{
		OnDeath.Broadcast(DamageInstigator);
	}
}

void UDTHealthComponent::Heal(float Amount)
{
	if (Amount <= 0.f || IsDead())
	{
		return;
	}

	const float OldHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth + Amount, 0.f, MaxHealth);
	OnHealthChanged.Broadcast(this, CurrentHealth, CurrentHealth - OldHealth);
}
