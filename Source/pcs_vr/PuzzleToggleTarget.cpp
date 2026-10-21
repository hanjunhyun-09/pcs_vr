#include "PuzzleToggleTarget.h"

#include "Components/LightComponent.h"
#include "PuzzleSign.h"

DEFINE_LOG_CATEGORY_STATIC(LogToggleTarget, Log, All);

APuzzleToggleTarget::APuzzleToggleTarget()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void APuzzleToggleTarget::BeginPlay()
{
	Super::BeginPlay();
	bOn = bStartOn;
	SpinRate = bOn ? SpinSpeed : 0.f;
	Apply();
}

void APuzzleToggleTarget::Toggle()
{
	SetOn(!bOn);
}

void APuzzleToggleTarget::SetOn(bool bNewOn, bool bNotify)
{
	if (bOn == bNewOn)
	{
		return;
	}
	bOn = bNewOn;
	UE_LOG(LogToggleTarget, Log, TEXT("%s %s"), DisplayName.IsEmpty() ? *GetName() : *DisplayName, bOn ? TEXT("켜짐") : TEXT("꺼짐"));
	Apply();
	if (bNotify)
	{
		OnToggled.Broadcast(this, bOn);
	}
}

void APuzzleToggleTarget::Apply()
{
	for (AActor* LightActor : Lights)
	{
		if (!LightActor)
		{
			continue;
		}
		TInlineComponentArray<ULightComponent*> LightComps;
		LightActor->GetComponents(LightComps);
		for (ULightComponent* L : LightComps)
		{
			L->SetVisibility(bOn);
		}
	}
	for (AActor* Shown : ShowWhenOn)
	{
		if (APuzzleSign* Sign = Cast<APuzzleSign>(Shown))
		{
			Sign->SetSignVisible(bOn);
		}
		else if (Shown)
		{
			Shown->SetActorHiddenInGame(!bOn);
		}
	}
}

void APuzzleToggleTarget::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 환풍기처럼 서서히 빨라지고 서서히 멈춘다
	const float Target = bOn ? SpinSpeed : 0.f;
	SpinRate = FMath::FInterpConstantTo(SpinRate, Target, DeltaTime, SpinSpeed / SpinUpTime);
	if (SpinRate <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	const FQuat Step(GetActorForwardVector(), FMath::DegreesToRadians(SpinRate * DeltaTime));
	for (AActor* Spinner : SpinWhenOn)
	{
		if (Spinner)
		{
			Spinner->AddActorWorldRotation(Step);
		}
	}
}
