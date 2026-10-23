#include "PuzzleButton.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "PuzzleUtils.h"
#include "UObject/ConstructorHelpers.h"

APuzzleButton::APuzzleButton()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// 벽에 붙은 판: 두께 4cm, 14cm x 14cm
	Plate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Plate"));
	Plate->SetupAttachment(Root);
	Plate->SetStaticMesh(CubeMesh.Object);
	Plate->SetRelativeLocation(FVector(2.f, 0.f, 0.f));
	Plate->SetRelativeScale3D(FVector(0.04f, 0.14f, 0.14f));

	// 버튼 머리: 지름 8cm, 길이 5cm, 앞(+X)으로 튀어나옴
	Cap = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cap"));
	Cap->SetupAttachment(Root);
	Cap->SetStaticMesh(CylinderMesh.Object);
	Cap->SetRelativeLocation(FVector(OutX, 0.f, 0.f));
	Cap->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	Cap->SetRelativeScale3D(FVector(0.08f, 0.08f, 0.05f));
	Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Interact = CreateDefaultSubobject<UPuzzleInteractComponent>(TEXT("Interact"));
	Interact->SetupAttachment(Root);
	Interact->SetRelativeLocation(FVector(7.f, 0.f, 0.f));
	Interact->InteractRadius = 20.f;
}

void APuzzleButton::BeginPlay()
{
	Super::BeginPlay();

	PuzzleUtils::ApplySolidColor(Plate, FLinearColor(0.12f, 0.12f, 0.13f));
	CapMID = PuzzleUtils::ApplySolidColor(Cap, CapColor, bHiddenAtStart ? 0.f : 1.5f);
	Interact->OnInteract.AddDynamic(this, &APuzzleButton::HandleInteract);

	if (bHiddenAtStart)
	{
		Cap->SetRelativeLocation(FVector(HiddenX, 0.f, 0.f));
		Interact->bInteractEnabled = false;
	}
}

void APuzzleButton::PopOut()
{
	if (Interact->bInteractEnabled || bPressedOnce)
	{
		return;
	}
	PuzzleUtils::SetSolidColor(CapMID, CapColor, 1.5f);
	Interact->bInteractEnabled = true;
	StartMove(OutX, 0.4f);
}

void APuzzleButton::HandleInteract(AActor* InteractInstigator)
{
	Press();
}

void APuzzleButton::Press()
{
	if (bOneShot && bPressedOnce)
	{
		return;
	}
	bPressedOnce = true;
	if (bOneShot)
	{
		Interact->bInteractEnabled = false;
		PuzzleUtils::SetSolidColor(CapMID, FLinearColor(0.2f, 0.8f, 0.25f), 1.5f);
	}
	bReturnAfterPress = !bOneShot;
	StartMove(PressedX, 0.08f);
	OnPressed.Broadcast();
}

void APuzzleButton::StartMove(float ToX, float Time)
{
	MoveFromX = Cap->GetRelativeLocation().X;
	MoveToX = ToX;
	MoveTime = FMath::Max(Time, 0.01f);
	MoveAlpha = 0.f;
	SetActorTickEnabled(true);
}

void APuzzleButton::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	MoveAlpha = FMath::Min(1.f, MoveAlpha + DeltaTime / MoveTime);
	Cap->SetRelativeLocation(FVector(FMath::Lerp(MoveFromX, MoveToX, MoveAlpha), 0.f, 0.f));

	if (MoveAlpha >= 1.f)
	{
		if (bReturnAfterPress)
		{
			bReturnAfterPress = false;
			StartMove(OutX, 0.15f);
			return;
		}
		SetActorTickEnabled(false);
	}
}
