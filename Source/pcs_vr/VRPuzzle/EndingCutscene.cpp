#include "EndingCutscene.h"

#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "PuzzleSign.h"
#include "StereoRendering.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogEnding, Log, All);

AEndingCutscene::AEndingCutscene()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AEndingCutscene::BeginPlay()
{
	Super::BeginPlay();

	// 쪽지·크레딧은 자기 장면이 올 때까지 숨겨 둔다
	for (const FEndingShot& Shot : Shots)
	{
		if (Shot.Sign)
		{
			Shot.Sign->SetSignVisible(false);
		}
	}

	// 엘리베이터에서 검은 화면으로 넘어왔으니 검은 화면을 유지한 채 시작
	Fade(1.f, 1.f, 0.f, FLinearColor::Black, true);
	GetWorldTimerManager().SetTimer(StartHandle, this, &AEndingCutscene::StartCutscene, FMath::Max(StartDelay, 0.01f), false);
}

bool AEndingCutscene::IsHeadsetActive()
{
	// 헤드셋 런타임(Meta Horizon 등)만 깔려 있고 헤드셋이 없으면 XRSystem은 있어도 스테레오 렌더링은 꺼져 있다
	return GEngine && GEngine->StereoRenderingDevice.IsValid() && GEngine->StereoRenderingDevice->IsStereoEnabled();
}

void AEndingCutscene::StartCutscene()
{
	if (bStarted)
	{
		return;
	}
	bStarted = true;
	bHeadset = IsHeadsetActive();

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			// 손·레이저가 장면에 보이지 않게
			Pawn->DisableInput(PC);
			Pawn->SetActorHiddenInGame(true);
		}
	}

	UE_LOG(LogEnding, Log, TEXT("엔딩 컷신 시작: 장면 %d개 (%s)"), Shots.Num(), bHeadset ? TEXT("VR") : TEXT("데스크톱"));
	PlayShot(0);
}

void AEndingCutscene::PlayShot(int32 Index)
{
	if (!Shots.IsValidIndex(Index))
	{
		FinishCutscene();
		return;
	}

	ShotIndex = Index;
	ShotTime = 0.f;
	PageIndex = INDEX_NONE;
	const FEndingShot& Shot = Shots[Index];

	if (Shot.Camera)
	{
		CameraStart = Shot.Camera->GetActorTransform();
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			PC->SetViewTargetWithBlend(Shot.Camera, 0.f);
		}
	}
	if (Shot.Sign)
	{
		Shot.Sign->SetSignVisible(true);
	}
	if (Shot.MoveActor)
	{
		MoveStart = Shot.MoveActor->GetActorLocation();
	}

	Fade(1.f, 0.f, Shot.FadeIn, Shot.FadeColor, false);
	SetActorTickEnabled(true);
	UE_LOG(LogEnding, Log, TEXT("엔딩 장면 %d/%d: %s (%.1f초)"), Index + 1, Shots.Num(),
		Shot.Camera ? *Shot.Camera->GetName() : TEXT("카메라 없음"), Shot.Duration);

	const float FadeOutAt = FMath::Max(Shot.Duration - Shot.FadeOut, 0.01f);
	GetWorldTimerManager().SetTimer(FadeHandle, this, &AEndingCutscene::FadeOutShot, FadeOutAt, false);
	GetWorldTimerManager().SetTimer(NextHandle, FTimerDelegate::CreateUObject(this, &AEndingCutscene::PlayShot, Index + 1), Shot.Duration, false);
}

void AEndingCutscene::FadeOutShot()
{
	if (Shots.IsValidIndex(ShotIndex))
	{
		const FEndingShot& Shot = Shots[ShotIndex];
		Fade(0.f, 1.f, Shot.FadeOut, Shot.FadeColor, true);
	}
}

void AEndingCutscene::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!Shots.IsValidIndex(ShotIndex))
	{
		return;
	}
	const FEndingShot& Shot = Shots[ShotIndex];
	ShotTime += DeltaTime;
	const float Alpha = FMath::Clamp(ShotTime / Shot.Duration, 0.f, 1.f);
	const float Eased = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f);

	// 데스크톱에서만 카메라를 장면 시작 자리 → EndCamera 자리로 천천히 옮긴다
	if (!bHeadset && Shot.Camera && Shot.EndCamera)
	{
		const FTransform To = Shot.EndCamera->GetActorTransform();
		const FVector Loc = FMath::Lerp(CameraStart.GetLocation(), To.GetLocation(), Eased);
		const FRotator Rot = FQuat::Slerp(CameraStart.GetRotation(), To.GetRotation(), Eased).Rotator();
		Shot.Camera->SetActorLocationAndRotation(Loc, Rot);
	}

	if (Shot.MoveActor)
	{
		Shot.MoveActor->SetActorLocation(MoveStart + Shot.MoveOffset * Eased);
	}

	if (Shot.Sign && Shot.Pages.Num() > 0)
	{
		const int32 NewPage = FMath::Min(FMath::FloorToInt(Alpha * Shot.Pages.Num()), Shot.Pages.Num() - 1);
		if (NewPage != PageIndex)
		{
			PageIndex = NewPage;
			Shot.Sign->SetSignText(Shot.Pages[NewPage]);
		}
	}
}

void AEndingCutscene::FinishCutscene()
{
	SetActorTickEnabled(false);
	UE_LOG(LogEnding, Log, TEXT("엔딩 컷신 끝 → %s"), *NextLevel.ToString());
	Fade(1.f, 1.f, 0.f, FLinearColor::Black, true);
	GetWorldTimerManager().SetTimer(NextHandle, this, &AEndingCutscene::OpenNextLevel, 1.f, false);
}

void AEndingCutscene::OpenNextLevel()
{
	UGameplayStatics::OpenLevel(this, NextLevel);
}

void AEndingCutscene::Fade(float From, float To, float Time, const FLinearColor& Color, bool bHold)
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(From, To, FMath::Max(Time, 0.01f), Color, false, bHold);
		}
	}
}
