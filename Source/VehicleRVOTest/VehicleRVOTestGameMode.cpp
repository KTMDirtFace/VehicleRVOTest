// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "VehicleRVOTest.h"
#include "VehicleRVOTestGameMode.h"
#include "VehicleRVOTestPawn.h"
#include "VehicleRVOTestHud.h"

AVehicleRVOTestGameMode::AVehicleRVOTestGameMode()
{
	DefaultPawnClass = AVehicleRVOTestPawn::StaticClass();
	HUDClass = AVehicleRVOTestHud::StaticClass();
}
