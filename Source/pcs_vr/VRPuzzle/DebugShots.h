#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace DebugShots
{
	/**
	 * 셰이더 컴파일이 끝난 뒤(머티리얼을 바꾼 직후에는 한동안 기본 체크무늬로 보임) + 2초 뒤에 Action을 부른다.
	 * 헤드셋 없이 스크린샷으로 확인하는 콘솔 명령(pcs.Shots, pcs.Stage4Test)이 같이 쓴다.
	 */
	PCS_VR_API void WhenReady(UWorld* World, TFunction<void()> Action);
}
