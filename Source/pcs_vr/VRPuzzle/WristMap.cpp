#include "WristMap.h"

#include "DebugShots.h"
#include "DesktopModeSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "PuzzleSign.h"
#include "PuzzleUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogWristMap, Log, All);

namespace
{
	// 지도에 그리는 경로 (일자형 구조, 노션 "스테이지 설계 > 전체 구조")
	const TCHAR* RouteStages[] = { TEXT("격리실"), TEXT("원근"), TEXT("관측"), TEXT("장치"), TEXT("코어") };

	// 손목 지도판에 보이는 최근 메모 수 (최신 1개는 크게, 나머지는 한 줄씩)
	constexpr int32 ShownMemoCount = 3;

	FString OneLine(const FText& Text)
	{
		return Text.ToString().Replace(TEXT("\n"), TEXT(" "));
	}
}

// ---------------------------------------------------------------- 기록 (게임 인스턴스)

UWristMapSubsystem* UWristMapSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UWristMapSubsystem>() : nullptr;
}

FString UWristMapSubsystem::StageLabelForMap(const FString& MapName)
{
	static const TMap<FString, FString> Labels = {
		{ TEXT("L_01_Isolation"),   TEXT("격리실") },
		{ TEXT("L_02_Perspective"), TEXT("원근") },
		{ TEXT("L_03_Observation"), TEXT("관측") },
		{ TEXT("L_04_Device"),      TEXT("장치") },
		{ TEXT("L_05_Core"),        TEXT("코어") },
	};
	const FString* Found = Labels.Find(MapName);
	return Found ? *Found : FString();
}

void UWristMapSubsystem::AddMemo(const FText& Memo, const UObject* WorldContext)
{
	if (Memo.IsEmpty())
	{
		return;
	}
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const FString Stage = World ? StageLabelForMap(UGameplayStatics::GetCurrentLevelName(World, true)) : FString();

	// 같은 힌트가 다시 나오면 중복으로 쌓지 않고 최신으로만 올린다
	Memos.RemoveAll([&](const FWristMemo& M) { return M.Text.EqualTo(Memo); });
	Memos.Add({ Memo, Stage });
	++Version;
	UE_LOG(LogWristMap, Log, TEXT("지도 메모 추가 [%s] %s (총 %d개)"), *Stage, *OneLine(Memo), Memos.Num());
}

void UWristMapSubsystem::SetCoreCode(const FString& Code)
{
	if (CoreCode != Code)
	{
		CoreCode = Code;
		++Version;
		UE_LOG(LogWristMap, Log, TEXT("지도에 코어 정지 코드 기록: %s"), *Code);
	}
}

void UWristMapSubsystem::ResetForNewGame()
{
	if (Memos.Num() > 0 || !CoreCode.IsEmpty())
	{
		UE_LOG(LogWristMap, Log, TEXT("새 게임: 지도 기록 비움"));
	}
	Memos.Reset();
	CoreCode.Reset();
	++Version;
}

// ---------------------------------------------------------------- 지도판 (월드)

bool UWristMapWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UWristMapWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	const FString MapName = UGameplayStatics::GetCurrentLevelName(&InWorld, true);
	UWristMapSubsystem* Record = UWristMapSubsystem::Get(&InWorld);
	if (Record && (MapName == TEXT("L_MainMenu") || MapName == TEXT("L_00_Intro")))
	{
		Record->ResetForNewGame();
	}

	StageLabel = UWristMapSubsystem::StageLabelForMap(MapName);
	bEnabled = !StageLabel.IsEmpty();
	if (!bEnabled)
	{
		return;
	}

	// 어두운 방에서는 자동 노출 때문에 밝은 바탕이 하얗게 날아가서, 지도 메모와 같은 어두운 바탕 + 밝은 글씨. 22.5 x 16cm
	// 글자 높이 약 0.85cm: 45cm 거리에서 눈앞 지도 메모(90cm 거리, 1.8cm)와 비슷한 크기로 보인다
	Panel = InWorld.SpawnActorDeferred<APuzzleSign>(APuzzleSign::StaticClass(), FTransform::Identity);
	if (Panel)
	{
		Panel->FontSize = 34;
		Panel->DrawSize = FVector2D(900.f, 640.f);
		Panel->PixelsPerCm = 40.f;
		Panel->TextColor = FLinearColor(1.f, 0.93f, 0.78f);
		Panel->BackgroundColor = FLinearColor(0.09f, 0.07f, 0.05f, 0.92f);
		Panel->bVisibleAtStart = false;
		Panel->FinishSpawning(FTransform::Identity);
	}
	ShownVersion = -1;
	bPanelVisible = false;
}

void UWristMapWorldSubsystem::RefreshText()
{
	const UWristMapSubsystem* Record = UWristMapSubsystem::Get(GetWorld());
	if (!Panel || !Record || ShownVersion == Record->GetVersion())
	{
		return;
	}
	ShownVersion = Record->GetVersion();

	FString Text = TEXT("연구소 지도\n");
	for (int32 i = 0; i < UE_ARRAY_COUNT(RouteStages); ++i)
	{
		if (i > 0)
		{
			Text += TEXT(" · ");
		}
		Text += StageLabel == RouteStages[i] ? FString::Printf(TEXT("[%s]"), RouteStages[i]) : FString(RouteStages[i]);
	}
	Text += TEXT("\n");

	if (!Record->GetCoreCode().IsEmpty())
	{
		Text += FString::Printf(TEXT("\n코어 정지 코드  %s\n"), *Record->GetCoreCode());
	}

	Text += TEXT("\n— 세진의 메모 —\n");
	const TArray<FWristMemo>& Memos = Record->GetMemos();
	if (Memos.Num() == 0)
	{
		Text += TEXT("(아직 메모 없음)");
	}
	else
	{
		// 최신 메모는 그대로 (줄바꿈 유지), 그 전 메모는 한 줄씩
		Text += Memos.Last().Text.ToString();
		for (int32 i = Memos.Num() - 2; i >= FMath::Max(0, Memos.Num() - ShownMemoCount); --i)
		{
			Text += FString::Printf(TEXT("\n\n· %s"), *OneLine(Memos[i].Text));
		}
	}
	Panel->SetSignText(FText::FromString(Text));
}

void UWristMapWorldSubsystem::SetPanelVisible(bool bVisible)
{
	if (Panel && bVisible != bPanelVisible)
	{
		bPanelVisible = bVisible;
		Panel->SetSignVisible(bVisible);
	}
}

void UWristMapWorldSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!bEnabled || !Panel || !World)
	{
		return;
	}

	FVector EyeLoc;
	FRotator EyeRot;
	if (!PuzzleUtils::GetPlayerView(World, EyeLoc, EyeRot))
	{
		SetPanelVisible(false);
		return;
	}
	const FVector EyeFwd = EyeRot.Vector();

	FVector PanelLoc;
	bool bShow = false;

	// 데스크톱(또는 키보드): Tab을 누르고 있는 동안 눈앞 45cm
	const APlayerController* PC = World->GetFirstPlayerController();
	if (bForceShow || (PC && PC->IsInputKeyDown(EKeys::Tab)))
	{
		bShow = true;
		PanelLoc = EyeLoc + EyeFwd * 45.f;
	}
	// VR: 왼손을 눈앞으로 들어 올리면 손목 위
	else if (!UDesktopModeSubsystem::IsDesktopMode())
	{
		const APawn* Pawn = PuzzleUtils::GetPlayerPawn(World);
		if (const USceneComponent* LeftHand = Pawn ? PuzzleUtils::FindPawnComponent(Pawn, TEXT("MotionControllerLeftGrip")) : nullptr)
		{
			const FVector HandLoc = LeftHand->GetComponentLocation();
			const FVector ToHand = HandLoc - EyeLoc;
			const float Dist = ToHand.Size();
			const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(EyeFwd, ToHand.GetSafeNormal()), -1.f, 1.f)));
			if (Dist < MaxHandDistance && Angle < (bPanelVisible ? HideAngle : ShowAngle))
			{
				bShow = true;
				// 손에 묻히지 않게 눈 쪽으로 8cm, 위로 6cm
				PanelLoc = HandLoc - ToHand.GetSafeNormal() * 8.f + FVector(0.f, 0.f, 6.f);
			}
		}
	}

	if (bShow)
	{
		RefreshText();
		// 지도판의 앞면(+X)이 눈을 향하게
		Panel->SetActorLocationAndRotation(PanelLoc, (EyeLoc - PanelLoc).Rotation());
	}
	SetPanelVisible(bShow);
}

// ---------------------------------------------------------------- 디버그

/**
 * 헤드셋 없이 손목 지도 모습 확인용: 지정한 초 뒤마다 지도판을 눈앞에 띄우고 스크린샷을 남긴다 (Saved/Screenshots).
 * 콘솔: pcs.WristMapShot 20 300   (셰이더 준비 후 20초, 300초 뒤에 한 장씩)
 */
static FAutoConsoleCommandWithWorldAndArgs GWristMapShotCommand(
	TEXT("pcs.WristMapShot"),
	TEXT("지정한 초 뒤마다 손목 지도를 눈앞에 띄우고 스크린샷을 남긴다"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		TArray<float> Delays;
		for (const FString& A : Args)
		{
			Delays.Add(FMath::Max(0.f, FCString::Atof(*A)));
		}
		if (Delays.Num() == 0)
		{
			Delays.Add(0.f);
		}
		DebugShots::WhenReady(World, [World, Delays]()
		{
			for (int32 i = 0; i < Delays.Num(); ++i)
			{
				const float Delay = Delays[i] + 0.01f;
				const FString Name = FString::Printf(TEXT("WristMap_%d"), i);
				FTimerHandle ShowHandle;
				World->GetTimerManager().SetTimer(ShowHandle, FTimerDelegate::CreateWeakLambda(World, [World]()
				{
					if (UWristMapWorldSubsystem* Map = World->GetSubsystem<UWristMapWorldSubsystem>())
					{
						Map->bForceShow = true;
					}
				}), Delay, false);
				FTimerHandle ShotHandle;
				World->GetTimerManager().SetTimer(ShotHandle, FTimerDelegate::CreateWeakLambda(World, [World, Name]()
				{
					FScreenshotRequest::RequestScreenshot(Name, false, false);
					UE_LOG(LogWristMap, Log, TEXT("pcs.WristMapShot: %s"), *Name);
				}), Delay + 1.f, false);
				FTimerHandle HideHandle;
				World->GetTimerManager().SetTimer(HideHandle, FTimerDelegate::CreateWeakLambda(World, [World]()
				{
					if (UWristMapWorldSubsystem* Map = World->GetSubsystem<UWristMapWorldSubsystem>())
					{
						Map->bForceShow = false;
					}
				}), Delay + 2.f, false);
			}
		});
	}));
