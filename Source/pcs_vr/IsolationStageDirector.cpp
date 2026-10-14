#include "IsolationStageDirector.h"

#include "Components/LightComponent.h"
#include "Engine/Light.h"
#include "Engine/World.h"
#include "PerspectiveCube.h"
#include "PuzzleButton.h"
#include "PuzzleCardReader.h"
#include "PuzzleDoor.h"
#include "PuzzleElevator.h"
#include "PuzzleLever.h"
#include "PuzzleSign.h"
#include "PuzzleTeleportPad.h"
#include "PuzzleUtils.h"
#include "TimerManager.h"
#include "VRSmoothLocomotionSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogStage1, Log, All);

AIsolationStageDirector::AIsolationStageDirector()
{
	PrimaryActorTick.bCanEverTick = true;

	CardHintMemo = FText::FromString(TEXT("참가자 데려오던 직원들, 카드 자주 흘렸어. 바닥 봐봐."));
	CubeGrownMemo = FText::FromString(TEXT("봤지? 코어가 아직 살아 있다는 증거야.\n엘리베이터 타고 원근 실험실로 가."));
}

void AIsolationStageDirector::BeginPlay()
{
	Super::BeginPlay();

	// 1스테이지부터는 침대·책상 같은 낮은 물체에도 이동이 막힌다
	if (UVRSmoothLocomotionSubsystem* Locomotion = GetWorld()->GetSubsystem<UVRSmoothLocomotionSubsystem>())
	{
		Locomotion->bBlockLowObstacles = true;
	}

	// 방 조명: 레벨에 넣어 둔 밝기를 기억하고 처음엔 끈다
	for (AActor* A : RoomLights)
	{
		if (ALight* L = Cast<ALight>(A))
		{
			RoomLightIntensity.Add(L, L->GetLightComponent()->Intensity);
		}
	}
	SetRoomLights(false);

	if (EntranceReader) { EntranceReader->SetPowered(true); EntranceReader->OnAccepted.AddDynamic(this, &AIsolationStageDirector::HandleEntranceAccepted); }
	if (Pad) { Pad->OnReached.AddDynamic(this, &AIsolationStageDirector::HandlePadReached); }
	if (Lever) { Lever->OnActivated.AddDynamic(this, &AIsolationStageDirector::HandleLeverPulled); }
	if (ExitReader) { ExitReader->SetPowered(false); ExitReader->OnAccepted.AddDynamic(this, &AIsolationStageDirector::HandleExitAccepted); ExitReader->OnDenied.AddDynamic(this, &AIsolationStageDirector::HandleExitDenied); }
	if (ExitButton) { ExitButton->OnPressed.AddDynamic(this, &AIsolationStageDirector::HandleExitButton); }
	if (Cube) { Cube->OnGrown.AddDynamic(this, &AIsolationStageDirector::HandleCubeGrown); }

	// 눈앞에 띄우는 지도 메모 (45cm x 10cm, 눈앞 90cm)
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
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

void AIsolationStageDirector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bEnteredRoom && RoomEntryVolume)
	{
		FVector ViewLoc;
		FRotator ViewRot;
		if (PuzzleUtils::GetPlayerView(this, ViewLoc, ViewRot))
		{
			FVector Origin, Extent;
			RoomEntryVolume->GetActorBounds(false, Origin, Extent);
			if (FBox(Origin - Extent, Origin + Extent).IsInside(ViewLoc))
			{
				bEnteredRoom = true;
				UE_LOG(LogStage1, Log, TEXT("격리실 입장 -> 뒤의 문 닫힘"));
				if (RoomEntryDoor) { RoomEntryDoor->Close(); }
				SetActorTickEnabled(false);
			}
		}
	}
}

void AIsolationStageDirector::HandleEntranceAccepted()
{
	UE_LOG(LogStage1, Log, TEXT("출입 승인 -> 출입문 열림"));
	if (EntranceDoor) { EntranceDoor->Open(); }
}

void AIsolationStageDirector::HandlePadReached()
{
	UE_LOG(LogStage1, Log, TEXT("발판 도착"));
}

void AIsolationStageDirector::HandleLeverPulled()
{
	UE_LOG(LogStage1, Log, TEXT("전원 레버 -> 조명 켜짐"));
	for (AActor* A : EmergencyLights)
	{
		if (ALight* L = Cast<ALight>(A))
		{
			L->GetLightComponent()->SetVisibility(false);
		}
	}
	// 깜빡깜빡 몇 번 하다가 켜짐
	FlickerCount = 0;
	GetWorldTimerManager().SetTimer(FlickerHandle, this, &AIsolationStageDirector::FlickerStep, 0.12f, true);
}

void AIsolationStageDirector::FlickerStep()
{
	++FlickerCount;
	const bool bOn = (FlickerCount % 2) == 1;
	SetRoomLights(bOn);
	if (FlickerCount >= 7)
	{
		GetWorldTimerManager().ClearTimer(FlickerHandle);
		SetRoomLights(true);
		for (APuzzleSign* Sign : RevealOnPower)
		{
			if (Sign) { Sign->SetSignVisible(true); }
		}
		if (ExitReader) { ExitReader->SetPowered(true); }
	}
}

void AIsolationStageDirector::SetRoomLights(bool bOn)
{
	for (const TPair<TWeakObjectPtr<AActor>, float>& Pair : RoomLightIntensity)
	{
		if (ALight* L = Cast<ALight>(Pair.Key.Get()))
		{
			L->GetLightComponent()->SetIntensity(bOn ? Pair.Value : 0.f);
		}
	}
}

void AIsolationStageDirector::ShowCardHint()
{
	if (ExitReader && !ExitReader->IsAccepted())
	{
		ShowMemo(CardHintMemo, 7.f);
	}
}

void AIsolationStageDirector::HandleExitDenied()
{
	// 노션: '권한 없음'이 뜨고 30초 동안 진행이 없으면 지도 메모 힌트
	if (!GetWorldTimerManager().IsTimerActive(CardHintHandle))
	{
		GetWorldTimerManager().SetTimer(CardHintHandle, this, &AIsolationStageDirector::ShowCardHint, CardHintDelay, false);
	}
}

void AIsolationStageDirector::HandleExitAccepted()
{
	UE_LOG(LogStage1, Log, TEXT("관리자 카드 승인 -> 봉쇄 해제 버튼"));
	GetWorldTimerManager().ClearTimer(CardHintHandle);
	if (ExitButton) { ExitButton->PopOut(); }
}

void AIsolationStageDirector::HandleExitButton()
{
	UE_LOG(LogStage1, Log, TEXT("봉쇄 해제 -> 문 열림"));
	if (ExitDoor) { ExitDoor->Open(); }
	GetWorldTimerManager().SetTimer(ElevatorHandle, this, &AIsolationStageDirector::ElevatorFallback, ElevatorFallbackDelay, false);
}

void AIsolationStageDirector::HandleCubeGrown()
{
	UE_LOG(LogStage1, Log, TEXT("큐브가 커짐 -> 엘리베이터 도착"));
	GetWorldTimerManager().ClearTimer(ElevatorHandle);
	ShowMemo(CubeGrownMemo, 6.f);
	if (Elevator) { Elevator->Arrive(); }
}

void AIsolationStageDirector::ElevatorFallback()
{
	UE_LOG(LogStage1, Log, TEXT("큐브를 안 키워도 시간이 지나 엘리베이터 도착 (막힘 방지)"));
	if (Elevator) { Elevator->Arrive(); }
}

void AIsolationStageDirector::ShowMemo(const FText& Memo, float Duration)
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
	GetWorldTimerManager().SetTimer(MemoHandle, this, &AIsolationStageDirector::HideMemo, Duration, false);
}

void AIsolationStageDirector::HideMemo()
{
	if (MemoSign)
	{
		MemoSign->SetSignVisible(false);
	}
}
