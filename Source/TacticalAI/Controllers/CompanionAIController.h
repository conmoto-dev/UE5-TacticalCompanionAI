// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "CompanionAIController.generated.h"

/**
 * 
 */
UCLASS()
class TACTICALAI_API ACompanionAIController : public AAIController
{
	GENERATED_BODY()
	
public:
	// 기본 생성자 대신 FObjectInitializer를 파라미터로 받는 생성자로 변경
	ACompanionAIController(const FObjectInitializer& ObjectInitializer);
};
