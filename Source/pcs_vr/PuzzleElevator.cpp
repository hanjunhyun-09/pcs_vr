#include "PuzzleElevator.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "PuzzleTextWidget.h"
#include "PuzzleUtils.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogElevator, Log, All);

namespace
{
	constexpr float DoorHalfWidth = 60.f;   // 문 한 짝 폭 (전체 120cm)
	constexpr float DoorHeight = 220.f;
	constexpr float DoorX = 112.f;          // 앞벽 바깥쪽
	constexpr float DoorMoveTime = 1.2f;
}

APuzzleElevator::APuzzleElevator()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// 내부 200 x 200 x 260cm, 벽 두께 10cm
	AddBox(TEXT("Floor"), FVector(0.f, 0.f, -5.f), FVector(220.f, 220.f, 10.f), CubeMesh.Object);
	AddBox(TEXT("Ceiling"), FVector(0.f, 0.f, 265.f), FVector(220.f, 220.f, 10.f), CubeMesh.Object);
	AddBox(TEXT("WallBack"), FVector(-105.f, 0.f, 130.f), FVector(10.f, 220.f, 260.f), CubeMesh.Object);
	AddBox(TEXT("WallLeft"), FVector(0.f, -105.f, 130.f), FVector(200.f, 10.f, 260.f), CubeMesh.Object);
	AddBox(TEXT("WallRight"), FVector(0.f, 105.f, 130.f), FVector(200.f, 10.f, 260.f), CubeMesh.Object);
	AddBox(TEXT("FrontLeft"), FVector(105.f, -85.f, 130.f), FVector(10.f, 50.f, 260.f), CubeMesh.Object);
	AddBox(TEXT("FrontRight"), FVector(105.f, 85.f, 130.f), FVector(10.f, 50.f, 260.f), CubeMesh.Object);
	AddBox(TEXT("FrontTop"), FVector(105.f, 0.f, 240.f), FVector(10.f, 120.f, 40.f), CubeMesh.Object);

	DoorLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorLeft"));
	DoorLeft->SetupAttachment(Root);
	DoorLeft->SetStaticMesh(CubeMesh.Object);
	DoorLeft->SetMobility(EComponentMobility::Movable);
	DoorLeft->SetRelativeScale3D(FVector(4.f, DoorHalfWidth, DoorHeight) / 100.f);

	DoorRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorRight"));
	DoorRight->SetupAttachment(Root);
	DoorRight->SetStaticMesh(CubeMesh.Object);
	DoorRight->SetMobility(EComponentMobility::Movable);
	DoorRight->SetRelativeScale3D(FVector(4.f, DoorHalfWidth, DoorHeight) / 100.f);

	// 호출 버튼: 문 오른쪽 벽, 높이 120cm
	CallButton = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CallButton"));
	CallButton->SetupAttachment(Root);
	CallButton->SetStaticMesh(CubeMesh.Object);
	CallButton->SetRelativeLocation(FVector(111.f, 85.f, 120.f));
	CallButton->SetRelativeScale3D(FVector(0.02f, 0.08f, 0.12f));

	CallInteract = CreateDefaultSubobject<UPuzzleInteractComponent>(TEXT("CallInteract"));
	CallInteract->SetupAttachment(Root);
	CallInteract->SetRelativeLocation(FVector(114.f, 85.f, 120.f));
	CallInteract->InteractRadius = 16.f;

	// 문 위 안내판 (60cm x 12cm)
	Display = CreateDefaultSubobject<UWidgetComponent>(TEXT("Display"));
	Display->SetupAttachment(Root);
	Display->SetRelativeLocation(FVector(111.f, 0.f, 240.f));
	Display->SetRelativeScale3D(FVector(0.1f));
	Display->SetWidgetSpace(EWidgetSpace::World);
	Display->SetWidgetClass(UPuzzleTextWidget::StaticClass());
	Display->SetDrawSize(FVector2D(600.f, 120.f));
	Display->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Display->SetCanEverAffectNavigation(false);

	CabinLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("CabinLight"));
	CabinLight->SetupAttachment(Root);
	CabinLight->SetRelativeLocation(FVector(0.f, 0.f, 240.f));
	CabinLight->SetIntensityUnits(ELightUnits::Candelas);
	CabinLight->SetIntensity(20.f);
	CabinLight->SetAttenuationRadius(400.f);
	CabinLight->SetLightColor(FLinearColor(1.f, 0.95f, 0.85f));
}

UStaticMeshComponent* APuzzleElevator::AddBox(const TCHAR* Name, const FVector& Center, const FVector& SizeCm, UStaticMesh* Mesh)
{
	UStaticMeshComponent* Box = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Box->SetupAttachment(Root);
	Box->SetStaticMesh(Mesh);
	Box->SetRelativeLocation(Center);
	Box->SetRelativeScale3D(SizeCm / 100.f);
	Shell.Add(Box);
	return Box;
}

void APuzzleElevator::BeginPlay()
{
	Super::BeginPlay();

	for (UStaticMeshComponent* Box : Shell)
	{
		PuzzleUtils::ApplySolidColor(Box, FLinearColor(0.42f, 0.44f, 0.46f));
	}
	PuzzleUtils::ApplySolidColor(DoorLeft, FLinearColor(0.6f, 0.62f, 0.64f));
	PuzzleUtils::ApplySolidColor(DoorRight, FLinearColor(0.6f, 0.62f, 0.64f));
	PuzzleUtils::ApplySolidColor(CallButton, FLinearColor(0.9f, 0.85f, 0.7f), 0.8f);

	CallInteract->OnInteract.AddDynamic(this, &APuzzleElevator::HandleCall);
	Display->InitWidget();
	SetDisplay(TEXT("▼  대기 중…"), FLinearColor(1.f, 0.6f, 0.1f));
	DoorAlpha = 0.f;

	if (bArrivalOnly)
	{
		CallInteract->bInteractEnabled = false;
		GetWorldTimerManager().SetTimer(LevelHandle, this, &APuzzleElevator::Arrive, FMath::Max(ArrivalDelay, 0.01f), false);
	}
}

void APuzzleElevator::SetDisplay(const FString& Message, const FLinearColor& Color)
{
	if (UPuzzleTextWidget* W = Cast<UPuzzleTextWidget>(Display->GetUserWidgetObject()))
	{
		W->SetContent(FText::FromString(Message), 44, Color, FLinearColor(0.f, 0.f, 0.f, 1.f));
	}
}

void APuzzleElevator::HandleCall(AActor* InteractInstigator)
{
	if (State == EState::Waiting)
	{
		// 눌러도 아직 안 옴 (큐브를 키우거나 시간이 지나면 도착)
		SetDisplay(TEXT("호출됨 · 대기 중…"), FLinearColor(1.f, 0.6f, 0.1f));
	}
}

void APuzzleElevator::Arrive()
{
	if (State != EState::Waiting)
	{
		return;
	}
	UE_LOG(LogElevator, Log, TEXT("엘리베이터 도착"));
	State = EState::Opening;
	if (bOutOfService)
	{
		SetDisplay(TEXT("도착 · 운행 준비 중"), FLinearColor(1.f, 0.6f, 0.1f));
	}
	else
	{
		SetDisplay(TEXT("도착"), FLinearColor(0.3f, 1.f, 0.4f));
	}
	OnArrived.Broadcast();
}

bool APuzzleElevator::IsPlayerInside() const
{
	FVector ViewLoc;
	FRotator ViewRot;
	if (!PuzzleUtils::GetPlayerView(this, ViewLoc, ViewRot))
	{
		return false;
	}
	const FVector Local = GetActorTransform().InverseTransformPosition(ViewLoc);
	return Local.X > -100.f && Local.X < 85.f && FMath::Abs(Local.Y) < 100.f && Local.Z > 0.f && Local.Z < 260.f;
}

void APuzzleElevator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	switch (State)
	{
	case EState::Opening:
		DoorAlpha = FMath::Min(1.f, DoorAlpha + DeltaTime / DoorMoveTime);
		if (DoorAlpha >= 1.f)
		{
			State = EState::Open;
		}
		break;

	case EState::Open:
		if (bArrivalOnly || bOutOfService)
		{
			break;
		}
		InsideTime = IsPlayerInside() ? InsideTime + DeltaTime : 0.f;
		if (InsideTime >= DepartDelay)
		{
			State = EState::Closing;
			SetDisplay(TEXT("▲  출발"), FLinearColor(0.3f, 1.f, 0.4f));
		}
		break;

	case EState::Closing:
		DoorAlpha = FMath::Max(0.f, DoorAlpha - DeltaTime / DoorMoveTime);
		if (DoorAlpha <= 0.f)
		{
			State = EState::Leaving;
			if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
			{
				if (PC->PlayerCameraManager)
				{
					PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, 0.6f, FLinearColor::Black, true, true);
				}
			}
			GetWorldTimerManager().SetTimer(LevelHandle, this, &APuzzleElevator::OpenNextLevel, 1.5f, false);
		}
		break;

	default:
		break;
	}

	const float Eased = FMath::InterpEaseInOut(0.f, 1.f, DoorAlpha, 2.f);
	const float Offset = DoorHalfWidth * 0.5f + DoorHalfWidth * Eased;
	DoorLeft->SetRelativeLocation(FVector(DoorX, -Offset, DoorHeight * 0.5f));
	DoorRight->SetRelativeLocation(FVector(DoorX, Offset, DoorHeight * 0.5f));
}

void APuzzleElevator::OpenNextLevel()
{
	UE_LOG(LogElevator, Log, TEXT("다음 레벨로: %s"), *NextLevel.ToString());
	UGameplayStatics::OpenLevel(this, NextLevel);
}
