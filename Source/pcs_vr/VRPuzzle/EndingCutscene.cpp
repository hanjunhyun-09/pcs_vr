#include "EndingCutscene.h"

#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "PuzzleSign.h"
#include "StereoRendering.h"
#include "TimerManager.h"
#include "UnrealClient.h"

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


namespace
{
	/** 셰이더 컴파일이 끝나거나 5분이 지나면 Action을 부른다 (처음 -game으로 켜면 셰이더가 수천 개 남아 있을 수 있음) */
	void WhenShadersReadyOrTimeout(TWeakObjectPtr<UWorld> WeakWorld, double WaitStart, TFunction<void()> Action)
	{
		UWorld* World = WeakWorld.Get();
		if (!World)
		{
			return;
		}
		int32 Remaining = 0;
#if WITH_EDITOR
		Remaining = GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0;
#endif
		FTimerHandle Handle;
		if (Remaining > 0 && FPlatformTime::Seconds() - WaitStart < 300.0)
		{
			UE_LOG(LogEnding, Log, TEXT("셰이더 컴파일 기다리는 중 (%d개 남음)"), Remaining);
			World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(World, [WeakWorld, WaitStart, Action]()
			{
				WhenShadersReadyOrTimeout(WeakWorld, WaitStart, Action);
			}), 2.f, false);
			return;
		}
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(World, [Action]() { Action(); }), 0.5f, false);
	}

	/** 레벨의 엔딩 컷신을 찾아 진행을 멈춘다 (화면 페이드도 걷어 낸다) */
	AEndingCutscene* StopCutsceneForShots(UWorld* World)
	{
		AEndingCutscene* Cutscene = nullptr;
		for (TActorIterator<AEndingCutscene> It(World); It; ++It)
		{
			Cutscene = *It;
			break;
		}
		if (!Cutscene)
		{
			UE_LOG(LogEnding, Warning, TEXT("엔딩 컷신을 찾지 못했습니다 (L_06_Ending에서 실행)"));
			return nullptr;
		}
		World->GetTimerManager().ClearAllTimersForObject(Cutscene);
		Cutscene->SetActorTickEnabled(false);
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
		{
			if (PC->PlayerCameraManager)
			{
				PC->PlayerCameraManager->StopCameraFade();
			}
		}
		return Cutscene;
	}

	void ViewFrom(UWorld* World, ACameraActor* Camera)
	{
		APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
		if (PC && Camera)
		{
			PC->SetViewTargetWithBlend(Camera, 0.f);
		}
	}

	void Shoot(const FString& Name)
	{
		FScreenshotRequest::RequestScreenshot(Name, false, false);
		UE_LOG(LogEnding, Log, TEXT("스크린샷: %s"), *Name);
	}

	void QuitAfter(UWorld* World, float Delay)
	{
		FTimerHandle QuitHandle;
		World->GetTimerManager().SetTimer(QuitHandle, FTimerDelegate::CreateWeakLambda(World, [World]()
		{
			UE_LOG(LogEnding, Log, TEXT("스크린샷 끝, 게임 종료"));
			if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
			{
				PC->ConsoleCommand(TEXT("quit"));
			}
		}), Delay, false);
	}
}

/**
 * 헤드셋 없이 크레딧 확인용: 컷신을 멈추고 크레딧 카메라로 크레딧 쪽마다 스크린샷을 남긴다 (Saved/Screenshots, Credits_1 ...).
 * 콘솔: pcs.CreditsShots [quit]   (quit이면 다 찍은 뒤 게임을 끈다)
 * 예) UnrealEditor.exe pcs_vr.uproject /Game/VRPuzzle/Maps/L_06_Ending -game -windowed -ResX=1600 -ResY=900 -ExecCmds="pcs.CreditsShots quit"
 */
static FAutoConsoleCommandWithWorldAndArgs GCreditsShotsCommand(
	TEXT("pcs.CreditsShots"),
	TEXT("엔딩 컷신을 멈추고 크레딧 쪽마다 스크린샷을 남긴다 (pcs.CreditsShots [quit])"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		AEndingCutscene* Cutscene = StopCutsceneForShots(World);
		const FEndingShot* Credits = Cutscene
			? Cutscene->Shots.FindByPredicate([](const FEndingShot& S) { return S.Sign && S.Pages.Num() > 0; })
			: nullptr;
		if (!Credits)
		{
			return;
		}
		ViewFrom(World, Credits->Camera);
		TWeakObjectPtr<APuzzleSign> WeakSign = Credits->Sign.Get();
		WeakSign->SetSignVisible(true);
		WeakSign->SetSignText(Credits->Pages[0]);
		const TArray<FText> Pages = Credits->Pages;
		const bool bQuit = Args.Contains(TEXT("quit"));

		WhenShadersReadyOrTimeout(World, FPlatformTime::Seconds(), [World, WeakSign, Pages, bQuit]()
		{
			for (int32 i = 0; i < Pages.Num(); ++i)
			{
				const float Start = 0.01f + 1.2f * i;
				FTimerHandle PageHandle;
				World->GetTimerManager().SetTimer(PageHandle, FTimerDelegate::CreateWeakLambda(World, [WeakSign, Text = Pages[i]]()
				{
					if (WeakSign.IsValid())
					{
						WeakSign->SetSignText(Text);
					}
				}), Start, false);
				FTimerHandle ShotHandle;
				World->GetTimerManager().SetTimer(ShotHandle, FTimerDelegate::CreateWeakLambda(World, [i]()
				{
					Shoot(FString::Printf(TEXT("Credits_%d"), i + 1));
				}), Start + 0.7f, false);
			}
			if (bQuit)
			{
				QuitAfter(World, 1.2f * Pages.Num() + 1.5f);
			}
		});
	}));

/**
 * 헤드셋 없이 엔딩 장면 확인용: 컷신을 멈추고 장면 카메라마다 스크린샷을 남긴다 (Saved/Screenshots, Ending_1 ...).
 * 데스크톱에서 천천히 움직이는 장면은 끝 카메라(EndCamera) 자리도 찍는다 (Ending_1b ...).
 * 콘솔: pcs.EndingShots [quit]
 */
static FAutoConsoleCommandWithWorldAndArgs GEndingShotsCommand(
	TEXT("pcs.EndingShots"),
	TEXT("엔딩 컷신을 멈추고 장면 카메라마다 스크린샷을 남긴다 (pcs.EndingShots [quit])"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		AEndingCutscene* Cutscene = StopCutsceneForShots(World);
		if (!Cutscene)
		{
			return;
		}
		// 카메라 목록: (이름, 카메라, 글자판)
		struct FView { FString Name; TWeakObjectPtr<ACameraActor> Camera; TWeakObjectPtr<APuzzleSign> Sign; FText Text; };
		TArray<FView> Views;
		for (int32 i = 0; i < Cutscene->Shots.Num(); ++i)
		{
			const FEndingShot& S = Cutscene->Shots[i];
			const FText Text = S.Pages.Num() > 0 ? S.Pages[0] : FText::GetEmpty();
			Views.Add({ FString::Printf(TEXT("Ending_%d"), i + 1), S.Camera.Get(), S.Sign.Get(), Text });
			if (S.EndCamera)
			{
				Views.Add({ FString::Printf(TEXT("Ending_%db"), i + 1), S.EndCamera.Get(), S.Sign.Get(), Text });
			}
		}
		const bool bQuit = Args.Contains(TEXT("quit"));
		ViewFrom(World, Views.Num() > 0 ? Views[0].Camera.Get() : nullptr);

		WhenShadersReadyOrTimeout(World, FPlatformTime::Seconds(), [World, Views, bQuit]()
		{
			for (int32 i = 0; i < Views.Num(); ++i)
			{
				const FView V = Views[i];
				const float Start = 0.01f + 1.5f * i;
				FTimerHandle ViewHandle;
				World->GetTimerManager().SetTimer(ViewHandle, FTimerDelegate::CreateWeakLambda(World, [World, V]()
				{
					ViewFrom(World, V.Camera.Get());
					if (APuzzleSign* Sign = V.Sign.Get())
					{
						Sign->SetSignVisible(true);
						if (!V.Text.IsEmpty())
						{
							Sign->SetSignText(V.Text);
						}
					}
				}), Start, false);
				FTimerHandle ShotHandle;
				World->GetTimerManager().SetTimer(ShotHandle, FTimerDelegate::CreateWeakLambda(World, [Name = V.Name]()
				{
					Shoot(Name);
				}), Start + 1.0f, false);
			}
			if (bQuit)
			{
				QuitAfter(World, 1.5f * Views.Num() + 1.5f);
			}
		});
	}));
