#include "VRInteractionSubsystem.h"

#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "PerspectiveCube.h"
#include "PuzzleHandDevice.h"
#include "PuzzleInteractComponent.h"
#include "PuzzleUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogVRInteract, Log, All);

bool UVRInteractionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UVRInteractionSubsystem::Register(UPuzzleInteractComponent* Comp)
{
	Interactables.AddUnique(Comp);
}

void UVRInteractionSubsystem::Unregister(UPuzzleInteractComponent* Comp)
{
	Interactables.Remove(Comp);
}

void UVRInteractionSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bInputReady)
	{
		UpdateTriggers();
		UpdateRayGrab();
		return;
	}
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (PC && PC->InputComponent)
	{
		SetupInput(PC);
	}
}

void UVRInteractionSubsystem::SetupInput(APlayerController* PC)
{
	const ULocalPlayer* LP = PC->GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Input = LP ? LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PC->InputComponent);
	if (!Input || !EIC)
	{
		return;
	}

	// VR 트리거: OpenXR은 프로젝트 설정의 PMI_VRTemplate에 들어 있는 입력만 받는다.
	// 그래서 새 키 매핑을 만들지 않고, 템플릿이 이미 트리거에 연결해 둔 액션의 값을 읽는다.
	// - IA_Hand_IndexCurl_*: 손 애니메이션용 (IMC_Hands, 항상 켜져 있음)
	// - IA_Shoot_*: 무기용 (IMC_Weapon_Left/Right, 여기서 켜 준다)
	LeftTriggerActions = {
		LoadObject<UInputAction>(nullptr, TEXT("/Game/VRTemplate/Input/Actions/Hands/IA_Hand_IndexCurl_Left.IA_Hand_IndexCurl_Left")),
		LoadObject<UInputAction>(nullptr, TEXT("/Game/VRTemplate/Input/Actions/IA_Shoot_Left.IA_Shoot_Left")) };
	RightTriggerActions = {
		LoadObject<UInputAction>(nullptr, TEXT("/Game/VRTemplate/Input/Actions/Hands/IA_Hand_IndexCurl_Right.IA_Hand_IndexCurl_Right")),
		LoadObject<UInputAction>(nullptr, TEXT("/Game/VRTemplate/Input/Actions/IA_Shoot_Right.IA_Shoot_Right")) };
	for (const TCHAR* Path : { TEXT("/Game/VRTemplate/Input/IMC_Weapon_Left.IMC_Weapon_Left"), TEXT("/Game/VRTemplate/Input/IMC_Weapon_Right.IMC_Weapon_Right") })
	{
		if (UInputMappingContext* WeaponContext = LoadObject<UInputMappingContext>(nullptr, Path))
		{
			if (!Input->HasMappingContext(WeaponContext))
			{
				Input->AddMappingContext(WeaponContext, 1);
			}
		}
	}

	// 그립: 템플릿이 그립에 연결해 둔 액션 (IA_Grab_*: IMC_Default, IA_Hand_Grasp_*: IMC_Hands)
	LeftGripActions = {
		LoadObject<UInputAction>(nullptr, TEXT("/Game/VRTemplate/Input/Actions/IA_Grab_Left.IA_Grab_Left")),
		LoadObject<UInputAction>(nullptr, TEXT("/Game/VRTemplate/Input/Actions/Hands/IA_Hand_Grasp_Left.IA_Hand_Grasp_Left")) };
	RightGripActions = {
		LoadObject<UInputAction>(nullptr, TEXT("/Game/VRTemplate/Input/Actions/IA_Grab_Right.IA_Grab_Right")),
		LoadObject<UInputAction>(nullptr, TEXT("/Game/VRTemplate/Input/Actions/Hands/IA_Hand_Grasp_Right.IA_Hand_Grasp_Right")) };

	// 데스크톱 테스트: E 키 / R 키 / Q 키 (키보드는 OpenXR을 거치지 않아서 새 매핑으로 충분)
	DesktopAction = NewObject<UInputAction>(this, TEXT("IA_Puzzle_Interact_Desktop"));
	DesktopRayAction = NewObject<UInputAction>(this, TEXT("IA_Puzzle_RayGrab_Desktop"));
	DesktopDeviceLeftAction = NewObject<UInputAction>(this, TEXT("IA_Puzzle_DeviceLeft_Desktop"));
	Context = NewObject<UInputMappingContext>(this, TEXT("IMC_Puzzle_Interact"));
	Context->MapKey(DesktopAction, EKeys::E);
	Context->MapKey(DesktopRayAction, EKeys::R);
	Context->MapKey(DesktopDeviceLeftAction, EKeys::Q);
	Input->AddMappingContext(Context, 5);
	EIC->BindAction(DesktopAction, ETriggerEvent::Started, this, &UVRInteractionSubsystem::OnDesktopInteract);
	EIC->BindAction(DesktopRayAction, ETriggerEvent::Started, this, &UVRInteractionSubsystem::OnDesktopRayGrab);
	EIC->BindAction(DesktopDeviceLeftAction, ETriggerEvent::Started, this, &UVRInteractionSubsystem::OnDesktopDeviceLeft);

	PlayerInput = Input->GetPlayerInput();
	bInputReady = true;
	UE_LOG(LogVRInteract, Log, TEXT("트리거 상호작용 입력 준비 완료 (템플릿 트리거/그립 액션 읽기 / 데스크톱 E, R, Q)"));
}

float UVRInteractionSubsystem::ReadTrigger(const TArray<TObjectPtr<UInputAction>>& Actions) const
{
	float Value = 0.f;
	if (UEnhancedPlayerInput* EnhancedInput = PlayerInput.Get())
	{
		for (const UInputAction* Action : Actions)
		{
			if (Action)
			{
				const FInputActionValue V = EnhancedInput->GetActionValue(Action);
				Value = FMath::Max(Value, V.GetValueType() == EInputActionValueType::Boolean ? (V.Get<bool>() ? 1.f : 0.f) : FMath::Abs(V.Get<float>()));
			}
		}
	}
	return Value;
}

void UVRInteractionSubsystem::UpdateTriggers()
{
	// 끝까지 당기면(0.75 이상) 한 번 누름, 0.4 아래로 놓으면 다시 누를 수 있음
	const float Left = ReadTrigger(LeftTriggerActions);
	const float Right = ReadTrigger(RightTriggerActions);
	if (!bLeftDown && Left > 0.75f) { bLeftDown = true; OnLeftTrigger(); }
	else if (bLeftDown && Left < 0.4f) { bLeftDown = false; }
	if (!bRightDown && Right > 0.75f) { bRightDown = true; OnRightTrigger(); }
	else if (bRightDown && Right < 0.4f) { bRightDown = false; }
}

void UVRInteractionSubsystem::SetActiveDevice(APuzzleHandDevice* Device)
{
	ActiveDevice = Device;
	if (!Device)
	{
		return;
	}
	// 레이로 들고 있던 큐브는 놓는다 (장치를 찬 동안에는 물건을 잡을 수 없음)
	for (FRayHand* Hand : { &LeftRay, &RightRay })
	{
		if (APerspectiveCube* Cube = Hand->Held.Get())
		{
			Cube->EndRayGrab();
		}
		if (APerspectiveCube* Old = Hand->Aimed.Get())
		{
			Old->SetAimHighlight(false);
		}
		Hand->Held.Reset();
		Hand->Aimed.Reset();
	}
	if (APerspectiveCube* Cube = DesktopHeld.Get())
	{
		Cube->EndRayGrab();
	}
	DesktopHeld.Reset();
}

void UVRInteractionSubsystem::OnLeftTrigger()
{
	if (APuzzleHandDevice* Device = ActiveDevice.Get())
	{
		Device->PressLeft();
		return;
	}
	InteractWithHand(TEXT("MotionControllerLeftGrip"));
}

void UVRInteractionSubsystem::OnRightTrigger()
{
	if (APuzzleHandDevice* Device = ActiveDevice.Get())
	{
		Device->PressRight();
		return;
	}
	InteractWithHand(TEXT("MotionControllerRightGrip"));
}

void UVRInteractionSubsystem::OnDesktopDeviceLeft()
{
	if (APuzzleHandDevice* Device = ActiveDevice.Get())
	{
		Device->PressLeft();
	}
}

void UVRInteractionSubsystem::InteractWithHand(const TCHAR* GripName)
{
	APawn* Pawn = PuzzleUtils::GetPlayerPawn(this);
	const USceneComponent* Hand = PuzzleUtils::FindPawnComponent(Pawn, GripName);
	if (!Hand)
	{
		return;
	}
	const FVector HandLoc = Hand->GetComponentLocation();

	UPuzzleInteractComponent* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (const TWeakObjectPtr<UPuzzleInteractComponent>& Weak : Interactables)
	{
		UPuzzleInteractComponent* C = Weak.Get();
		if (!C || !C->bInteractEnabled)
		{
			continue;
		}
		const float Dist = FVector::Dist(HandLoc, C->GetComponentLocation());
		if (Dist <= C->InteractRadius && Dist < BestDist)
		{
			Best = C;
			BestDist = Dist;
		}
	}
	if (Best)
	{
		UE_LOG(LogVRInteract, Log, TEXT("%s 트리거 -> %s"), GripName, *Best->GetOwner()->GetName());
		Best->Interact(Pawn);
		return;
	}

	// 진단용: 가장 가까운 대상과의 거리 (손이 얼마나 멀었는지). 누를 대상이 없는 맵에서는 남기지 않음
	if (Interactables.Num() == 0)
	{
		return;
	}
	float Nearest = TNumericLimits<float>::Max();
	FString NearestName = TEXT("(없음)");
	for (const TWeakObjectPtr<UPuzzleInteractComponent>& Weak : Interactables)
	{
		const UPuzzleInteractComponent* C = Weak.Get();
		if (C && C->bInteractEnabled)
		{
			const float Dist = FVector::Dist(HandLoc, C->GetComponentLocation());
			if (Dist < Nearest)
			{
				Nearest = Dist;
				NearestName = C->GetOwner()->GetName();
			}
		}
	}
	UE_LOG(LogVRInteract, Log, TEXT("%s 트리거: 닿은 대상 없음 (가장 가까운 %s 까지 %.0fcm)"), GripName, *NearestName, Nearest);
}

void UVRInteractionSubsystem::OnDesktopInteract()
{
	if (APuzzleHandDevice* Device = ActiveDevice.Get())
	{
		Device->PressRight();
		return;
	}
	FVector ViewLoc;
	FRotator ViewRot;
	if (!PuzzleUtils::GetPlayerView(this, ViewLoc, ViewRot))
	{
		return;
	}
	const FVector Dir = ViewRot.Vector();

	// 시선(광선)에서 가장 가까운 대상. 광선과의 거리가 30cm 이내, 앞쪽 DesktopReach 이내.
	UPuzzleInteractComponent* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (const TWeakObjectPtr<UPuzzleInteractComponent>& Weak : Interactables)
	{
		UPuzzleInteractComponent* C = Weak.Get();
		if (!C || !C->bInteractEnabled)
		{
			continue;
		}
		const FVector ToTarget = C->GetComponentLocation() - ViewLoc;
		const float Along = FVector::DotProduct(ToTarget, Dir);
		if (Along < 0.f || Along > DesktopReach)
		{
			continue;
		}
		const float OffRay = (ToTarget - Dir * Along).Size();
		if (OffRay < 30.f && OffRay < BestScore)
		{
			Best = C;
			BestScore = OffRay;
		}
	}
	if (Best)
	{
		UE_LOG(LogVRInteract, Log, TEXT("E 키 -> %s"), *Best->GetOwner()->GetName());
		Best->Interact(PuzzleUtils::GetPlayerPawn(this));
	}
}

// ---------------------------------------------------------------- 원근 큐브 레이 잡기

void UVRInteractionSubsystem::UpdateRayGrab()
{
	if (ActiveDevice.IsValid())
	{
		return;
	}
	UpdateRayHand(LeftRay, ReadTrigger(LeftGripActions));
	UpdateRayHand(RightRay, ReadTrigger(RightGripActions));
}

void UVRInteractionSubsystem::UpdateRayHand(FRayHand& Hand, float GripValue)
{
	APawn* Pawn = PuzzleUtils::GetPlayerPawn(this);
	USceneComponent* Aim = PuzzleUtils::FindPawnComponent(Pawn, Hand.AimName);
	USceneComponent* Grip = PuzzleUtils::FindPawnComponent(Pawn, Hand.GripName);
	if (!Aim || !Grip)
	{
		return;
	}

	// 그립을 놓으면 레이로 잡고 있던 큐브도 놓는다 (→ 시선 방향으로 투사되어 커질 수 있음)
	if (Hand.bGripDown)
	{
		if (GripValue < 0.3f)
		{
			Hand.bGripDown = false;
			if (APerspectiveCube* Cube = Hand.Held.Get())
			{
				Cube->EndRayGrab();
			}
			Hand.Held.Reset();
		}
		return;
	}

	// 겨누고 있는 큐브를 밝게 표시
	APerspectiveCube* Target = TraceCube(Aim->GetComponentLocation(), Aim->GetForwardVector(), Grip->GetComponentLocation());
	if (Hand.Aimed.Get() != Target)
	{
		if (APerspectiveCube* Old = Hand.Aimed.Get())
		{
			Old->SetAimHighlight(false);
		}
		if (Target)
		{
			Target->SetAimHighlight(true);
		}
		Hand.Aimed = Target;
	}

	if (GripValue > 0.7f)
	{
		Hand.bGripDown = true;
		if (Target)
		{
			Hand.Aimed.Reset();
			Target->BeginRayGrab(Grip, FVector::ZeroVector);
			Hand.Held = Target;
			UE_LOG(LogVRInteract, Log, TEXT("%s 그립 -> %s 레이로 잡기"), Hand.GripName, *Target->GetName());
		}
		else if (APerspectiveCube* Locked = TraceCube(Aim->GetComponentLocation(), Aim->GetForwardVector(), Grip->GetComponentLocation(), true))
		{
			// 아직 잡을 수 없는 큐브를 겨누고 그립 → 진행 담당이 안내 메모를 띄운다
			Locked->NotifyRayGrabRefused();
		}
	}
}

APerspectiveCube* UVRInteractionSubsystem::TraceCube(const FVector& Start, const FVector& Dir, const FVector& MinDistFrom, bool bIncludeLocked) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(PuzzleRayGrab), false);
	if (APawn* Pawn = PuzzleUtils::GetPlayerPawn(this))
	{
		Params.AddIgnoredActor(Pawn);
	}
	// 손떨림을 감안해 가는 구(반지름 4cm)로 쓸어 본다
	FHitResult Hit;
	if (!World->SweepSingleByChannel(Hit, Start, Start + Dir * RayGrabDistance, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(4.f), Params))
	{
		return nullptr;
	}
	APerspectiveCube* Cube = Cast<APerspectiveCube>(Hit.GetActor());
	const bool bGrabbable = Cube && (bIncludeLocked ? !Cube->IsHeld() : Cube->CanRayGrab());
	if (!bGrabbable || FVector::Dist(Cube->GetActorLocation(), MinDistFrom) < MinRayGrabDistance)
	{
		return nullptr;
	}
	return Cube;
}

void UVRInteractionSubsystem::OnDesktopRayGrab()
{
	if (ActiveDevice.IsValid())
	{
		return;
	}
	if (APerspectiveCube* Held = DesktopHeld.Get())
	{
		Held->EndRayGrab();
		DesktopHeld.Reset();
		UE_LOG(LogVRInteract, Log, TEXT("R 키 -> %s 놓기"), *Held->GetName());
		return;
	}
	FVector ViewLoc;
	FRotator ViewRot;
	USceneComponent* Cam = PuzzleUtils::GetPlayerCamera(this);
	if (!Cam || !PuzzleUtils::GetPlayerView(this, ViewLoc, ViewRot))
	{
		return;
	}
	if (APerspectiveCube* Cube = TraceCube(ViewLoc, ViewRot.Vector(), ViewLoc))
	{
		// 카메라 앞 60cm, 눈높이에 든다 (아래로 들면 헤드셋 없이 테스트할 때 카메라가 바닥 높이라 시선이 바로 바닥에 닿음)
		Cube->BeginRayGrab(Cam, FVector(60.f, 0.f, 5.f));
		DesktopHeld = Cube;
		UE_LOG(LogVRInteract, Log, TEXT("R 키 -> %s 레이로 잡기"), *Cube->GetName());
	}
	else if (APerspectiveCube* Locked = TraceCube(ViewLoc, ViewRot.Vector(), ViewLoc, true))
	{
		Locked->NotifyRayGrabRefused();
	}
}

// 헤드셋 없이 테스트: 콘솔에 pcs.RayGrabTest → 가장 가까운 원근 큐브를 카메라 앞으로 레이 잡기, 1.5초 뒤 놓기
static FAutoConsoleCommandWithWorld GRayGrabTestCommand(
	TEXT("pcs.RayGrabTest"),
	TEXT("가장 가까운 원근 큐브를 카메라 앞으로 레이로 잡아 왔다가 1.5초 뒤 놓는다 (헤드셋 없이 테스트용)"),
	FConsoleCommandWithWorldDelegate::CreateStatic([](UWorld* World)
	{
		USceneComponent* Cam = PuzzleUtils::GetPlayerCamera(World);
		if (!World || !Cam)
		{
			UE_LOG(LogVRInteract, Warning, TEXT("pcs.RayGrabTest: 플레이어 카메라 없음"));
			return;
		}
		APerspectiveCube* Nearest = nullptr;
		float NearestDist = TNumericLimits<float>::Max();
		for (TActorIterator<APerspectiveCube> It(World); It; ++It)
		{
			const float Dist = FVector::Dist(It->GetActorLocation(), Cam->GetComponentLocation());
			if (It->CanRayGrab() && Dist < NearestDist)
			{
				Nearest = *It;
				NearestDist = Dist;
			}
		}
		if (!Nearest)
		{
			UE_LOG(LogVRInteract, Warning, TEXT("pcs.RayGrabTest: 잡을 원근 큐브 없음"));
			return;
		}
		Nearest->BeginRayGrab(Cam, FVector(60.f, 0.f, 5.f));
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Nearest, [Nearest]() { Nearest->EndRayGrab(); }), 1.5f, false);
	}));
