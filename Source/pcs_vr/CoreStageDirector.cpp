#include "CoreStageDirector.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DebugShots.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "PerspectiveCube.h"
#include "PuzzleBlueprintSlot.h"
#include "PuzzleCoreSocket.h"
#include "PuzzleDoor.h"
#include "PuzzleElevator.h"
#include "PuzzleGrabbable.h"
#include "PuzzleHandDevice.h"
#include "PuzzleRecorder.h"
#include "PuzzleSign.h"
#include "PuzzleToggleTarget.h"
#include "PuzzleUtils.h"
#include "PuzzleViewPoint.h"
#include "TimerManager.h"
#include "UnrealClient.h"
#include "VRSmoothLocomotionSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogStage5, Log, All);

namespace
{
	/** 부품이 0 / 1 / 2 / 3개 끼워졌을 때 코어 맥박 (Hz). 모일수록 느려짐 (노션: 코어의 맥박이 느려지며) */
	constexpr float PulseRates[4] = { 0.9f, 0.65f, 0.45f, 0.25f };
	constexpr float EndingPulseRate = 0.12f;
	constexpr float CoreFadeTime = 3.f;
	const FLinearColor LabelWaiting(0.55f, 0.6f, 0.65f);
	const FLinearColor LabelDone(0.4f, 1.f, 0.55f);
}

ACoreStageDirector::ACoreStageDirector()
{
	PrimaryActorTick.bCanEverTick = true;

	NarrowDoorHintMemo = FText::FromString(TEXT("그대로는 안 들어가.\n일단 작게 만들어."));
	BalconyHintMemo = FText::FromString(TEXT("난간의 번호판 옆에 서 봐.\n우리가 쓰던 관측 자리야."));
	NoCodeHintMemo = FText::FromString(TEXT("코드는 위층 관측 발코니에서\n볼 수 있어."));
	CodeRecordedMemo = FText::FromString(TEXT("코드 ◆ ● ▲ ■\n지도에 적어 뒀어."));
}

void ACoreStageDirector::BeginPlay()
{
	Super::BeginPlay();

	if (UVRSmoothLocomotionSubsystem* Locomotion = GetWorld()->GetSubsystem<UVRSmoothLocomotionSubsystem>())
	{
		Locomotion->bBlockLowObstacles = true;
	}

	if (ViewA) { ViewA->OnSolved.AddDynamic(this, &ACoreStageDirector::HandleViewSolved); }
	if (ViewB) { ViewB->OnSolved.AddDynamic(this, &ACoreStageDirector::HandleViewSolved); }
	if (Device)
	{
		Device->OnPressed.AddDynamic(this, &ACoreStageDirector::HandlePressed);
		Device->OnUnlocked.AddDynamic(this, &ACoreStageDirector::HandleUnlocked);
	}
	if (AxisSlot) { AxisSlot->OnFilled.AddDynamic(this, &ACoreStageDirector::HandleAxisFilled); }
	if (TriangleSocket) { TriangleSocket->OnFilled.AddDynamic(this, &ACoreStageDirector::HandleTriangleFilled); }
	if (SquareSocket) { SquareSocket->OnFilled.AddDynamic(this, &ACoreStageDirector::HandleSquareFilled); }

	if (CoreMesh)
	{
		if (UStaticMeshComponent* Mesh = CoreMesh->FindComponentByClass<UStaticMeshComponent>())
		{
			Mesh->SetMobility(EComponentMobility::Movable);
			CoreMID = PuzzleUtils::ApplySolidColor(Mesh, CoreColor, 2.f);
		}
	}
	for (AActor* LightActor : CoreLights)
	{
		if (UPointLightComponent* Light = LightActor ? LightActor->FindComponentByClass<UPointLightComponent>() : nullptr)
		{
			CoreLightComps.Add(Light);
			CoreLightBase.Add(Light->Intensity);
		}
	}
	for (APuzzleSign* Label : SlotLabels)
	{
		if (Label) { Label->SetSignColor(LabelWaiting); }
	}
	if (ExitNote)
	{
		ExitNote->SetSignVisible(false);
	}
	UpdateCodeBoard();

	// 보상 부품은 처음엔 숨김. 부품들의 BeginPlay(색칠, 잡기 컴포넌트 찾기)가 끝난 다음 숨긴다
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		HidePart(TrianglePart);
		HidePart(SquarePart);
	}));

	// 눈앞에 띄우는 지도 메모 (1 ~ 4스테이지와 같은 모양)
	MemoSign = GetWorld()->SpawnActorDeferred<APuzzleSign>(APuzzleSign::StaticClass(), FTransform::Identity, this);
	if (MemoSign)
	{
		MemoSign->FontSize = 36;
		MemoSign->DrawSize = FVector2D(1000.f, 300.f);
		MemoSign->PixelsPerCm = 20.f;
		MemoSign->TextColor = FLinearColor(1.f, 0.95f, 0.8f);
		MemoSign->BackgroundColor = FLinearColor(0.05f, 0.08f, 0.1f, 0.85f);
		MemoSign->bVisibleAtStart = false;
		MemoSign->FinishSpawning(FTransform::Identity);
	}
}

void ACoreStageDirector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateCore(DeltaTime);
	if (!bEnding)
	{
		UpdateHints(DeltaTime);
	}
}

// ---------------------------------------------------------------- 코어 맥박

void ACoreStageDirector::UpdateCore(float DeltaTime)
{
	const float Rate = bEnding ? EndingPulseRate : PulseRates[FMath::Clamp(FilledCount, 0, 3)];
	PulsePhase += DeltaTime * Rate * 2.f * PI;
	if (bFinished)
	{
		FadeOut = FMath::Max(0.f, FadeOut - DeltaTime / CoreFadeTime);
	}
	const float Pulse = 0.5f + 0.5f * FMath::Sin(PulsePhase);
	PuzzleUtils::SetSolidColor(CoreMID, CoreColor, (0.2f + 0.9f * Pulse) * FadeOut);
	for (int32 i = 0; i < CoreLightComps.Num(); ++i)
	{
		if (UPointLightComponent* Light = CoreLightComps[i].Get())
		{
			Light->SetIntensity(CoreLightBase[i] * (0.35f + 0.65f * Pulse) * FadeOut);
		}
	}
}

// ---------------------------------------------------------------- 시점 구역

void ACoreStageDirector::HandleViewSolved()
{
	BalconyTime = 0.f;
	const bool bA = ViewA && ViewA->IsSolved();
	const bool bB = ViewB && ViewB->IsSolved();
	UE_LOG(LogStage5, Log, TEXT("시점 구역: ① %d / ② %d"), bA, bB);
	if (!bA || !bB || bHasCode)
	{
		return;
	}
	bHasCode = true;
	UE_LOG(LogStage5, Log, TEXT("시점 구역 클리어 → 보관함 열림 (▲ 부품), 코드 ◆●▲■ 기록"));
	if (LockerDoor)
	{
		LockerDoor->Open();
	}
	RevealPart(TrianglePart);
	UpdateCodeBoard();
	ShowMemo(CodeRecordedMemo);
}

void ACoreStageDirector::UpdateCodeBoard()
{
	if (CodeBoard)
	{
		CodeBoard->SetSignText(FText::FromString(bHasCode
			? TEXT("지도 기록 · 코어 정지 코드\n◆  ●  ▲  ■")
			: TEXT("지도 기록 · 코어 정지 코드\n?  ?  ?  ?")));
	}
}

// ---------------------------------------------------------------- 장치 구역

int32 ACoreStageDirector::CountProgress() const
{
	// 입력 기록의 끝 k개가 정답의 처음 k개와 같은 가장 큰 k (APuzzleHandDevice와 같은 판정)
	const TArray<int32>& Solution = Device->Solution;
	for (int32 K = FMath::Min(Inputs.Num(), Solution.Num()); K > 0; --K)
	{
		bool bMatch = true;
		for (int32 i = 0; i < K && bMatch; ++i)
		{
			bMatch = Inputs[Inputs.Num() - K + i] == Solution[i];
		}
		if (bMatch)
		{
			return K;
		}
	}
	return 0;
}

void ACoreStageDirector::HandlePressed(int32 SymbolIndex)
{
	if (!Device)
	{
		return;
	}
	Inputs.Add(SymbolIndex);
	while (Inputs.Num() > FMath::Max(Device->Solution.Num(), 1))
	{
		Inputs.RemoveAt(0);
	}
	// 노션: 입력할 때마다 코어가 크게 울림 → 맥박 한 번
	PulsePhase = 0.5f * PI;

	// 램프는 맞게 입력한 기호만 켜져 있다. 틀리면 모두 꺼짐 (노션: 벌은 없지만 긴장감)
	const int32 Progress = CountProgress();
	for (int32 i = 0; i < PanelLamps.Num(); ++i)
	{
		bool bOn = false;
		for (int32 k = 0; k < Progress; ++k)
		{
			bOn |= Device->Solution[k] == i;
		}
		if (PanelLamps[i])
		{
			PanelLamps[i]->SetOn(bOn, false);
		}
	}
	UE_LOG(LogStage5, Log, TEXT("장치 구역: %s 입력 → 맞는 순서 %d/%d%s"), *APuzzleHandDevice::SymbolOf(SymbolIndex), Progress,
		Device->Solution.Num(), Progress == 0 ? TEXT(" (틀림, 램프 모두 꺼짐)") : TEXT(""));
}

void ACoreStageDirector::HandleUnlocked()
{
	UE_LOG(LogStage5, Log, TEXT("장치 구역 클리어 → 장치 풀림, 제어판에서 ■ 부품"));
	for (APuzzleToggleTarget* Lamp : PanelLamps)
	{
		if (Lamp) { Lamp->SetOn(true, false); }
	}
	RevealPart(SquarePart);
}

// ---------------------------------------------------------------- 보상 부품

void ACoreStageDirector::HidePart(APuzzleGrabbable* Part)
{
	if (!Part)
	{
		return;
	}
	if (UStaticMeshComponent* Mesh = Part->GetMesh())
	{
		Mesh->SetSimulatePhysics(false);
	}
	Part->SetActorHiddenInGame(true);
	Part->SetActorEnableCollision(false);
}

void ACoreStageDirector::RevealPart(APuzzleGrabbable* Part)
{
	if (!Part)
	{
		return;
	}
	Part->SetActorHiddenInGame(false);
	Part->SetActorEnableCollision(true);
	if (UStaticMeshComponent* Mesh = Part->GetMesh())
	{
		Mesh->SetSimulatePhysics(true);
	}
}

// ---------------------------------------------------------------- 코어 정지

void ACoreStageDirector::HandleAxisFilled() { PartFilled(0, LogAxis); }
void ACoreStageDirector::HandleTriangleFilled() { PartFilled(1, LogTriangle); }
void ACoreStageDirector::HandleSquareFilled() { PartFilled(2, LogSquare); }

float ACoreStageDirector::LogDuration(const APuzzleRecorder* Log)
{
	// APuzzleRecorder: 제목 2.5초 + 한 줄씩 LineDuration
	return Log ? 2.5f + Log->Lines.Num() * Log->LineDuration : 0.f;
}

void ACoreStageDirector::PartFilled(int32 Index, APuzzleRecorder* Log)
{
	if (bFilled[Index])
	{
		return;
	}
	bFilled[Index] = true;
	++FilledCount;
	static const TCHAR* Names[3] = { TEXT("● 코어 축"), TEXT("▲ 부품"), TEXT("■ 부품") };
	UE_LOG(LogStage5, Log, TEXT("코어 슬롯: %s 끼움 (%d/3)"), Names[Index], FilledCount);
	if (SlotLabels.IsValidIndex(Index) && SlotLabels[Index])
	{
		SlotLabels[Index]->SetSignColor(LabelDone);
	}
	HideMemo();
	if (Log)
	{
		Log->Play();
	}
	if (FilledCount == 3)
	{
		// 마지막 부품의 음성 기록이 끝난 뒤 나연의 마지막 음성
		bEnding = true;
		GetWorldTimerManager().SetTimer(StoryHandle, this, &ACoreStageDirector::PlayFinal, LogDuration(Log) + 1.f, false);
	}
}

void ACoreStageDirector::PlayFinal()
{
	UE_LOG(LogStage5, Log, TEXT("부품 3개 → 나연의 마지막 음성"));
	if (LogFinal)
	{
		LogFinal->Play();
	}
	// 노션: 마지막 음성이 나오는 동안에는 출구가 열리지 않게 해서 끝까지 듣게 함
	GetWorldTimerManager().SetTimer(StoryHandle, this, &ACoreStageDirector::Finish, LogDuration(LogFinal) + 0.5f, false);
}

void ACoreStageDirector::Finish()
{
	UE_LOG(LogStage5, Log, TEXT("5스테이지 클리어 → 코어 빛이 꺼지고 출구 엘리베이터 열림"));
	bFinished = true;
	if (ExitElevator)
	{
		ExitElevator->Arrive();
	}
	if (ExitNote)
	{
		ExitNote->SetSignVisible(true);
	}
}

// ---------------------------------------------------------------- 막힘 방지

void ACoreStageDirector::UpdateHints(float DeltaTime)
{
	FVector ViewLoc;
	FRotator ViewRot;
	if (!PuzzleUtils::GetPlayerView(this, ViewLoc, ViewRot))
	{
		return;
	}

	// 코어 축을 들고 좁은문 앞에서 40초
	if (!bNarrowHintShown && !bFilled[0])
	{
		bool bHoldingAxis = false;
		for (TActorIterator<APerspectiveCube> It(GetWorld()); It && !bHoldingAxis; ++It)
		{
			bHoldingAxis = It->ActorHasTag(AxisTag) && It->IsHeld();
		}
		const bool bNearDoor = FVector::Dist2D(ViewLoc, NarrowDoorLocation) < 300.f;
		NarrowDoorTime = bHoldingAxis && bNearDoor ? NarrowDoorTime + DeltaTime : 0.f;
		if (NarrowDoorTime >= NarrowDoorHintDelay)
		{
			bNarrowHintShown = true;
			ShowMemo(NarrowDoorHintMemo);
		}
	}

	// 발코니에서 90초 진행 없음 (헤드셋이 없으면 카메라가 바닥 높이라 조금 아래까지 발코니로 봄)
	if (!bBalconyHintShown && !bHasCode)
	{
		BalconyTime = ViewLoc.Z > BalconyFloorZ - 20.f ? BalconyTime + DeltaTime : 0.f;
		if (BalconyTime >= BalconyHintDelay)
		{
			bBalconyHintShown = true;
			ShowMemo(BalconyHintMemo, 8.f);
		}
	}

	// 코드 없이 장치 구역 진입
	if (!bNoCodeHintShown && !bHasCode)
	{
		const bool bInZone = ViewLoc.X >= DeviceZoneMin.X && ViewLoc.X <= DeviceZoneMax.X && ViewLoc.Y >= DeviceZoneMin.Y &&
			ViewLoc.Y <= DeviceZoneMax.Y && ViewLoc.Z >= DeviceZoneMin.Z - 50.f && ViewLoc.Z <= DeviceZoneMax.Z;
		if (bInZone)
		{
			bNoCodeHintShown = true;
			ShowMemo(NoCodeHintMemo);
		}
	}
}

void ACoreStageDirector::ShowMemo(const FText& Memo, float Duration)
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
	GetWorldTimerManager().SetTimer(MemoHandle, this, &ACoreStageDirector::HideMemo, Duration, false);
}

void ACoreStageDirector::HideMemo()
{
	if (MemoSign)
	{
		MemoSign->SetSignVisible(false);
	}
}

// ---------------------------------------------------------------- 헤드셋 없이 테스트

void ACoreStageDirector::RunSelfTest()
{
	APawn* Pawn = PuzzleUtils::GetPlayerPawn(this);
	USceneComponent* Cam = PuzzleUtils::GetPlayerCamera(this);
	if (!Pawn || !Cam || !Device || !AxisSlot || !TriangleSocket || !SquareSocket || !ViewA || !ViewB)
	{
		UE_LOG(LogStage5, Warning, TEXT("pcs.Stage5Test: 플레이어 / 카메라 / 퍼즐 부품 없음"));
		return;
	}

	// 카메라를 Eye에 두고 Target을 보게 (헤드셋이 없으면 카메라를 직접 옮긴다)
	auto LookFrom = [Cam](const FVector& Eye, const FVector& Target)
	{
		Cam->SetWorldLocation(Eye);
		Cam->SetWorldRotation((Target - Eye).Rotation());
	};
	auto Overview = [this, LookFrom]()
	{
		const FVector Core = CoreMesh ? CoreMesh->GetActorLocation() : GetActorLocation();
		LookFrom(Core + FVector(-50.f, -780.f, 260.f), Core + FVector(0.f, 0.f, 150.f));
	};
	auto MoveInto = [](AActor* Part, const AActor* Socket, float Out)
	{
		if (Part && Socket)
		{
			Part->SetActorLocation(Socket->GetActorLocation() + Socket->GetActorForwardVector() * Out, false, nullptr, ETeleportType::TeleportPhysics);
		}
	};

	struct FStep
	{
		float Time;
		const TCHAR* Shot;   // nullptr이면 스크린샷 없음
		TFunction<void()> Action;
	};
	const float FinalWait = LogDuration(LogSquare) + LogDuration(LogFinal) + 3.f;
	TArray<FStep> Steps = {
		{ 0.5f, TEXT("S5_0_Hall"), Overview },
		// 시점 구역: ① ② 자리에서 본 모습 → 둘 다 맞춤
		{ 1.5f, TEXT("S5_1_ViewA"), [this, LookFrom]() { LookFrom(ViewA->GetActorLocation() + FVector(0.f, 0.f, 160.f), ViewA->LookTarget); } },
		{ 2.5f, TEXT("S5_2_ViewB"), [this, LookFrom]() { LookFrom(ViewB->GetActorLocation() + FVector(0.f, 0.f, 160.f), ViewB->LookTarget); } },
		{ 3.0f, nullptr, [this]() { ViewA->Solve(); ViewB->Solve(); } },
		{ 4.5f, TEXT("S5_3_Locker"), [this, LookFrom]() {
			if (TrianglePart) { LookFrom(TrianglePart->GetActorLocation() + FVector(0.f, -150.f, 40.f), TrianglePart->GetActorLocation()); } } },
		// 원근법 구역: 코어 축을 슬롯 규격(1.2m)으로 맞춰 슬롯 앞에 놓음
		{ 5.0f, nullptr, [this]() {
			for (TActorIterator<APerspectiveCube> It(GetWorld()); It; ++It)
			{
				if (It->ActorHasTag(AxisTag))
				{
					It->SetActorScale3D(It->GetActorScale3D() * (AxisSlot->SlotSize / FMath::Max(It->GetSizeCm(), 1.f)));
					It->SetActorRotation(AxisSlot->GetActorRotation());
					It->GetMesh()->SetSimulatePhysics(false);
					It->SetActorLocation(AxisSlot->GetActorLocation() + AxisSlot->GetActorForwardVector() * 30.f, false, nullptr,
						ETeleportType::TeleportPhysics);
					break;
				}
			} } },
		{ 5.5f, nullptr, [this, MoveInto]() { MoveInto(TrianglePart, TriangleSocket, 15.f); } },
		// 장치 구역: 장착 → ◆ ● ▲ ■ (중간에 한 번 틀려서 램프가 꺼지는지 확인)
		{ 6.0f, nullptr, [this]() { Device->Equip(); } },
		{ 6.5f, nullptr, [this]() { Device->PressLeft(); Device->PressLeft(); Device->PressLeft(); Device->PressRight(); } },   // ◆
		{ 7.0f, nullptr, [this]() { Device->PressLeft(); Device->PressRight(); } },                                             // ● (2/4)
		{ 7.5f, TEXT("S5_4_PanelProgress"), [this, LookFrom]() {
			if (PanelLamps.Num() > 0 && PanelLamps[0]) { LookFrom(Device->GetActorLocation() + FVector(-110.f, 0.f, 90.f), PanelLamps[0]->GetActorLocation()); } } },
		{ 8.0f, nullptr, [this]() { Device->PressLeft(); Device->PressLeft(); Device->PressRight(); } },                         // ■ 틀림 → 모두 꺼짐
		{ 8.5f, TEXT("S5_5_PanelWrong"), [] {} },
		{ 9.0f, nullptr, [this]() { Device->PressLeft(); Device->PressRight(); } },                                             // ◆
		{ 9.5f, nullptr, [this]() { Device->PressLeft(); Device->PressRight(); } },                                             // ●
		{ 10.0f, nullptr, [this]() { Device->PressLeft(); Device->PressRight(); } },                                            // ▲
		{ 10.5f, nullptr, [this]() { Device->PressLeft(); Device->PressRight(); } },                                            // ■ → 해제
		{ 11.5f, TEXT("S5_6_SquarePart"), [] {} },
		{ 12.0f, nullptr, [this, MoveInto]() { MoveInto(SquarePart, SquareSocket, 15.f); } },
		{ 13.0f, TEXT("S5_7_CoreFull"), Overview },
		{ 13.0f + FinalWait, TEXT("S5_8_Ending"), Overview },
	};

	for (int32 i = 0; i < Steps.Num(); ++i)
	{
		FTimerHandle Handle;
		const FStep Step = Steps[i];
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Step, i]()
		{
			Step.Action();
			UE_LOG(LogStage5, Log, TEXT("pcs.Stage5Test %02d: 코드 %d / 슬롯 ● %d ▲ %d ■ %d / 장치 장착 %d 해제 %d / 엔딩 %d 끝 %d"),
				i, bHasCode, AxisSlot->IsFilled(), TriangleSocket->IsFilled(), SquareSocket->IsFilled(), Device->IsEquipped(),
				Device->IsUnlocked(), bEnding, bFinished);
			if (Step.Shot)
			{
				FScreenshotRequest::RequestScreenshot(Step.Shot, false, false);
			}
		}), Step.Time, false);
	}
}

static FAutoConsoleCommandWithWorld GStage5TestCommand(
	TEXT("pcs.Stage5Test"),
	TEXT("5스테이지를 헤드셋 없이 처음부터 끝까지 자동으로 풀어 보고 스크린샷을 남긴다"),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
	{
		for (TActorIterator<ACoreStageDirector> It(World); It; ++It)
		{
			TWeakObjectPtr<ACoreStageDirector> Director = *It;
			DebugShots::WhenReady(World, [Director]() { if (Director.IsValid()) { Director->RunSelfTest(); } });
			return;
		}
		UE_LOG(LogStage5, Warning, TEXT("pcs.Stage5Test: 이 맵에 5스테이지 진행 담당이 없음"));
	}));
