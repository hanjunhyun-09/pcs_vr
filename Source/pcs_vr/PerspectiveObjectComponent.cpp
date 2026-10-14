#include "PerspectiveObjectComponent.h"

#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogPerspective, Log, All);

namespace
{
	// 상자(반 크기 Extent)가 Dir 방향으로 중심에서 얼마나 튀어나오는지
	float SupportAlong(const FVector& Extent, const FVector& Dir)
	{
		return static_cast<float>(FMath::Abs(Extent.X * Dir.X) + FMath::Abs(Extent.Y * Dir.Y) + FMath::Abs(Extent.Z * Dir.Z));
	}

	// 겹침 검사 이분 탐색 횟수 (8번이면 거리 오차 0.4% 이내)
	constexpr int32 SearchSteps = 8;
}

UPerspectiveObjectComponent::UPerspectiveObjectComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UPerspectiveObjectComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	OriginalScale = Owner->GetActorScale3D();
	Prim = Cast<UPrimitiveComponent>(Owner->GetRootComponent());
	if (!Prim)
	{
		UE_LOG(LogPerspective, Warning, TEXT("%s: 루트 컴포넌트가 Static Mesh 같은 프리미티브가 아니라서 원근법이 동작하지 않습니다."), *Owner->GetName());
		return;
	}

	if (bAutoHookVRGrab)
	{
		// VR 템플릿의 GrabComponent (블루프린트 클래스 GrabComponent_C)를 같은 액터에서 찾는다
		TInlineComponentArray<UActorComponent*> Comps;
		Owner->GetComponents(Comps);
		for (UActorComponent* C : Comps)
		{
			if (C && C->GetClass()->GetName().Contains(TEXT("GrabComponent")))
			{
				VRGrabComp = C;
				break;
			}
		}
		if (VRGrabComp)
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(VRGrabPollHandle, this, &UPerspectiveObjectComponent::CheckVRGrab, 0.02f, true);
			}
		}
	}
}

void UPerspectiveObjectComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bHeld)
	{
		EndHold();
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(VRGrabPollHandle);
	}
	Super::EndPlay(EndPlayReason);
}

bool UPerspectiveObjectComponent::BeginHold(USceneComponent* Eye, USceneComponent* Aim)
{
	AActor* Owner = GetOwner();
	if (bHeld || !Owner || !Prim || !Eye)
	{
		return false;
	}

	EyeComp = Eye;
	AimComp = Aim ? Aim : Eye;

	GrabScale = Owner->GetActorScale3D();
	GrabRotation = Owner->GetActorQuat();
	GrabEyeYaw = Eye->GetComponentRotation().Yaw;
	GrabDistance = FMath::Max(static_cast<float>(FVector::Dist(Eye->GetComponentLocation(), Prim->Bounds.Origin)), MinHoldDistance);

	// 들고 있는 동안은 물리를 끄고 직접 움직인다. 플레이어(폰)와는 부딪히지 않게
	bWasSimulating = Prim->IsSimulatingPhysics();
	Prim->SetSimulatePhysics(false);
	SavedPawnResponse = Prim->GetCollisionResponseToChannel(ECC_Pawn);
	Prim->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	bHeld = true;
	SetComponentTickEnabled(true);
	UE_LOG(LogPerspective, Log, TEXT("%s 들기 시작: 거리 %.0fcm, 배율 %.2f"), *Owner->GetName(), GrabDistance, GetScaleRatio());
	OnHoldStarted.Broadcast();
	return true;
}

void UPerspectiveObjectComponent::EndHold()
{
	if (!bHeld)
	{
		return;
	}
	bHeld = false;
	bHeldByVRGrab = false;
	SetComponentTickEnabled(false);
	EyeComp.Reset();
	AimComp.Reset();

	if (Prim)
	{
		Prim->SetCollisionResponseToChannel(ECC_Pawn, SavedPawnResponse);
		if (bWasSimulating)
		{
			Prim->SetSimulatePhysics(true);
			Prim->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Prim->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		}
	}

	const float Ratio = GetScaleRatio();
	UE_LOG(LogPerspective, Log, TEXT("%s 놓음: 배율 %.2f"), GetOwner() ? *GetOwner()->GetName() : TEXT("?"), Ratio);
	OnReleased.Broadcast(Ratio);
}

float UPerspectiveObjectComponent::GetScaleRatio() const
{
	const AActor* Owner = GetOwner();
	if (!Owner || FMath::IsNearlyZero(OriginalScale.X))
	{
		return 1.f;
	}
	return static_cast<float>(Owner->GetActorScale3D().X / OriginalScale.X);
}

void UPerspectiveObjectComponent::ResetScale()
{
	if (AActor* Owner = GetOwner())
	{
		Owner->SetActorScale3D(OriginalScale);
	}
}

void UPerspectiveObjectComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bHeld)
	{
		UpdateHeld();
	}
}

bool UPerspectiveObjectComponent::IsBlockedAt(const FVector& Center, const FVector& Extent) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(PerspectiveOverlap), false, GetOwner());
	if (const USceneComponent* Eye = EyeComp.Get())
	{
		Params.AddIgnoredActor(Eye->GetOwner());
	}

	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);

	const FVector Shrunk = (Extent - FVector(SkinWidth)).ComponentMax(FVector(0.1f));
	return World->OverlapAnyTestByObjectType(Center, FQuat::Identity, Objects, FCollisionShape::MakeBox(Shrunk), Params);
}

void UPerspectiveObjectComponent::UpdateHeld()
{
	AActor* Owner = GetOwner();
	USceneComponent* Eye = EyeComp.Get();
	USceneComponent* Aim = AimComp.IsValid() ? AimComp.Get() : Eye;
	UWorld* World = GetWorld();
	if (!Owner || !Prim || !Eye || !World)
	{
		EndHold();
		return;
	}

	// 1. 바라보는 곳 찾기 (Aim 방향으로 라인 트레이스)
	const FVector EyeLoc = Eye->GetComponentLocation();
	const FVector AimStart = Aim->GetComponentLocation();
	const FVector AimDir = Aim->GetForwardVector();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(PerspectiveTrace), false, Owner);
	Params.AddIgnoredActor(Eye->GetOwner());

	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(Hit, AimStart, AimStart + AimDir * MaxTraceDistance, ECC_Visibility, Params);
	const FVector Target = bHit ? Hit.ImpactPoint : AimStart + AimDir * MaxTraceDistance;

	// 물체는 항상 "눈 → 바라보는 곳" 직선 위에 둔다 → 보이는 크기가 그대로 유지됨
	const FVector ToTarget = Target - EyeLoc;
	const float HitDist = static_cast<float>(ToTarget.Size());
	if (HitDist < KINDA_SMALL_NUMBER)
	{
		return;
	}
	const FVector Dir = ToTarget / HitDist;

	// 2. 회전: 잡았을 때 눈과 물체의 좌우 방향 관계를 유지
	const float EyeYaw = Eye->GetComponentRotation().Yaw;
	const FQuat Rot = FQuat(FRotator(0.f, EyeYaw - GrabEyeYaw, 0.f)) * GrabRotation;

	// 3. 잡았을 때 크기(k = 1) 기준의 바운드. 위치·크기가 k배가 되면 중심 오프셋과 크기도 k배
	const FBoxSphereBounds UnitBounds = Prim->CalcBounds(FTransform(Rot, FVector::ZeroVector, GrabScale));
	const FVector UnitOffset = UnitBounds.Origin;
	const FVector UnitExtent = UnitBounds.BoxExtent;

	const float G = GrabDistance;
	const float GrabRatio = FMath::IsNearlyZero(OriginalScale.X) ? 1.f : static_cast<float>(GrabScale.X / OriginalScale.X);
	const float MinDist = FMath::Max(G * MinScale / GrabRatio, MinHoldDistance);
	const float MaxDist = FMath::Max(G * MaxScale / GrabRatio, MinDist);

	// 4. 물체 앞면이 벽에 닿는 거리: d + Support × (d / G) = HitDist
	float D = bHit ? HitDist / (1.f + SupportAlong(UnitExtent, Dir) / G) : MaxDist;
	D = FMath::Clamp(D, MinDist, MaxDist);

	// 5. 벽에 묻힘 방지: 비스듬한 바닥·모서리에 겹치면 겹치지 않는 가장 먼 거리를 찾는다
	auto CenterAt = [&](float Dist) { return EyeLoc + Dir * Dist; };
	auto ExtentAt = [&](float Dist) { return UnitExtent * (Dist / G); };
	if (IsBlockedAt(CenterAt(D), ExtentAt(D)))
	{
		float Lo = MinDist;
		float Hi = D;
		for (int32 i = 0; i < SearchSteps; ++i)
		{
			const float Mid = 0.5f * (Lo + Hi);
			if (IsBlockedAt(CenterAt(Mid), ExtentAt(Mid)))
			{
				Hi = Mid;
			}
			else
			{
				Lo = Mid;
			}
		}
		D = Lo;
	}

	// 6. 적용: 새 크기 = 잡았을 때 크기 × (새 거리 ÷ 잡았을 때 거리)
	const float K = D / G;
	const FVector Location = CenterAt(D) - UnitOffset * K;
	Owner->SetActorScale3D(GrabScale * K);
	Owner->SetActorLocationAndRotation(Location, Rot, false, nullptr, ETeleportType::TeleportPhysics);
}

bool UPerspectiveObjectComponent::IsVRGrabHeld() const
{
	if (!VRGrabComp)
	{
		return false;
	}
	const FBoolProperty* HeldProp = FindFProperty<FBoolProperty>(VRGrabComp->GetClass(), TEXT("bIsHeld"));
	return HeldProp && HeldProp->GetPropertyValue_InContainer(VRGrabComp);
}

void UPerspectiveObjectComponent::CheckVRGrab()
{
	const bool bVRHeld = IsVRGrabHeld();

	if (bVRHeld && !bHeld)
	{
		// 템플릿이 물체를 손(모션 컨트롤러)에 붙였다 → 떼어 내고 원근법 모드로
		AActor* Owner = GetOwner();
		USceneComponent* Hand = Owner && Owner->GetRootComponent() ? Owner->GetRootComponent()->GetAttachParent() : nullptr;
		AActor* Pawn = Hand ? Hand->GetOwner() : nullptr;
		UCameraComponent* Cam = Pawn ? Pawn->FindComponentByClass<UCameraComponent>() : nullptr;
		if (!Cam)
		{
			return;
		}

		// 그립 컨트롤러 대신 같은 손의 Aim 컨트롤러가 있으면 그쪽 방향을 쓴다 (손가락이 가리키는 방향)
		USceneComponent* Aim = Hand;
		const FString AimName = Hand->GetName().Replace(TEXT("Grip"), TEXT("Aim"));
		TInlineComponentArray<USceneComponent*> SceneComps;
		Pawn->GetComponents(SceneComps);
		for (USceneComponent* C : SceneComps)
		{
			if (C && C != Hand && C->GetName().Equals(AimName))
			{
				Aim = C;
				break;
			}
		}

		Owner->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		if (BeginHold(Cam, Aim))
		{
			bHeldByVRGrab = true;
		}
	}
	else if (!bVRHeld && bHeld && bHeldByVRGrab)
	{
		EndHold();
	}
}
