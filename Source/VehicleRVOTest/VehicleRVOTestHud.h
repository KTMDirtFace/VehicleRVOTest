// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "GameFramework/HUD.h"
#include "VehicleRVOTestHud.generated.h"


UCLASS(config = Game)
class AVehicleRVOTestHud : public AHUD
{
	GENERATED_BODY()

public:
	AVehicleRVOTestHud();

	/** Font used to render the vehicle info */
	UPROPERTY()
	UFont* HUDFont;

	// Begin AHUD interface
	virtual void DrawHUD() override;
	// End AHUD interface
};
