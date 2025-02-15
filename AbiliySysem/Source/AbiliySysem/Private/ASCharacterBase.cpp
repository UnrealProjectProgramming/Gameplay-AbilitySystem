// Fill out your copyright notice in the Description page of Project Settings.

#include "ASCharacterBase.h"

#include "Abilities/GameplayAbility.h"
#include "ASAttributeSetBase.h"
#include "AIController.h"
#include "GameFramework/PlayerController.h"
#include "BrainComponent.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Actor.h"
#include "ASPlayerControllerBase.h"
#include "GameplayAbilityBase.h"


// Sets default values
AASCharacterBase::AASCharacterBase()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	AbilitySystemComp = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComp"));
	AttributeSetBaseComp = CreateDefaultSubobject<UASAttributeSetBase>(TEXT("AttributeSetBaseComp"));
	bHasDied = false;
	TeamID = 255;
}

// Called when the game starts or when spawned
void AASCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// Subscribing to OnHealthChangedDelegate
	if (AttributeSetBaseComp)
	{
		AttributeSetBaseComp->OnHealthChange.AddDynamic(this, &AASCharacterBase::OnHealthChanged);
		AttributeSetBaseComp->OnManaChange.AddDynamic(this, &AASCharacterBase::OnManaChanged);
		AttributeSetBaseComp->OnStrengthChange.AddDynamic(this, &AASCharacterBase::OnStrengthChanged);
	}
	AutoDetermineTeamIDByControllerType();
	AddGameplayTag(FullHealthTag);
}

// Called every frame
void AASCharacterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AASCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

UAbilitySystemComponent* AASCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComp;
}

void AASCharacterBase::AquireAbility(TSubclassOf<UGameplayAbility> AbilityToAquire)
{
	if (AbilitySystemComp)
	{
		if (HasAuthority() && AbilityToAquire)
		{
			AbilitySystemComp->GiveAbility(FGameplayAbilitySpec(AbilityToAquire, 1, 0));
		}
		AbilitySystemComp->InitAbilityActorInfo(this, this);
	}
}

void AASCharacterBase::AquireAbilities(TArray<TSubclassOf<class UGameplayAbility>> AbilitiesToAquire)
{
	for (auto AbilityToAquire : AbilitiesToAquire)
	{
		AquireAbility(AbilityToAquire);
		if (AbilityToAquire->IsChildOf(UGameplayAbilityBase::StaticClass()))
		{
			TSubclassOf<UGameplayAbilityBase> AbilityBaseClass = AbilityToAquire.Get(); // Dereference back into the class , we can allso write it *AbilityToAquire
			if (AbilityBaseClass)
			{
				AddAbilityToUI(AbilityBaseClass);
			}
		}
	}
}

bool AASCharacterBase::IsOtherHostile(AASCharacterBase* Other)
{
	return TeamID != Other->TeamID;
}

void AASCharacterBase::AddGameplayTag(FGameplayTag& TagToAdd)
{
	GetAbilitySystemComponent()->AddLooseGameplayTag(TagToAdd);
	GetAbilitySystemComponent()->SetTagMapCount(TagToAdd, 1);
}

void AASCharacterBase::RemoveGameplayTag(FGameplayTag& TagToRemove)
{
	GetAbilitySystemComponent()->RemoveLooseGameplayTag(TagToRemove);
}

void AASCharacterBase::PushCharacter(FVector ImpulseDirection, float ImpulseeStrength, float StunDuration)
{
	UCharacterMovementComponent* CharacterMovementComp = GetCharacterMovement();
	if (CharacterMovementComp)
	{
		auto DefaultGroundFriction = CharacterMovementComp->GroundFriction;
		CharacterMovementComp->GroundFriction = 0;
		CharacterMovementComp->AddImpulse(ImpulseDirection * ImpulseeStrength, true);

		Stun_TimerDelegate.BindUFunction(this, "Stun", StunDuration);
		GetWorldTimerManager().SetTimer(FrictionReenableDelay_TimeHandle, Stun_TimerDelegate, StunDuration, false);

		CharacterMovementComp->GroundFriction = DefaultGroundFriction;
	}

}


void AASCharacterBase::Stun(float StunTime)
{
	DisableInputControl();
	GetWorldTimerManager().SetTimer(Stun_TimeHandle, this, &AASCharacterBase::EnableInputControl, StunTime, false);
}


void AASCharacterBase::ApplyGESpecHandleToTargetDataSpecHandle(const FGameplayEffectSpecHandle& GESpecHandle, const FGameplayAbilityTargetDataHandle& TargetDataHandle)
{
	for (TSharedPtr<FGameplayAbilityTargetData> Data : TargetDataHandle.Data)
	{
		Data->ApplyGameplayEffectSpec(*GESpecHandle.Data.Get());
	}
}

void AASCharacterBase::OnHealthChanged(float Health, float MaxHealth)
{
	if (Health <= 0 && !bHasDied)
	{
		bHasDied = true;
		Dead();
		BP_StartDyingSequence();
	}
	BP_OnHealthChanged(Health, MaxHealth);
}

void AASCharacterBase::OnManaChanged(float Mana, float MaxMana)
{
	BP_OnManaChanged(Mana, MaxMana);
}

void AASCharacterBase::OnStrengthChanged(float Strength, float MaxStrength)
{
	BP_OnStrengthChanged(Strength, MaxStrength);
}

void AASCharacterBase::AutoDetermineTeamIDByControllerType()
{
	if (GetController() && GetController()->IsPlayerController())
	{
		TeamID = 0;
	}
}

void AASCharacterBase::Dead()
{
	DisableInputControl();
}

void AASCharacterBase::DisableInputControl()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC)
	{
		PC->DisableInput(PC);
	}
	AAIController* AIC = Cast<AAIController>(GetController());
	if (AIC)
	{
		AIC->GetBrainComponent()->StopLogic("Dead");
	}
}

void AASCharacterBase::EnableInputControl()
{
	if (!bHasDied)
	{
		APlayerController* PC = Cast<APlayerController>(GetController());
		if (PC)
		{
			PC->EnableInput(PC);
		}
		AAIController* AIC = Cast<AAIController>(GetController());
		if (AIC)
		{
			AIC->GetBrainComponent()->RestartLogic();
		}
	}
}

void AASCharacterBase::AddAbilityToUI(TSubclassOf<UGameplayAbilityBase> AbilityToAdd)
{
	AASPlayerControllerBase* PlayerControllerBase = Cast<AASPlayerControllerBase>(GetController());
	if (PlayerControllerBase)
	{
		UGameplayAbilityBase* AbilityInstance = AbilityToAdd.Get()->GetDefaultObject<UGameplayAbilityBase>();
		if (AbilityInstance)
		{
			FGamePlayAbilityInfo AbilityInfo = AbilityInstance->GetAbilityInfo();
			PlayerControllerBase->AddAbilityToUI(AbilityInfo);
		}
	}
}
