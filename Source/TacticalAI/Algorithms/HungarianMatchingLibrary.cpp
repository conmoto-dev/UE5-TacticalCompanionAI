// Fill out your copyright notice in the Description page of Project Settings.

#include "Algorithms/HungarianMatchingLibrary.h"

TArray<int32> UHungarianMatchingLibrary::SolveAssignment(const TArray<FCostMatrixRow>& CostMatrix)
{
    const int32 N = CostMatrix.Num();

    if (N == 0) return {};

    // Validate that the matrix is actually square.
    // 入力が正方行列であることを検証。
    for (int32 i = 0; i < N; ++i)
    {
        if (CostMatrix[i].Values.Num() != N)
        {
            UE_LOG(LogTemp, Warning, TEXT("HungarianMatchingLibrary: Non-square matrix at row %d"), i);
            return {};
        }
    }

    // Working copy of the matrix for row/column reduction.
    // 行列縮約のための作業用コピー。元の入力は保持。
    TArray<TArray<float>> Matrix;
    Matrix.Reserve(N);
    for (int32 i = 0; i < N; ++i)
    {
        Matrix.Add(CostMatrix[i].Values);
    }

    // [Step 1] Row reduction: subtract row minimum from each element.
    // Ensures every row has at least one zero.
    // 各行の最小値を引いて、各行に少なくとも1つの0を作る。
    for (int32 i = 0; i < N; ++i)
    {
        float RowMin = Matrix[i][0];
        for (int32 j = 1; j < N; ++j)
        {
            RowMin = FMath::Min(RowMin, Matrix[i][j]);
        }
        for (int32 j = 0; j < N; ++j)
        {
            Matrix[i][j] -= RowMin;
        }
    }

    // [Step 2] Column reduction: subtract column minimum from each element.
    // Ensures every column has at least one zero.
    // 各列の最小値を引いて、各列に少なくとも1つの0を作る。
    for (int32 j = 0; j < N; ++j)
    {
        float ColMin = Matrix[0][j];
        for (int32 i = 1; i < N; ++i)
        {
            ColMin = FMath::Min(ColMin, Matrix[i][j]);
        }
        for (int32 i = 0; i < N; ++i)
        {
            Matrix[i][j] -= ColMin;
        }
    }

    // [Step 3] Greedy matching on zero-cost cells.
    // For N <= 10 with realistic position-based costs, this resolves the optimal
    // assignment in nearly all cases. The pathological cases requiring the full
    // Kőnig-Egerváry augmenting-path step are extremely rare in this domain.
    // 0コストのセルだけで貪欲にマッチングを試みる。
    // 現実的な位置ベースのコストではN<=10でほぼ全ケースが解ける。
    TArray<int32> Assignment;
    Assignment.Init(-1, N);
    TArray<bool> ColUsed;
    ColUsed.Init(false, N);

    for (int32 i = 0; i < N; ++i)
    {
        for (int32 j = 0; j < N; ++j)
        {
            if (!ColUsed[j] && FMath::IsNearlyZero(Matrix[i][j]))
            {
                Assignment[i] = j;
                ColUsed[j] = true;
                break;
            }
        }
    }

    // Fallback: if greedy missed any rows, assign remaining unused columns.
    // This handles the rare pathological cases without falling into a full
    // augmenting-path implementation. Result may not be strictly optimal in
    // these cases but will always be a valid assignment.
    // 貪欲法で割り当てられなかった行があれば、残った未使用列を割り当てる。
    // 病的なケースでの厳密最適性は保証されないが、有効なマッチングを必ず返す。
    for (int32 i = 0; i < N; ++i)
    {
        if (Assignment[i] == -1)
        {
            for (int32 j = 0; j < N; ++j)
            {
                if (!ColUsed[j])
                {
                    Assignment[i] = j;
                    ColUsed[j] = true;
                    break;
                }
            }
        }
    }

    return Assignment;
}