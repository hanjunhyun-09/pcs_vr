#include "PuzzleCardReader.h"

#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "PuzzleGrabbable.h"
#include "PuzzleTextWidget.h"
#include "PuzzleUtils.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogCardReader, Log, All);

namespace
{
	const FLinearColor ReaderRed(1.f, 0.05f, 0.03f);
	const FLinearColor ReaderGreen(0.1f, 1.f, 0.2f);
}

APuzzleCardReader::APuzzleCardReader()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// 본체: 두께 4cm, 폭 10cm, 높이 15cm
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetStaticMesh(CubeMesh.Object);
	Body->SetRelativeLocation(FVector(2.f, 0.f, 0.f));
	Body->SetRelativeScale3D(FVector(0.04f, 0.1f, 0.15f));

	Led = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Led"));
	Led->SetupAttachment(Root);
	Led->SetStaticMesh(SphereMesh.Object);
	Led->SetRelativeLocation(FVector(4.f, 0.f, 5.f));
	Led->SetRelativeScale3D(FVector(0.025f));
	Led->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 메시지 화면: 리더기 위 (30cm x 8cm)
	Screen = CreateDefaultSubobject<UWidgetComponent>(TEXT("Screen"));
	Screen->SetupAttachment(Root);
	Screen->SetRelativeLocation(FVector(3.f, 0.f, 20.f));
	Screen->SetRelativeScale3D(FVector(0.06f));
	Screen->SetWidgetSpace(EWidgetSpace::World);
	Screen->SetWidgetClass(UPuzzleTextWidget::StaticClass());
	Screen->SetDrawSize(FVector2D(600.f, 150.f));
	Screen->SetBlendMode(EWidgetBlendMode::Transparent);
	Screen->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Screen->SetCanEverAffectNavigation(false);

	AcceptedMessage = FText::FromString(TEXT("출입 승인"));
	DeniedMessage = FText::FromString(TEXT("권한 없음"));
}

void APuzzleCardReader::BeginPlay()
{
	Super::BeginPlay();

	PuzzleUtils::ApplySolidColor(Body, FLinearColor(0.08f, 0.08f, 0.09f));
	LedMID = PuzzleUtils::ApplySolidColor(Led, ReaderRed, 0.f);
	Screen->InitWidget();
	HideMessage();
	UpdateLed();

	GetWorld()->GetTimerManager().SetTimer(CheckHandle, this, &APuzzleCardReader::CheckCards, 0.1f, true);
}

void APuzzleCardReader::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CheckHandle);
		World->GetTimerManager().ClearTimer(MessageHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void APuzzleCardReader::SetPowered(bool bNewPowered)
{
	bPowered = bNewPowered;
	CardsInRange.Reset();
	UpdateLed();
}

void APuzzleCardReader::UpdateLed()
{
	if (!bPowered)
	{
		PuzzleUtils::SetSolidColor(LedMID, FLinearColor(0.05f, 0.05f, 0.05f), 0.f);
	}
	else
	{
		PuzzleUtils::SetSolidColor(LedMID, bAccepted ? ReaderGreen : ReaderRed, 6.f);
	}
}

void APuzzleCardReader::CheckCards()
{
	if (!bPowered || bAccepted)
	{
		return;
	}

	const FVector Sensor = GetActorTransform().TransformPosition(FVector(8.f, 0.f, 0.f));
	TSet<TWeakObjectPtr<AActor>> NowInRange;

	for (TActorIterator<APuzzleGrabbable> It(GetWorld()); It; ++It)
	{
		APuzzleGrabbable* Card = *It;
		if (Card->IsHolstered())
		{
			continue;
		}
		FName CardTag = NAME_None;
		for (const FName& Tag : Card->Tags)
		{
			if (Tag.ToString().StartsWith(TEXT("Keycard")))
			{
				CardTag = Tag;
				break;
			}
		}
		if (CardTag.IsNone())
		{
			continue;
		}
		const float Dist = FVector::Dist(Card->GetActorLocation(), Sensor);
		if (Dist > DetectRadius)
		{
			// 진단용: 가까이 가져왔는데 인식이 안 될 때 거리 확인
			const double Now = GetWorld()->GetTimeSeconds();
			if (Dist < DetectRadius * 3.f && Now - LastNearLogTime > 1.0)
			{
				LastNearLogTime = Now;
				UE_LOG(LogCardReader, Log, TEXT("%s: %s 가 %.0fcm 거리 (인식 거리 %.0fcm)"), *GetName(), *CardTag.ToString(), Dist, DetectRadius);
			}
			continue;
		}

		NowInRange.Add(Card);
		if (CardsInRange.Contains(Card))
		{
			continue; // 계속 대고 있는 카드는 한 번만 처리
		}

		if (CardTag == AcceptTag)
		{
			UE_LOG(LogCardReader, Log, TEXT("%s: %s 승인"), *GetName(), *CardTag.ToString());
			bAccepted = true;
			UpdateLed();
			ShowMessage(AcceptedMessage, ReaderGreen, 4.f);
			OnAccepted.Broadcast();
			return;
		}

		UE_LOG(LogCardReader, Log, TEXT("%s: %s 거부"), *GetName(), *CardTag.ToString());
		ShowMessage(DeniedMessage, ReaderRed, 4.f);
		OnDenied.Broadcast();
	}
	CardsInRange = MoveTemp(NowInRange);
}

void APuzzleCardReader::ShowMessage(const FText& Message, const FLinearColor& Color, float Duration)
{
	if (UPuzzleTextWidget* W = Cast<UPuzzleTextWidget>(Screen->GetUserWidgetObject()))
	{
		W->SetContent(Message, 34, Color, FLinearColor(0.f, 0.f, 0.f, 0.85f));
	}
	Screen->SetVisibility(true);
	GetWorld()->GetTimerManager().SetTimer(MessageHandle, this, &APuzzleCardReader::HideMessage, Duration, false);
}

void APuzzleCardReader::HideMessage()
{
	Screen->SetVisibility(false);
}
