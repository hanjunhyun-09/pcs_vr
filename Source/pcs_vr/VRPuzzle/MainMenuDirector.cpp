#include "MainMenuDirector.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "PuzzleButton.h"
#include "PuzzleSign.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogMainMenu, Log, All);

AMainMenuDirector::AMainMenuDirector()
{
	PrimaryActorTick.bCanEverTick = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AMainMenuDirector::BeginPlay()
{
	Super::BeginPlay();

	if (StartButton)
	{
		StartButton->OnPressed.AddDynamic(this, &AMainMenuDirector::HandleStart);
	}
	if (QuitButton)
	{
		QuitButton->OnPressed.AddDynamic(this, &AMainMenuDirector::HandleQuit);
	}
	Fade(1.f, 0.f, FadeInTime, false);
	UE_LOG(LogMainMenu, Log, TEXT("메인 화면 준비 (시작 버튼 %d / 종료 버튼 %d)"), StartButton != nullptr, QuitButton != nullptr);
}

void AMainMenuDirector::HandleStart()
{
	if (bBusy)
	{
		return;
	}
	bBusy = true;
	UE_LOG(LogMainMenu, Log, TEXT("게임 시작 → %s"), *StartLevel.ToString());
	if (HintSign)
	{
		HintSign->SetSignText(FText::FromString(TEXT("시작하는 중…")));
	}
	Fade(0.f, 1.f, FadeOutTime, true);
	GetWorldTimerManager().SetTimer(ActionHandle, this, &AMainMenuDirector::OpenStartLevel, FadeOutTime + 0.2f, false);
}

void AMainMenuDirector::HandleQuit()
{
	if (bBusy)
	{
		return;
	}
	bBusy = true;
	UE_LOG(LogMainMenu, Log, TEXT("게임 종료"));
	Fade(0.f, 1.f, FadeOutTime, true);
	GetWorldTimerManager().SetTimer(ActionHandle, this, &AMainMenuDirector::QuitNow, FadeOutTime + 0.2f, false);
}

void AMainMenuDirector::OpenStartLevel()
{
	UGameplayStatics::OpenLevel(this, StartLevel);
}

void AMainMenuDirector::QuitNow()
{
	UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(this, 0), EQuitPreference::Quit, false);
}

void AMainMenuDirector::Fade(float From, float To, float Time, bool bHold)
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(From, To, FMath::Max(Time, 0.01f), FLinearColor::Black, false, bHold);
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GMenuPressCommand(
	TEXT("pcs.MenuPress"),
	TEXT("메인 화면 버튼을 누른다 (헤드셋 없이 테스트용): pcs.MenuPress start | quit"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		for (TActorIterator<AMainMenuDirector> It(World); It; ++It)
		{
			const bool bQuit = Args.Num() > 0 && Args[0].Equals(TEXT("quit"), ESearchCase::IgnoreCase);
			if (APuzzleButton* Button = bQuit ? It->QuitButton.Get() : It->StartButton.Get())
			{
				Button->Press();
			}
			return;
		}
		UE_LOG(LogMainMenu, Warning, TEXT("pcs.MenuPress: 이 맵에 메인 화면 진행 담당이 없음"));
	}));
