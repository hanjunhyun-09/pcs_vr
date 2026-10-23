#include "VRSmoothLocomotionSubsystem.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
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
#include "NavigationSystem.h"
#include "PuzzleGrabbable.h"
#include "PuzzleUtils.h"
#include "UObject/UnrealType.h"

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

void UVRSmoothLocomotionSubsystem::DisableTemplateTeleport(APawn* Pawn)
{
	// 템플릿 VRPawn은 같은 IA_Move(오른쪽 스틱)로 텔레포트한다.
	// OpenXR 때문에 입력을 가로챌 수 없어서, 매 프레임 끝에 "텔레포트 위치가 유효함" 값을 꺼 둔다.
	// → 스틱을 놓을 때(Completed) 템플릿이 확인하는 값이 항상 false라서 텔레포트가 일어나지 않는다.
	// (이 서브시스템의 Tick은 입력 처리와 폰 Tick보다 뒤에 실행됨)
	if (const FBoolProperty* ValidProp = FindFProperty<FBoolProperty>(Pawn->GetClass(), TEXT("bValidTeleportLocation")))
	{
		ValidProp->SetPropertyValue_InContainer(Pawn, false);
	}

	// 텔레포트 궤적과 도착 표시도 숨긴다
	if (USceneComponent* Arc = PuzzleUtils::FindPawnComponent(Pawn, TEXT("TeleportTraceNiagaraSystem")))
	{
		if (Arc->IsVisible())
		{
			Arc->SetVisibility(false, true);
		}
	}
	if (const FObjectProperty* VisProp = FindFProperty<FObjectProperty>(Pawn->GetClass(), TEXT("TeleportVisualizerReference")))
	{
		if (AActor* Visualizer = Cast<AActor>(VisProp->GetObjectPropertyValue_InContainer(Pawn)))
		{
			Visualizer->SetActorHiddenInGame(true);
		}
	}
}

bool UVRSmoothLocomotionSubsystem::IsSolidForPawn(const UPrimitiveComponent* Comp, const AActor* Actor, const APawn* Pawn) const
{
	// 트리거 박스처럼 겹침만 감지하는 영역은 통과 (플레이어를 실제로 막는 충돌만 벽으로 본다)
	if (!Comp || Comp->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block)
	{
		return false;
	}
	const APuzzleGrabbable* Grabbable = Cast<APuzzleGrabbable>(Actor);
	if (Grabbable && (Grabbable->IsHeld() || Grabbable->IsHolstered()))
	{
		return false;
	}
	// 손에 붙어 있는 물건 (인트로 카드·지도 등 템플릿 GrabComponent로 잡은 것)
	if (Actor && (Actor->GetAttachParentActor() == Pawn
		|| PuzzleUtils::IsGrabComponentHeld(PuzzleUtils::FindGrabComponent(Actor))))
	{
		return false;
	}
	return true;
}

bool UVRSmoothLocomotionSubsystem::FindGround(const FVector2D& XY, float TopZ, float BottomZ, float Radius, const APawn* Pawn, float& OutZ) const
{
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VRLocomotionGround), false, Pawn);

	const FVector Start(XY.X, XY.Y, TopZ);
	const FVector End(XY.X, XY.Y, BottomZ);
	TArray<FHitResult> Hits;
	if (Radius > 0.f)
	{
		GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, Objects, FCollisionShape::MakeSphere(Radius), Params);
	}
	else
	{
		GetWorld()->LineTraceMultiByObjectType(Hits, Start, End, Objects, Params);
	}
	for (const FHitResult& H : Hits)
	{
		// 시작부터 겹친 벽은 바닥이 아니다. 선으로 찾을 때는 비탈·옆면도 제외 (윗면만)
		// (구로 훑을 때는 모서리에 걸쳐도 그 윗면으로 친다)
		if (H.bStartPenetrating || (Radius <= 0.f && H.ImpactNormal.Z < 0.7f) || !IsSolidForPawn(H.GetComponent(), H.GetActor(), Pawn))
		{
			continue;
		}
		OutZ = H.ImpactPoint.Z;
		return true;
	}
	return false;
}

bool UVRSmoothLocomotionSubsystem::HasNavMeshNear(const FVector& Point) const
{
	const UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation Out;
	// NavMesh는 벽·모서리에서 에이전트 반경(35cm)만큼 떨어져 생기므로 옆으로 넉넉하게 찾는다
	return Nav && Nav->ProjectPointToNavigation(Point, Out, FVector(60.f, 60.f, 60.f));
}

void UVRSmoothLocomotionSubsystem::FadeIn(APawn* Pawn) const
{
	const APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (PC && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 0.25f, FLinearColor::Black);
	}
}

bool UVRSmoothLocomotionSubsystem::TryClimb(APawn* Pawn, const UCameraComponent* Cam, const FVector& Dir, const FHitResult& BlockHit)
{
	const FVector CamLoc = Cam->GetComponentLocation();
	const float FloorZ = Pawn->GetActorLocation().Z;

	// 막힌 면에서 40cm 안쪽 지점의 윗면 (선반·큐브 윗면은 모서리에서 조금 들어가야 NavMesh가 있다)
	const FVector2D Land(BlockHit.ImpactPoint.X + Dir.X * 40.f, BlockHit.ImpactPoint.Y + Dir.Y * 40.f);
	const AActor* Target = BlockHit.GetActor();
	float TopZ = 0.f;
	if (!FindGround(Land, FloorZ + MaxClimbHeight + 30.f, FloorZ + MinClimbHeight, 0.f, Pawn, TopZ)
		|| TopZ - FloorZ < MinClimbHeight || TopZ - FloorZ > MaxClimbHeight)
	{
		const UPrimitiveComponent* Comp = BlockHit.GetComponent();
		const float TargetRise = Comp ? Comp->Bounds.GetBox().Max.Z - FloorZ : 0.f;
		UE_LOG(LogVRLocomotion, Log, TEXT("오르기 실패: %s 윗면 높이 %.0fcm (오를 수 있는 높이 %.0f~%.0fcm)"),
			Target ? *Target->GetName() : TEXT("?"), TargetRise, MinClimbHeight, MaxClimbHeight);
		return false;
	}
	const float Rise = TopZ - FloorZ;

	// 올라선 자리에 설 공간이 있는지 (무릎~머리 높이)
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VRLocomotionClimb), false, Pawn);
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, FVector(Land.X, Land.Y, TopZ + 100.f), FQuat::Identity, Objects,
		FCollisionShape::MakeCapsule(CollisionRadius, 70.f), Params);
	for (const FOverlapResult& O : Overlaps)
	{
		if (IsSolidForPawn(O.GetComponent(), O.GetActor(), Pawn))
		{
			UE_LOG(LogVRLocomotion, Log, TEXT("오르기 실패: 윗면에 설 공간이 없음 (%s)"), O.GetActor() ? *O.GetActor()->GetName() : TEXT("?"));
			return false;
		}
	}

	// 텔레포트로 갈 수 있는 곳(NavMesh)만: 1m 미만 큐브, 난간, 디스펜서 등에는 오르지 않는다
	if (!HasNavMeshNear(FVector(Land.X, Land.Y, TopZ)))
	{
		UE_LOG(LogVRLocomotion, Log, TEXT("오르기 실패: %s 윗면에 NavMesh가 없음"), BlockHit.GetActor() ? *BlockHit.GetActor()->GetName() : TEXT("?"));
		return false;
	}

	// 머리(카메라)가 착지 지점 위에 오도록 폰을 옮긴다
	Pawn->AddActorWorldOffset(FVector(Land.X - CamLoc.X, Land.Y - CamLoc.Y, Rise), false);
	FadeIn(Pawn);
	UE_LOG(LogVRLocomotion, Log, TEXT("오르기: %s 위로 %.0fcm"), BlockHit.GetActor() ? *BlockHit.GetActor()->GetName() : TEXT("?"), Rise);
	return true;
}

void UVRSmoothLocomotionSubsystem::FollowGround(APawn* Pawn, const UCameraComponent* Cam)
{
	const FVector CamLoc = Cam->GetComponentLocation();
	const float FloorZ = Pawn->GetActorLocation().Z;

	// 머리 아래를 반지름 15cm로 훑는다 (모서리에서 몸을 조금 내밀어도 바로 떨어지지 않게)
	float GroundZ = 0.f;
	if (!FindGround(FVector2D(CamLoc.X, CamLoc.Y), FloorZ + 30.f, FloorZ - 2000.f, 15.f, Pawn, GroundZ))
	{
		return;
	}
	const float Drop = FloorZ - GroundZ;
	if (Drop < 3.f)
	{
		return;
	}
	// 갈 수 없는 곳(난간 너머 아래층 등)으로는 떨어지지 않는다
	if (Drop > MinClimbHeight && !HasNavMeshNear(FVector(CamLoc.X, CamLoc.Y, GroundZ)))
	{
		return;
	}
	Pawn->AddActorWorldOffset(FVector(0.f, 0.f, -Drop), false);
	if (Drop > MinClimbHeight)
	{
		FadeIn(Pawn);
		UE_LOG(LogVRLocomotion, Log, TEXT("내려옴: %.0fcm"), Drop);
	}
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

	DisableTemplateTeleport(Pawn);

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

	const UCameraComponent* Cam = Pawn->FindComponentByClass<UCameraComponent>();
	if (!Cam)
	{
		return;
	}
	if (bAllowClimb)
	{
		FollowGround(Pawn, Cam);
	}

	if (FMath::Abs(Axis) < DeadZone)
	{
		ClimbHeldTime = 0.f;
		return;
	}
	Axis = FMath::Sign(Axis) * (FMath::Abs(Axis) - DeadZone) / (1.f - DeadZone);

	const FRotator Yaw(0.f, Cam->GetComponentRotation().Yaw, 0.f);
	const FVector Dir = FRotationMatrix(Yaw).GetUnitAxis(EAxis::X);
	FVector Delta = Dir * Axis * MoveSpeed * DeltaTime;

	// 벽·문에 막히면 멈추고, 비스듬히 부딪히면 벽을 따라 미끄러진다.
	// - 가슴 높이(바닥에서 최소 120cm, 구 아래쪽이 100cm라 인트로 상자 덮개에 안 걸림): 항상 검사 → 벽·문
	// - 무릎 높이(45cm): bBlockLowObstacles일 때만 (L_01부터. 인트로는 택배 상자를 지나갈 수 있게 끔)
	// 손에 들었거나 허리에 걸린 카드·큐브는 이동을 막지 않는다 (예전에 손에 든 소품이 이동을 막던 문제).
	if (CollisionRadius > 0.f)
	{
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_WorldStatic);
		Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(VRLocomotion), false, Pawn);
		const FCollisionShape Sphere = FCollisionShape::MakeSphere(CollisionRadius);
		const FVector CamLoc = Cam->GetComponentLocation();
		const float FloorZ = Pawn->GetActorLocation().Z;
		TArray<FVector, TInlineAllocator<2>> Probes;
		Probes.Add(FVector(CamLoc.X, CamLoc.Y, FMath::Max(FloorZ + 120.f, CamLoc.Z - 40.f)));
		if (bBlockLowObstacles)
		{
			Probes.Add(FVector(CamLoc.X, CamLoc.Y, FloorZ + 45.f));
		}

		auto IsBlocked = [&](const FVector& Move, FHitResult& OutHit)
		{
			for (const FVector& P : Probes)
			{
				TArray<FHitResult> Hits;
				World->SweepMultiByObjectType(Hits, P, P + Move, FQuat::Identity, Objects, Sphere, Params);
				for (const FHitResult& H : Hits)
				{
					if (IsSolidForPawn(H.GetComponent(), H.GetActor(), Pawn))
					{
						OutHit = H;
						return true;
					}
				}
			}
			return false;
		};

		FHitResult Hit;
		if (!IsBlocked(Delta, Hit))
		{
			ClimbHeldTime = 0.f;
		}
		else
		{
			// 앞으로 밀며 막혀 있으면 잠시 뒤 그 윗면으로 올라간다
			if (bAllowClimb && Axis > 0.f)
			{
				ClimbHeldTime += DeltaTime;
				if (ClimbHeldTime >= ClimbHoldTime)
				{
					ClimbHeldTime = 0.f;
					if (TryClimb(Pawn, Cam, Dir, Hit))
					{
						return;
					}
				}
			}

			const FVector WallNormal = FVector(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0.f).GetSafeNormal();
			const FVector Slide = FVector::VectorPlaneProject(Delta, WallNormal);
			FHitResult SlideHit;
			if (Slide.SizeSquared() < 0.01f || IsBlocked(Slide, SlideHit))
			{
				if (Now - LastLogTime > 1.0)
				{
					LastLogTime = Now;
					UE_LOG(LogVRLocomotion, Log, TEXT("이동이 막힘: %s"), Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("(액터 없음)"));
				}
				return;
			}
			Delta = Slide;
		}
	}

	Pawn->AddActorWorldOffset(Delta, false);
}
