#include "VRSmoothLocomotionSubsystem.h"

#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"

DEFINE_LOG_CATEGORY_STATIC(LogVRLocomotion, Log, All);

namespace
{
	// 템플릿 IMC_Default가 쓰는 오른쪽 조이스틱 위/아래 키 (원래 텔레포트용, 확실히 동작함)
	const TArray<FKey>& RightStickYKeys()
	{
		static const TArray<FKey> Keys = {
			FKey(TEXT("OculusTouch_Right_Thumbstick_Y")),
			FKey(TEXT("ValveIndex_Right_Thumbstick_Y")),
			FKey(TEXT("MixedReality_Right_Thumbstick_Y")),
		};
		return Keys;
	}

	// 왼쪽 조이스틱 위/아래 (동작하면 같이 쓴다)
	const TArray<FKey>& LeftStickYKeys()
	{
		static const TArray<FKey> Keys = {
			FKey(TEXT("OculusTouch_Left_Thumbstick_Y")),
			FKey(TEXT("ValveIndex_Left_Thumbstick_Y")),
			FKey(TEXT("MixedReality_Left_Thumbstick_Y")),
		};
		return Keys;
	}
}

bool UVRSmoothLocomotionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

float UVRSmoothLocomotionSubsystem::ReadRawStick(APlayerController* PC) const
{
	return 0.f;
}

void UVRSmoothLocomotionSubsystem::SetupInput(APlayerController* PC)
{
	const ULocalPlayer* LP = PC->GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Input = LP ? LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PC->InputComponent);
	if (!Input || !EIC)
	{
		UE_LOG(LogVRLocomotion, Warning, TEXT("Enhanced Input을 아직 사용할 수 없습니다 (Input=%d, EIC=%d)"), Input != nullptr, EIC != nullptr);
		return;
	}

	// 템플릿의 이동 입력(IA_Move)을 건드리지 않고, 같은 액션의 값을 읽기만 한다.
	// (이번 버전은 원인 확인용: 텔레포트도 그대로 남아 있다)
	UInputAction* TemplateMove = LoadObject<UInputAction>(nullptr, TEXT("/Game/VRTemplate/Input/Actions/IA_Move.IA_Move"));
	if (!TemplateMove)
	{
		UE_LOG(LogVRLocomotion, Warning, TEXT("IA_Move를 찾지 못했습니다."));
		return;
	}
	MoveAction = TemplateMove;

	EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &UVRSmoothLocomotionSubsystem::OnMoveAxis);
	EIC->BindAction(MoveAction, ETriggerEvent::Completed, this, &UVRSmoothLocomotionSubsystem::OnMoveEnd);
	EIC->BindAction(MoveAction, ETriggerEvent::Canceled, this, &UVRSmoothLocomotionSubsystem::OnMoveEnd);

	bInputReady = true;
	UE_LOG(LogVRLocomotion, Log, TEXT("템플릿 IA_Move 입력에 연결했습니다 (조이스틱 앞으로 = 이동 + 텔레포트 궤적)"));
}

void UVRSmoothLocomotionSubsystem::OnMoveAxis(const FInputActionValue& Value)
{
	EnhancedAxis = Value.Get<float>();
}

void UVRSmoothLocomotionSubsystem::OnMoveEnd(const FInputActionValue& Value)
{
	EnhancedAxis = 0.f;
}

void UVRSmoothLocomotionSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn || !Pawn->GetClass()->GetName().Contains(TEXT("VRPawn")))
	{
		return;
	}

	if (!bInputReady)
	{
		SetupInput(PC);
	}

	const double Now = World->GetTimeSeconds();

	// 진단: 손과 카드의 거리 (2초마다)
	if (Now - LastBlockLogTime > 2.0)
	{
		LastBlockLogTime = Now;
		USceneComponent* LGrip = nullptr;
		USceneComponent* RGrip = nullptr;
		TInlineComponentArray<USceneComponent*> SC;
		Pawn->GetComponents(SC);
		for (USceneComponent* C : SC)
		{
			const FString N = C->GetName();
			if (N.Equals(TEXT("MotionControllerLeftGrip"))) { LGrip = C; }
			if (N.Equals(TEXT("MotionControllerRightGrip"))) { RGrip = C; }
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetClass()->GetName().Contains(TEXT("BP_Intro_Keycard")))
			{
				const FVector CL = It->GetActorLocation();
				UE_LOG(LogVRLocomotion, Log, TEXT("[진단] 카드=(%.0f,%.0f,%.0f) 왼손거리=%.0fcm 오른손거리=%.0fcm 폰=(%.0f,%.0f,%.0f) 조이스틱=%.2f"),
					CL.X, CL.Y, CL.Z,
					LGrip ? FVector::Dist(LGrip->GetComponentLocation(), CL) : -1.f,
					RGrip ? FVector::Dist(RGrip->GetComponentLocation(), CL) : -1.f,
					Pawn->GetActorLocation().X, Pawn->GetActorLocation().Y, Pawn->GetActorLocation().Z, EnhancedAxis);
				break;
			}
		}
	}

	// 이벤트로 받은 값과, 입력 시스템에서 직접 읽은 IA_Move 값 중 큰 쪽을 쓴다
	float Polled = 0.f;
	if (const ULocalPlayer* LP = PC->GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InSys = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (UEnhancedPlayerInput* EnhancedInput = InSys->GetPlayerInput())
			{
				if (MoveAction)
				{
					Polled = EnhancedInput->GetActionValue(MoveAction).Get<float>();
				}
			}
		}
	}
	float Axis = FMath::Abs(EnhancedAxis) > FMath::Abs(Polled) ? EnhancedAxis : Polled;

	static double LastAxisLog = 0.0;
	if (Now - LastAxisLog > 1.0 && FMath::Abs(Axis) > 0.05f)
	{
		LastAxisLog = Now;
		UE_LOG(LogVRLocomotion, Log, TEXT("IA_Move 값: 이벤트=%.2f 직접읽기=%.2f"), EnhancedAxis, Polled);
	}

	if (FMath::Abs(Axis) < DeadZone)
	{
		return;
	}
	Axis = FMath::Sign(Axis) * (FMath::Abs(Axis) - DeadZone) / (1.f - DeadZone);

	const UCameraComponent* Cam = Pawn->FindComponentByClass<UCameraComponent>();
	if (!Cam)
	{
		return;
	}
	const FRotator Yaw(0.f, Cam->GetComponentRotation().Yaw, 0.f);
	const FVector Dir = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X);
	const FVector Delta = Dir * Axis * MoveSpeed * DeltaTime;

	// 이동 방향 앞쪽에 벽이 있으면 멈춘다
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VRLocomotion), false, Pawn);
	const FVector Start = Cam->GetComponentLocation();
	const FVector End = Start + Dir * FMath::Sign(Axis) * (CollisionRadius + Delta.Size());
	// 벽 충돌 검사는 기본으로 꺼 둔다. (소품이 눈높이에 걸려 이동을 막는 문제가 있었음)
	// 이후 스테이지에 벽 전용 충돌이 생기면 CollisionRadius > 0 으로 다시 켠다.
	if (CollisionRadius > 0.f && World->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeSphere(CollisionRadius), Params) && Hit.bBlockingHit)
	{
		if (Now - LastLogTime > 1.0)
		{
			LastLogTime = Now;
			UE_LOG(LogVRLocomotion, Log, TEXT("이동이 막힘: %s"), Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("(액터 없음)"));
		}
		return;
	}

	Pawn->AddActorWorldOffset(Delta, false);
}
