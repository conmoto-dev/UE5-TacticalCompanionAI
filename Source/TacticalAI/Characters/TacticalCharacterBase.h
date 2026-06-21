// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "TacticalCharacterBase.generated.h"

// =========================================================================
// 동료·적이 공유하는 캐릭터 베이스 (카메라·입력 없음).
// 경계 기준은 "플레이어 빙의 가능 여부" — 카메라/입력은 빙의되는 동료(APartyCharacter)
// 에만 두고, 빙의와 무관한 공통 이동 특성(회전·속도 기본값)만 이 층에 모은다.
// 향후 HP·피격·사망 등 동료/적 공통 자산이 생기면 여기에 올린다(현재는 자리만 확보).
// =========================================================================
// 仲間と敵が共有するキャラ基底（カメラ・入力なし）。
// 境界は「プレイヤー憑依の可否」— カメラ/入力は憑依される仲間側にのみ置き、
// ここには憑依に依存しない共通の移動特性のみ集約する。
UCLASS(Abstract)
class TACTICALAI_API ATacticalCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	ATacticalCharacterBase();
};