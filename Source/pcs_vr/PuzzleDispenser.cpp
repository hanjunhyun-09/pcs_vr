#include "PuzzleDispenser.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "PerspectiveCube.h"
#include "PuzzleInteractComponent.h"
#include "PuzzleUtils.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogDispenser, Log, All);

namespace
{
	constexpr float PedestalHeight = 100.f;
	constexpr float PedestalSize = 60.f;
}

APuzzleDispenser::APuzzleDispenser()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// 받침대 60 x 60 x 100cm
	Pedestal = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pedestal"));
	Pedestal->SetupAttachment(Root);
	Pedestal->SetStaticMesh(CubeMesh.Object);
	Pedestal->SetRelativeLocation(FVector(0.f, 0.f, PedestalHeight * 0.5f));
	Pedestal->SetRelativeScale3D(FVector(PedestalSize, PedestalSize, PedestalHeight) / 100.f);

	// 윗면 받침 (물체가 놓이는 곳 표시)
	Tray = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tray"));
	Tray->SetupAttachment(Root);
	Tray->SetStaticMesh(CubeMesh.Object);
	Tray->SetRelativeLocation(FVector(0.f, 0.f, PedestalHeight + 1.f));
	Tray->SetRelativeScale3D(FVector(PedestalSize - 10.f, PedestalSize - 10.f, 2.f) / 100.f);

	// 앞면 버튼 (지름 10cm, 높이 85cm)
	Button = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Button"));
	Button->SetupAttachment(Root);
	Button->SetStaticMesh(CylinderMesh.Object);
	Button->SetRelativeLocation(FVector(PedestalSize * 0.5f + 2.f, 0.f, 85.f));
	Button->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	Button->SetRelativeScale3D(FVector(0.1f, 0.1f, 0.04f));
	Button->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Interact = CreateDefaultSubobject<UPuzzleInteractComponent>(TEXT("Interact"));
	Interact->SetupAttachment(Root);
	Interact->SetRelativeLocation(FVector(PedestalSize * 0.5f + 5.f, 0.f, 85.f));
	Interact->InteractRadius = 20.f;
}

void APuzzleDispenser::BeginPlay()
{
	Super::BeginPlay();
	PuzzleUtils::ApplySolidColor(Pedestal, FLinearColor(0.3f, 0.32f, 0.34f));
	PuzzleUtils::ApplySolidColor(Tray, SpawnColor, 0.4f);
	ButtonMID = PuzzleUtils::ApplySolidColor(Button, FLinearColor(0.1f, 0.6f, 1.f), 1.5f);
	Interact->OnInteract.AddDynamic(this, &APuzzleDispenser::HandleInteract);
	if (bSpawnOnStart)
	{
		Dispense();
	}
}

void APuzzleDispenser::HandleInteract(AActor* InteractInstigator)
{
	Dispense();
}

APerspectiveCube* APuzzleDispenser::Dispense()
{
	if (!SpawnClass)
	{
		UE_LOG(LogDispenser, Warning, TEXT("%s: SpawnClass가 없습니다"), *GetName());
		return nullptr;
	}

	// 전에 나온 물체 (손에 들고 있으면 그대로 둔다)
	for (APerspectiveCube* Old : { LastSpawned.Get(), ReplaceActor.Get() })
	{
		if (IsValid(Old) && !Old->IsHeld())
		{
			Old->Destroy();
		}
	}
	ReplaceActor = nullptr;

	const FVector Loc = GetActorTransform().TransformPosition(FVector(0.f, 0.f, PedestalHeight + 2.f + SpawnSize * 0.5f));
	const FTransform SpawnTM(GetActorRotation(), Loc, FVector(SpawnSize / 100.f));
	APerspectiveCube* Cube = GetWorld()->SpawnActorDeferred<APerspectiveCube>(SpawnClass, SpawnTM, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Cube)
	{
		return nullptr;
	}
	Cube->Color = SpawnColor;
	Cube->StartRatio = 1.f;
	if (SpawnMaxScale > 0.f)
	{
		Cube->MaxScale = SpawnMaxScale;
	}
	Cube->Tags.Append(SpawnTags);
	Cube->FinishSpawning(SpawnTM);
	LastSpawned = Cube;
	UE_LOG(LogDispenser, Log, TEXT("%s: %s 내보냄 (%.0fcm)"), *GetName(), *Cube->GetName(), SpawnSize);
	return Cube;
}
