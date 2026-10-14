#include "PerspectiveCube.h"

#include "CollisionQueryParams.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "PuzzleUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogPerspective, Log, All);

APerspectiveCube::APerspectiveCube()
{
	Color = FLinearColor(0.1f, 0.35f, 0.9f);
	Emissive = 0.3f;
}

void APerspectiveCube::BeginPlay()
{
	Super::BeginPlay();
	InitialScale = GetActorScale3D();
}

float APerspectiveCube::GetScaleRatio() const
{
	return InitialScale.X > KINDA_SMALL_NUMBER ? GetActorScale3D().X / InitialScale.X : 1.f;
}

void APerspectiveCube::OnReleased()
{
	ProjectFromView();
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

	// 바닥/벽에 비스듬히 박히면 눈 쪽으로 조금씩 당긴다 (크기도 같이 줄여서 보이는 크기 유지)
	const FQuat Rot = GetActorQuat();
	for (int32 i = 0; i < 30; ++i)
	{
		const FCollisionShape Box = FCollisionShape::MakeBox(LocalExtent * NewScale * 0.98f);
		if (!World->OverlapBlockingTestByChannel(NewLoc, Rot, ECC_WorldStatic, Box, Params))
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

	UE_LOG(LogPerspective, Log, TEXT("%s: 놓은 거리 %.0fcm -> %.0fcm, 배율 %.2f"), *GetName(), D0, NewDist, GetScaleRatio());

	if (!bGrownFired && GetScaleRatio() >= GrownThreshold)
	{
		bGrownFired = true;
		OnGrown.Broadcast();
	}
}
