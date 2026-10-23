#include "PuzzleTeleportPad.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "PuzzleUtils.h"
#include "UObject/ConstructorHelpers.h"

APuzzleTeleportPad::APuzzleTeleportPad()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	Disc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Disc"));
	SetRootComponent(Disc);
	Disc->SetStaticMesh(CylinderMesh.Object);
	Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Disc->SetCanEverAffectNavigation(false);

	// 가운데 어두운 원 → ◎ 모양
	Ring = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ring"));
	Ring->SetupAttachment(Disc);
	Ring->SetStaticMesh(CylinderMesh.Object);
	Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ring->SetCanEverAffectNavigation(false);
	Ring->SetUsingAbsoluteScale(true);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Disc);
	Glow->SetUsingAbsoluteScale(true);
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetIntensity(4.f);
	Glow->SetAttenuationRadius(250.f);
	Glow->SetCastShadows(false);
}

void APuzzleTeleportPad::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// 액터 위치 = 바닥 높이. 원판은 두께 1cm
	Disc->SetRelativeScale3D(FVector(Diameter / 100.f, Diameter / 100.f, 0.01f));
	Ring->SetWorldScale3D(FVector(Diameter * 0.55f / 100.f, Diameter * 0.55f / 100.f, 0.012f));
	Ring->SetRelativeLocation(FVector(0.f, 0.f, 15.f));
	Glow->SetRelativeLocation(FVector(0.f, 0.f, 1500.f));
	Glow->SetLightColor(GlowColor);
}

void APuzzleTeleportPad::BeginPlay()
{
	Super::BeginPlay();
	DiscMID = PuzzleUtils::ApplySolidColor(Disc, GlowColor, 4.f);
	RingMID = PuzzleUtils::ApplySolidColor(Ring, FLinearColor(0.02f, 0.03f, 0.04f), 0.f);
}

void APuzzleTeleportPad::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bReached)
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	PuzzleUtils::SetSolidColor(DiscMID, GlowColor, 2.f + 3.f * (0.5f + 0.5f * FMath::Sin(Now * 3.0)));

	FVector ViewLoc;
	FRotator ViewRot;
	if (!PuzzleUtils::GetPlayerView(this, ViewLoc, ViewRot))
	{
		return;
	}
	const FVector PadLoc = GetActorLocation();
	const float Horizontal = FVector::Dist2D(ViewLoc, PadLoc);
	const float Up = ViewLoc.Z - PadLoc.Z;
	if (Horizontal <= Diameter * 0.5f && Up > 0.f && Up < 250.f)
	{
		bReached = true;
		PuzzleUtils::SetSolidColor(DiscMID, FLinearColor(0.05f, 0.08f, 0.1f), 0.f);
		Glow->SetVisibility(false);
		SetActorTickEnabled(false);
		OnReached.Broadcast();
	}
}
