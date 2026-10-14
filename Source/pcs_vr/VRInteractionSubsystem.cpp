#include "VRInteractionSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
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

	// 데스크톱 테스트: E 키 (키보드는 OpenXR을 거치지 않아서 새 매핑으로 충분)
	DesktopAction = NewObject<UInputAction>(this, TEXT("IA_Puzzle_Interact_Desktop"));
	Context = NewObject<UInputMappingContext>(this, TEXT("IMC_Puzzle_Interact"));
	Context->MapKey(DesktopAction, EKeys::E);
	Input->AddMappingContext(Context, 5);
	EIC->BindAction(DesktopAction, ETriggerEvent::Started, this, &UVRInteractionSubsystem::OnDesktopInteract);

	PlayerInput = Input->GetPlayerInput();
	bInputReady = true;
	UE_LOG(LogVRInteract, Log, TEXT("트리거 상호작용 입력 준비 완료 (템플릿 트리거 액션 읽기 / 데스크톱 E)"));
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

void UVRInteractionSubsystem::OnLeftTrigger()
{
	InteractWithHand(TEXT("MotionControllerLeftGrip"));
}

void UVRInteractionSubsystem::OnRightTrigger()
{
	InteractWithHand(TEXT("MotionControllerRightGrip"));
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
