#include "PuzzleRingDisc.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PuzzleButton.h"
#include "PuzzleUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogRingDisc, Log, All);

namespace
{
	constexpr float IdleEmissive = 0.25f;
	constexpr float SolvedEmissive = 2.5f;
	constexpr float FlashTime = 0.8f;
}

APuzzleRingDisc::APuzzleRingDisc()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void APuzzleRingDisc::BeginPlay()
{
	Super::BeginPlay();

	const FQuat DiscInv = GetActorQuat().Inverse();
	const int32 Num = Rings.Num();
	RingRest.SetNum(Num);
	Current.SetNum(Num);
	Shown.SetNum(Num);
	Target.SetNum(Num);
	const float StepDeg = 360.f / FMath::Max(StepsPerTurn, 2);
	for (int32 i = 0; i < Num; ++i)
	{
		RingRest[i] = Rings[i] ? DiscInv * Rings[i]->GetActorQuat() : FQuat::Identity;
		Current[i] = StartSteps.IsValidIndex(i) ? ((StartSteps[i] % StepsPerTurn) + StepsPerTurn) % StepsPerTurn : 0;
		Shown[i] = Target[i] = Current[i] * StepDeg;
	}
	ApplyAngles();

	// 버튼은 고리 순서대로 (동적 델리게이트라 고리 번호를 함께 넘길 수 없어서 함수를 나눔)
	using FHandler = void (APuzzleRingDisc::*)();
	static const FHandler Handlers[] = { &APuzzleRingDisc::HandleButton0, &APuzzleRingDisc::HandleButton1,
		&APuzzleRingDisc::HandleButton2, &APuzzleRingDisc::HandleButton3 };
	static const FName HandlerNames[] = { TEXT("HandleButton0"), TEXT("HandleButton1"), TEXT("HandleButton2"), TEXT("HandleButton3") };
	for (int32 i = 0; i < Buttons.Num() && i < UE_ARRAY_COUNT(Handlers); ++i)
	{
		if (Buttons[i])
		{
			Buttons[i]->bOneShot = false;
			Buttons[i]->OnPressed.__Internal_AddDynamic(this, Handlers[i], HandlerNames[i]);
		}
	}

	for (AActor* Piece : Pieces)
	{
		if (!Piece)
		{
			continue;
		}
		TInlineComponentArray<UStaticMeshComponent*> Meshes;
		Piece->GetComponents(Meshes);
		for (UStaticMeshComponent* M : Meshes)
		{
			if (UMaterialInstanceDynamic* MID = PuzzleUtils::ApplySolidColor(M, PieceColor, IdleEmissive))
			{
				PieceMIDs.Add(MID);
			}
		}
	}
	// 처음부터 이어져 있으면 안 되므로 (배치 실수) 로그로 알린다
	if (Num > 0 && Current.FilterByPredicate([this](int32 C) { return C == Current[0]; }).Num() == Num)
	{
		UE_LOG(LogRingDisc, Warning, TEXT("%s: 시작부터 모든 고리 칸이 같음 (StartSteps 확인)"), *GetName());
	}
}

void APuzzleRingDisc::HandleButton0() { TurnRing(0); }
void APuzzleRingDisc::HandleButton1() { TurnRing(1); }
void APuzzleRingDisc::HandleButton2() { TurnRing(2); }
void APuzzleRingDisc::HandleButton3() { TurnRing(3); }

void APuzzleRingDisc::TurnRing(int32 Index)
{
	if (bSolved || !Current.IsValidIndex(Index))
	{
		return;
	}
	Current[Index] = (Current[Index] + 1) % StepsPerTurn;
	Target[Index] += 360.f / StepsPerTurn;   // 늘 같은 방향으로 돈다 (보이는 각도는 360을 넘어가도 됨)
	UE_LOG(LogRingDisc, Log, TEXT("%s: 고리 %d → %d칸"), *GetName(), Index, Current[Index]);
	CheckSolved();
}

void APuzzleRingDisc::CheckSolved()
{
	if (bSolved || Current.Num() == 0)
	{
		return;
	}
	for (int32 C : Current)
	{
		if (C != Current[0])
		{
			return;
		}
	}
	bSolved = true;
	// 그림이 이어졌으면 원판 전체를 바로 선 방향으로 돌려 기호가 똑바로 읽히게
	for (int32 i = 0; i < Current.Num(); ++i)
	{
		const float Turns = FMath::CeilToFloat(Target[i] / 360.f - KINDA_SMALL_NUMBER);
		Target[i] = Turns * 360.f;
		Current[i] = 0;
	}
	for (APuzzleButton* Button : Buttons)
	{
		if (Button) { Button->bOneShot = true; }
	}
	UE_LOG(LogRingDisc, Log, TEXT("%s: 그림이 이어짐 → 성공"), *GetName());
	SolvedTime = GetWorld()->GetTimeSeconds() + TurnTime * 2.f;   // 바로 서는 동안 기다렸다가 번쩍
	OnSolved.Broadcast();
}

void APuzzleRingDisc::Solve()
{
	if (bSolved)
	{
		return;
	}
	for (int32 i = 0; i < Current.Num(); ++i)
	{
		Current[i] = Current.Num() > 0 ? Current[0] : 0;
	}
	CheckSolved();
}

void APuzzleRingDisc::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 한 칸(StepDeg)을 TurnTime초에 도는 속도로 목표 각도까지
	const float Speed = (360.f / FMath::Max(StepsPerTurn, 2)) / FMath::Max(TurnTime, 0.05f);
	bool bMoved = false;
	for (int32 i = 0; i < Shown.Num(); ++i)
	{
		if (!FMath::IsNearlyEqual(Shown[i], Target[i], 0.01f))
		{
			Shown[i] = FMath::FInterpConstantTo(Shown[i], Target[i], DeltaTime, Speed * (bSolved ? 3.f : 1.f));
			bMoved = true;
		}
	}
	if (bMoved)
	{
		ApplyAngles();
	}

	if (bSolved && SolvedTime >= 0.f)
	{
		const float T = GetWorld()->GetTimeSeconds() - SolvedTime;
		if (T >= 0.f)
		{
			const float Flash = T < FlashTime ? FMath::Sin(T / FlashTime * PI) : 0.f;
			SetPieceGlow(SolvedEmissive + Flash * 4.f);
			if (T >= FlashTime)
			{
				SolvedTime = -1.f;
			}
		}
	}
}

void APuzzleRingDisc::ApplyAngles()
{
	const FQuat Disc = GetActorQuat();
	for (int32 i = 0; i < Rings.Num(); ++i)
	{
		if (AActor* Ring = Rings[i])
		{
			// 원판 앞 방향(X)을 축으로. 플레이어 쪽에서 보면 시계 방향
			const FQuat Spin(FVector::XAxisVector, FMath::DegreesToRadians(Shown[i]));
			Ring->SetActorRotation(Disc * Spin * RingRest[i]);
		}
	}
}

void APuzzleRingDisc::SetPieceGlow(float Emissive)
{
	for (UMaterialInstanceDynamic* MID : PieceMIDs)
	{
		PuzzleUtils::SetSolidColor(MID, PieceColor, Emissive);
	}
}
