#include "IntroKeycardTrigger.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogIntroKeycard, Log, All);

UIntroKeycardTrigger::UIntroKeycardTrigger()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UIntroKeycardTrigger::BeginPlay()
{
	Super::BeginPlay();

	// VR 템플릿의 GrabComponent (블루프린트 클래스 GrabComponent_C)를 같은 액터에서 찾는다
	if (AActor* Owner = GetOwner())
	{
		TInlineComponentArray<UActorComponent*> Comps;
		Owner->GetComponents(Comps);
		for (UActorComponent* C : Comps)
		{
			if (C && C->GetClass()->GetName().Contains(TEXT("GrabComponent")))
			{
				GrabComp = C;
				break;
			}
		}
	}

	if (!GrabComp)
	{
		UE_LOG(LogIntroKeycard, Warning, TEXT("GrabComponent를 찾지 못했습니다. 이 액터에 GrabComponent가 있어야 합니다."));
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(PollHandle, this, &UIntroKeycardTrigger::CheckGrabbed, PollInterval, true);
	}
}

void UIntroKeycardTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PollHandle);
		World->GetTimerManager().ClearTimer(LevelHandle);
	}
	Super::EndPlay(EndPlayReason);
}

bool UIntroKeycardTrigger::IsGrabComponentHeld() const
{
	if (!GrabComp)
	{
		return false;
	}
	const FBoolProperty* HeldProp = FindFProperty<FBoolProperty>(GrabComp->GetClass(), TEXT("bIsHeld"));
	if (!HeldProp)
	{
		return false;
	}
	return HeldProp->GetPropertyValue_InContainer(GrabComp);
}

void UIntroKeycardTrigger::CheckGrabbed()
{
	if (!bTriggered && IsGrabComponentHeld())
	{
		TriggerNow();
	}
}

void UIntroKeycardTrigger::TriggerNow()
{
	if (bTriggered)
	{
		return;
	}
	bTriggered = true;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	World->GetTimerManager().ClearTimer(PollHandle);

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, FadeTime, FLinearColor::Black, true, true);
		}
	}

	UE_LOG(LogIntroKeycard, Log, TEXT("카드를 잡음 -> %s 로 이동"), *NextLevelName.ToString());
	World->GetTimerManager().SetTimer(LevelHandle, this, &UIntroKeycardTrigger::OpenNextLevel, FMath::Max(FadeTime, 0.01f), false);
}

void UIntroKeycardTrigger::OpenNextLevel()
{
	UGameplayStatics::OpenLevel(this, NextLevelName);
}
