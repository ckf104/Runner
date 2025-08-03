#include "Containers/Array.h"
#include "HAL/Runnable.h"
#include "Math/MathFwd.h"

class FPCGWorker : public FRunnable
{
public:
  FPCGWorker(int32 InMaxPointsInCell, int32 InMinPointsInCell, int32 InXCellNumber, int32 InYCellNumber)
    : MaxPointsInCell(InMaxPointsInCell), MinPointsInCell(InMinPointsInCell), XCellNumber(InXCellNumber), YCellNumber(InYCellNumber)
  {
    Heights[0].SetNumUninitialized(XCellNumber * YCellNumber);
    Heights[1].SetNumUninitialized(XCellNumber * YCellNumber);
    Points[0].SetNumUninitialized(XCellNumber * YCellNumber);
    Points[1].SetNumUninitialized(XCellNumber * YCellNumber);
  }

  int32 MaxPointsInCell;
  int32 MinPointsInCell;
  int32 XCellNumber;
  int32 YCellNumber;

  int32 ReadIndex = 0;
  
  TArray<double> Heights[2];
	TArray<FVector2D> Points[2];

	uint32 Run() override;
};