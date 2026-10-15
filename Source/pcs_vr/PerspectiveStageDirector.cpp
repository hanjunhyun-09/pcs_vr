#include "PerspectiveStageDirector.h"

#include "Components/LightComponent.h"
#include "Engine/Light.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "PerspectiveCube.h"
#include "PuzzleButton.h"
#include "PuzzleDoor.h"
#include "PuzzlePressurePlate.h"
#include "PuzzleSign.h"
#include "PuzzleUtils.h"
#include "TimerManager.h"
#include "VRSmoothLocomotionSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogStage2, Log, All);

APerspectiveStageDirector::APerspectiveStageDirector()
{
	PrimaryActorTick.bCanEverTick = true;

	ArrivalMemo = FText::FromString(TEXT("통로 전원 스위치는 높은 선반 위에 있어."));
	ShelfHintMemo = FText::FromString(TEXT("멀리 보고 놓으면 커져. 1층 엘리베이터 앞에서처럼."));
	ClimbHintMemo = FText::FromString(TEXT("큰 큐브는 밟고 올라갈 수 있어.\n큐브 위로 텔레포트해 봐."));
	BoxHintMemo = FText::FromString(TEXT("가까이선 너무 커. 멀리서 보면 작아 보이지 않아?"));
	PlateHintMemo = FText::FromString(TEXT("가벼운 건 안 돼. 크고 무거운 게 필요해."));
	ClearMemo = FText::FromString(TEXT("그 노트, 네 글씨 맞지?\n넌 그날 이미 알고 있었어."));
	LockedBoxMemo = FText::FromString(TEXT("통로가 깜깜해서 상자가 잘 안 보여.\n선반 위 스위치부터."));
}

void APerspectiveStageDirector::BeginPlay()
{
	Super::BeginPlay();

	if (UVRSmoothLocomotionSubsystem* Locomotion = GetWorld()->GetSubsystem<UVRSmoothLocomotionSubsystem>())
	{
		Locomotion->bBlockLowObstacles = true;
	}

	// 통로 조명: 레벨에 넣어 둔 밝기를 기억하고 처음엔 끈다
	for (AActor* A : CorridorLights)
	{
		if (ALight* L = Cast<ALight>(A))
		{
			CorridorLightIntensity.Add(L, L->GetLightComponent()->Intensity);
		}
	}
	SetCorridorLights(false);

	if (GiantBox)
	{
		GiantBox->bRayGrabEnabled = false;
		GiantBox->OnGrabRefused.AddDynamic(this, &APerspectiveStageDirector::HandleBoxRefused);
	}
	if (ShelfSwitch) { ShelfSwitch->OnPressed.AddDynamic(this, &APerspectiveStageDirector::HandleShelfSwitch); }
	if (Plate) { Plate->OnPressed.AddDynamic(this, &APerspectiveStageDirector::HandlePlatePressed); }

	// 눈앞에 띄우는 지도 메모 (1스테이지와 같은 모양)
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

	GetWorldTimerManager().SetTimer(ArrivalHandle, this, &APerspectiveStageDirector::ShowArrivalMemo, ArrivalMemoDelay, false);
}

void APerspectiveStageDirector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bCleared)
	{
		return;
	}
	if (!bSwitchPressed)
	{
		UpdateShelfHints(DeltaTime);
		return;
	}

	// 난간 옆 디스펜서가 상자를 새로 내보내면 원래 상자는 사라질 수 있다
	APerspectiveCube* Box = IsValid(GiantBox) ? GiantBox.Get() : nullptr;
	if (!bBoxShrunk)
	{
		if (!Box)
		{
			return;
		}
		if (!Box->IsHeld() && Box->GetScaleRatio() <= BoxShrunkRatio)
		{
			bBoxShrunk = true;
			UE_LOG(LogStage2, Log, TEXT("2-2: 거대 상자를 줄임 (배율 %.2f)"), Box->GetScaleRatio());
			GetWorldTimerManager().SetTimer(PlateHintHandle, this, &APerspectiveStageDirector::ShowPlateHint, PlateHintDelay, false);
			return;
		}
		// 2-2 막힘 방지: 상자 앞에서 계속 서성임
		FVector ViewLoc;
		FRotator ViewRot;
		if (!bBoxHintShown && PuzzleUtils::GetPlayerView(this, ViewLoc, ViewRot))
		{
			NearBoxTime = FVector::Dist2D(ViewLoc, Box->GetActorLocation()) < BoxHintRadius ? NearBoxTime + DeltaTime : 0.f;
			if (NearBoxTime >= BoxHintDelay)
			{
				bBoxHintShown = true;
				ShowMemo(BoxHintMemo);
			}
		}
	}
}

void APerspectiveStageDirector::HandleShelfSwitch()
{
	if (bSwitchPressed)
	{
		return;
	}
	bSwitchPressed = true;
	UE_LOG(LogStage2, Log, TEXT("2-1: 통로 전원 스위치"));
	SetCorridorLights(true);
	if (GiantBox) { GiantBox->bRayGrabEnabled = true; }
}

void APerspectiveStageDirector::HandlePlatePressed()
{
	if (bCleared)
	{
		return;
	}
	bCleared = true;
	UE_LOG(LogStage2, Log, TEXT("2-3: 압력판 눌림 → 출구 문"));
	GetWorldTimerManager().ClearTimer(PlateHintHandle);
	if (ExitDoor) { ExitDoor->Open(); }
	ShowMemo(ClearMemo, 8.f);
}

void APerspectiveStageDirector::HandleBoxRefused()
{
	// 손으로 잡거나 레이로 잡으려 했는데 안 됨 → 지금 단계에 맞는 메모 (8초에 한 번만)
	const double Now = GetWorld()->GetTimeSeconds();
	if (bCleared || bBoxShrunk || Now - LastRefusedMemoTime < 8.0)
	{
		return;
	}
	LastRefusedMemoTime = Now;
	if (!bSwitchPressed)
	{
		ShowMemo(LockedBoxMemo);
	}
	else
	{
		bBoxHintShown = true;
		ShowMemo(BoxHintMemo);
	}
}

void APerspectiveStageDirector::ShowArrivalMemo()
{
	if (!bSwitchPressed)
	{
		ShowMemo(ArrivalMemo, 7.f);
	}
}

void APerspectiveStageDirector::UpdateShelfHints(float DeltaTime)
{
	StageTime += DeltaTime;

	// 디스펜서 큐브 등을 1m 이상(밟고 설 수 있는 크기)으로 키웠는지
	if (!bCubeGrown)
	{
		for (TActorIterator<APerspectiveCube> It(GetWorld()); It; ++It)
		{
			if (*It != GiantBox && !It->IsHeld() && It->GetSizeCm() >= 100.f)
			{
				bCubeGrown = true;
				UE_LOG(LogStage2, Log, TEXT("2-1: 큐브를 %.0fcm로 키움"), It->GetSizeCm());
				break;
			}
		}
	}

	if (!bCubeGrown)
	{
		if (!bShelfHintShown && StageTime >= ShelfHintDelay)
		{
			bShelfHintShown = true;
			ShowMemo(ShelfHintMemo);
		}
		return;
	}

	// 키웠는데 스위치 근처(2.5m)에 못 오고 있음 → 밟고 올라가라는 힌트
	FVector ViewLoc;
	FRotator ViewRot;
	if (!bClimbHintShown && ShelfSwitch && PuzzleUtils::GetPlayerView(this, ViewLoc, ViewRot))
	{
		const bool bNearSwitch = FVector::Dist(ViewLoc, ShelfSwitch->GetActorLocation()) < 250.f;
		GrownCubeTime = bNearSwitch ? 0.f : GrownCubeTime + DeltaTime;
		if (GrownCubeTime >= ClimbHintDelay)
		{
			bClimbHintShown = true;
			ShowMemo(ClimbHintMemo, 8.f);
		}
	}
}

void APerspectiveStageDirector::ShowPlateHint()
{
	if (!bCleared)
	{
		ShowMemo(PlateHintMemo);
	}
}

void APerspectiveStageDirector::SetCorridorLights(bool bOn)
{
	for (const TPair<TWeakObjectPtr<AActor>, float>& Pair : CorridorLightIntensity)
	{
		if (ALight* L = Cast<ALight>(Pair.Key.Get()))
		{
			L->GetLightComponent()->SetIntensity(bOn ? Pair.Value : 0.f);
		}
	}
}

void APerspectiveStageDirector::ShowMemo(const FText& Memo, float Duration)
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
	GetWorldTimerManager().SetTimer(MemoHandle, this, &APerspectiveStageDirector::HideMemo, Duration, false);
}

void APerspectiveStageDirector::HideMemo()
{
	if (MemoSign)
	{
		MemoSign->SetSignVisible(false);
	}
}
