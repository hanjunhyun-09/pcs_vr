#include "PuzzleLever.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "PuzzleUtils.h"
#include "UObject/ConstructorHelpers.h"

APuzzleLever::APuzzleLever()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// 배전함: 깊이 15cm, 폭 35cm, 높이 50cm (뒷면이 벽에 닿음)
	Box = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Box"));
	Box->SetupAttachment(Root);
	Box->SetStaticMesh(CubeMesh.Object);
	Box->SetRelativeLocation(FVector(7.5f, 0.f, 0.f));
	Box->SetRelativeScale3D(FVector(0.15f, 0.35f, 0.5f));

	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	Pivot->SetupAttachment(Root);
	Pivot->SetRelativeLocation(FVector(17.f, 0.f, 0.f));
	Pivot->SetRelativeRotation(FRotator(UpPitch, 0.f, 0.f));

	Handle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Handle"));
	Handle->SetupAttachment(Pivot);
	Handle->SetStaticMesh(CylinderMesh.Object);
	Handle->SetRelativeLocation(FVector(0.f, 0.f, 14.f));
	Handle->SetRelativeScale3D(FVector(0.04f, 0.04f, 0.28f));
	Handle->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Knob = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Knob"));
	Knob->SetupAttachment(Pivot);
	Knob->SetStaticMesh(SphereMesh.Object);
	Knob->SetRelativeLocation(FVector(0.f, 0.f, 29.f));
	Knob->SetRelativeScale3D(FVector(0.08f));
	Knob->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Indicator = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Indicator"));
	Indicator->SetupAttachment(Root);
	Indicator->SetStaticMesh(SphereMesh.Object);
	Indicator->SetRelativeLocation(FVector(15.f, 12.f, 18.f));
	Indicator->SetRelativeScale3D(FVector(0.03f));
	Indicator->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	IndicatorLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("IndicatorLight"));
	IndicatorLight->SetupAttachment(Indicator);
	IndicatorLight->SetRelativeLocation(FVector(60.f, 0.f, 0.f));
	IndicatorLight->SetIntensityUnits(ELightUnits::Candelas);
	IndicatorLight->SetIntensity(0.6f);
	IndicatorLight->SetAttenuationRadius(80.f);
	IndicatorLight->SetLightColor(FLinearColor(0.2f, 1.f, 0.3f));
	IndicatorLight->SetCastShadows(false);

	Interact = CreateDefaultSubobject<UPuzzleInteractComponent>(TEXT("Interact"));
	Interact->SetupAttachment(Knob);
	Interact->InteractRadius = 25.f;
}

void APuzzleLever::BeginPlay()
{
	Super::BeginPlay();

	PuzzleUtils::ApplySolidColor(Box, FLinearColor(0.18f, 0.2f, 0.22f));
	PuzzleUtils::ApplySolidColor(Handle, FLinearColor(0.6f, 0.6f, 0.62f));
	PuzzleUtils::ApplySolidColor(Knob, FLinearColor(0.7f, 0.08f, 0.05f));
	IndicatorMID = PuzzleUtils::ApplySolidColor(Indicator, FLinearColor(0.2f, 1.f, 0.3f), 8.f);

	Interact->OnInteract.AddDynamic(this, &APuzzleLever::HandleInteract);
}

void APuzzleLever::HandleInteract(AActor* InteractInstigator)
{
	Pull();
}

void APuzzleLever::Pull()
{
	if (bPulled)
	{
		return;
	}
	bPulled = true;
	bAnimating = true;
	AnimAlpha = 0.f;
	Interact->bInteractEnabled = false;
	SetActorTickEnabled(true);
}

void APuzzleLever::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bAnimating)
	{
		SetActorTickEnabled(false);
		return;
	}

	AnimAlpha = FMath::Min(1.f, AnimAlpha + DeltaTime / PullTime);
	const float Eased = FMath::InterpEaseIn(0.f, 1.f, AnimAlpha, 2.f);
	Pivot->SetRelativeRotation(FRotator(FMath::Lerp(UpPitch, DownPitch, Eased), 0.f, 0.f));

	if (AnimAlpha >= 1.f)
	{
		bAnimating = false;
		// 내려간 뒤에는 표시등을 끈다 (전원이 방 전체로 넘어감)
		PuzzleUtils::SetSolidColor(IndicatorMID, FLinearColor(0.05f, 0.2f, 0.07f), 0.f);
		IndicatorLight->SetVisibility(false);
		OnActivated.Broadcast();
	}
}
