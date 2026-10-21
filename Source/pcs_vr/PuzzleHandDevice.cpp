#include "PuzzleHandDevice.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "IXRTrackingSystem.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "PuzzleTextWidget.h"
#include "PuzzleToggleTarget.h"
#include "PuzzleUtils.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "VRInteractionSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogHandDevice, Log, All);

namespace
{
	const TCHAR* const Symbols[] = { TEXT("●"), TEXT("▲"), TEXT("■"), TEXT("◆") };

	// 상자 크기와 윗면 배치 (상자 좌표: +X = 플레이어에게서 먼 쪽, +Y = 오른손 쪽, +Z = 위)
	const FVector BodySize(22.f, 46.f, 14.f);
	constexpr float TopZ = 7.f;
	const FVector DialCenter(1.f, -12.f, TopZ + 0.9f);
	constexpr float DialLabelRadius = 8.6f;
	const FVector WindowCenter(0.f, 4.f, TopZ + 0.9f);
	const FVector ButtonCenter(1.f, 16.f, TopZ);
	constexpr float ButtonTravel = 1.2f;
	const FVector SleeveEntry[] = { FVector(-15.f, -10.f, -1.f), FVector(-15.f, 10.f, -1.f) };   // 왼손, 오른손

	// 받침대 위에 놓인 상자 (받침대 좌표). 소매가 플레이어 쪽(+X)을 보게 반 바퀴 돌려 놓음
	const FTransform BoxRestOnRack(FRotator(0.f, 180.f, 0.f), FVector(0.f, 0.f, 19.f));

	// 윗면 글자판: 위(+Z)를 보고, 글자 위쪽이 먼 쪽(+X)을 향함 → 상자를 든 사람이 바로 읽음
	const FRotator TopDisplayRotation(90.f, 180.f, 0.f);

	const FLinearColor BodyColor(0.62f, 0.58f, 0.48f);      // 바랜 베이지 도장
	const FLinearColor TopColor(0.16f, 0.17f, 0.18f);
	const FLinearColor MetalColor(0.45f, 0.46f, 0.47f);
	const FLinearColor RubberColor(0.04f, 0.04f, 0.045f);
	const FLinearColor ButtonColor(0.75f, 0.12f, 0.08f);
	const FLinearColor LampOff(0.08f, 0.1f, 0.09f);
	const FLinearColor LampOn(0.3f, 1.f, 0.4f);
	const FLinearColor WindowBg(0.02f, 0.05f, 0.04f, 1.f);
	const FLinearColor WindowText(0.45f, 1.f, 0.6f);         // 오래된 녹색 형광 화면
	const FLinearColor WindowDim(0.12f, 0.3f, 0.18f);

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

	Nameplate = FText::FromString(TEXT("피험 장치 01"));
	Engraving = FText::FromString(TEXT("피험 장치 01 — 착용자: 오나연"));
	LockSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Engine/VREditor/Sounds/UI/Object_Snaps_To_Another_Actor.Object_Snaps_To_Another_Actor")));
	DialSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Engine/VREditor/Sounds/UI/Object_Snaps_To_Grid.Object_Snaps_To_Grid")));
	ButtonSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Engine/VREditor/Sounds/UI/Click_on_Button.Click_on_Button")));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* Cube = CubeMesh.Object;
	UStaticMesh* Cylinder = CylinderMesh.Object;
	UStaticMesh* Sphere = SphereMesh.Object;
	const FRotator AlongX(90.f, 0.f, 0.f);   // 원기둥(Z축)을 X축 방향으로 눕힘

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);

	// ---- 받침대: 책상 위 판 + 상자 양 끝을 받치는 지지대 두 개
	AddPart(TEXT("RackBase"), Root, Cube, FVector(0.f, 0.f, 2.f), FVector(34.f, 54.f, 4.f));
	AddPart(TEXT("RackSupportL"), Root, Cube, FVector(0.f, 18.f, 8.f), FVector(24.f, 6.f, 8.f));
	AddPart(TEXT("RackSupportR"), Root, Cube, FVector(0.f, -18.f, 8.f), FVector(24.f, 6.f, 8.f));

	RackInteract = CreateDefaultSubobject<UPuzzleInteractComponent>(TEXT("RackInteract"));
	RackInteract->SetupAttachment(Root);
	RackInteract->SetRelativeLocation(FVector(0.f, 0.f, 20.f));
	RackInteract->InteractRadius = 35.f;

	// ---- 장치 상자
	Box = CreateDefaultSubobject<USceneComponent>(TEXT("Box"));
	Box->SetupAttachment(Root);
	Box->SetMobility(EComponentMobility::Movable);
	Box->SetRelativeTransform(BoxRestOnRack);

	AddPart(TEXT("Body"), Box, Cube, FVector::ZeroVector, BodySize);
	AddPart(TEXT("TopPlate"), Box, Cube, FVector(0.f, 0.f, TopZ + 0.4f), FVector(BodySize.X - 2.f, BodySize.Y - 2.f, 0.8f));
	AddPart(TEXT("TrimFront"), Box, Cube, FVector(BodySize.X * 0.5f, 0.f, 0.f), FVector(1.f, BodySize.Y + 1.f, BodySize.Z + 1.f));
	AddPart(TEXT("TrimBack"), Box, Cube, FVector(-BodySize.X * 0.5f, 0.f, 0.f), FVector(1.f, BodySize.Y + 1.f, BodySize.Z + 1.f));
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector Corner((i & 1 ? 1.f : -1.f) * (BodySize.X * 0.5f - 1.8f), (i & 2 ? 1.f : -1.f) * (BodySize.Y * 0.5f - 1.8f), TopZ + 0.9f);
		AddPart(*FString::Printf(TEXT("Bolt_%d"), i), Box, Cylinder, Corner, FVector(1.2f, 1.2f, 0.4f));
	}

	// 손을 넣는 고무 소매 (뒷면, 플레이어 쪽)
	for (int32 i = 0; i < 2; ++i)
	{
		const FVector Entry = SleeveEntry[i];
		AddPart(i == 0 ? TEXT("SleeveL") : TEXT("SleeveR"), Box, Cylinder, Entry + FVector(2.f, 0.f, 0.f), FVector(12.f, 12.f, 6.f), AlongX);
		AddPart(i == 0 ? TEXT("HoleL") : TEXT("HoleR"), Box, Cylinder, Entry + FVector(-1.1f, 0.f, 0.f), FVector(8.5f, 8.5f, 0.4f), AlongX);
	}

	// 다이얼 (왼손): 돌아가는 원판 + 화살표, 둘레에 기호 4개
	DialPivot = CreateDefaultSubobject<USceneComponent>(TEXT("DialPivot"));
	DialPivot->SetupAttachment(Box);
	DialPivot->SetMobility(EComponentMobility::Movable);
	DialPivot->SetRelativeLocation(DialCenter);
	AddPart(TEXT("DialRing"), Box, Cylinder, DialCenter + FVector(0.f, 0.f, -0.5f), FVector(15.f, 15.f, 0.4f));
	AddPart(TEXT("DialDisk"), DialPivot, Cylinder, FVector(0.f, 0.f, 0.4f), FVector(9.5f, 9.5f, 1.6f));
	AddPart(TEXT("DialKnob"), DialPivot, Cylinder, FVector(0.f, 0.f, 1.6f), FVector(3.f, 3.f, 1.6f));
	AddPart(TEXT("DialPointer"), DialPivot, Cube, FVector(3.f, 0.f, 1.3f), FVector(4.f, 0.9f, 0.5f));
	for (int32 i = 0; i < UE_ARRAY_COUNT(Symbols); ++i)
	{
		// 0번(●)이 먼 쪽, 한 칸마다 위에서 봐서 시계 방향으로 90도
		const float Angle = FMath::DegreesToRadians(90.f * i);
		const FVector Loc = DialCenter + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * DialLabelRadius + FVector(0.f, 0.f, 0.1f);
		DialLabels.Add(AddDisplay(*FString::Printf(TEXT("DialLabel_%d"), i), Box, Loc, FVector2D(100.f, 100.f), 0.042f));
	}

	// 기호 창 (가운데): 오래된 녹색 화면에 지금 다이얼 기호
	AddPart(TEXT("WindowFrame"), Box, Cube, WindowCenter + FVector(0.f, 0.f, -0.4f), FVector(10.f, 10.f, 0.8f));
	SymbolWindow = AddDisplay(TEXT("SymbolWindow"), Box, WindowCenter + FVector(0.f, 0.f, 0.05f), FVector2D(200.f, 200.f), 0.04f);

	// 큰 빨간 버튼 (오른손)
	AddPart(TEXT("ButtonBase"), Box, Cylinder, ButtonCenter + FVector(0.f, 0.f, 0.5f), FVector(12.f, 12.f, 1.f));
	ButtonCap = AddPart(TEXT("ButtonCap"), Box, Cylinder, ButtonCenter + FVector(0.f, 0.f, 2.2f), FVector(8.5f, 8.5f, 2.4f));

	// 진행 램프 4개 (플레이어 쪽 가장자리)
	for (int32 i = 0; i < 4; ++i)
	{
		ProgressLamps.Add(AddPart(*FString::Printf(TEXT("Lamp_%d"), i), Box, Sphere, FVector(-8.f, -4.5f + 3.f * i, TopZ + 0.9f), FVector(1.8f, 1.8f, 1.8f)));
	}

	NameplateDisplay = AddDisplay(TEXT("Nameplate"), Box, FVector(8.2f, 4.f, TopZ + 0.85f), FVector2D(400.f, 60.f), 0.035f);

	// 풀린 뒤 받침대 앞쪽에 보이는 각인 (위를 보고, 플레이어 쪽에서 읽힘)
	EngravingDisplay = AddDisplay(TEXT("EngravingDisplay"), Root, FVector(14.5f, 0.f, 4.2f), FVector2D(640.f, 70.f), 0.04f);
	EngravingDisplay->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
}

UStaticMeshComponent* APuzzleHandDevice::AddPart(const TCHAR* Name, USceneComponent* Parent, UStaticMesh* Mesh, const FVector& Center, const FVector& SizeCm,
	const FRotator& Rotation)
{
	UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Part->SetupAttachment(Parent);
	Part->SetStaticMesh(Mesh);
	Part->SetMobility(EComponentMobility::Movable);
	Part->SetRelativeLocationAndRotation(Center, Rotation);
	Part->SetRelativeScale3D(SizeCm / 100.f);
	// 손에 든 상자가 몸이나 벽에 걸리지 않게, 받침대도 손이 들어가야 하니 충돌 없음
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetCanEverAffectNavigation(false);
	Part->CastShadow = Parent != Root;   // 받침대는 책상 위라 그림자 생략
	Parts.Add(Part);
	return Part;
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
	// 장착 전에는 화면 밖(책상 위)에 있다가 눈앞으로 오므로, 화면 밖에서도 글자를 그려 둔다 (안 그러면 처음에 까맣게 보임)
	W->SetTickWhenOffscreen(true);
	W->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	W->SetCanEverAffectNavigation(false);
	return W;
}

void APuzzleHandDevice::BeginPlay()
{
	Super::BeginPlay();

	for (UStaticMeshComponent* Part : Parts)
	{
		const FString N = Part->GetName();
		FLinearColor Color = BodyColor;
		float Emissive = 0.f;
		if (N.StartsWith(TEXT("Rack"))) { Color = FLinearColor(0.2f, 0.21f, 0.22f); }
		else if (N.StartsWith(TEXT("TopPlate")) || N.StartsWith(TEXT("WindowFrame"))) { Color = TopColor; }
		else if (N.StartsWith(TEXT("Trim")) || N.StartsWith(TEXT("Bolt")) || N.StartsWith(TEXT("Dial")) || N.StartsWith(TEXT("ButtonBase"))) { Color = MetalColor; }
		else if (N.StartsWith(TEXT("Sleeve"))) { Color = RubberColor; }
		else if (N.StartsWith(TEXT("Hole"))) { Color = FLinearColor::Black; }
		if (N == TEXT("DialPointer")) { Color = FLinearColor(0.9f, 0.85f, 0.7f); Emissive = 0.3f; }

		UMaterialInstanceDynamic* MID = PuzzleUtils::ApplySolidColor(Part, Color, Emissive);
		if (Part == ButtonCap) { ButtonMID = PuzzleUtils::ApplySolidColor(Part, ButtonColor, 0.3f); }
		if (ProgressLamps.Contains(Part)) { LampMIDs.Add(PuzzleUtils::ApplySolidColor(Part, LampOff, 0.f)); }
		(void)MID;
	}

	BoxRestTM = Box->GetRelativeTransform();

	RackInteract->OnInteract.AddDynamic(this, &APuzzleHandDevice::HandleRackInteract);
	TArray<UWidgetComponent*> Widgets = { SymbolWindow.Get(), NameplateDisplay.Get(), EngravingDisplay.Get() };
	for (UWidgetComponent* W : DialLabels) { Widgets.Add(W); }
	for (UWidgetComponent* W : Widgets) { W->InitWidget(); }

	if (UPuzzleTextWidget* W = Cast<UPuzzleTextWidget>(NameplateDisplay->GetUserWidgetObject()))
	{
		W->SetContent(Nameplate, 40, FLinearColor(0.12f, 0.1f, 0.08f), FLinearColor(0.75f, 0.72f, 0.6f, 1.f));
	}
	for (int32 i = 0; i < DialLabels.Num(); ++i)
	{
		if (UPuzzleTextWidget* W = Cast<UPuzzleTextWidget>(DialLabels[i]->GetUserWidgetObject()))
		{
			W->SetContent(FText::FromString(SymbolOf(i)), 80, FLinearColor(0.9f, 0.88f, 0.8f), FLinearColor(0.f, 0.f, 0.f, 0.f));
		}
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
		SetHandsHidden(false);
	}
	Super::EndPlay(EndPlayReason);
}

void APuzzleHandDevice::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bEquipped && !bUnlocked && IsHeadsetActive() && AreHandsInSleeves())
	{
		Equip();
	}
	if (bEquipped)
	{
		FollowHands(DeltaTime);
	}

	// 다이얼은 한 칸(90도)씩 딸깍 돌아가고, 버튼은 눌렸다 올라온다
	DialAngle = FMath::FInterpTo(DialAngle, 90.f * DialIndex, DeltaTime, 18.f);
	DialPivot->SetRelativeRotation(FRotator(0.f, DialAngle, 0.f));
	ButtonPress = FMath::FInterpTo(ButtonPress, 0.f, DeltaTime, 10.f);
	ButtonCap->SetRelativeLocation(ButtonCenter + FVector(0.f, 0.f, 2.2f - ButtonTravel * ButtonPress));
	PuzzleUtils::SetSolidColor(ButtonMID, ButtonColor, 0.3f + 2.5f * ButtonPress);

	if (LampFlash > 0.f)
	{
		LampFlash = FMath::Max(0.f, LampFlash - DeltaTime);
		UpdateDisplays();
	}
}

bool APuzzleHandDevice::AreHandsInSleeves() const
{
	const APawn* Pawn = PuzzleUtils::GetPlayerPawn(this);
	const USceneComponent* LeftGrip = PuzzleUtils::FindPawnComponent(Pawn, TEXT("MotionControllerLeftGrip"));
	const USceneComponent* RightGrip = PuzzleUtils::FindPawnComponent(Pawn, TEXT("MotionControllerRightGrip"));
	if (!LeftGrip || !RightGrip)
	{
		return false;
	}
	const FTransform& TM = Box->GetComponentTransform();
	return FVector::Dist(LeftGrip->GetComponentLocation(), TM.TransformPosition(SleeveEntry[0])) <= SleeveRadius
		&& FVector::Dist(RightGrip->GetComponentLocation(), TM.TransformPosition(SleeveEntry[1])) <= SleeveRadius;
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
	bHasSmoothed = false;
	RackInteract->bInteractEnabled = false;
	SetHandsHidden(true);
	if (UVRInteractionSubsystem* Interaction = GetWorld()->GetSubsystem<UVRInteractionSubsystem>())
	{
		Interaction->SetActiveDevice(this);
	}
	PlaySound(LockSound);
	Buzz(true, 0.8f);
	Buzz(false, 0.8f);
	UE_LOG(LogHandDevice, Log, TEXT("%s: 장치에 양손 잠김 (%s)"), *GetName(), IsHeadsetActive() ? TEXT("VR 손") : TEXT("데스크톱 카메라"));
	FollowHands(0.f);
	UpdateDisplays();
	OnEquipped.Broadcast();
}

void APuzzleHandDevice::FollowHands(float DeltaTime)
{
	FTransform Target;
	const APawn* Pawn = PuzzleUtils::GetPlayerPawn(this);
	const USceneComponent* LeftGrip = PuzzleUtils::FindPawnComponent(Pawn, TEXT("MotionControllerLeftGrip"));
	const USceneComponent* RightGrip = PuzzleUtils::FindPawnComponent(Pawn, TEXT("MotionControllerRightGrip"));
	if (IsHeadsetActive() && LeftGrip && RightGrip)
	{
		// 상자의 Y축 = 왼손 → 오른손, X축 = 두 손이 가리키는 방향의 평균 (Y에 수직으로 맞춤)
		const FVector L = LeftGrip->GetComponentLocation();
		const FVector R = RightGrip->GetComponentLocation();
		FVector YAxis = (R - L).GetSafeNormal();
		if (YAxis.IsNearlyZero())
		{
			YAxis = Box->GetRightVector();
		}
		FVector XAxis = LeftGrip->GetForwardVector() + RightGrip->GetForwardVector();
		XAxis = (XAxis - YAxis * FVector::DotProduct(XAxis, YAxis)).GetSafeNormal();
		if (XAxis.IsNearlyZero())
		{
			XAxis = Box->GetForwardVector();
		}
		const FQuat Rot = FRotationMatrix::MakeFromYX(YAxis, XAxis).ToQuat();
		Target = FTransform(Rot, (L + R) * 0.5f + Rot.RotateVector(VRCenterOffset));
	}
	else if (const USceneComponent* Cam = PuzzleUtils::GetPlayerCamera(this))
	{
		// 데스크톱: 화면 아래 가운데에 윗면이 보이게 들고 있음
		Target = FTransform(DesktopRotation, DesktopOffset) * Cam->GetComponentTransform();
	}
	else
	{
		return;
	}

	// 손떨림을 조금 걸러서 상자가 묵직하게 따라오게
	if (!bHasSmoothed || DeltaTime <= 0.f)
	{
		SmoothedTM = Target;
		bHasSmoothed = true;
	}
	else
	{
		const float Alpha = FMath::Clamp(DeltaTime * 25.f, 0.f, 1.f);
		SmoothedTM.SetLocation(FMath::Lerp(SmoothedTM.GetLocation(), Target.GetLocation(), Alpha));
		SmoothedTM.SetRotation(FQuat::Slerp(SmoothedTM.GetRotation(), Target.GetRotation(), Alpha));
	}
	Box->SetWorldLocationAndRotation(SmoothedTM.GetLocation(), SmoothedTM.GetRotation());
}

void APuzzleHandDevice::ReturnToRack()
{
	Box->SetRelativeTransform(BoxRestTM);
}

void APuzzleHandDevice::SetHandsHidden(bool bHide)
{
	// 손은 상자 안에 있으니 보이지 않는다 (Statik). VR 템플릿의 손 모델(스켈레탈 메시)을 숨김
	if (bHide)
	{
		HiddenHands.Reset();
		if (const APawn* Pawn = PuzzleUtils::GetPlayerPawn(this))
		{
			TInlineComponentArray<USkeletalMeshComponent*> Meshes;
			Pawn->GetComponents(Meshes);
			for (USkeletalMeshComponent* Mesh : Meshes)
			{
				if (Mesh->IsVisible())
				{
					Mesh->SetVisibility(false, true);
					HiddenHands.Add(Mesh);
				}
			}
		}
		return;
	}
	for (const TWeakObjectPtr<USceneComponent>& Hand : HiddenHands)
	{
		if (USceneComponent* C = Hand.Get())
		{
			C->SetVisibility(true, true);
		}
	}
	HiddenHands.Reset();
}

void APuzzleHandDevice::Buzz(bool bLeft, float Amplitude)
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !IsHeadsetActive())
	{
		return;
	}
	const EControllerHand Hand = bLeft ? EControllerHand::Left : EControllerHand::Right;
	PC->SetHapticsByValue(0.6f, Amplitude, Hand);
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(PC, [PC, Hand]() { PC->SetHapticsByValue(0.f, 0.f, Hand); }), 0.06f, false);
}

void APuzzleHandDevice::PlaySound(const TSoftObjectPtr<USoundBase>& Sound, float Volume)
{
	if (USoundBase* S = Sound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(this, S, Box->GetComponentLocation(), Volume);
	}
}

void APuzzleHandDevice::PressLeft()
{
	if (!bEquipped)
	{
		return;
	}
	DialIndex = (DialIndex + 1) % UE_ARRAY_COUNT(Symbols);
	PlaySound(DialSound);
	Buzz(true, 0.4f);
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
	ButtonPress = 1.f;
	PlaySound(ButtonSound);
	Buzz(false, 0.7f);
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
	LampFlash = 0.12f;
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
	SetHandsHidden(false);
	PlaySound(LockSound);
	Buzz(true, 1.f);
	Buzz(false, 1.f);
	EngravingDisplay->SetVisibility(true);
	UE_LOG(LogHandDevice, Log, TEXT("%s: 해제 절차 맞음 → 장치 풀림"), *GetName());
	UpdateDisplays();
	OnUnlocked.Broadcast();
}

void APuzzleHandDevice::UpdateDisplays()
{
	if (UPuzzleTextWidget* W = Cast<UPuzzleTextWidget>(SymbolWindow->GetUserWidgetObject()))
	{
		const FString Shown = bUnlocked ? TEXT("해제") : SymbolOf(DialIndex);
		W->SetContent(FText::FromString(Shown), bUnlocked ? 64 : 130, bEquipped || bUnlocked ? WindowText : WindowDim, WindowBg);
	}
	// 진행 램프: 맞는 순서만큼 켜짐. 버튼을 누른 순간에는 잠깐 모두 깜빡
	const int32 Lit = bUnlocked ? LampMIDs.Num() : (bShowProgress ? CountProgress() : 0);
	for (int32 i = 0; i < LampMIDs.Num(); ++i)
	{
		const bool bOn = i < Lit;
		const float Flash = LampFlash > 0.f ? 0.6f : 0.f;
		PuzzleUtils::SetSolidColor(LampMIDs[i], bOn ? LampOn : LampOff, bOn ? 3.f : Flash);
	}
}
