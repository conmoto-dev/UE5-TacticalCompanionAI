// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/TacticalCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ATacticalCharacterBase::ATacticalCharacterBase()
{
	// 공통 캡슐 기본값. 인간형 표준 — 슬라임·보스 등 체격이 다른 종류는 자식/BP에서 덮어쓴다.
	// 共通カプセル既定値。体格の違う種類は子/BPで上書き。
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	// 컨트롤러 회전이 캐릭터를 직접 돌리지 않게 — 캐릭터는 "이동 방향"으로 돈다.
	// AI 이동(동료·적)과 third-person 빙의 양쪽에 맞는 공통 설정이라 베이스에 둔다.
	// コントローラー回転でキャラを直接回さず移動方向に向ける。AI移動と憑依の両方に適合。
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw   = false;
	bUseControllerRotationRoll  = false;

	// 공통 이동 특성 기본값. 종류별 조정(속도 등)은 자식/BP에서.
	// 共通の移動特性。種類別の調整は子/BPで。
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->bOrientRotationToMovement  = true;
	MoveComp->RotationRate               = FRotator(0.f, 500.f, 0.f);
	MoveComp->JumpZVelocity              = 500.f;
	MoveComp->AirControl                 = 0.35f;
	MoveComp->MaxWalkSpeed               = 500.f;
	MoveComp->MinAnalogWalkSpeed         = 20.f;
	MoveComp->BrakingDecelerationWalking = 2000.f;
	MoveComp->BrakingDecelerationFalling = 1500.f;
}