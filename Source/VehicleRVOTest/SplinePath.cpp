// Fill out your copyright notice in the Description page of Project Settings.

#include "VehicleRVOTest.h"
#include "SplinePath.h"

// Sets default values
ASplinePath::ASplinePath()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;// true;


	SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("SplineMesh"));
	
	//SetRootComponent(splineComponent);
}

// Called when the game starts or when spawned
void ASplinePath::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ASplinePath::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );

}

int32 ASplinePath::GetClosestSegmentIndex(const FVector &WorldLocation)
{
	if (SplineComponent == nullptr)
		return INDEX_NONE;

	const FVector LocalLocation = SplineComponent->ComponentToWorld.InverseTransformPosition(WorldLocation);

	// Find Closest Segment Index.
	const int32 NumPoints = SplineComponent->SplineCurves.Position.Points.Num();
	const int32 NumSegments = SplineComponent->SplineCurves.Position.bIsLooped ? NumPoints : NumPoints - 1;

	int32 BestSegmentIndex = INDEX_NONE;
	if (NumPoints > 1)
	{
		float BestDistanceSq;
		float BestResult = SplineComponent->SplineCurves.Position.InaccurateFindNearestOnSegment(LocalLocation, 0, BestDistanceSq);
		BestSegmentIndex = 0;
		for (int32 Segment = 1; Segment < NumSegments; ++Segment)
		{
			float LocalDistanceSq;
			float LocalResult = SplineComponent->SplineCurves.Position.InaccurateFindNearestOnSegment(LocalLocation, Segment, LocalDistanceSq);
			if (LocalDistanceSq < BestDistanceSq)
			{
				BestDistanceSq = LocalDistanceSq;
				BestResult = LocalResult;
				BestSegmentIndex = Segment;
			}
		}
	}

	if (NumPoints == 1)
		BestSegmentIndex = 0;

	return BestSegmentIndex;
}

FVector ASplinePath::GetLocationAlongSplineFromWorldLocation(const FVector &StartWorldLocation, float distFrom, ESplineCoordinateSpace::Type CoordinateSpace)
{
	FVector Location = FVector::ZeroVector;

	int32 CloseSegmentIndex = GetClosestSegmentIndex(StartWorldLocation);

	const int32 NumPoints = SplineComponent->SplineCurves.Position.Points.Num();
	bool bIsLooped = SplineComponent->SplineCurves.Position.bIsLooped;
	const int32 NumSegments = bIsLooped ? NumPoints : NumPoints - 1;

	if (CloseSegmentIndex != INDEX_NONE)
	{
		int32 NextSegmentIndex = CloseSegmentIndex + 1;
		if (bIsLooped)
		{
			if (NextSegmentIndex >= NumPoints)
				NextSegmentIndex = 0;
		}
		else
		{
			if (NextSegmentIndex >= NumPoints)
				NextSegmentIndex = NumPoints - 1;
		}


		float DistToCloseIndex = SplineComponent->GetDistanceAlongSplineAtSplinePoint(CloseSegmentIndex);
		float DistToNextIndex = SplineComponent->GetDistanceAlongSplineAtSplinePoint(NextSegmentIndex);

		float KeyIndexToCloseSegment = SplineComponent->GetInputKeyAtDistanceAlongSpline(DistToCloseIndex);
		float KeyIndexToNextSegment = SplineComponent->GetInputKeyAtDistanceAlongSpline(DistToNextIndex);
		// If we looped around. then use the end.
		if (NextSegmentIndex == 0)
			KeyIndexToNextSegment = 1.0f;

		float KeyIndexAtLocation = SplineComponent->FindInputKeyClosestToWorldLocation(StartWorldLocation);
		const float TimeMultiplier = SplineComponent->Duration / (bIsLooped ? NumPoints : (NumPoints - 1.0f));
		KeyIndexAtLocation *= TimeMultiplier;

		float SegmentLength = SplineComponent->GetSegmentLength(CloseSegmentIndex);
		// Percentage we are on the segment....
		// Approximation.
		float perc = (KeyIndexAtLocation - KeyIndexToCloseSegment) / (KeyIndexToNextSegment - KeyIndexToCloseSegment);
		float DistFromClose = SegmentLength * perc;
		//float RemDistToNext = SegmentLength * (1 - perc);

		float NewDistanceAhead = DistToCloseIndex + DistFromClose + distFrom;
		float SplineLength = SplineComponent->GetSplineLength();
		if (NewDistanceAhead > SplineComponent->GetSplineLength())
			NewDistanceAhead = NewDistanceAhead - SplineLength;

		Location = SplineComponent->GetLocationAtDistanceAlongSpline(NewDistanceAhead, CoordinateSpace);
	}

#if 0
	// Hack DebugDraw All points
	for (int32 i = 0; i < NumPoints; i++)
	{
		FVector location = GetLocationAtSplinePoint(i, CoordinateSpace);
		DrawDebugSphere(GWorld, location, 50, 10, FColor::Magenta);
	}
#endif

	return Location;
}