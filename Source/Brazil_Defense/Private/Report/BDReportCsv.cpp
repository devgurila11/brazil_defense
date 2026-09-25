// Brazil Defense. What the reports share: the board counted, and a row appended to a CSV.

#include "Report/BDReportCsv.h"

#include "BDLog.h"
#include "Debug/BDSimSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Placement/BDPlaceableData.h"
#include "Placement/BDPlacementComponent.h"
#include "Save/BDMatchSave.h"
#include "Tower/BDTowerBase.h"

FString BDReportCsv::Quote(const FString& Text)
{
	return TEXT("\"") + Text.Replace(TEXT("\""), TEXT("\"\"")) + TEXT("\"");
}

FString BDReportCsv::Decimal(const double Value)
{
	return FString::Printf(TEXT("%.2f"), Value);
}

const TCHAR* BDReportCsv::Mode(const UWorld& World)
{
	const UBDSimSubsystem* Sim = World.GetSubsystem<UBDSimSubsystem>();
	if (Sim != nullptr && Sim->IsRunning())
	{
		return TEXT("Sim");
	}
	return FApp::CanEverRender() ? TEXT("Screen") : TEXT("Headless");
}

FBDBoardTally BDReportCsv::TallyBoard(UWorld& World)
{
	FBDBoardTally Tally;

	const APlayerController* Controller = World.GetFirstPlayerController();
	const UBDPlacementComponent* Placement = Controller != nullptr ? Controller->FindComponentByClass<UBDPlacementComponent>() : nullptr;
	if (Placement != nullptr)
	{
		TArray<FBDSavedPiece> Pieces;
		Placement->CaptureBoard(Pieces);
		for (const FBDSavedPiece& Piece : Pieces)
		{
			const UBDPlaceableData* Data = Cast<UBDPlaceableData>(Piece.Data.ResolveObject());
			const EBDPieceKind Kind = Data != nullptr ? Data->GetPieceKind() : EBDPieceKind::Objective;
			if (Kind == EBDPieceKind::Objective)
			{
				continue;
			}
			++Tally.PerPiece.FindOrAdd(Piece.Data);
			Tally.Towers += Kind == EBDPieceKind::Tower ? 1 : 0;
			Tally.Characters += Kind == EBDPieceKind::Character ? 1 : 0;
			Tally.Platforms += Kind == EBDPieceKind::Platform ? 1 : 0;
			Tally.Dividers += Kind == EBDPieceKind::Divider ? 1 : 0;
		}
	}

	for (TActorIterator<ABDTowerBase> It(&World); It; ++It)
	{
		++Tally.Defenders;
		Tally.LevelSum += It->GetTowerLevel();
		Tally.TopLevel = FMath::Max(Tally.TopLevel, It->GetTowerLevel());
	}
	return Tally;
}

bool BDReportCsv::AppendRow(const FString& Path, const TArray<FColumn>& Columns)
{
	FString Header;
	FString Row;
	for (int32 Index = 0; Index < Columns.Num(); ++Index)
	{
		const TCHAR* Separator = Index > 0 ? TEXT(",") : TEXT("");
		Header += Separator + Columns[Index].Name;
		Row += Separator + Columns[Index].Value;
	}

	IFileManager& Files = IFileManager::Get();
	bool bNeedsHeader = !Files.FileExists(*Path);
	if (!bNeedsHeader)
	{
		TArray<FString> Lines;
		FFileHelper::LoadFileToStringArray(Lines, *Path);
		if (Lines.Num() > 0 && Lines[0] != Header && Header.StartsWith(Lines[0] + TEXT(",")))
		{
			int32 Added = 0;
			for (const TCHAR Character : Header.RightChop(Lines[0].Len()))
			{
				Added += Character == TEXT(',') ? 1 : 0;
			}
			FString Padded = Header + LINE_TERMINATOR;
			for (int32 Index = 1; Index < Lines.Num(); ++Index)
			{
				if (!Lines[Index].IsEmpty())
				{
					Padded += Lines[Index] + FString::ChrN(Added, TEXT(',')) + LINE_TERMINATOR;
				}
			}
			FFileHelper::SaveStringToFile(Padded, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
			UE_LOG(LogBDMatch, Log, TEXT("  %d column(s) added at the end of %s: the %d row(s) already there were padded."),
				Added, *FPaths::GetCleanFilename(Path), Lines.Num() - 1);
			Lines[0] = Header;
		}
		if (Lines.Num() == 0 || Lines[0] != Header)
		{
			const FString Aside = FPaths::Combine(FPaths::GetPath(Path),
				FString::Printf(TEXT("%s-%s.csv"), *FPaths::GetBaseFilename(Path), *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"))));
			Files.Move(*Aside, *Path);
			UE_LOG(LogBDMatch, Log, TEXT("  the columns changed: the old report was kept as %s."), *Aside);
			bNeedsHeader = true;
		}
	}

	const FString Text = (bNeedsHeader ? Header + LINE_TERMINATOR : FString()) + Row + LINE_TERMINATOR;
	if (!FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &Files, FILEWRITE_Append))
	{
		UE_LOG(LogBDMatch, Error, TEXT("  a row could not be written to %s."), *Path);
		return false;
	}
	return true;
}
