#include "PuzzleLiftPad.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "PuzzleUtils.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogLiftPad, Log, All);

namespace
{
	constexpr float FadeOutTime = 0.2f;
	constexpr float FadeInTime = 0.35f;
}

APuzzleLiftPad::APuzzleLiftPad()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Disc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Disc"));
	Disc->SetupAttachment(Root);
	Disc->SetStaticMesh(CylinderMesh.Object);
	Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Disc->SetCanEverAffectNavigation(false);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Root);
	Glow->SetRelativeLocation(FVector(0.f, 0.f, 30.f));
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetIntensity(4.f);
	Glow->SetAttenuationRadius(250.f);
	Glow->SetCastShadows(false);
}

void APuzzleLiftPad::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// 기본 원기둥은 지름 100cm, 높이 100cm, 가운데 기준
	Disc->SetRelativeScale3D(FVector(Diameter / 100.f, Diameter / 100.f, 0.02f));
	Disc->SetRelativeLocation(FVector(0.f, 0.f, 1.f));
	Glow->SetLightColor(GlowColor);
}

void APuzzleLiftPad::BeginPlay()
{
	Super::BeginPlay();
	DiscMID = PuzzleUtils::ApplySolidColor(Disc, GlowColor, 2.f);
}

void APuzzleLiftPad::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const double Now = GetWorld()->GetTimeSeconds();
	if (bMoving)
	{
		return;
	}

	FVector ViewLoc;
	FRotator ViewRot;
	bool bOnPad = false;
	if (PuzzleUtils::GetPlayerView(this, ViewLoc, ViewRot))
	{
		const FVector PadLoc = GetActorLocation();
		const float Up = ViewLoc.Z - PadLoc.Z;
		// 헤드셋이 없으면 카메라가 바닥 높이라 Up이 0 근처 → 아래쪽 여유를 둔다
		bOnPad = FVector::Dist2D(ViewLoc, PadLoc) <= Diameter * 0.5f && Up > -20.f && Up < 250.f;
	}

	StandTime = bOnPad ? StandTime + DeltaTime : 0.f;
	const float Charge = HoldTime > 0.f ? FMath::Clamp(StandTime / HoldTime, 0.f, 1.f) : 1.f;
	PuzzleUtils::SetSolidColor(DiscMID, GlowColor, 1.5f + 2.f * (0.5f + 0.5f * FMath::Sin(Now * 2.5)) + 6.f * Charge);
	if (bOnPad && StandTime >= HoldTime)
	{
		Lift();
	}
}

void APuzzleLiftPad::Lift()
{
	if (bMoving)
	{
		return;
	}
	bMoving = true;
	StandTime = 0.f;
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, FadeOutTime, FLinearColor::Black, false, true);
		}
	}
	GetWorldTimerManager().SetTimer(MoveHandle, this, &APuzzleLiftPad::MovePlayer, FadeOutTime + 0.05f, false);
}

void APuzzleLiftPad::MovePlayer()
{
	APawn* Pawn = PuzzleUtils::GetPlayerPawn(this);
	FVector ViewLoc;
	FRotator ViewRot;
	if (Pawn && PuzzleUtils::GetPlayerView(this, ViewLoc, ViewRot))
	{
		// 머리(카메라)의 수평 위치가 Destination 위에 오게. 높이는 발판 → 도착 바닥 차이만큼
		const FVector Delta(Destination.X - ViewLoc.X, Destination.Y - ViewLoc.Y, Destination.Z - GetActorLocation().Z);
		Pawn->SetActorLocation(Pawn->GetActorLocation() + Delta, false, nullptr, ETeleportType::TeleportPhysics);
		UE_LOG(LogLiftPad, Log, TEXT("%s: 플레이어 이동 → %s"), *GetName(), *Destination.ToString());
		OnLifted.Broadcast();
	}
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, FadeInTime, FLinearColor::Black, false, false);
		}
	}
	// 도착한 쪽 발판을 곧바로 다시 타지 않게 잠깐 쉬었다가 다시 감지
	GetWorldTimerManager().SetTimer(MoveHandle, FTimerDelegate::CreateWeakLambda(this, [this]() { bMoving = false; }), 1.f, false);
}
