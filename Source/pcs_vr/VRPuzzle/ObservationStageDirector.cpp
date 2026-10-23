#include "ObservationStageDirector.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "PuzzleBlueprintSlot.h"
#include "PuzzleDoor.h"
#include "PuzzleKeypad.h"
#include "PuzzleSign.h"
#include "PuzzleUtils.h"
#include "PuzzleViewPoint.h"
#include "TimerManager.h"
#include "VRSmoothLocomotionSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogStage3, Log, All);

namespace
{
	const FLinearColor LockClosedColor(1.f, 0.3f, 0.2f);
	const FLinearColor LockOpenColor(0.3f, 1.f, 0.45f);
	const TCHAR* const LockNames[] = { TEXT("◯"), TEXT("▣") };
}

AObservationStageDirector::AObservationStageDirector()
{
	PrimaryActorTick.bCanEverTick = true;

	PaintHintMemo = FText::FromString(TEXT("바닥의 발자국, 누가 일부러 그려 둔 것 같지 않아?"));
	PipeHintMemo = FText::FromString(TEXT("바닥의 눈금 선이 만나는 곳.\n우리 때 관측 위치는 항상 거기였어."));
	SlotHintMemo = FText::FromString(TEXT("조각을 들고 빈 칸이랑 크기가 같아 보일 때 놓아 봐."));
	ClearMemo = FText::FromString(TEXT("그날 네가 멈추자고 한 거, 나도 들었어."));
}

void AObservationStageDirector::BeginPlay()
{
	Super::BeginPlay();

	if (UVRSmoothLocomotionSubsystem* Locomotion = GetWorld()->GetSubsystem<UVRSmoothLocomotionSubsystem>())
	{
		Locomotion->bBlockLowObstacles = true;
	}

	if (Keypad) { Keypad->OnSolved.AddDynamic(this, &AObservationStageDirector::HandleKeypadSolved); }
	if (PipeView) { PipeView->OnSolved.AddDynamic(this, &AObservationStageDirector::HandlePipesSolved); }
	if (Slot) { Slot->OnFilled.AddDynamic(this, &AObservationStageDirector::HandleSlotFilled); }
	SetLock(0, false);
	SetLock(1, false);

	// 눈앞에 띄우는 지도 메모 (1·2스테이지와 같은 모양)
	MemoSign = GetWorld()->SpawnActorDeferred<APuzzleSign>(APuzzleSign::StaticClass(), FTransform::Identity, this);
	if (MemoSign)
	{
		MemoSign->FontSize = 36;
		MemoSign->DrawSize = FVector2D(900.f, 200.f);
		MemoSign->PixelsPerCm = 20.f;
		MemoSign->TextColor = FLinearColor(1.f, 0.95f, 0.8f);
		MemoSign->BackgroundColor = FLinearColor(0.05f, 0.08f, 0.1f, 0.85f);
		MemoSign->bVisibleAtStart = false;
		MemoSign->FinishSpawning(FTransform::Identity);
	}
}

void AObservationStageDirector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bCleared)
	{
		return;
	}

	// 3-1: 페인트 글씨도 못 찾고 키패드도 못 풂
	if (!bShutterOpen)
	{
		StageTime += DeltaTime;
		const bool bPaintSeen = PaintView && PaintView->IsSolved();
		if (!bPaintHintShown && !bPaintSeen && StageTime >= PaintHintDelay)
		{
			bPaintHintShown = true;
			ShowMemo(PaintHintMemo);
		}
		return;
	}

	// 3-2 / 3-3: 셔터가 열린 뒤 (순서 상관없음)
	InnerTime += DeltaTime;
	if (!bPipesSolved && !bPipeHintShown && InnerTime >= PipeHintDelay)
	{
		bPipeHintShown = true;
		ShowMemo(PipeHintMemo, 8.f);
	}
	// 3-3 힌트는 파이프를 맞춘 뒤부터 센다 (먼저 도면을 풀면 힌트 없음)
	if (bPipesSolved && !bSlotFilled)
	{
		SlotTime += DeltaTime;
		if (!bSlotHintShown && SlotTime >= SlotHintDelay)
		{
			bSlotHintShown = true;
			ShowMemo(SlotHintMemo, 8.f);
		}
	}
}

void AObservationStageDirector::HandleKeypadSolved()
{
	if (bShutterOpen)
	{
		return;
	}
	bShutterOpen = true;
	UE_LOG(LogStage3, Log, TEXT("3-1: 비밀번호 1114 → 셔터 열림"));
	if (Shutter) { Shutter->Open(); }
}

void AObservationStageDirector::HandlePipesSolved()
{
	if (bPipesSolved)
	{
		return;
	}
	bPipesSolved = true;
	UE_LOG(LogStage3, Log, TEXT("3-2: 파이프 원 완성 → 잠금 1 해제"));
	if (RingGroove) { RingGroove->SetSignColor(FLinearColor(1.f, 0.6f, 0.15f)); }
	SetLock(0, true);
	CheckClear();
}

void AObservationStageDirector::HandleSlotFilled()
{
	if (bSlotFilled)
	{
		return;
	}
	bSlotFilled = true;
	UE_LOG(LogStage3, Log, TEXT("3-3: 도면 조각 끼움 → 잠금 2 해제"));
	if (CoreDiagram) { CoreDiagram->SetSignColor(FLinearColor(0.5f, 0.85f, 1.f)); }
	// 노션: "도면이 완성되면 코어 내부 구조가 빛나며". 도면의 선들은 CoreDiagram 태그가 붙은 빛 묶음(APuzzleViewPoint)
	for (TActorIterator<APuzzleViewPoint> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(TEXT("CoreDiagram")))
		{
			It->Solve();
		}
	}
	SetLock(1, true);
	CheckClear();
}

void AObservationStageDirector::SetLock(int32 Index, bool bOpen)
{
	if (!LockLamps.IsValidIndex(Index) || !LockLamps[Index] || Index >= UE_ARRAY_COUNT(LockNames))
	{
		return;
	}
	LockLamps[Index]->SetSignText(FText::FromString(FString::Printf(TEXT("%s  %s"), LockNames[Index], bOpen ? TEXT("해제") : TEXT("잠김"))));
	LockLamps[Index]->SetSignColor(bOpen ? LockOpenColor : LockClosedColor);
}

void AObservationStageDirector::CheckClear()
{
	if (bCleared || !bPipesSolved || !bSlotFilled)
	{
		return;
	}
	bCleared = true;
	UE_LOG(LogStage3, Log, TEXT("3스테이지 클리어 → 출구 문"));
	if (ExitDoor) { ExitDoor->Open(); }
	ShowMemo(ClearMemo, 8.f);
}

void AObservationStageDirector::ShowMemo(const FText& Memo, float Duration)
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
	GetWorldTimerManager().SetTimer(MemoHandle, this, &AObservationStageDirector::HideMemo, Duration, false);
}

void AObservationStageDirector::HideMemo()
{
	if (MemoSign)
	{
		MemoSign->SetSignVisible(false);
	}
}
