// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Containers/Map.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HAL/Platform.h"
#include "Math/MathFwd.h"
#include "ProceduralMeshComponent.h"
#include <random>
#include "Templates/SubclassOf.h"
#include "WorldGenerator.generated.h"

struct RandomPoint
{
	FVector2D UVPos;	 // 0 - 1 的随机变量表示点的坐标
	FRotator Rotation; // 0 - 1 的随机变量表示点的旋转
	FVector Scale;
};

UENUM()
enum class EDrawType : uint8
{
	None,
	Gaussian,
	Cubic,
};

UENUM()
enum class EDistanceFunc : uint8
{
	Euclidean,
	Xaxis, // 仅考虑 X 轴距离
	Yaxis, // 仅考虑 Y 轴距离
				 // Manhattan,
};

UCLASS()
class RUNNER_API AWorldGenerator : public AActor
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxRegionCount = 2; // 最多同时存在的 Region 数量
	static constexpr int32 MaxThreadCount = 4; // 最大线程数

	// Sets default values for this actor's properties
	AWorldGenerator();

	double PerlinCosTheta;
	double PerlinSinTheta;
	FInt32Point PerlinOffset;
	int32 BarrierRandom;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Generation")
	TObjectPtr<class UMaterialInterface> TileMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Generation")
	TArray<TObjectPtr<class ABarrierSpawner>> BarrierSpawners; // 存储生成的障碍物生成器实例

	// 一个方格的大小
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Generation")
	float CellSize = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Generation")
	int32 XCellNumber = 50;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Generation")
	int32 YCellNumber = 50;

	// 由每个 barrier 类自己决定生成多少个障碍物
	// UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Generation")
	// int32 MinBarrierCount = 30;

	// UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Generation")
	// int32 MaxBarrierCount = 60;

	// 点之间的最小距离, 移到 BarrierSpawner 中了
	// UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Generation")
	// float PoissonDistance = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Generation")
	int32 SampleCountBeforeReject = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Generation")
	double MaxTextureCoords = 2000.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Generation")
	double RegionSize = 40000.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Generation")
	FVector2D TextureSize = FVector2D(1024.0f, 1024.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Generation")
	TArray<float> PerlinFreq;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Generation")
	TArray<float> PerlinAmplitude;

	// 各个 barrier group 采用的距离函数
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Generation")
	TArray<EDistanceFunc> GroupDistanceFunc;

	// TrianglesBuffer 仅在 begin play 时被填充一次，之后只读
	TArray<int32> TrianglesBuffer;
	TArray<FVector2D> UV1Buffer;

	// 在此之前的属性都是在运行时只读的，也允许其它线程访问

	enum class EBufferState : int8
	{
		Idle,
		Busy,
		Completed
	};
	// 仅允许 game 线程访问!
	EBufferState BufferStateGameThreadOnly[MaxThreadCount] = { EBufferState::Idle };
	FGraphEventRef AsyncTaskRef[MaxThreadCount]; // 异步任务引用
	FInt32Point TilesInBuilding[MaxThreadCount]; // 每个线程的偏移量

	UPROPERTY(VisibleAnywhere, Category = "World Generation")
	mutable TObjectPtr<class UProceduralMeshComponent> ProceduralMeshComp[MaxRegionCount];

protected:
	// Called when the game starts or when spawned
	void BeginPlay() override;
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	void InitDataBuffer();
	void SortBarrierSpawners();
	// void SpawnBarrierSpawners();

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	FInt32Point GetPlayerTile() const;

	// 检查该 tile 是否在 player 周围（如果离 player 较远我们可以释放它）
	bool IsNeccessrayTile(FInt32Point Tile) const;
	// 检查该 tile 是否有效（是否在 TileMap 中）
	bool IsValidTile(FInt32Point Tile) const
	{
		for (int32 i = 0; i < MaxRegionCount; ++i)
		{
			if (TileMap[i].Contains(Tile))
			{
				return true; // Tile is found in one of the regions
			}
		}
		return false; // Tile is not found in any region
	}
	bool CanRemoveTile(FInt32Point Tile) const;

	// 寻找一个可以替换的 section, 如果没有找到则返回 -1
	int32 FindReplaceableSection(int32 PMCIndex);

	// 发起一个异步任务来生成 tile 数据
	bool GenerateOneTile(FInt32Point Tile);
	void CreateMeshFromTileData();
	// 根据当前玩家的位置生成新的 tiles
	void GenerateNewTiles();

	// 该函数可以从任意线程中调用
	FVector2D GetUVFromPosAnyThread(FVector Position) const;

	// 该函数可以从任意线程中调用
	double GetHeightFromPerlinAnyThread(FVector2D Pos, FInt32Point CellPos) const;

	// 该函数可以从任意线程中调用
	// int32 GetBarrierCountForTileAnyThread(FInt32Point Tile, int32 BarrierIndex, double RandomValue) const;

	FInt32Point GetRegionFromHorizontalPos(FVector2D Pos) const
	{
		return FInt32Point(FMath::FloorToInt(Pos.X / RegionSize), FMath::FloorToInt(Pos.Y / RegionSize));
	}
	// 获取对应位置的 PMC 和它在数组中的编号
	TPair<UProceduralMeshComponent*, int32> GetPMCFromHorizontalPos(FVector2D Pos) const;
	FInt32Point GetRegionFromPMC(UProceduralMeshComponent* PMC) const
	{
		auto TestPos = PMC->GetComponentLocation() + FVector(RegionSize / 2.0, RegionSize / 2.0, 0.0);
		return FInt32Point(FMath::FloorToInt(TestPos.X / RegionSize), FMath::FloorToInt(TestPos.Y / RegionSize));
	}
	FInt32Point GetTileFromHorizontalPos(FVector2D Pos) const;

	// Visual 表示这里的高度和位置是不考虑障碍物，仅考虑地形的
	double GetVisualHeightFromHorizontalPos(FVector2D Pos) const;
	FVector GetVisualWorldPositionFromUV(FVector2D UV, FInt32Point Tile) const;

	// 这里的高度不仅考虑地形，还会考虑障碍物
	double GetHeightFromHorizontalPos(FVector2D Pos /*, class UPrimitiveComponent*& HitComponent*/) const;

	FVector2D GetUVandTileFromPos(FVector2D Pos, FInt32Point& Tile) const;
	int32 GetTriangleFromUV(FVector2D UV, FVector2D& BarycentricCoords) const;
	FVector GetNormalFromHorizontalPos(FVector2D Pos) const;
	FVector GetNormalFromUVandTile(FVector2D UV, FInt32Point Tile) const;

	void PMCClear(int32 PMCIndex);

	// 对高度图进行后处理微调
	void PostProcessHeightMap(FInt32Point Tile, TArray<FVector>& VerticesBuffer);

	// FVector TransformUVToWorldPos(RandomPoint& Point, FInt32Point Tile) const;

	void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent);
#endif // WITH_EDITOR

private:
	// 缓存延迟 spawn 的 spawner 需要的数据
	mutable TMap<FIntVector, TArray<RandomPoint>> CachedSpawnData;

	mutable TArray<FInt32Point> TileMap[MaxRegionCount]; // 用于存储生成的方格位置

	// TArray<FVector> VerticesBuffer;
	// TArray<FVector> NormalsBuffer;
	// TArray<FVector2D> UV0Buffer;
	// TArray<FProcMeshTangent> TangentsBuffer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Generation", meta = (AllowPrivateAccess = "true"))
	bool bOneLineMode = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (AllowPrivateAccess = "true"))
	bool bDebugMode = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (AllowPrivateAccess = "true"))
	bool bEnablePostProcessHeightMap = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (AllowPrivateAccess = "true"))
	bool bDrawSamplingPoint = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (AllowPrivateAccess = "true"))
	bool bCheckPoissonSampling = false;

	UPROPERTY(EditAnywhere, Category = "Debug", meta = (AllowPrivateAccess = "true"))
	int32 DebugBarrierRandom = 0;

	UPROPERTY(EditAnywhere, Category = "Debug", meta = (AllowPrivateAccess = "true"))
	int32 DebugTheta = 0;

	UPROPERTY(EditAnywhere, Category = "Debug", meta = (AllowPrivateAccess = "true"))
	FInt32Point DebugOffset = FInt32Point(0, 0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (AllowPrivateAccess = "true"))
	EDrawType DrawType = EDrawType::Gaussian;

	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (AllowPrivateAccess = "true"))
	// FVector2D UVOffset = FVector2D(0.0f, 0.0f); // 用于调试 UV 偏移

	mutable int64 VersionNumber[MaxRegionCount]; // 对应每个 Region 的版本号
	mutable int64 CurrentVersionIndex = 0;

	void DebugPrint() const;

	// 邪气追赶逻辑
public:
	UPROPERTY(EditAnywhere, Category = "Evil Chase")
	TObjectPtr<class UMaterialParameterCollection> EvilChaseMaterialCollection;

	UPROPERTY(EditAnywhere, Category = "Evil Chase")
	TArray<float> EvilChaseSpeed;

	UPROPERTY(EditAnywhere, Category = "Evil Chase")
	TArray<float> EvilMaxDistance;

	double GetEvilPos() { return EvilPos; }
	void SetGameStart() { bGameStart = true; }

	// 导弹逻辑
public:
	UPROPERTY(EditAnywhere, Category = "Missile")
	TObjectPtr<class UMissileComponent> MissileComponent;

	// 用于导弹生成
	FVector2D ClampToWorld(FVector2D Pos, double ObjectRadius) const;

	// 难度分级
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Generation")
	TArray<float> DifficultyThresholds; // 该难度结束的时间

	int32 CurrentDifficulty = 0; // 当前难度
	float WorldTime = 0.0f;			 // 世界时间

	FInt32Point GetPlayerStartTile() const
	{
		return PlayerStartTile;
	}

private:
	void UpdateEvilPos(float DeltaTime);

	bool bGameStart = false;
	FInt32Point PlayerStartTile;

	UPROPERTY(EditAnywhere, Category = "Evil Chase", meta = (AllowPrivateAccess = "true"))
	double EvilPos;

	// 多线程数据
private:
	struct alignas(64) TaskBuffer
	{
		TArray<RandomPoint> RandomPoints; // 用于存储生成的随机点
		TArray<int32> BarriersCount;			// 存储每个 Barrier Spawner 生成的障碍物数量
		TArray<FVector> VerticesBuffer;
		TArray<FVector> NormalsBuffer;
		TArray<FVector2D> UV0Buffer;
		TArray<FProcMeshTangent> TangentsBuffer;
		// 这玩意怎么这么大，是否有必要每个线程一个？
		std::mt19937_64 RandomEngine; // 随机数引擎
	};
	// TaskDataBuffers 用于存储每个线程的任务数据, 64 Bytes 对齐
	TaskBuffer TaskDataBuffers[MaxThreadCount];
	// 在异步线程中执行
	void GenerateOneTileAsync(int32 BufferIndex, int32 Difficulty, FInt32Point Tile, FVector2D PositionOffset);
	void GenerateRandomPointsAsync(int32 BufferIndex, int32 Difficulty, FInt32Point Tile, TArray<RandomPoint>& RandomPoints);

	void GenerateUniformRandomPointsAsync(int32 BufferIndex, int32 Difficulty, TArray<RandomPoint>& RandomPoints);
	// 使用泊松采样生成随机点
	void GeneratePoissonRandomPointsAsync(int32 BufferIndex, int32 Difficulty, TArray<RandomPoint>& RandomPoints);

	// 各种数学函数测试
	using DistanceFuncType = bool (*)(FVector2D, FVector2D, double);
	// 欧式距离
	static bool EuclideanDistance(FVector2D A, FVector2D B, double MinDistance)
	{
		return FVector2D::DistSquared(A, B) < MinDistance * MinDistance;
	}
	// 仅考虑 X 轴距离
	static bool XaxisDistance(FVector2D A, FVector2D B, double MinDistance)
	{
		return FMath::Abs(A.X - B.X) < MinDistance;
	}
	// 仅考虑 Y 轴距离
	static bool YaxisDistance(FVector2D A, FVector2D B, double MinDistance)
	{
		return FMath::Abs(A.Y - B.Y) < MinDistance;
	}
	static DistanceFuncType GetDistanceFunc(EDistanceFunc Func)
	{
		switch (Func)
		{
			case EDistanceFunc::Euclidean:
				return EuclideanDistance;
			case EDistanceFunc::Xaxis:
				return XaxisDistance;
			case EDistanceFunc::Yaxis:
				return YaxisDistance;
			default:
				return EuclideanDistance; // 默认使用欧式距离
		}
	}

	template <class T>
	static int32 PoissonSampling(double XSize, double YSize, double MinDistance, int32 SampleCountBeforeReject, std::mt19937_64& RandomEngine, T DistLambda, TArray<FVector2D>& OutPoints);

	// 二维高斯分布
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Math|Gaussian", meta = (AllowPrivateAccess = "true"))
	double SigmaX = 0.1; // 0.6 - 0.8
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Math|Gaussian", meta = (AllowPrivateAccess = "true"))
	double SigmaY = 0.1; // 0.6 - 0.8
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Math|Gaussian", meta = (AllowPrivateAccess = "true"))
	double GSAmplitude = 10.0; // 2000
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Math|Gaussian", meta = (AllowPrivateAccess = "true"))
	double GSScale = 0.01; // 0.001

	double Gaussian2D(double X, double Y) const
	{
		X = X * GSScale;
		Y = Y * GSScale;
		return (GSAmplitude / (2.0 * PI * SigmaX * SigmaY)) * FMath::Exp(-(X * X / (2.0 * SigmaX * SigmaX) + Y * Y / (2.0 * SigmaY * SigmaY)));
	}
	void GenerateGaussian2D(TArray<FVector>& Vertices) const;
};
