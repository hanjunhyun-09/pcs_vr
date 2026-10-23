#include "PuzzleBlueprintSlot.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "PerspectiveCube.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlueprintSlot, Log, All);

APuzzleBlueprintSlot::APuzzleBlueprintSlot()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Sensor = CreateDefaultSubobject<UBoxComponent>(TEXT("Sensor"));
	Sensor->SetupAttachment(Root);
	Sensor->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Sensor->SetGenerateOverlapEvents(true);
	Sensor->SetCanEverAffectNavigation(false);
}

void APuzzleBlueprintSlot::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// 홈 안쪽(벽 속 10cm)부터 방 쪽 SensorDepth까지
	Sensor->SetRelativeLocation(FVector((SensorDepth - 10.f) * 0.5f, 0.f, 0.f));
	Sensor->SetBoxExtent(FVector((SensorDepth + 10.f) * 0.5f, SlotSize * 0.5f, SlotSize * 0.5f));
}

void APuzzleBlueprintSlot::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bFilled)
	{
		return;
	}

	TArray<AActor*> Overlaps;
	Sensor->GetOverlappingActors(Overlaps, APerspectiveCube::StaticClass());
	for (AActor* A : Overlaps)
	{
		APerspectiveCube* Piece = Cast<APerspectiveCube>(A);
		if (!Piece || Piece->IsHeld() || (!RequiredTag.IsNone() && !Piece->ActorHasTag(RequiredTag)))
		{
			continue;
		}
		const float Fit = Piece->GetSizeCm() / FMath::Max(SlotSize, 1.f);
		if (Fit >= MinFit && Fit <= MaxFit)
		{
			SnapPiece(Piece);
			return;
		}
		const double Now = GetWorld()->GetTimeSeconds();
		if (Now - LastRejectLogTime > 2.0)
		{
			LastRejectLogTime = Now;
			UE_LOG(LogBlueprintSlot, Log, TEXT("%s: %s 크기가 홈의 %.0f%% (필요 %.0f%% ~ %.0f%%)"),
				*GetName(), *Piece->GetName(), Fit * 100.f, MinFit * 100.f, MaxFit * 100.f);
		}
	}
}

void APuzzleBlueprintSlot::SnapPiece(APerspectiveCube* Piece)
{
	bFilled = true;
	UStaticMeshComponent* Mesh = Piece->GetMesh();
	const UStaticMesh* SM = Mesh ? Mesh->GetStaticMesh() : nullptr;
	if (!Mesh || !SM)
	{
		return;
	}

	// 판의 가장 얇은 축이 벽에 수직(+X)이 되게 돌리고, 가장 긴 변이 SlotSize가 되게 크기 보정
	const FVector Extent = SM->GetBounds().BoxExtent * Piece->GetActorScale3D();
	const float Scale = SlotSize / FMath::Max(Extent.GetMax() * 2.f, 1.f);
	FVector NewScale = Piece->GetActorScale3D() * Scale;

	const FVector SlotX = GetActorForwardVector();
	const FVector SlotZ = GetActorUpVector();
	FRotator Rot;
	float HalfThick;
	if (Extent.Z <= Extent.X && Extent.Z <= Extent.Y)
	{
		Rot = FRotationMatrix::MakeFromZX(SlotX, SlotZ).Rotator();   // 로컬 Z(얇은 축) → 벽 수직
		HalfThick = Extent.Z * Scale;
	}
	else if (Extent.X <= Extent.Y)
	{
		Rot = FRotationMatrix::MakeFromXZ(SlotX, SlotZ).Rotator();   // 로컬 X(얇은 축) → 벽 수직
		HalfThick = Extent.X * Scale;
	}
	else
	{
		Rot = FRotationMatrix::MakeFromYZ(SlotX, SlotZ).Rotator();   // 로컬 Y(얇은 축) → 벽 수직
		HalfThick = Extent.Y * Scale;
	}

	Mesh->SetSimulatePhysics(false);
	Piece->bRayGrabEnabled = false;
	Piece->SetActorScale3D(NewScale);
	Piece->SetActorLocationAndRotation(GetActorLocation() + SlotX * HalfThick, Rot, false, nullptr, ETeleportType::TeleportPhysics);
	Piece->SetActorEnableCollision(false);   // 다시 잡히지 않게
	Piece->SetAimHighlight(true);
	UE_LOG(LogBlueprintSlot, Log, TEXT("%s: %s 끼움 (크기 보정 x%.2f)"), *GetName(), *Piece->GetName(), Scale);
	OnFilled.Broadcast();
}
