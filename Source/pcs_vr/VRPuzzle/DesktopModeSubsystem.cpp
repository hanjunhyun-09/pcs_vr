#include "DesktopModeSubsystem.h"

#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "IXRTrackingSystem.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "PerspectiveCube.h"
#include "PuzzleGrabbable.h"
#include "PuzzleUtils.h"
#include "VRInteractionSubsystem.h"
#include "VRSmoothLocomotionSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogDesktopMode, Log, All);

bool UDesktopModeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

bool UDesktopModeSubsystem::IsDesktopMode()
{
	// UVRInteractionSubsystem의 눈높이 보정, APuzzleHandDevice와 같은 기준
	return !(GEngine && GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowed());
}

void UDesktopModeSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();
	UVRSmoothLocomotionSubsystem* Locomotion = World ? World->GetSubsystem<UVRSmoothLocomotionSubsystem>() : nullptr;
	if (!IsDesktopMode())
	{
		if (Locomotion)
		{
			Locomotion->SetDesktopMove(FVector2D::ZeroVector);
		}
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	USceneComponent* Cam = PuzzleUtils::GetPlayerCamera(this);
	if (!Pawn || !Cam)
	{
		return;
	}
	if (!bInputReady && PC->InputComponent)
	{
		SetupInput(PC);
	}

	if (Locomotion)
	{
		Locomotion->SetDesktopMove(MoveInput);
	}

	// 잡고 있던 물건이 다른 이유로 놓였으면 (카드가 허리로 돌아감, 큐브가 너무 커서 놓침 등) 정리
	if (HeldGrab.IsValid() && !PuzzleUtils::IsGrabComponentHeld(HeldGrab.Get()))
	{
		HeldGrab.Reset();
	}

	PlaceHands(Pawn, Cam);

#if ENABLE_DRAW_DEBUG
	// 화면 가운데 조준점
	DrawDebugPoint(World, Cam->GetComponentLocation() + Cam->GetForwardVector() * 50.f, 4.f, FColor::White, false, -1.f, SDPG_Foreground);
#endif
}

void UDesktopModeSubsystem::SetupInput(APlayerController* PC)
{
	const ULocalPlayer* LP = PC->GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Input = LP ? LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PC->InputComponent);
	if (!Input || !EIC)
	{
		return;
	}

	// 에셋을 따로 만들지 않도록 입력 액션과 매핑을 코드에서 만든다 (키보드·마우스는 OpenXR을 거치지 않음)
	MoveAction = NewObject<UInputAction>(this, TEXT("IA_Desktop_Move"));
	MoveAction->ValueType = EInputActionValueType::Axis2D;
	LookAction = NewObject<UInputAction>(this, TEXT("IA_Desktop_Look"));
	LookAction->ValueType = EInputActionValueType::Axis2D;
	GrabAction = NewObject<UInputAction>(this, TEXT("IA_Desktop_Grab"));
	Context = NewObject<UInputMappingContext>(this, TEXT("IMC_Desktop"));

	// WASD → (X = 오른쪽, Y = 앞)
	Context->MapKey(MoveAction, EKeys::W).Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(Context));
	{
		FEnhancedActionKeyMapping& S = Context->MapKey(MoveAction, EKeys::S);
		S.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(Context));
		S.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
	}
	Context->MapKey(MoveAction, EKeys::D);
	Context->MapKey(MoveAction, EKeys::A).Modifiers.Add(NewObject<UInputModifierNegate>(Context));
	Context->MapKey(LookAction, EKeys::Mouse2D);
	Context->MapKey(GrabAction, EKeys::LeftMouseButton);
	Input->AddMappingContext(Context, 6);

	EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &UDesktopModeSubsystem::OnMove);
	EIC->BindAction(MoveAction, ETriggerEvent::Completed, this, &UDesktopModeSubsystem::OnMoveEnd);
	EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &UDesktopModeSubsystem::OnLook);
	EIC->BindAction(GrabAction, ETriggerEvent::Started, this, &UDesktopModeSubsystem::OnGrab);

	PC->SetInputMode(FInputModeGameOnly());
	PC->bShowMouseCursor = false;

	if (const USceneComponent* Cam = PuzzleUtils::GetPlayerCamera(this))
	{
		LookYaw = Cam->GetRelativeRotation().Yaw;
		LookPitch = Cam->GetRelativeRotation().Pitch;
	}
	bInputReady = true;
	UE_LOG(LogDesktopMode, Log, TEXT("헤드셋 없음 → 데스크톱 테스트 모드 (마우스 시점, WASD 이동, 왼쪽 클릭 잡기, E 상호작용, R 레이 잡기, Q 장치 다이얼)"));
}

void UDesktopModeSubsystem::PlaceHands(APawn* Pawn, const USceneComponent* Cam)
{
	const FTransform CamTM = Cam->GetComponentTransform();
	const FVector CamLoc = CamTM.GetLocation();
	const FVector Fwd = Cam->GetForwardVector();
	const FQuat Rot = CamTM.GetRotation();

	FVector Right = CamTM.TransformPosition(HandOffset);
	const FVector Left = CamTM.TransformPosition(FVector(HandOffset.X, -HandOffset.Y, HandOffset.Z));

	// 물건을 든 채 면(카드 리더기, 코어 슬롯 등)을 보면 오른손을 그 면 바로 앞까지 뻗는다
	if (HeldGrab.IsValid())
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DesktopHandReach), false, Pawn);
		Params.AddIgnoredActor(HeldGrab->GetOwner());
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, CamLoc + Fwd * HandExtendReach, ECC_Visibility, Params))
		{
			Right = Hit.ImpactPoint - Fwd * 3.f;
		}
	}

	// 헤드셋이 없으면 MotionController는 추적되지 않아 위치를 바꾸지 않는다 → 여기서 정한 위치가 유지됨
	// (이 서브시스템의 Tick은 폰·컴포넌트 Tick보다 뒤에 실행됨)
	for (const TCHAR* Name : { TEXT("MotionControllerRightGrip"), TEXT("MotionControllerRightAim") })
	{
		if (USceneComponent* C = PuzzleUtils::FindPawnComponent(Pawn, Name))
		{
			C->SetWorldLocationAndRotation(Right, Rot);
		}
	}
	for (const TCHAR* Name : { TEXT("MotionControllerLeftGrip"), TEXT("MotionControllerLeftAim") })
	{
		if (USceneComponent* C = PuzzleUtils::FindPawnComponent(Pawn, Name))
		{
			C->SetWorldLocationAndRotation(Left, Rot);
		}
	}
}

void UDesktopModeSubsystem::OnMove(const FInputActionValue& Value)
{
	MoveInput = Value.Get<FVector2D>();
}

void UDesktopModeSubsystem::OnMoveEnd(const FInputActionValue& Value)
{
	MoveInput = FVector2D::ZeroVector;
}

void UDesktopModeSubsystem::OnLook(const FInputActionValue& Value)
{
	USceneComponent* Cam = PuzzleUtils::GetPlayerCamera(this);
	if (!Cam || !IsDesktopMode())
	{
		return;
	}
	const FVector2D Axis = Value.Get<FVector2D>();
	LookYaw += Axis.X * MouseSensitivity;
	LookPitch = FMath::Clamp(LookPitch + Axis.Y * MouseSensitivity, -85.f, 85.f);
	Cam->SetRelativeRotation(FRotator(LookPitch, LookYaw, 0.f));
}

void UDesktopModeSubsystem::OnGrab(const FInputActionValue& Value)
{
	UVRInteractionSubsystem* Interaction = GetWorld()->GetSubsystem<UVRInteractionSubsystem>();
	APawn* Pawn = PuzzleUtils::GetPlayerPawn(this);
	USceneComponent* Cam = PuzzleUtils::GetPlayerCamera(this);
	if (!Interaction || !Pawn || !Cam || !IsDesktopMode() || Interaction->GetActiveDevice())
	{
		// 손 장치를 찬 동안에는 물건을 잡을 수 없다 (VR과 같음)
		return;
	}

	// 들고 있으면 놓기
	if (UActorComponent* Held = HeldGrab.Get())
	{
		HeldGrab.Reset();
		PuzzleUtils::ReleaseGrabComponent(Held);
		UE_LOG(LogDesktopMode, Log, TEXT("왼쪽 클릭 -> %s 놓기"), *Held->GetOwner()->GetName());
		return;
	}
	if (Interaction->IsDesktopRayHolding())
	{
		Interaction->ToggleDesktopRayGrab();
		return;
	}

	// 시선 앞에서 처음 만나는 잡을 수 있는 물건. 벽(WorldStatic)에 닿으면 그 너머는 보지 않는다
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DesktopGrab), false, Pawn);
	const FVector Start = Cam->GetComponentLocation();
	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByObjectType(Hits, Start, Start + Cam->GetForwardVector() * HandReach, FQuat::Identity, Objects,
		FCollisionShape::MakeSphere(6.f), Params);
	for (const FHitResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		if (Cast<APerspectiveCube>(Target))
		{
			// 원근 큐브는 손으로 끌어오면 크기가 안 맞으므로 레이 잡기로 (보이는 크기 유지)
			break;
		}
		if (GrabWithRightHand(Pawn, Target))
		{
			return;
		}
		const UPrimitiveComponent* Comp = Hit.GetComponent();
		if (Comp && Comp->GetCollisionObjectType() == ECC_WorldStatic)
		{
			break;
		}
	}

	// 손으로 잡을 게 없으면 멀리 있는 원근 큐브 레이 잡기 (R 키와 같음)
	Interaction->ToggleDesktopRayGrab();
	if (Interaction->IsDesktopRayHolding())
	{
		return;
	}

	// 그것도 없으면 허리에 걸린 물건(카드)을 꺼낸다. 허리는 눈 아래 60cm라 데스크톱에서 겨누기 어려우므로
	// VR에서 손을 허리로 가져가는 동작 대신 이렇게 한다
	for (TActorIterator<APuzzleGrabbable> It(GetWorld()); It; ++It)
	{
		if (It->IsHolstered() && GrabWithRightHand(Pawn, *It))
		{
			return;
		}
	}
}

bool UDesktopModeSubsystem::GrabWithRightHand(APawn* Pawn, AActor* Target)
{
	UActorComponent* Grab = PuzzleUtils::FindGrabComponent(Target);
	USceneComponent* Hand = PuzzleUtils::FindPawnComponent(Pawn, TEXT("MotionControllerRightGrip"));
	if (!Grab || !Hand || PuzzleUtils::IsGrabComponentHeld(Grab))
	{
		return false;
	}
	// 손을 물건 위치로 옮겨서 잡는다 → 다음 Tick에 손이 제자리로 오면서 물건을 가져온다
	Hand->SetWorldLocation(Target->GetActorLocation());
	if (!PuzzleUtils::TryGrabWithHand(Grab, Hand))
	{
		return false;
	}
	HeldGrab = Grab;
	UE_LOG(LogDesktopMode, Log, TEXT("왼쪽 클릭 -> %s 잡기"), *Target->GetName());
	return true;
}
