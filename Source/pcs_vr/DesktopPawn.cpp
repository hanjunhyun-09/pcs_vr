#include "DesktopPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Interactable.h"
#include "PerspectiveObjectComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogDesktopPawn, Log, All);

ADesktopPawn::ADesktopPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 92.f);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.f, 0.f, EyeHeight - 92.f));
	Camera->bUsePawnControlRotation = true;

	PhysicsHandle = CreateDefaultSubobject<UPhysicsHandleComponent>(TEXT("PhysicsHandle"));

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->MaxWalkSpeed = 300.f;
}

void ADesktopPawn::BeginPlay()
{
	Super::BeginPlay();

	// 에디터에서 EyeHeight를 바꿨을 수 있으니 다시 맞춘다 (캡슐 중심 기준)
	Camera->SetRelativeLocation(FVector(0.f, 0.f, EyeHeight - GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
}

void ADesktopPawn::CreateInputObjects()
{
	if (DesktopContext)
	{
		return;
	}

	// 에셋을 따로 만들지 않도록 입력 액션과 매핑을 코드에서 만든다
	MoveAction = NewObject<UInputAction>(this, TEXT("IA_DesktopMove"));
	MoveAction->ValueType = EInputActionValueType::Axis2D;
	LookAction = NewObject<UInputAction>(this, TEXT("IA_DesktopLook"));
	LookAction->ValueType = EInputActionValueType::Axis2D;
	GrabAction = NewObject<UInputAction>(this, TEXT("IA_DesktopGrab"));
	GrabAction->ValueType = EInputActionValueType::Boolean;
	InteractAction = NewObject<UInputAction>(this, TEXT("IA_DesktopInteract"));
	InteractAction->ValueType = EInputActionValueType::Boolean;
	TeleportAction = NewObject<UInputAction>(this, TEXT("IA_DesktopTeleport"));
	TeleportAction->ValueType = EInputActionValueType::Boolean;

	DesktopContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Desktop"));

	// WASD → (X = 오른쪽, Y = 앞)
	{
		FEnhancedActionKeyMapping& W = DesktopContext->MapKey(MoveAction, EKeys::W);
		W.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(DesktopContext));
	}
	{
		FEnhancedActionKeyMapping& S = DesktopContext->MapKey(MoveAction, EKeys::S);
		S.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(DesktopContext));
		S.Modifiers.Add(NewObject<UInputModifierNegate>(DesktopContext));
	}
	DesktopContext->MapKey(MoveAction, EKeys::D);
	{
		FEnhancedActionKeyMapping& A = DesktopContext->MapKey(MoveAction, EKeys::A);
		A.Modifiers.Add(NewObject<UInputModifierNegate>(DesktopContext));
	}

	// 마우스 (UE 1인칭 템플릿과 같이 Y를 뒤집는다)
	{
		FEnhancedActionKeyMapping& Mouse = DesktopContext->MapKey(LookAction, EKeys::Mouse2D);
		UInputModifierNegate* NegateY = NewObject<UInputModifierNegate>(DesktopContext);
		NegateY->bX = false;
		NegateY->bZ = false;
		Mouse.Modifiers.Add(NegateY);
	}

	DesktopContext->MapKey(GrabAction, EKeys::LeftMouseButton);
	DesktopContext->MapKey(InteractAction, EKeys::E);
	DesktopContext->MapKey(TeleportAction, EKeys::RightMouseButton);
}

void ADesktopPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	CreateInputObjects();

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogDesktopPawn, Warning, TEXT("Enhanced Input 컴포넌트가 아닙니다. DefaultInput.ini를 확인하세요."));
		return;
	}
	EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADesktopPawn::OnMove);
	EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADesktopPawn::OnLook);
	EIC->BindAction(GrabAction, ETriggerEvent::Started, this, &ADesktopPawn::OnGrab);
	EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &ADesktopPawn::OnInteract);
	EIC->BindAction(TeleportAction, ETriggerEvent::Started, this, &ADesktopPawn::OnTeleport);
}

void ADesktopPawn::PawnClientRestart()
{
	Super::PawnClientRestart();
	CreateInputObjects();

	APlayerController* PC = Cast<APlayerController>(GetController());
	const ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Input = LP ? LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Input)
	{
		return;
	}

	// VRPawn이 남긴 VR 입력 매핑은 지우고 데스크톱 매핑만 쓴다
	Input->ClearAllMappings();
	Input->AddMappingContext(DesktopContext, 100);

	PC->SetInputMode(FInputModeGameOnly());
	PC->bShowMouseCursor = false;
}

void ADesktopPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const FVector CamLoc = Camera->GetComponentLocation();
	const FVector CamFwd = Camera->GetForwardVector();

	if (PhysicsHandle->GetGrabbedComponent())
	{
		PhysicsHandle->SetTargetLocation(CamLoc + CamFwd * PhysicsHoldDistance);
	}

#if ENABLE_DRAW_DEBUG
	if (bShowCrosshair)
	{
		DrawDebugPoint(GetWorld(), CamLoc + CamFwd * 50.f, 4.f, FColor::White, false, -1.f, SDPG_Foreground);
	}
#endif
}

bool ADesktopPawn::TraceFromCamera(float Range, FHitResult& OutHit) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DesktopPawnTrace), false, this);
	if (IsValid(HeldPerspective))
	{
		Params.AddIgnoredActor(HeldPerspective->GetOwner());
	}
	const FVector Start = Camera->GetComponentLocation();
	const FVector End = Start + Camera->GetForwardVector() * Range;
	return GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, Params);
}

void ADesktopPawn::ReleaseHeld()
{
	if (IsValid(HeldPerspective))
	{
		HeldPerspective->EndHold();
	}
	HeldPerspective = nullptr;

	if (PhysicsHandle->GetGrabbedComponent())
	{
		PhysicsHandle->ReleaseComponent();
	}
}

void ADesktopPawn::ToggleGrab()
{
	// 들고 있던 원근법 물체가 다른 이유로 놓였거나 사라졌으면 정리
	if (HeldPerspective && (!IsValid(HeldPerspective) || !HeldPerspective->IsHeld()))
	{
		HeldPerspective = nullptr;
	}

	if (HeldPerspective || PhysicsHandle->GetGrabbedComponent())
	{
		ReleaseHeld();
		return;
	}

	FHitResult Hit;
	if (!TraceFromCamera(GrabRange, Hit))
	{
		return;
	}
	AActor* Target = Hit.GetActor();
	if (!Target)
	{
		return;
	}

	// 1. 원근법 물체: 멀리서도 잡을 수 있음
	if (UPerspectiveObjectComponent* Persp = Target->FindComponentByClass<UPerspectiveObjectComponent>())
	{
		if (Persp->BeginHold(Camera))
		{
			HeldPerspective = Persp;
		}
		return;
	}

	if (Hit.Distance > InteractRange)
	{
		return;
	}

	// 2. 잡으면 반응하는 물체 (인트로 출입카드 등): 상호작용으로 처리
	if (UInteractLibrary::TryInteract(Target, this))
	{
		return;
	}

	// 3. 일반 물리 물체: 눈앞에 들고 다니기
	UPrimitiveComponent* Comp = Hit.GetComponent();
	if (Comp && Comp->IsSimulatingPhysics())
	{
		PhysicsHandle->GrabComponentAtLocationWithRotation(Comp, NAME_None, Comp->GetComponentLocation(), Comp->GetComponentRotation());
	}
}

void ADesktopPawn::InteractInFront()
{
	FHitResult Hit;
	if (TraceFromCamera(InteractRange, Hit))
	{
		UInteractLibrary::TryInteract(Hit.GetActor(), this);
	}
}

void ADesktopPawn::TeleportToAim()
{
	FHitResult Hit;
	if (!TraceFromCamera(TeleportRange, Hit))
	{
		return;
	}
	// 바닥처럼 위를 향한 면에만 텔레포트 (VR 텔레포트처럼 벽은 안 됨)
	if (Hit.ImpactNormal.Z < 0.7f)
	{
		return;
	}

	const FVector Dest = Hit.ImpactPoint + FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.f);
	if (!TeleportTo(Dest, GetActorRotation()))
	{
		UE_LOG(LogDesktopPawn, Log, TEXT("텔레포트할 공간이 부족합니다."));
	}
}

void ADesktopPawn::OnMove(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotator Yaw(0.f, GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Axis.X);
}

void ADesktopPawn::OnLook(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X * MouseSensitivity);
	AddControllerPitchInput(Axis.Y * MouseSensitivity);
}

void ADesktopPawn::OnGrab(const FInputActionValue& Value)
{
	ToggleGrab();
}

void ADesktopPawn::OnInteract(const FInputActionValue& Value)
{
	InteractInFront();
}

void ADesktopPawn::OnTeleport(const FInputActionValue& Value)
{
	TeleportToAim();
}
