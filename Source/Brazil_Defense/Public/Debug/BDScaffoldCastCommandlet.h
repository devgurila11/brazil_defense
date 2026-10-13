// Brazil Defense. Tooling: lay out the cast - data assets, art folders and texts - for production.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BDScaffoldCastCommandlet.generated.h"

/**
 * Makes room for every character of the cast before its art exists (BDCast):
 *
 *   UnrealEditor-Cmd <uproject> -run=BDScaffoldCast -Candidatos=20 -Ministros=11 -Personagens=4 -Hordas=4 [-DryRun]
 *
 * 1. The three characters already in the game become the first of their kind, moved with
 *    the editor's rename (references fixed, redirectors removed) into their folder with the
 *    art only they use: DA_Candidate -> DA_Candidato_01, DA_Enemy_Test (the donkey and its
 *    skins) -> DA_Horda_01, DA_Shooter_Pistol (Nicole) -> DA_Personagem_01. What another
 *    asset also uses (the pistol the Agent holds too, the master material) stays where it is.
 * 2. Every DA_<Kind>_NN up to the count asked for is created when missing: the numbers of
 *    the first of its kind (health, speed, cost, levels), every art slot empty. A candidate
 *    is UBDCandidateData, with the rallies of his number (FBDRallyParams::DefaultsFor); one
 *    made before the rallies existed is turned into it in place, the only rewrite it does.
 * 3. Each character gets its folders Mesh, Textures, Materials, Anim and Audio, each with a
 *    .gitkeep so git holds them while they are empty.
 * 4. The string tables BD_pt and BD_en get Candidate.NN, Minister.NN, Shooter.NN and
 *    Enemy.NN with a provisional text.
 * 5. The counts are written to the cast settings, and BD.Cast.Audit is logged.
 *
 * Safe to run again: it creates what is missing and never overwrites, renames back or
 * deletes anything. A larger count only adds; a smaller one is ignored. A count left out
 * keeps the one in the settings. -DryRun only says what it would do.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDScaffoldCastCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UBDScaffoldCastCommandlet();

	//~ Begin UCommandlet interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet interface
};
