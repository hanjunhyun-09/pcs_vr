#include "PuzzleDoor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "PuzzleUtils.h"
#include "UObject/ConstructorHelpers.h"

APuzzleDoor::APuzzleDoor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Panel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Panel"));
	Panel->SetupAttachment(Root);
	Panel->SetStaticMesh(CubeMesh.Object);
	Panel->SetMobility(EComponentMobility::Movable);
}

void APuzzleDoor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyPanel(bStartOpen ? 1.f : 0.f);
}

void APuzzleDoor::BeginPlay()
{
	Super::BeginPlay();
	PuzzleUtils::ApplySolidColor(Panel, DoorColor);
	bTargetOpen = bStartOpen;
	OpenAmount = bStartOpen ? 1.f : 0.f;
	ApplyPanel(OpenAmount);
}

void APuzzleDoor::ApplyPanel(float Amount)
{
	Panel->SetRelativeScale3D(FVector(Thickness, Width, Height) / 100.f);
	Panel->SetRelativeLocation(FVector(0.f, Width * Amount, Height * 0.5f));
}

void APuzzleDoor::Open()
{
	bTargetOpen = true;
	SetActorTickEnabled(true);
}

void APuzzleDoor::Close()
{
	bTargetOpen = false;
	SetActorTickEnabled(true);
}

void APuzzleDoor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const float Target = bTargetOpen ? 1.f : 0.f;
	OpenAmount = FMath::FInterpConstantTo(OpenAmount, Target, DeltaTime, 1.f / MoveTime);
	ApplyPanel(FMath::InterpEaseInOut(0.f, 1.f, OpenAmount, 2.f));
	if (FMath::IsNearlyEqual(OpenAmount, Target))
	{
		SetActorTickEnabled(false);
	}
}
