#include "PuzzleCoreSocket.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PuzzleGrabbable.h"
#include "PuzzleUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogCoreSocket, Log, All);

APuzzleCoreSocket::APuzzleCoreSocket()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Sensor = CreateDefaultSubobject<USphereComponent>(TEXT("Sensor"));
	Sensor->SetupAttachment(Root);
	Sensor->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Sensor->SetGenerateOverlapEvents(true);
	Sensor->SetCanEverAffectNavigation(false);
}

void APuzzleCoreSocket::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Sensor->SetSphereRadius(Radius);
	Sensor->SetRelativeLocation(FVector(Radius * 0.5f, 0.f, 0.f));   // 슬롯 면 앞쪽으로
}

void APuzzleCoreSocket::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bFilled)
	{
		return;
	}

	TArray<AActor*> Overlaps;
	Sensor->GetOverlappingActors(Overlaps, APuzzleGrabbable::StaticClass());
	for (AActor* A : Overlaps)
	{
		APuzzleGrabbable* Part = Cast<APuzzleGrabbable>(A);
		if (Part && !Part->IsHeld() && !Part->IsHidden() && (RequiredTag.IsNone() || Part->ActorHasTag(RequiredTag)))
		{
			SnapPart(Part);
			return;
		}
	}
}

void APuzzleCoreSocket::SnapPart(APuzzleGrabbable* Part)
{
	bFilled = true;
	if (UStaticMeshComponent* Mesh = Part->GetMesh())
	{
		Mesh->SetSimulatePhysics(false);
	}
	Part->bHolsterToPlayer = false;
	Part->SetActorLocationAndRotation(GetActorLocation() + GetActorForwardVector() * SnapOffset, GetActorRotation(), false, nullptr,
		ETeleportType::TeleportPhysics);
	Part->SetActorEnableCollision(false);   // 다시 잡히지 않게
	Part->Emissive = FMath::Max(Part->Emissive, 2.f);
	PuzzleUtils::ApplySolidColor(Part->GetMesh(), Part->Color, Part->Emissive);
	UE_LOG(LogCoreSocket, Log, TEXT("%s: %s 끼움"), *GetName(), *Part->GetName());
	OnFilled.Broadcast();
}
