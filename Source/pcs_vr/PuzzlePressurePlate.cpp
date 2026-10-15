#include "PuzzlePressurePlate.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "PerspectiveCube.h"
#include "PuzzleUtils.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogPressurePlate, Log, All);

namespace
{
	constexpr float FrameHeight = 10.f;
	constexpr float PlateThickness = 6.f;
	constexpr float PressedDepth = 5.f;   // 무거운 물체: 판이 이만큼 내려가서 그대로
	constexpr float LightDepth = 1.5f;    // 가벼운 물체: 살짝만
	const FLinearColor IdleColor(0.85f, 0.5f, 0.05f);
	const FLinearColor PressedColor(0.1f, 0.9f, 0.25f);
}

APuzzlePressurePlate::APuzzlePressurePlate()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Frame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Frame"));
	Frame->SetupAttachment(Root);
	Frame->SetStaticMesh(CubeMesh.Object);

	Plate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Plate"));
	Plate->SetupAttachment(Root);
	Plate->SetStaticMesh(CubeMesh.Object);
	Plate->SetMobility(EComponentMobility::Movable);

	// 판 위 60cm 공간에 들어온 물체를 본다
	Sensor = CreateDefaultSubobject<UBoxComponent>(TEXT("Sensor"));
	Sensor->SetupAttachment(Root);
	Sensor->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Sensor->SetGenerateOverlapEvents(true);
	Sensor->SetCanEverAffectNavigation(false);
}

void APuzzlePressurePlate::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// 테두리: 판보다 10cm 넓고 낮은 틀. 판: 틀 위로 PlateThickness만큼 올라옴
	Frame->SetRelativeLocation(FVector(0.f, 0.f, FrameHeight * 0.5f));
	Frame->SetRelativeScale3D(FVector(Size.X + 20.f, Size.Y + 20.f, FrameHeight) / 100.f);
	Plate->SetRelativeLocation(FVector(0.f, 0.f, FrameHeight + PlateThickness * 0.5f - Depth));
	Plate->SetRelativeScale3D(FVector(Size.X, Size.Y, PlateThickness) / 100.f);
	Sensor->SetRelativeLocation(FVector(0.f, 0.f, FrameHeight + 40.f));
	Sensor->SetBoxExtent(FVector(Size.X * 0.5f, Size.Y * 0.5f, 40.f));
}

void APuzzlePressurePlate::BeginPlay()
{
	Super::BeginPlay();
	PuzzleUtils::ApplySolidColor(Frame, FLinearColor(0.12f, 0.12f, 0.13f));
	PlateMID = PuzzleUtils::ApplySolidColor(Plate, IdleColor, 0.6f);
}

void APuzzlePressurePlate::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	float TargetDepth = bPressed ? PressedDepth : 0.f;
	if (!bPressed)
	{
		TArray<AActor*> Overlaps;
		Sensor->GetOverlappingActors(Overlaps);
		for (AActor* A : Overlaps)
		{
			const APerspectiveCube* Cube = Cast<APerspectiveCube>(A);
			if (Cube && !Cube->IsHeld() && Cube->IsHeavy())
			{
				bPressed = true;
				TargetDepth = PressedDepth;
				PuzzleUtils::SetSolidColor(PlateMID, PressedColor, 1.5f);
				UE_LOG(LogPressurePlate, Log, TEXT("%s: %s (배율 %.2f)로 눌림"), *GetName(), *Cube->GetName(), Cube->GetScaleRatio());
				OnPressed.Broadcast();
				break;
			}
			if (Cast<APuzzleGrabbable>(A))
			{
				// 가벼운 물체: 살짝 눌렸다가 0.6초 뒤 다시 올라옴
				LightTouchTime = GetWorld()->GetTimeSeconds();
			}
		}
		if (!bPressed && LightTouchTime >= 0.f && GetWorld()->GetTimeSeconds() - LightTouchTime < 0.6f)
		{
			TargetDepth = LightDepth;
		}
	}

	Depth = FMath::FInterpTo(Depth, TargetDepth, DeltaTime, 8.f);
	Plate->SetRelativeLocation(FVector(0.f, 0.f, FrameHeight + PlateThickness * 0.5f - Depth));
}
