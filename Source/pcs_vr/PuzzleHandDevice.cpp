#include "PuzzleHandDevice.h"

#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "IXRTrackingSystem.h"
#include "PuzzleTextWidget.h"
#include "PuzzleToggleTarget.h"
#include "PuzzleUtils.h"
#include "UObject/ConstructorHelpers.h"
#include "VRInteractionSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogHandDevice, Log, All);

namespace
{
	const TCHAR* const Symbols[] = { TEXT("●"), TEXT("▲"), TEXT("■"), TEXT("◆") };

	// 거치대 칸 가운데 (액터 기준). 플레이어는 +X 쪽에서 -X를 보고 서므로 플레이어 왼쪽 = +Y
	const FVector LeftCradle(0.f, 18.f, 8.f);
	const FVector RightCradle(0.f, -18.f, 8.f);

	// 장치 윗면 표시창: 장치 윗면(+Z)을 보고, 글자 위쪽이 손가락 쪽(+X)을 향하게
	const FRotator TopDisplayRotation(90.f, 180.f, 0.f);

	const FLinearColor DisplayBg(0.01f, 0.03f, 0.04f, 1.f);
	const FLinearColor DialColor(0.35f, 0.9f, 1.f);
	const FLinearColor DimColor(0.15f, 0.3f, 0.35f);
	const FLinearColor ProgressColor(0.3f, 1.f, 0.45f);
	const FLinearColor ButtonColor(0.85f, 0.3f, 0.2f);

	bool IsHeadsetActive()
	{
		return GEngine && GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowed();
	}
}

FString APuzzleHandDevice::SymbolOf(int32 Index)
{
	return Index >= 0 && Index < UE_ARRAY_COUNT(Symbols) ? Symbols[Index] : TEXT("?");
}

APuzzleHandDevice::APuzzleHandDevice()
{
	PrimaryActorTick.bCanEverTick = true;

	Engraving = FText::FromString(TEXT("피험 장치 01 — 착용자: 오나연"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);

	// 거치대: 받침 30 x 60cm, 칸마다 손을 밀어 넣는 터널 (뒤벽 + 지붕)
	AddBox(TEXT("RackBase"), Root, FVector(0.f, 0.f, 2.f), FVector(30.f, 60.f, 4.f));
	for (const FVector& Cradle : { LeftCradle, RightCradle })
	{
		const bool bLeft = Cradle.Y > 0.f;
		AddBox(bLeft ? TEXT("CradleL_Pad") : TEXT("CradleR_Pad"), Root, FVector(0.f, Cradle.Y, 4.5f), FVector(20.f, 17.f, 1.f));
		AddBox(bLeft ? TEXT("CradleL_Back") : TEXT("CradleR_Back"), Root, FVector(-10.f, Cradle.Y, 11.f), FVector(3.f, 19.f, 14.f));
		AddBox(bLeft ? TEXT("CradleL_Roof") : TEXT("CradleR_Roof"), Root, FVector(-2.f, Cradle.Y, 18.f), FVector(16.f, 19.f, 2.f));
	}

	RackInteract = CreateDefaultSubobject<UPuzzleInteractComponent>(TEXT("RackInteract"));
	RackInteract->SetupAttachment(Root);
	RackInteract->SetRelativeLocation(FVector(0.f, 0.f, 10.f));
	RackInteract->InteractRadius = 30.f;

	// 장치 두 개: 손목에 감는 14 x 9 x 5cm 상자. 로컬 +X = 손가락 쪽
	LeftDevice = CreateDefaultSubobject<USceneComponent>(TEXT("LeftDevice"));
	LeftDevice->SetupAttachment(Root);
	LeftDevice->SetMobility(EComponentMobility::Movable);
	LeftDevice->SetRelativeLocationAndRotation(LeftCradle + FVector(0.f, 0.f, -1.f), FRotator(0.f, 180.f, 0.f));

	RightDevice = CreateDefaultSubobject<USceneComponent>(TEXT("RightDevice"));
	RightDevice->SetupAttachment(Root);
	RightDevice->SetMobility(EComponentMobility::Movable);
	RightDevice->SetRelativeLocationAndRotation(RightCradle + FVector(0.f, 0.f, -1.f), FRotator(0.f, 180.f, 0.f));

	DeviceParts.Add(AddBox(TEXT("LeftBody"), LeftDevice, FVector::ZeroVector, FVector(14.f, 9.f, 5.f)));
	DeviceParts.Add(AddBox(TEXT("LeftDial"), LeftDevice, FVector(5.5f, 0.f, 2.8f), FVector(2.f, 7.f, 1.f)));
	DeviceParts.Add(AddBox(TEXT("RightBody"), RightDevice, FVector::ZeroVector, FVector(14.f, 9.f, 5.f)));
	RightButton = AddBox(TEXT("RightButton"), RightDevice, FVector(4.f, 0.f, 3.2f), FVector(4.f, 4.f, 1.5f));

	// 왼손 표시창 6 x 6cm (기호 하나), 오른손 표시창 7.2 x 2.4cm (진행 점)
	DialDisplay = AddDisplay(TEXT("DialDisplay"), LeftDevice, FVector(-1.f, 0.f, 2.6f), FVector2D(200.f, 200.f), 0.03f);
	ProgressDisplay = AddDisplay(TEXT("ProgressDisplay"), RightDevice, FVector(-3.f, 0.f, 2.6f), FVector2D(240.f, 80.f), 0.03f);

	// 풀린 뒤 거치대 받침 앞쪽에 보이는 각인 (24 x 3cm, 위를 보고 플레이어 쪽에서 읽힘)
	EngravingDisplay = AddDisplay(TEXT("EngravingDisplay"), Root, FVector(11.f, 0.f, 4.2f), FVector2D(600.f, 80.f), 0.04f);
	EngravingDisplay->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
}

UStaticMeshComponent* APuzzleHandDevice::AddBox(const TCHAR* Name, USceneComponent* Parent, const FVector& Center, const FVector& SizeCm)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMeshComponent* Box = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Box->SetupAttachment(Parent);
	Box->SetStaticMesh(CubeMesh.Object);
	Box->SetMobility(EComponentMobility::Movable);
	Box->SetRelativeLocation(Center);
	Box->SetRelativeScale3D(SizeCm / 100.f);
	// 손에 붙는 장치가 몸이나 벽에 걸리지 않게, 거치대도 손이 들어가야 하니 충돌 없음
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->SetCanEverAffectNavigation(false);
	if (Parent == Root)
	{
		RackParts.Add(Box);
	}
	return Box;
}

UWidgetComponent* APuzzleHandDevice::AddDisplay(const TCHAR* Name, USceneComponent* Parent, const FVector& Location, const FVector2D& DrawSize, float Scale)
{
	UWidgetComponent* W = CreateDefaultSubobject<UWidgetComponent>(Name);
	W->SetupAttachment(Parent);
	W->SetMobility(EComponentMobility::Movable);
	W->SetRelativeLocationAndRotation(Location, TopDisplayRotation);
	W->SetRelativeScale3D(FVector(Scale));
	W->SetWidgetSpace(EWidgetSpace::World);
	W->SetWidgetClass(UPuzzleTextWidget::StaticClass());
	W->SetDrawSize(DrawSize);
	// 장착 전에는 화면 밖(책상 위)에 있다가 손/카메라에 붙으므로, 화면 밖에서도 글자를 그려 둔다 (안 그러면 처음에 까맣게 보임)
	W->SetTickWhenOffscreen(true);
	W->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	W->SetCanEverAffectNavigation(false);
	return W;
}

void APuzzleHandDevice::BeginPlay()
{
	Super::BeginPlay();

	for (UStaticMeshComponent* Part : RackParts)
	{
		PuzzleUtils::ApplySolidColor(Part, FLinearColor(0.22f, 0.24f, 0.26f));
	}
	for (UStaticMeshComponent* Part : DeviceParts)
	{
		PuzzleUtils::ApplySolidColor(Part, FLinearColor(0.55f, 0.57f, 0.6f), 0.05f);
	}
	ButtonMID = PuzzleUtils::ApplySolidColor(RightButton, ButtonColor, 0.4f);

	LeftRestTM = LeftDevice->GetRelativeTransform();
	RightRestTM = RightDevice->GetRelativeTransform();

	RackInteract->OnInteract.AddDynamic(this, &APuzzleHandDevice::HandleRackInteract);
	for (UWidgetComponent* W : { DialDisplay.Get(), ProgressDisplay.Get(), EngravingDisplay.Get() })
	{
		W->InitWidget();
	}
	if (UPuzzleTextWidget* W = Cast<UPuzzleTextWidget>(EngravingDisplay->GetUserWidgetObject()))
	{
		W->SetContent(Engraving, 40, FLinearColor(0.75f, 0.72f, 0.65f), FLinearColor(0.f, 0.f, 0.f, 0.f));
	}
	EngravingDisplay->SetVisibility(false);
	UpdateDisplays();
}

void APuzzleHandDevice::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bEquipped)
	{
		if (UVRInteractionSubsystem* Interaction = GetWorld()->GetSubsystem<UVRInteractionSubsystem>())
		{
			Interaction->SetActiveDevice(nullptr);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void APuzzleHandDevice::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bEquipped && !bUnlocked && IsHeadsetActive() && AreHandsInCradles())
	{
		Equip();
	}

	if (ButtonFlash > 0.f)
	{
		ButtonFlash = FMath::Max(0.f, ButtonFlash - DeltaTime);
		PuzzleUtils::SetSolidColor(ButtonMID, ButtonColor, ButtonFlash > 0.f ? 3.f : 0.4f);
	}
}

bool APuzzleHandDevice::AreHandsInCradles() const
{
	const APawn* Pawn = PuzzleUtils::GetPlayerPawn(this);
	const USceneComponent* LeftGrip = PuzzleUtils::FindPawnComponent(Pawn, TEXT("MotionControllerLeftGrip"));
	const USceneComponent* RightGrip = PuzzleUtils::FindPawnComponent(Pawn, TEXT("MotionControllerRightGrip"));
	if (!LeftGrip || !RightGrip)
	{
		return false;
	}
	const FTransform& TM = GetActorTransform();
	return FVector::Dist(LeftGrip->GetComponentLocation(), TM.TransformPosition(LeftCradle)) <= CradleRadius
		&& FVector::Dist(RightGrip->GetComponentLocation(), TM.TransformPosition(RightCradle)) <= CradleRadius;
}

void APuzzleHandDevice::HandleRackInteract(AActor* InteractInstigator)
{
	Equip();
}

void APuzzleHandDevice::Equip()
{
	if (bEquipped || bUnlocked)
	{
		return;
	}
	bEquipped = true;
	RackInteract->bInteractEnabled = false;
	AttachDevices();
	if (UVRInteractionSubsystem* Interaction = GetWorld()->GetSubsystem<UVRInteractionSubsystem>())
	{
		Interaction->SetActiveDevice(this);
	}
	UE_LOG(LogHandDevice, Log, TEXT("%s: 장치 장착 (%s)"), *GetName(), IsHeadsetActive() ? TEXT("VR 손") : TEXT("데스크톱 카메라"));
	UpdateDisplays();
	OnEquipped.Broadcast();
}

void APuzzleHandDevice::AttachDevices()
{
	const APawn* Pawn = PuzzleUtils::GetPlayerPawn(this);
	USceneComponent* LeftGrip = PuzzleUtils::FindPawnComponent(Pawn, TEXT("MotionControllerLeftGrip"));
	USceneComponent* RightGrip = PuzzleUtils::FindPawnComponent(Pawn, TEXT("MotionControllerRightGrip"));
	if (IsHeadsetActive() && LeftGrip && RightGrip)
	{
		LeftDevice->AttachToComponent(LeftGrip, FAttachmentTransformRules::KeepRelativeTransform);
		RightDevice->AttachToComponent(RightGrip, FAttachmentTransformRules::KeepRelativeTransform);
		LeftDevice->SetRelativeLocationAndRotation(VRHandOffset, FRotator::ZeroRotator);
		RightDevice->SetRelativeLocationAndRotation(VRHandOffset, FRotator::ZeroRotator);
		return;
	}

	// 데스크톱: 화면 아래 양쪽에, 윗면(표시창)이 카메라를 보게 세운다
	if (USceneComponent* Cam = PuzzleUtils::GetPlayerCamera(this))
	{
		const FRotator FaceCamera(90.f, 0.f, 0.f);
		LeftDevice->AttachToComponent(Cam, FAttachmentTransformRules::KeepRelativeTransform);
		RightDevice->AttachToComponent(Cam, FAttachmentTransformRules::KeepRelativeTransform);
		LeftDevice->SetRelativeLocationAndRotation(DesktopOffset, FaceCamera);
		RightDevice->SetRelativeLocationAndRotation(FVector(DesktopOffset.X, -DesktopOffset.Y, DesktopOffset.Z), FaceCamera);
	}
}

void APuzzleHandDevice::ReturnToRack()
{
	LeftDevice->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
	RightDevice->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
	LeftDevice->SetRelativeTransform(LeftRestTM);
	RightDevice->SetRelativeTransform(RightRestTM);
}

void APuzzleHandDevice::PressLeft()
{
	if (!bEquipped)
	{
		return;
	}
	DialIndex = (DialIndex + 1) % UE_ARRAY_COUNT(Symbols);
	UE_LOG(LogHandDevice, Log, TEXT("%s: 다이얼 → %s"), *GetName(), *SymbolOf(DialIndex));
	UpdateDisplays();
	OnDialChanged.Broadcast(DialIndex);
}

void APuzzleHandDevice::PressRight()
{
	if (!bEquipped)
	{
		return;
	}
	ButtonFlash = 0.15f;
	PuzzleUtils::SetSolidColor(ButtonMID, ButtonColor, 3.f);
	if (Targets.IsValidIndex(DialIndex) && Targets[DialIndex])
	{
		Targets[DialIndex]->Toggle();
	}

	// 마지막 입력 Solution.Num()번만 기억한다 (켜짐/꺼짐 상태와 상관없음)
	History.Add(DialIndex);
	while (History.Num() > FMath::Max(Solution.Num(), 1))
	{
		History.RemoveAt(0);
	}
	UE_LOG(LogHandDevice, Log, TEXT("%s: 작동 %s (맞는 순서 %d/%d)"), *GetName(), *SymbolOf(DialIndex), CountProgress(), Solution.Num());
	UpdateDisplays();
	OnPressed.Broadcast(DialIndex);

	if (Solution.Num() > 0 && History == Solution)
	{
		Unlock();
	}
}

int32 APuzzleHandDevice::CountProgress() const
{
	// 입력 기록의 끝 k개가 정답의 처음 k개와 같은 가장 큰 k
	for (int32 K = FMath::Min(History.Num(), Solution.Num()); K > 0; --K)
	{
		bool bMatch = true;
		for (int32 i = 0; i < K && bMatch; ++i)
		{
			bMatch = History[History.Num() - K + i] == Solution[i];
		}
		if (bMatch)
		{
			return K;
		}
	}
	return 0;
}

void APuzzleHandDevice::Unlock()
{
	bUnlocked = true;
	bEquipped = false;
	if (UVRInteractionSubsystem* Interaction = GetWorld()->GetSubsystem<UVRInteractionSubsystem>())
	{
		Interaction->SetActiveDevice(nullptr);
	}
	ReturnToRack();
	EngravingDisplay->SetVisibility(true);
	UE_LOG(LogHandDevice, Log, TEXT("%s: 해제 절차 맞음 → 장치 풀림"), *GetName());
	UpdateDisplays();
	OnUnlocked.Broadcast();
}

void APuzzleHandDevice::UpdateDisplays()
{
	if (UPuzzleTextWidget* W = Cast<UPuzzleTextWidget>(DialDisplay->GetUserWidgetObject()))
	{
		W->SetContent(FText::FromString(SymbolOf(DialIndex)), 140, bEquipped ? DialColor : DimColor, DisplayBg);
	}
	if (UPuzzleTextWidget* W = Cast<UPuzzleTextWidget>(ProgressDisplay->GetUserWidgetObject()))
	{
		FString Shown;
		if (bUnlocked)
		{
			Shown = TEXT("해제");
		}
		else if (bShowProgress)
		{
			const int32 Filled = CountProgress();
			for (int32 i = 0; i < Solution.Num(); ++i)
			{
				Shown += (i > 0 ? TEXT(" ") : TEXT(""));
				Shown += i < Filled ? TEXT("●") : TEXT("○");
			}
		}
		W->SetContent(FText::FromString(Shown), 44, bEquipped || bUnlocked ? ProgressColor : DimColor, DisplayBg);
	}
}
