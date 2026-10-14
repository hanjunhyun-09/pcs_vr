#include "PuzzleGrabbable.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "PuzzleUtils.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogPuzzleGrab, Log, All);

APuzzleGrabbable::APuzzleGrabbable()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetStaticMesh(CubeMesh.Object);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetSimulatePhysics(true);
	Mesh->SetGenerateOverlapEvents(true);
	Mesh->SetCanEverAffectNavigation(false);

	// 보이지 않는 잡기 영역 (GrabAssistExtent > 0 일 때만 켜짐). 카드 크기(액터 스케일)와 상관없이 실제 cm 크기
	GrabAssist = CreateDefaultSubobject<UBoxComponent>(TEXT("GrabAssist"));
	GrabAssist->SetupAttachment(Mesh);
	GrabAssist->SetUsingAbsoluteScale(true);
	GrabAssist->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GrabAssist->SetCanEverAffectNavigation(false);
	GrabAssist->SetGenerateOverlapEvents(false);
	GrabAssist->SetHiddenInGame(true);
}

void APuzzleGrabbable::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (GrabAssistExtent.GetMin() > 0.f)
	{
		// 템플릿의 잡기 검사는 PhysicsBody 종류의 충돌만 찾는다. 물리 충돌은 없이(QueryOnly) 검사에만 걸리게 한다
		GrabAssist->SetBoxExtent(GrabAssistExtent);
		GrabAssist->SetCollisionObjectType(ECC_PhysicsBody);
		GrabAssist->SetCollisionResponseToAllChannels(ECR_Ignore);
		GrabAssist->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	else
	{
		GrabAssist->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void APuzzleGrabbable::BeginPlay()
{
	Super::BeginPlay();

	MID = PuzzleUtils::ApplySolidColor(Mesh, Color, Emissive);
	GrabComp = PuzzleUtils::FindGrabComponent(this);
	if (!GrabComp)
	{
		UE_LOG(LogPuzzleGrab, Warning, TEXT("%s: GrabComponent가 없습니다 (블루프린트에 붙여야 잡을 수 있음)"), *GetName());
	}
	if (bHolsterToPlayer)
	{
		bHolstered = true;
		Mesh->SetSimulatePhysics(false);
	}
}

bool APuzzleGrabbable::IsHeld() const
{
	return PuzzleUtils::IsGrabComponentHeld(GrabComp);
}

void APuzzleGrabbable::SetGlow(float NewEmissive)
{
	PuzzleUtils::SetSolidColor(MID, Color, NewEmissive);
}

void APuzzleGrabbable::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const bool bHeld = IsHeld();
	const double Now = GetWorld()->GetTimeSeconds();
	if (bHeld && !bWasHeld)
	{
		bHolstered = false;
		UE_LOG(LogPuzzleGrab, Log, TEXT("%s 잡음"), *GetName());
		OnGrabbed();
	}
	else if (!bHeld && bWasHeld)
	{
		ReleasedTime = Now;
		UE_LOG(LogPuzzleGrab, Log, TEXT("%s 놓음"), *GetName());
		OnReleased();
	}
	bWasHeld = bHeld;

	if (bGlowPulse && !bHeld)
	{
		SetGlow(Emissive * (0.35f + 0.65f * (0.5f + 0.5f * FMath::Sin(Now * 4.0))));
	}

	if (bHolsterToPlayer && !bHeld && (bHolstered || Now - ReleasedTime >= HolsterDelay))
	{
		UpdateHolster();
	}
}

void APuzzleGrabbable::UpdateHolster()
{
	FVector ViewLoc;
	FRotator ViewRot;
	if (!PuzzleUtils::GetPlayerView(this, ViewLoc, ViewRot))
	{
		return;
	}
	if (!bHolstered)
	{
		bHolstered = true;
		Mesh->SetSimulatePhysics(false);
	}
	// 고개를 숙여도 허리 위치는 그대로: 카메라의 좌우 방향(Yaw)만 따른다
	const FRotator Yaw(0.f, ViewRot.Yaw, 0.f);
	const FVector Loc = ViewLoc + Yaw.RotateVector(FVector(HolsterOffset.X, HolsterOffset.Y, 0.f)) + FVector(0.f, 0.f, HolsterOffset.Z);
	SetActorLocationAndRotation(Loc, Yaw + FRotator(0.f, 0.f, 70.f), false, nullptr, ETeleportType::TeleportPhysics);
}
