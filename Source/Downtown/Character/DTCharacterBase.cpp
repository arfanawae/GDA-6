#include "Character/DTCharacterBase.h"
#include "Combat/DTHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Wanted/DTWantedSubsystem.h"

ADTCharacterBase::ADTCharacterBase()
{
	HealthComponent = CreateDefaultSubobject<UDTHealthComponent>(TEXT("Health"));
}

void ADTCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	HealthComponent->OnDeath.AddDynamic(this, &ADTCharacterBase::HandleDeath);
}

bool ADTCharacterBase::IsDead() const
{
	return HealthComponent->IsDead();
}

float ADTCharacterBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float Actual = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	AActor* Killer = EventInstigator && EventInstigator->GetPawn() ? EventInstigator->GetPawn() : DamageCauser;
	HealthComponent->ApplyDamage(Actual, Killer);
	return Actual;
}

void ADTCharacterBase::HandleDeath(AActor* Killer)
{
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	GetMesh()->SetSimulatePhysics(true);

	if (DeathCrime)
	{
		if (UDTWantedSubsystem* Wanted = GetWorld()->GetSubsystem<UDTWantedSubsystem>())
		{
			Wanted->ReportCrime(DeathCrime, GetActorLocation(), Killer); // ignored unless Killer is the player
		}
	}

	OnDied(Killer);

	if (!IsPlayerControlled())
	{
		DetachFromControllerPendingDestroy();
		if (CorpseLifeSpan > 0.f)
		{
			SetLifeSpan(CorpseLifeSpan);
		}
	}
}
