#include "DeviceStageDirector.h"

#include "DebugShots.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "PuzzleDoor.h"
#include "UnrealClient.h"
#include "PuzzleElevator.h"
#include "PuzzleHandDevice.h"
#include "PuzzleSign.h"
#include "PuzzleToggleTarget.h"
#include "PuzzleUtils.h"
#include "TimerManager.h"
#include "VRSmoothLocomotionSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogStage4, Log, All);

namespace
{
	constexpr float PaperFlyTime = 1.6f;
	constexpr float MonitorDaySeconds = 1.5f;   // 모니터가 켜져 있으면 이 간격마다 날짜가 하루씩 늘어남 (노션: 오싹한 연출)
}

ADeviceStageDirector::ADeviceStageDirector()
{
	PrimaryActorTick.bCanEverTick = true;

	IdleHintMemo = FText::FromString(TEXT("일단 아무거나 눌러봐.\n방이 대답해 줄 거야."));
	HiddenHintMemo = FText::FromString(TEXT("내 메모는 밝은 데서는 안 보여.\n그날처럼 빨간불만 켜 봐."));
	ClearMemo = FText::FromString(TEXT("사고 뒤에 측정값이 바뀌었어.\n네가 기록한 값이 아니야.\n그걸 증명할 수 있는 건 코어 안의 기록뿐이야.\n그리고… 나연이를 이제 보내주고 싶어."));
}

void ADeviceStageDirector::BeginPlay()
{
	Super::BeginPlay();

	if (UVRSmoothLocomotionSubsystem* Locomotion = GetWorld()->GetSubsystem<UVRSmoothLocomotionSubsystem>())
	{
		Locomotion->bBlockLowObstacles = true;
	}

	if (Device)
	{
		Device->OnEquipped.AddDynamic(this, &ADeviceStageDirector::HandleEquipped);
		Device->OnPressed.AddDynamic(this, &ADeviceStageDirector::HandlePressed);
		Device->OnUnlocked.AddDynamic(this, &ADeviceStageDirector::HandleUnlocked);
	}
	for (APuzzleToggleTarget* Target : GetTargets())
	{
		Target->OnToggled.AddDynamic(this, &ADeviceStageDirector::HandleToggled);
	}
	if (SymbolTable)
	{
		SymbolTable->SetSignVisible(false);
	}
	UpdateMonitorText();
	// 장비들의 BeginPlay(처음 켜짐 상태)가 끝난 다음 칠판을 맞춘다
	GetWorldTimerManager().SetTimerForNextTick(this, &ADeviceStageDirector::UpdateBoard);

	// 눈앞에 띄우는 지도 메모 (1 ~ 3스테이지와 같은 모양)
	MemoSign = GetWorld()->SpawnActorDeferred<APuzzleSign>(APuzzleSign::StaticClass(), FTransform::Identity, this);
	if (MemoSign)
	{
		MemoSign->FontSize = 36;
		MemoSign->DrawSize = FVector2D(1000.f, 300.f);   // 클리어 메모가 4줄이라 다른 스테이지보다 조금 크게
		MemoSign->PixelsPerCm = 20.f;
		MemoSign->TextColor = FLinearColor(1.f, 0.95f, 0.8f);
		MemoSign->BackgroundColor = FLinearColor(0.05f, 0.08f, 0.1f, 0.85f);
		MemoSign->bVisibleAtStart = false;
		MemoSign->FinishSpawning(FTransform::Identity);
	}
}

TArray<APuzzleToggleTarget*> ADeviceStageDirector::GetTargets() const
{
	TArray<APuzzleToggleTarget*> Out;
	for (APuzzleToggleTarget* T : { RoomLight.Get(), WarningLight.Get(), Monitor.Get(), Fan.Get() })
	{
		if (T)
		{
			Out.Add(T);
		}
	}
	return Out;
}

void ADeviceStageDirector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdatePapers(DeltaTime);

	RecBlinkTime += DeltaTime;
	const bool bRecOn = FMath::Fmod(RecBlinkTime, 1.2f) < 0.6f;
	for (AActor* Lamp : RecLamps)
	{
		if (Lamp && Lamp->IsHidden() == bRecOn)
		{
			Lamp->SetActorHiddenInGame(!bRecOn);
		}
	}

	if (Monitor && Monitor->IsOn())
	{
		const int32 Before = FMath::FloorToInt(MonitorDays);
		MonitorDays += DeltaTime / MonitorDaySeconds;
		if (FMath::FloorToInt(MonitorDays) != Before)
		{
			UpdateMonitorText();
		}
	}

	if (!bEquipped || bCleared)
	{
		return;
	}
	IdleTime += DeltaTime;
	EquippedTime += DeltaTime;
	if (!bIdleHintShown && IdleTime >= IdleHintDelay)
	{
		bIdleHintShown = true;
		ShowMemo(IdleHintMemo);
	}
	if (!bHiddenHintShown && !bHiddenSeen && EquippedTime >= HiddenHintDelay)
	{
		bHiddenHintShown = true;
		ShowMemo(HiddenHintMemo, 8.f);
	}
}

void ADeviceStageDirector::HandleEquipped()
{
	if (bEquipped)
	{
		return;
	}
	bEquipped = true;
	UE_LOG(LogStage4, Log, TEXT("장치 장착 → 문 닫힘, 방 불 꺼짐"));
	if (EntryDoor)
	{
		EntryDoor->Close();
	}
	for (APuzzleToggleTarget* Target : GetTargets())
	{
		Target->SetOn(false, false);
	}
	UpdateBoard();
}

void ADeviceStageDirector::HandlePressed(int32 SymbolIndex)
{
	// 노션: 입력이 있으면 막힘 방지 타이머 리셋
	IdleTime = 0.f;
}

void ADeviceStageDirector::HandleToggled(APuzzleToggleTarget* Target, bool bOn)
{
	if (Target == Fan && bOn && !bPapersBlown)
	{
		BlowPapers();
	}
	if (Target == Monitor)
	{
		UpdateMonitorText();
	}
	UpdateBoard();
}

void ADeviceStageDirector::UpdateBoard()
{
	const bool bLightOn = RoomLight && RoomLight->IsOn();
	const bool bWarningOn = WarningLight && WarningLight->IsOn();
	for (APuzzleSign* Text : BoardTexts)
	{
		if (Text)
		{
			Text->SetSignVisible(bLightOn);
		}
	}
	const bool bHiddenVisible = bWarningOn && !bLightOn;
	if (HiddenText)
	{
		HiddenText->SetSignVisible(bHiddenVisible);
	}
	if (bHiddenVisible && !bHiddenSeen)
	{
		bHiddenSeen = true;
		UE_LOG(LogStage4, Log, TEXT("퍼즐 1: 숨은 글씨 발견 (조명 OFF + 경고등 ON)"));
	}
}

void ADeviceStageDirector::BlowPapers()
{
	bPapersBlown = true;
	UE_LOG(LogStage4, Log, TEXT("환풍기 → 종이가 날아가고 찢어진 기호표가 드러남"));
	const FVector FanLoc = Fan ? Fan->GetActorLocation() : GetActorLocation();
	for (AActor* Paper : Papers)
	{
		if (!Paper)
		{
			continue;
		}
		FVector Away = Paper->GetActorLocation() - FanLoc;
		Away.Z = 0.f;
		Away = Away.GetSafeNormal();
		FFlyingPaper& F = Flying.AddDefaulted_GetRef();
		F.Actor = Paper;
		F.Velocity = Away * FMath::FRandRange(180.f, 260.f) + FVector(FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(120.f, 200.f));
		F.SpinAxis = FMath::VRand();
		F.SpinRate = FMath::FRandRange(360.f, 720.f);
		if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Paper->GetRootComponent()))
		{
			Prim->SetMobility(EComponentMobility::Movable);
			Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	FlyTime = 0.f;
	if (SymbolTable)
	{
		SymbolTable->SetSignVisible(true);
	}
}

void ADeviceStageDirector::UpdatePapers(float DeltaTime)
{
	if (Flying.Num() == 0)
	{
		return;
	}
	FlyTime += DeltaTime;
	for (FFlyingPaper& F : Flying)
	{
		AActor* Paper = F.Actor.Get();
		if (!Paper)
		{
			continue;
		}
		F.Velocity.Z -= 250.f * DeltaTime;   // 종이라 천천히 떨어짐
		Paper->AddActorWorldOffset(F.Velocity * DeltaTime);
		Paper->AddActorWorldRotation(FQuat(F.SpinAxis, FMath::DegreesToRadians(F.SpinRate * DeltaTime)));
		if (FlyTime >= PaperFlyTime)
		{
			Paper->SetActorHiddenInGame(true);
		}
	}
	if (FlyTime >= PaperFlyTime)
	{
		Flying.Reset();
	}
}

void ADeviceStageDirector::UpdateMonitorText()
{
	if (!MonitorText)
	{
		return;
	}
	MonitorText->SetSignText(FText::Format(
		FText::FromString(TEXT("참가자 신호 상태\n\n피험 장치 01 | 오나연 | 신호 수신 중 ({0}일째)\n피험 장치 02 | — | 미사용\n피험 장치 03 | — | 미사용")),
		FText::AsNumber(FMath::FloorToInt(MonitorDays))));
}

void ADeviceStageDirector::HandleUnlocked()
{
	if (bCleared)
	{
		return;
	}
	bCleared = true;
	UE_LOG(LogStage4, Log, TEXT("4스테이지 클리어 → 장비 모두 꺼짐, 조명만 켜짐, 엘리베이터 도착"));
	HideMemo();
	for (APuzzleToggleTarget* Target : GetTargets())
	{
		Target->SetOn(false, false);
	}
	UpdateBoard();
	GetWorldTimerManager().SetTimer(ClearHandle, this, &ADeviceStageDirector::RestoreRoomLight, 0.8f, false);
}

void ADeviceStageDirector::RestoreRoomLight()
{
	if (RoomLight)
	{
		RoomLight->SetOn(true);
	}
	GetWorldTimerManager().SetTimer(ClearHandle, this, &ADeviceStageDirector::OpenExit, 1.f, false);
}

void ADeviceStageDirector::OpenExit()
{
	if (ExitElevator)
	{
		ExitElevator->Arrive();
	}
	ShowMemo(ClearMemo, 12.f);
}

void ADeviceStageDirector::ShowMemo(const FText& Memo, float Duration)
{
	USceneComponent* Cam = PuzzleUtils::GetPlayerCamera(this);
	if (!MemoSign || !Cam)
	{
		return;
	}
	MemoSign->AttachToComponent(Cam, FAttachmentTransformRules::KeepRelativeTransform);
	MemoSign->SetActorRelativeLocation(FVector(90.f, 0.f, -18.f));
	MemoSign->SetActorRelativeRotation(FRotator(10.f, 180.f, 0.f));
	MemoSign->SetSignText(Memo);
	MemoSign->SetSignVisible(true);
	GetWorldTimerManager().SetTimer(MemoHandle, this, &ADeviceStageDirector::HideMemo, Duration, false);
}

void ADeviceStageDirector::HideMemo()
{
	if (MemoSign)
	{
		MemoSign->SetSignVisible(false);
	}
}

// ---------------------------------------------------------------- 헤드셋 없이 테스트

void ADeviceStageDirector::RunSelfTest()
{
	APawn* Pawn = PuzzleUtils::GetPlayerPawn(this);
	USceneComponent* Cam = PuzzleUtils::GetPlayerCamera(this);
	if (!Pawn || !Cam || !Device)
	{
		UE_LOG(LogStage4, Warning, TEXT("pcs.Stage4Test: 플레이어 / 카메라 / 장치 없음"));
		return;
	}

	// 책상 앞(거치대에서 남쪽으로 85cm)에 서서 칠판 쪽을 본다. 헤드셋이 없으면 카메라가 바닥 높이라 눈높이(160cm)로 올림
	const FVector RackLoc = Device->GetActorLocation();
	const FVector Stand = RackLoc + Device->GetActorForwardVector() * 85.f;
	Pawn->SetActorLocation(FVector(Stand.X, Stand.Y, Pawn->GetActorLocation().Z));
	Pawn->SetActorRotation(FRotator(0.f, (-Device->GetActorForwardVector()).Rotation().Yaw, 0.f));
	Cam->SetWorldLocation(FVector(Stand.X, Stand.Y, RackLoc.Z - 74.5f + 160.f));
	Cam->SetRelativeRotation(FRotator(-8.f, 0.f, 0.f));

	struct FStep
	{
		const TCHAR* Shot;   // nullptr이면 스크린샷 없음
		TFunction<void(ADeviceStageDirector*)> Action;
	};
	auto Left = [](ADeviceStageDirector* D) { D->Device->PressLeft(); };
	auto Right = [](ADeviceStageDirector* D) { D->Device->PressRight(); };
	auto Nothing = [](ADeviceStageDirector* D) {};
	const TArray<FStep> Steps = {
		{ TEXT("S4_0_Start"), Nothing },
		{ nullptr, [](ADeviceStageDirector* D) { D->Device->Equip(); } },
		{ TEXT("S4_1_Equipped_Dark"), Nothing },
		{ nullptr, Nothing },
		{ TEXT("S4_1b_Dark_Later"), Nothing },
		{ nullptr, Left },                       // ▲
		{ nullptr, Right },                      // 경고등 ON (조명 OFF) → 숨은 글씨
		{ TEXT("S4_2_HiddenText"), Nothing },
		{ nullptr, Left }, { nullptr, Left },    // ◆
		{ nullptr, Right },                      // 환풍기 ON → 종이 날아감
		{ nullptr, Nothing }, { nullptr, Nothing }, { nullptr, Nothing },
		{ TEXT("S4_3_FanPapers"), Nothing },
		{ nullptr, Left }, { nullptr, Left },    // ▲
		{ nullptr, Right },
		{ nullptr, Left }, { nullptr, Left }, { nullptr, Left },   // ●
		{ nullptr, Right },
		{ TEXT("S4_4_Progress"), Nothing },
		{ nullptr, Left }, { nullptr, Left }, { nullptr, Left },   // ◆
		{ nullptr, Right },
		{ nullptr, Left }, { nullptr, Left }, { nullptr, Left },   // ■
		{ nullptr, Right },                      // ▲ ● ◆ ■ → 해제
		{ nullptr, Nothing }, { nullptr, Nothing }, { nullptr, Nothing }, { nullptr, Nothing },
		{ TEXT("S4_5_Cleared"), Nothing },
	};

	constexpr float StepTime = 0.7f;
	for (int32 i = 0; i < Steps.Num(); ++i)
	{
		FTimerHandle Handle;
		const FStep Step = Steps[i];
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Step, i]()
		{
			Step.Action(this);
			UE_LOG(LogStage4, Log, TEXT("pcs.Stage4Test %02d: 장착 %d / 해제 %d / 다이얼 %s / 조명 %d 경고등 %d 모니터 %d 환풍기 %d / 숨은 글씨 %d / 종이 %d"),
				i, Device->IsEquipped(), Device->IsUnlocked(), *APuzzleHandDevice::SymbolOf(Device->GetDialIndex()),
				RoomLight && RoomLight->IsOn(), WarningLight && WarningLight->IsOn(), Monitor && Monitor->IsOn(), Fan && Fan->IsOn(),
				bHiddenSeen, bPapersBlown);
			if (Step.Shot)
			{
				FScreenshotRequest::RequestScreenshot(Step.Shot, false, false);
			}
		}), StepTime * (i + 1), false);
	}
}

static FAutoConsoleCommandWithWorld GStage4TestCommand(
	TEXT("pcs.Stage4Test"),
	TEXT("4스테이지를 헤드셋 없이 처음부터 끝까지 자동으로 풀어 보고 스크린샷을 남긴다"),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
	{
		for (TActorIterator<ADeviceStageDirector> It(World); It; ++It)
		{
			// -ExecCmds로 시작하자마자 부를 때 플레이어가 아직 없고 셰이더도 컴파일 중일 수 있어서, 준비된 뒤 시작
			TWeakObjectPtr<ADeviceStageDirector> Director = *It;
			DebugShots::WhenReady(World, [Director]() { if (Director.IsValid()) { Director->RunSelfTest(); } });
			return;
		}
		UE_LOG(LogStage4, Warning, TEXT("pcs.Stage4Test: 이 맵에 4스테이지 진행 담당이 없음"));
	}));
