#include "PuzzleViewPoint.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PuzzleSign.h"
#include "PuzzleUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogViewPoint, Log, All);

namespace
{
	constexpr float FlashTime = 0.6f;
}

APuzzleViewPoint::APuzzleViewPoint()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void APuzzleViewPoint::BeginPlay()
{
	Super::BeginPlay();

	// 메시 조각은 단색 머티리얼로 칠해 두고 발광만 바꾼다
	for (AActor* Piece : Pieces)
	{
		if (!Piece || Cast<APuzzleSign>(Piece))
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
	ApplyGlow(0.f);
}

void APuzzleViewPoint::Solve()
{
	if (bSolved)
	{
		return;
	}
	bSolved = true;
	SolvedTime = GetWorld()->GetTimeSeconds();
	LastAlpha = -1.f;
	SetActorTickEnabled(true);
	UE_LOG(LogViewPoint, Log, TEXT("%s: 시점 맞추기 성공"), *GetName());
	OnSolved.Broadcast();
}

void APuzzleViewPoint::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bSolved)
	{
		// 한 번 번쩍인 뒤 밝은 채로
		const float T = GetWorld()->GetTimeSeconds() - SolvedTime;
		if (T <= FlashTime + DeltaTime)
		{
			const float Flash = FMath::Sin(FMath::Clamp(T / FlashTime, 0.f, 1.f) * PI);
			ApplyGlow(1.f + Flash);
		}
		else
		{
			SetActorTickEnabled(false);
		}
		return;
	}

	FVector ViewLoc;
	FRotator ViewRot;
	if (!PuzzleUtils::GetPlayerView(this, ViewLoc, ViewRot))
	{
		return;
	}

	const FVector ToTarget = (LookTarget - ViewLoc).GetSafeNormal();
	const float Dot = FVector::DotProduct(ViewRot.Vector(), ToTarget);

	// 가까울수록, 그리고 목표 쪽을 볼수록 밝게. 눈높이 판정이면 정답 눈 위치까지의 3D 거리로
	const bool bEyeMode = DesignEyeHeight > 0.f;
	const FVector Eye = GetActorLocation() + GetActorUpVector() * DesignEyeHeight;
	const float Distance = bEyeMode ? FVector::Dist(ViewLoc, Eye) : FVector::Dist2D(ViewLoc, GetActorLocation());
	const float Closeness = 1.f - FMath::Clamp(Distance / FMath::Max(GlowRadius, 1.f), 0.f, 1.f);
	const float Facing = FMath::Clamp((Dot - 0.7f) / FMath::Max(MinDot - 0.7f, 0.01f), 0.f, 1.f);
	ApplyGlow(Closeness * (0.4f + 0.6f * Facing));

	const FVector Local = GetActorTransform().InverseTransformPosition(ViewLoc);
	const bool bInZone = bEyeMode
		? FVector2D(Local.X, Local.Y).Size() <= ZoneSize.X * 0.5f && FMath::Abs(Local.Z - DesignEyeHeight) <= EyeHeightTolerance
		: FMath::Abs(Local.X) <= ZoneSize.X * 0.5f && FMath::Abs(Local.Y) <= ZoneSize.Y * 0.5f && Local.Z >= 0.f && Local.Z <= ZoneHeight;
	if (bInZone && Dot >= MinDot)
	{
		Solve();
	}
}

void APuzzleViewPoint::ApplyGlow(float Alpha)
{
	if (FMath::IsNearlyEqual(Alpha, LastAlpha, 0.01f))
	{
		return;
	}
	LastAlpha = Alpha;

	// Alpha 0~1: 멀다 → 가깝다, 1 이상(맞춘 뒤): 1 = 밝은 채로, 그 위는 번쩍임
	const float Emissive = Alpha <= 1.f && !bSolved
		? FMath::Lerp(IdleEmissive, NearEmissive, Alpha)
		: SolvedEmissive + (Alpha - 1.f) * 2.f;
	for (UMaterialInstanceDynamic* MID : PieceMIDs)
	{
		PuzzleUtils::SetSolidColor(MID, PieceColor, Emissive);
	}

	// 글자판(위젯)은 빛을 받지 않으므로 글자 색 밝기로 표현
	const float Brightness = Alpha <= 1.f && !bSolved ? FMath::Lerp(0.45f, 0.85f, Alpha) : 1.f + (Alpha - 1.f) * 0.5f;
	for (AActor* Piece : Pieces)
	{
		if (APuzzleSign* Sign = Cast<APuzzleSign>(Piece))
		{
			FLinearColor C = PieceColor * Brightness;
			C.A = 1.f;
			Sign->SetSignColor(C);
		}
	}
}
