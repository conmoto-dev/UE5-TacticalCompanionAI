// Fill out your copyright notice in the Description page of Project Settings.


#include "Controllers/CompanionAIController.h"
#include "AI/Components/TacticalCrowdFollowingComponent.h" // 우리가 새로 만들 클래스 헤더

// 기존의 빈 생성자 ACompanionAIController::ACompanionAIController() 대신 아래 코드를 사용합니다.
// 💡 부모(AAIController)가 원래 만들려던 "PathFollowingComponent"의 타입을 
// 우리가 만든 UTacticalCrowdFollowingComponent로 강제 교체해서 생성하라고 지시합니다.
ACompanionAIController::ACompanionAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UTacticalCrowdFollowingComponent>(TEXT("PathFollowingComponent")))
{

}

