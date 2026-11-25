#include "PuzzleSfx.h"

#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogPuzzleSfx, Log, All);

namespace
{
	const TCHAR* AssetName(PuzzleSfx::ESfx Sfx)
	{
		using PuzzleSfx::ESfx;
		switch (Sfx)
		{
		case ESfx::Click:        return TEXT("SFX_Click");
		case ESfx::Beep:         return TEXT("SFX_Beep");
		case ESfx::Accept:       return TEXT("SFX_Accept");
		case ESfx::Deny:         return TEXT("SFX_Deny");
		case ESfx::Success:      return TEXT("SFX_Success");
		case ESfx::LeverClunk:   return TEXT("SFX_LeverClunk");
		case ESfx::DoorOpen:     return TEXT("SFX_DoorOpen");
		case ESfx::ElevatorDing: return TEXT("SFX_ElevatorDing");
		case ESfx::PlateThud:    return TEXT("SFX_PlateThud");
		case ESfx::Dispense:     return TEXT("SFX_Dispense");
		case ESfx::Snap:         return TEXT("SFX_Snap");
		case ESfx::RingTurn:     return TEXT("SFX_RingTurn");
		case ESfx::PadChime:     return TEXT("SFX_PadChime");
		case ESfx::CoreShutdown: return TEXT("SFX_CoreShutdown");
		}
		return nullptr;
	}

	USoundBase* LoadSfx(PuzzleSfx::ESfx Sfx)
	{
		const TCHAR* Name = AssetName(Sfx);
		if (!Name)
		{
			return nullptr;
		}
		const FSoftObjectPath Path(FString::Printf(TEXT("/Game/VRPuzzle/Audio/SFX/%s.%s"), Name, Name));
		USoundBase* Sound = Cast<USoundBase>(Path.TryLoad());
		if (!Sound)
		{
			// 같은 경고가 계속 찍히지 않게 소리마다 한 번만
			static TSet<FString> Warned;
			if (!Warned.Contains(Name))
			{
				Warned.Add(Name);
				UE_LOG(LogPuzzleSfx, Warning, TEXT("효과음 %s 을 찾지 못함 (Scripts/make_sfx.py → import_sfx.py 로 임포트했는지 확인)"), Name);
			}
		}
		return Sound;
	}
}

void PuzzleSfx::Play(const UObject* WorldContext, ESfx Sfx, const FVector& Location, float Volume)
{
	if (!WorldContext)
	{
		return;
	}
	if (USoundBase* Sound = LoadSfx(Sfx))
	{
		UE_LOG(LogPuzzleSfx, Verbose, TEXT("%s 재생 (%s)"), *Sound->GetName(), *GetNameSafe(WorldContext));
		UGameplayStatics::PlaySoundAtLocation(WorldContext, Sound, Location, Volume);
	}
}

void PuzzleSfx::Play2D(const UObject* WorldContext, ESfx Sfx, float Volume)
{
	if (!WorldContext)
	{
		return;
	}
	if (USoundBase* Sound = LoadSfx(Sfx))
	{
		UE_LOG(LogPuzzleSfx, Verbose, TEXT("%s 재생 2D (%s)"), *Sound->GetName(), *GetNameSafe(WorldContext));
		UGameplayStatics::PlaySound2D(WorldContext, Sound, Volume);
	}
}
