#include "PerspectiveCube.h"

#include "CollisionQueryParams.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "PuzzleUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogPerspective, Log, All);

namespace
{
	const TCHAR* PerspectiveGridPath = TEXT("/Game/VRPuzzle/Materials/M_VRP_PerspectiveGrid.M_VRP_PerspectiveGrid");

	/** 파란 격자 무늬를 Color/Emissive로 칠한다 (머티리얼이 없으면 nullptr → 부모가 칠한 단색 그대로) */
	UMaterialInstanceDynamic* ApplyPerspectiveGrid(UStaticMeshComponent* Mesh, const FLinearColor& Color, float Emissive)
	{
		UMaterialInterface* Grid = LoadObject<UMaterialInterface>(nullptr, PerspectiveGridPath);
		if (!Grid)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* GridMID = UMaterialInstanceDynamic::Create(Grid, Mesh);
		PuzzleUtils::SetSolidColor(GridMID, Color, Emissive);
		for (int32 i = 0; i < FMath::Max(1, Mesh->GetNumMaterials()); ++i)
		{
			Mesh->SetMaterial(i, GridMID);
		}
		return GridMID;
	}
}

APerspectiveCube::APerspectiveCube()
{
	Color = FLinearColor(0.1f, 0.35f, 0.9f);
	Emissive = 0.3f;
}

void APerspectiveCube::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// 에디터에서도 원근 물체가 격자 무늬 + 지정한 색으로 보이게
	if (!GetWorld() || !GetWorld()->IsGameWorld())
	{
		ApplyPerspectiveGrid(Mesh, Color, Emissive);
	}
}

void APerspectiveCube::BeginPlay()
{
	Super::BeginPlay();
	// InitialScale = 기준 크기(배율 1.0)의 스케일
	InitialScale = GetActorScale3D() / FMath::Max(StartRatio, 0.05f);

	if (UMaterialInstanceDynamic* GridMID = ApplyPerspectiveGrid(Mesh, Color, Emissive))
	{
		MID = GridMID;
	}
	if (bFrozenUntilGrabbed)
	{
		bFrozen = true;
		Mesh->SetSimulatePhysics(false);
	}
	UpdateNavigation();
}

void APerspectiveCube::NotifyRayGrabRefused()
{
	UE_LOG(LogPerspective, Log, TEXT("%s: 아직 레이로 잡을 수 없음"), *GetName());
	OnGrabRefused.Broadcast();
}

float APerspectiveCube::GetScaleRatio() const
{
	return InitialScale.X > KINDA_SMALL_NUMBER ? GetActorScale3D().X / InitialScale.X : 1.f;
}

float APerspectiveCube::GetSizeCm() const
{
	const UStaticMesh* SM = Mesh->GetStaticMesh();
	return SM ? (SM->GetBounds().BoxExtent * GetActorScale3D()).GetMax() * 2.f : 0.f;
}

void APerspectiveCube::UpdateNavigation()
{
	Mesh->SetCanEverAffectNavigation(!IsHeld() && GetSizeCm() >= 100.f);
}

void APerspectiveCube::OnGrabbed()
{
	Mesh->SetCanEverAffectNavigation(false);
	if (!bRayHeld && GetSizeCm() > MaxHandGrabSize)
	{
		// 너무 커서 손으로는 못 든다 (레이로 멀리서 잡아야 함)
		UE_LOG(LogPerspective, Log, TEXT("%s: %.0fcm라 손으로 못 잡음 (최대 %.0fcm)"), *GetName(), GetSizeCm(), MaxHandGrabSize);
		bSkipNextProjection = true;
		PuzzleUtils::ReleaseGrabComponent(GetGrabComponent());
		OnGrabRefused.Broadcast();
	}
}

bool APerspectiveCube::IsHeld() const
{
	return bRayHeld || Super::IsHeld();
}

void APerspectiveCube::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bRayHeld && bPulling)
	{
		UpdateRayPull(DeltaTime);
	}
}

void APerspectiveCube::SetAimHighlight(bool bOn)
{
	if (bAimHighlight == bOn)
	{
		return;
	}
	bAimHighlight = bOn;
	SetGlow(bOn ? Emissive * 3.f + 1.f : Emissive);
}

void APerspectiveCube::BeginRayGrab(USceneComponent* Hand, FVector HoldOffset)
{
	FVector ViewLoc;
	FRotator ViewRot;
	if (!Hand || IsHeld() || !PuzzleUtils::GetPlayerView(this, ViewLoc, ViewRot))
	{
		return;
	}
	SetAimHighlight(false);
	RayHand = Hand;
	RayHoldOffset = HoldOffset;
	bRayHeld = true;
	bPulling = true;
	PullElapsed = 0.f;
	PullStartLoc = GetActorLocation();
	PullStartScale = GetActorScale3D();
	PullStartEyeDist = FMath::Max(FVector::Dist(PullStartLoc, ViewLoc), 1.f);
	Mesh->SetSimulatePhysics(false);
	Mesh->SetCanEverAffectNavigation(false);
	bFrozen = false;
	UE_LOG(LogPerspective, Log, TEXT("%s: 레이로 잡음 (눈에서 %.0fcm)"), *GetName(), PullStartEyeDist);
}

void APerspectiveCube::UpdateRayPull(float DeltaTime)
{
	USceneComponent* Hand = RayHand.Get();
	FVector ViewLoc;
	FRotator ViewRot;
	if (!Hand || !PuzzleUtils::GetPlayerView(this, ViewLoc, ViewRot))
	{
		EndRayGrab();
		return;
	}

	PullElapsed += DeltaTime;
	const float Alpha = FMath::SmoothStep(0.f, 1.f, FMath::Clamp(PullElapsed / RayPullTime, 0.f, 1.f));
	const FVector Target = Hand->GetComponentTransform().TransformPositionNoScale(RayHoldOffset);
	const FVector NewLoc = FMath::Lerp(PullStartLoc, Target, Alpha);

	// 눈에서 보이는 크기 유지: 크기 ∝ 눈까지 거리 (처음 크기 대비 MinScale~MaxScale)
	const float Factor = FVector::Dist(NewLoc, ViewLoc) / PullStartEyeDist;
	const float PullStartRatio = InitialScale.X > KINDA_SMALL_NUMBER ? PullStartScale.X / InitialScale.X : 1.f;
	const float Ratio = FMath::Clamp(PullStartRatio * Factor, MinScale, MaxScale);
	SetActorScale3D(InitialScale * Ratio);
	SetActorLocation(NewLoc, false, nullptr, ETeleportType::TeleportPhysics);

	if (Alpha >= 1.f)
	{
		bPulling = false;
		AttachToComponent(Hand, FAttachmentTransformRules::KeepWorldTransform);
		UE_LOG(LogPerspective, Log, TEXT("%s: 손에 도착, 배율 %.2f"), *GetName(), GetScaleRatio());
	}
}

void APerspectiveCube::EndRayGrab()
{
	if (!bRayHeld)
	{
		return;
	}
	bRayHeld = false;
	bPulling = false;
	RayHand.Reset();
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	// 다음 Tick에서 놓음으로 감지되어 OnReleased → ProjectFromView (물리도 다시 켜짐)
}

void APerspectiveCube::OnReleased()
{
	if (bSkipNextProjection)
	{
		bSkipNextProjection = false;
		Mesh->SetSimulatePhysics(!bFrozen);
	}
	else
	{
		ProjectFromView();
	}
	UpdateNavigation();
}

void APerspectiveCube::ProjectFromView()
{
	UWorld* World = GetWorld();
	FVector ViewLoc;
	FRotator ViewRot;
	if (!World || !Mesh->GetStaticMesh() || !PuzzleUtils::GetPlayerView(this, ViewLoc, ViewRot))
	{
		return;
	}

	const FVector CubeLoc = GetActorLocation();
	const FVector Dir = (CubeLoc - ViewLoc).GetSafeNormal();
	const float D0 = FVector::Dist(CubeLoc, ViewLoc);
	if (D0 < 1.f)
	{
		return;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(PerspectiveProject), false, this);
	if (APawn* Pawn = PuzzleUtils::GetPlayerPawn(this))
	{
		Params.AddIgnoredActor(Pawn);
	}

	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(Hit, ViewLoc, ViewLoc + Dir * MaxProjectDistance, ECC_Visibility, Params);
	const float SurfaceDist = bHit ? Hit.Distance : MaxProjectDistance;

	// 눈에서 보이는 크기 유지: 크기 ∝ 거리. 물체 앞면이 표면에 닿는 거리 X = 표면거리 / (1 + 반지름/거리)
	const FVector LocalExtent = Mesh->GetStaticMesh()->GetBounds().BoxExtent;
	const FVector CurScale = GetActorScale3D();
	const float HalfSize0 = (LocalExtent * CurScale).GetMax();
	const float K = HalfSize0 / D0;
	float NewDist = SurfaceDist / (1.f + K) - 1.f;

	// 처음 크기 대비 배율 제한
	const float CurRatio = GetScaleRatio();
	float Factor = NewDist / D0;
	const float ClampedRatio = FMath::Clamp(CurRatio * Factor, MinScale, MaxScale);
	Factor = ClampedRatio / FMath::Max(CurRatio, KINDA_SMALL_NUMBER);
	NewDist = D0 * Factor;

	FVector NewScale = CurScale * Factor;
	FVector NewLoc = ViewLoc + Dir * NewDist;

	// 바닥/벽이나 다른 물체(거대 상자 등)에 박히면 눈 쪽으로 조금씩 당긴다 (크기도 같이 줄여서 보이는 크기 유지)
	// 물체 자신의 충돌 설정으로 "막히는" 것만 본다 (압력판 감지 영역 같은 겹침 전용은 무시)
	const FQuat Rot = GetActorQuat();
	for (int32 i = 0; i < 30; ++i)
	{
		const FCollisionShape Box = FCollisionShape::MakeBox(LocalExtent * NewScale * 0.98f);
		if (!World->OverlapBlockingTestByProfile(NewLoc, Rot, Mesh->GetCollisionProfileName(), Box, Params))
		{
			break;
		}
		const float Shrink = 0.96f;
		if (CurRatio * Factor * Shrink < MinScale)
		{
			break;
		}
		Factor *= Shrink;
		NewDist = D0 * Factor;
		NewScale = CurScale * Factor;
		NewLoc = ViewLoc + Dir * NewDist;
	}

	Mesh->SetSimulatePhysics(false);
	SetActorScale3D(NewScale);
	SetActorLocation(NewLoc, false, nullptr, ETeleportType::TeleportPhysics);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Mesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

	UE_LOG(LogPerspective, Log, TEXT("%s: 놓은 거리 %.0fcm -> %.0fcm, 배율 %.2f (시선 끝: %s %.0fcm)"), *GetName(), D0, NewDist, GetScaleRatio(),
		bHit && Hit.GetActor() ? *Hit.GetActor()->GetActorLabel() : TEXT("없음"), SurfaceDist);

	if (!bGrownFired && GetScaleRatio() >= GrownThreshold)
	{
		bGrownFired = true;
		OnGrown.Broadcast();
	}
}
