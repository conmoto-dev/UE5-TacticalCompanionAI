// Copyright Epic Games, Inc. All Rights Reserved.

#include "TacticalAIGameMode.h"
#include "TacticalAIPlayerController.h"

ATacticalAIGameMode::ATacticalAIGameMode()
{
	PlayerControllerClass = ATacticalAIPlayerController::StaticClass();
	
	DefaultPawnClass = nullptr;
}