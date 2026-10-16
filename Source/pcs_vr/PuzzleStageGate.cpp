#include "PuzzleStageGate.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "PuzzleDoor.h"
#include "PuzzleUtils.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogStageGate, Log, All);

APuzzleStageGate::APuzzleStageGate()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void APuzzleStageGate::Unlock()
{
	bLocked = false;
}

void APuzzleStageGate::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FVector ViewLoc;
	FRotator ViewRot;
	if (bLocked || bLeaving || !PuzzleUtils::GetPlayerView(this, ViewLoc, ViewRot))
	{
		return;
	}

	// 가까이 오면 문이 열리고, 멀어지면 닫힌다
	if (Door)
	{
		const bool bNear = FVector::Dist2D(ViewLoc, GetActorLocation()) < OpenRadius;
		if (bNear && !Door->IsOpen())
		{
			Door->Open();
		}
		else if (!bNear && Door->IsOpen())
		{
			Door->Close();
		}
	}

	// 문 너머로 들어가면 다음 스테이지
	const FVector Local = GetActorTransform().InverseTransformPosition(ViewLoc);
	if (Local.X > 20.f && Local.X < TriggerDepth && FMath::Abs(Local.Y) < TriggerHalfWidth && Local.Z > 0.f && Local.Z < 260.f)
	{
		bLeaving = true;
		UE_LOG(LogStageGate, Log, TEXT("%s: 보안문 통과 → %s"), *GetName(), *NextLevel.ToString());
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			if (PC->PlayerCameraManager)
			{
				PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, 0.6f, FLinearColor::Black, true, true);
			}
		}
		GetWorldTimerManager().SetTimer(LevelHandle, this, &APuzzleStageGate::OpenNextLevel, 0.8f, false);
	}
}

void APuzzleStageGate::OpenNextLevel()
{
	UGameplayStatics::OpenLevel(this, NextLevel);
}
