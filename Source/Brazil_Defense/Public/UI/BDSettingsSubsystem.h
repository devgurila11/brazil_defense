// Brazil Defense. Loads the player's settings, applies them and keeps them saved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UI/BDSettingsSave.h"
#include "BDSettingsSubsystem.generated.h"

/**
 * The one owner of UBDSettingsSave. Loads it when the game instance starts, before any
 * screen is up, and applies it: scalability, window and resolution through the engine's
 * game user settings, volumes through a sound mix override on the classes named in
 * UBDUISettings, language through BDLoc. The options panel writes through the setters
 * here, each of which applies and saves on the spot, so there is no Apply button and
 * nothing to lose by closing the game.
 *
 * A missing save is the first run: the defaults are written out and applied.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem interface

	/** The live settings. Read only from outside: change them through the setters. */
	const UBDSettingsSave& Get() const { return *Settings; }

	//~ Graphics --------------------------------------------------------------

	/** 0 Low .. 3 Epic; -1 lets the engine detect. */
	void SetQualityLevel(int32 Level);

	/** Zero picks the display's own resolution. */
	void SetResolution(FIntPoint Resolution);

	void SetWindowMode(EBDWindowMode Mode);
	void SetVSync(bool bEnabled);

	//~ Audio -----------------------------------------------------------------

	/** 0..100. */
	void SetMusicVolume(int32 Volume);

	/** 0..100. */
	void SetEffectsVolume(int32 Volume);

	void SetMuted(bool bMuted);

	//~ Language ----------------------------------------------------------------

	void SetLanguage(EBDLanguage Language);

	/** Applies every setting again, as at startup. */
	void ApplyAll();

	/** Writes the settings to the save slot. Done by every setter; here for whoever needs it explicitly. */
	void Save();

private:
	void Load();
	void ApplyGraphics();
	void ApplyAudio(UWorld* World);
	void ApplyLanguage();

	/** Every game world gets the volume overrides pushed onto it when it comes up. */
	void HandleWorldInitialized(UWorld* World, const UWorld::InitializationValues Values);

	UPROPERTY(Transient)
	TObjectPtr<UBDSettingsSave> Settings;

	FDelegateHandle WorldInitializedHandle;
};
