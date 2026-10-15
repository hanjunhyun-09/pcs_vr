#include "PuzzleDispenser.h"

#include "Components/ArrowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogDispenser, Log, All);

APuzzleDispenser::APuzzleDispenser()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);

	SpawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("SpawnPoint"));
	SpawnPoint->SetupAttachment(Root);
	SpawnPoint->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
}

void APuzzleDispenser::BeginPlay()
{
	Super::BeginPlay();
	if (bSpawnOnBeginPlay)
	{
		Dispense();
	}
}

AActor* APuzzleDispenser::Dispense()
{
	UWorld* World = GetWorld();
	if (!World || !SpawnClass)
	{
		UE_LOG(LogDispenser, Warning, TEXT("%s: SpawnClass가 비어 있습니다."), *GetName());
		return nullptr;
	}

	if (IsValid(CurrentItem))
	{
		CurrentItem->Destroy();
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	CurrentItem = World->SpawnActor<AActor>(SpawnClass, SpawnPoint->GetComponentTransform(), Params);
	return CurrentItem;
}

void APuzzleDispenser::Interact_Implementation(AActor* Interactor)
{
	Dispense();
}
