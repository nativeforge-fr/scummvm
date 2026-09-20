/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "common/config-manager.h"
#include "common/system.h"
#include "common/archive.h"
#include "common/fs.h"
#include "common/error.h"
#include "common/file.h"
#include "common/rect.h"
#include "engines/util.h"
#include "image/bmp.h"

#include "cryomni3d/font_manager.h"
#include "cryomni3d/dialogs_manager.h"
#include "cryomni3d/fixed_image.h"
#include "cryomni3d/omni3d.h"

#include "cryomni3d/versailles/engine.h"

// 0 or commented: All videos and options screen
// 1: Skip videos at startup and at game start
// 2: Directly start a new game
//#define DEBUG_FAST_START 1

namespace CryOmni3D {
namespace Versailles {

// Widescreen standalone helpers, defined at the end of this file.
static Common::FSNode childCaseless(const Common::FSNode &parent, const Common::String &name);
static Common::Language parseLanguageCode(const Common::String &code, Common::Language fallback);
static Common::Language osDefaultLanguage(); // system language, mapped to a supported one (else English)

const FixedImageConfiguration CryOmni3DEngine_Versailles::kFixedImageConfiguration = {
	45, 223, 243, 238, 226, 198, 136, 145, 99, 113,
	470
};

CryOmni3DEngine_Versailles::CryOmni3DEngine_Versailles(OSystem *syst,
		const CryOmni3DGameDescription *gamedesc) : CryOmni3DEngine(syst, gamedesc),
	_audioLanguage(gamedesc->desc.language),
	_mainPalette(nullptr), _cursorPalette(nullptr), _transparentPaletteMap(nullptr),
	_transparentSrcStart(uint(-1)), _transparentSrcStop(uint(-1)), _transparentDstStart(uint(-1)),
	_transparentDstStop(uint(-1)), _transparentNewStart(uint(-1)), _transparentNewStop(uint(-1)),
	_currentPlace(nullptr), _currentWarpImage(nullptr), _fixedImage(nullptr),
	_transitionAnimateWarp(true), _forceRedrawWarp(false), _forcePaletteUpdate(false),
	_fadedPalette(false), _loadedSave(uint(-1)), _dialogsMan(this,
	        getFeatures() & GF_VERSAILLES_AUDIOPADDING_YES),
	_musicVolumeFactor(1.), _musicCurrentFile(nullptr), _omni3dSpeed(0),
	_isPlaying(false), _isVisiting(false), _abortCommand(kAbortQuit),
	_currentPlaceId(uint(-1)), _nextPlaceId(uint(-1)), _currentLevel(uint(-1)),
	_countingDown(false), _countdownNextEvent(0),
	_currentSubtitleSet(nullptr), _currentSubtitle(nullptr) {
}

CryOmni3DEngine_Versailles::~CryOmni3DEngine_Versailles() {
	delete _currentWarpImage;
	delete[] _mainPalette;
	delete[] _cursorPalette;
	delete[] _transparentPaletteMap;

	delete _fixedImage;
}

bool CryOmni3DEngine_Versailles::hasFeature(EngineFeature f) const {
	return CryOmni3DEngine::hasFeature(f)
	       || (f == kSupportsSavingDuringRuntime)
	       || (f == kSupportsLoadingDuringRuntime);
}

void CryOmni3DEngine_Versailles::initializePath(const Common::FSNode &gamePath) {
	// Widescreen standalone: remember the game path so the in-game language
	// switch can (re)mount the per-language data overlay under lang/<code>/.
	_gamePath = gamePath;

	// This works if the user has installed the game as required in the Wiki
	SearchMan.addDirectory(gamePath, 0, 4, false);

	// CDs/DVD path
	SearchMan.addSubDirectoryMatching(gamePath, "datas_v", 0, 4, false);

	// If user has copied CDs in different directories
	SearchMan.addSubDirectoryMatching(gamePath, "datas_v1", 0, 4, false);
	SearchMan.addSubDirectoryMatching(gamePath, "datas_v2", 0, 4, false);

	// Default hard-drive path location on CD
	if (getPlatform() == Common::kPlatformMacintosh) {
		switch (getLanguage()) {
			case Common::DE_DEU:
				SearchMan.addSubDirectoryMatching(gamePath, "versailles ordner/datav_hd", 0, 3, false);
				break;
			case Common::EN_ANY:
				SearchMan.addSubDirectoryMatching(gamePath, "versailles folder/datav_hd", 0, 3, false);
				break;
			case Common::FR_FRA:
				SearchMan.addSubDirectoryMatching(gamePath, "dossier versailles/datav_hd", 0, 3, false);
				break;
			default:
				// We don't know any other variant but don't error out
				break;
		}
	} else {
		SearchMan.addSubDirectoryMatching(gamePath, "install/data", 0, 3, false);
		SearchMan.addSubDirectoryMatching(gamePath, "install/datas_v", 0, 3, false);
	}
}

Common::Error CryOmni3DEngine_Versailles::run() {
	CryOmni3DEngine::run();

	// Widescreen standalone: apply the persisted TEXT and AUDIO language choices
	// (independent) before loading any language-dependent data. The base
	// game_data is French; other languages are mounted as overlays under
	// lang/<code>/ (text_datasv + text_install for text, audio_datasv for audio).
	if (ConfMan.hasKey("versailles_language")) {
		setCurrentLanguage(parseLanguageCode(ConfMan.get("versailles_language"), getLanguage()));
	} else {
		// First launch: default the TEXT language to the PC's system language
		// (French/English/German/Chinese), falling back to English.
		setCurrentLanguage(osDefaultLanguage());
	}
	if (ConfMan.hasKey("versailles_audio_language")) {
		_audioLanguage = parseLanguageCode(ConfMan.get("versailles_audio_language"), _audioLanguage);
	} else {
		// Chinese has no dub, so its audio defaults to English; otherwise match
		// the text language.
		_audioLanguage = (getLanguage() == Common::ZH_TWN) ? Common::EN_ANY : getLanguage();
	}
	// Subtitles on by default (needed for cross-language play and the Chinese
	// text-only version), unless the player already chose otherwise. Stored in
	// the persistent application domain.
	if (!ConfMan.hasKey("subtitles", Common::ConfigManager::kApplicationDomain)) {
		ConfMan.setBool("subtitles", true, Common::ConfigManager::kApplicationDomain);
	}
	applyLanguageOverlays();

	// First thing, load all data that was originally in the executable
	// We don't need anything prepared for that
	loadStaticData();

	_dialogsMan.init(138, _messages[22]);
	// Keep voice-filename padding in sync with the (possibly persisted) audio language.
	_dialogsMan.setPadAudioFileName(_audioLanguage != Common::EN_ANY);
	_gameVariables.resize(GameVariables::kMax);
	_omni3dMan.init(75. / 180. * M_PI);

	_dialogsMan.loadGTO(getFilePath(kFileTypeGTO, _localizedFilenames[LocalizedFilenames::kDialogs]));
	setupDialogVariables();
	setupDialogShows();

	setupImgScripts();

	_mainPalette = new byte[3 * 256];
	setupFonts();
	setupSprites();
	loadCursorsPalette();

	// Objects need messages and sprites
	setupObjects();

	_transparentPaletteMap = new byte[256];
	_transparentSrcStart = 0;
	_transparentSrcStop = 240;
	_transparentDstStart = 0;
	_transparentDstStop = 248;
	_transparentNewStart = 248;
	_transparentNewStop = 254;

	// Inventory has a size of 50
	_inventory.init(50, new Common::Functor1Mem<uint, void, Toolbar>(&_toolbar,
	                &Toolbar::inventoryChanged));

	// Init toolbar after we have setup sprites and fonts
	_toolbar.init(&_sprites, &_fontManager, &_messages, &_inventory, this);

	_fixedImage = new ZonFixedImage(*this, _inventory, _sprites, &kFixedImageConfiguration);

	// Documentation is needed by no one at init time, let's do it last
	initDocPeopleRecord();
	_docManager.init(&_sprites, &_fontManager, &_messages, this,
	                 getFilePath(kFileTypeText, _localizedFilenames[LocalizedFilenames::kAllDocs]),
	                 getFilePath(kFileTypeText, getFeatures() & GF_VERSAILLES_LINK_LOCALIZED ?
	                 _localizedFilenames[LocalizedFilenames::kLinksDocs] :
	                 "lien_doc.txt"));

	_countdownSurface.create(40, 15, Graphics::PixelFormat::createFormatCLUT8());

	// Widescreen hor+ (VERSAILLES_STANDALONE): widen the screen to 864x480
	// (~16:9). The OMNI3D panorama fills the full width; 4:3 content
	// (menus, fixed images, videos, toolbar) is drawn centered (pillarboxed)
	// via copyRectToScreen2D() with this horizontal offset.
	initGraphics(864, 480);
	g_screen2DOffsetX = (864 - 640) / 2; // 112
	setMousePos(Common::Point(320, 200));

	// Bilinear filtering: default OFF. Honor the value the in-game toggle
	// persisted in the application domain (our settings live there because the
	// standalone runs in a transient game domain).
	{
		bool filt = ConfMan.hasKey("filtering", Common::ConfigManager::kApplicationDomain)
		            ? ConfMan.getBool("filtering") : false;
		g_system->beginGFXTransaction();
		g_system->setFeatureState(OSystem::kFeatureFilteringMode, filt);
		g_system->endGFXTransaction();
	}

	syncSoundSettings();

	_isPlaying = false;
	_isVisiting = false;

	int saveSlot = ConfMan.getInt("save_slot");

#if !defined(DEBUG_FAST_START) || DEBUG_FAST_START<1
	if (saveSlot == -1) {
		// Don't play introduction if loading directly a game
		playTransitionEndLevel(-2);
		if (shouldAbort()) {
			return Common::kNoError;
		}
		playTransitionEndLevel(-1);
		if (shouldAbort()) {
			return Common::kNoError;
		}
	}
#endif

	bool stopGame = false;
	while (!stopGame) {
		bool exitLoop = false;
		uint nextStep = 0;
		if (saveSlot > -1) {
			nextStep = 28;
			_loadedSave = saveSlot + 1;
			// Called in options
			syncOmni3DSettings();
		} else {
#if defined(DEBUG_FAST_START) && DEBUG_FAST_START>=2
			nextStep = 27;
			// Called in options
			syncOmni3DSettings();
#endif
		}
		setCursor(181);
		while (!exitLoop) {
			_isPlaying = false;
			if (!nextStep) {
				nextStep = displayOptions();
			}
			if (nextStep == 40) {
				// Quit action
				exitLoop = true;
				stopGame = true;
			} else if (nextStep == 27 || nextStep == 28 || nextStep == 65) {
				// New game, Load game, Next level
				if (nextStep == 27) {
					// New game
#if !defined(DEBUG_FAST_START) || DEBUG_FAST_START<1
					playTransitionEndLevel(0);
					if (shouldAbort()) {
						stopGame = true;
						exitLoop = true;
						break;
					}
#endif
					changeLevel(1);
				} else if (nextStep == 28) {
					// Load game
					loadGame(_isVisiting, _loadedSave);
				} else if (nextStep == 65) {
					changeLevel(_currentLevel + 1);
				}

				_isPlaying = true;
				_toolbar.setInventoryEnabled(!_isVisiting);
				nextStep = 0;
				_abortCommand = kAbortNoAbort;

				gameStep();

				// We shouldn't return from gameStep without an abort command
				assert(_abortCommand != kAbortNoAbort);
				switch (_abortCommand) {
				default:
					error("Invalid abortCommand: %d", _abortCommand);
					// Shouldn't return
					return Common::kUnknownError;
				case kAbortFinished:
				case kAbortGameOver:
					// Go back to menu
					exitLoop = true;
					break;
				case kAbortQuit:
					exitLoop = true;
					stopGame = true;
					break;
				case kAbortNewGame:
					nextStep = 27;
					break;
				case kAbortLoadGame:
					nextStep = 28;
					break;
				case kAbortNextLevel:
					nextStep = 65;
					break;
				}
			}
		}
	}
	return Common::kNoError;
}

bool CryOmni3DEngine_Versailles::shouldAbort() {
	if (g_engine->shouldQuit()) {
		_abortCommand = kAbortQuit;
		return true;
	}
	// If we are not playing _abortCommand isn't used
	// Even GMM can't load game when not playing
	return _isPlaying && _abortCommand != kAbortNoAbort;
}

static bool checkFilePath(const Common::Path &basePath, Common::String &baseName, const char * const *extensions, Common::Path &fullPath) {
	if (!extensions) {
		fullPath = basePath.appendComponent(baseName);
		debug(3, "Trying file %s", fullPath.toString(Common::Path::kNativeSeparator).c_str());
		return Common::File::exists(fullPath);
	}

	int extBegin = baseName.size();
	while (*extensions != nullptr) {
		baseName += *extensions;
		fullPath = basePath.appendComponent(baseName);
		debug(3, "Trying file %s", fullPath.toString(Common::Path::kNativeSeparator).c_str());
		if (Common::File::exists(fullPath)) {
			return true;
		}
		baseName.erase(extBegin);
		extensions++;
	}

	return false;
}

Common::Path CryOmni3DEngine_Versailles::getFilePath(FileType fileType,
		const Common::String &baseName) const {
	const char **extensions = nullptr;
	bool hasLevel = false;
	const char *baseDir = nullptr;

	switch (fileType) {
		case kFileTypeAnimacti:
		case kFileTypeDialAnim:
		case kFileTypeWarpHNM: {
			static const char *extensions_[] = { "hnm", nullptr };
			extensions = extensions_;
			break;
		}
		case kFileTypeTransScene: {
			static const char *extensions_[] = { "hns", "hnm", nullptr };
			extensions = extensions_;
			break;
		}
		case kFileTypeDocBg:
		case kFileTypeFixedImg:
		case kFileTypeMenu:
		case kFileTypeObject:
		case kFileTypeTransSceneI:
		case kFileTypeWarpCyclo: {
			static const char *extensions_[] = { "hlz", nullptr };
			extensions = extensions_;
			break;
		}
		case kFileTypeDialSound:
		case kFileTypeMusic:
		case kFileTypeSound: {
			static const char *extensions_[] = { "wav", nullptr };
			extensions = extensions_;
			break;
		}
		case kFileTypeFont:
		case kFileTypeGTO:
		case kFileTypeSaveGameVisit:
		case kFileTypeSprite:
		case kFileTypeSpriteBmp:
		case kFileTypeText:
		case kFileTypeWAM:
			break;
		default:
			error("Invalid file type");
	}

	switch (fileType) {
		case kFileTypeAnimacti:
			baseDir = "animacti/level%d";
			hasLevel = true;
			break;
		case kFileTypeDocBg:
			baseDir = "basedoc/fonds";
			break;
		case kFileTypeDialAnim:
			baseDir = "dial/flc_dial";
			break;
		case kFileTypeDialSound:
			baseDir = "dial/voix";
			break;
		case kFileTypeFixedImg:
			baseDir = "img_fix/level%d";
			hasLevel = true;
			break;
		case kFileTypeFont:
			baseDir = "fonts";
			break;
		case kFileTypeGTO:
			baseDir = "gto";
			break;
		case kFileTypeMenu:
			baseDir = "menu";
			break;
		case kFileTypeMusic:
			baseDir = "music";
			break;
		case kFileTypeObject:
			baseDir = "objets";
			break;
		case kFileTypeSaveGameVisit:
			baseDir = "savegame/visite";
			break;
		case kFileTypeTransSceneI:
		case kFileTypeTransScene:
			baseDir = "sc_trans";
			break;
		case kFileTypeSound:
			baseDir = "sound";
			break;
		case kFileTypeSprite:
			baseDir = "spr8col";
			break;
		case kFileTypeSpriteBmp:
			baseDir = "spr8col/bmpOK";
			break;
		case kFileTypeText:
			baseDir = "textes";
			break;
		case kFileTypeWAM:
			baseDir = "wam";
			break;
		case kFileTypeWarpCyclo:
			baseDir = "warp/level%d/cyclo";
			hasLevel = true;
			break;
		case kFileTypeWarpHNM:
			baseDir = "warp/level%d/hnm";
			hasLevel = true;
			break;
		default:
			error("Invalid file type");
	}

	Common::String baseName_(baseName);
	if ((getPlatform() != Common::kPlatformMacintosh) ||
		(getLanguage() != Common::FR_FRA)) {
		// Replace dashes by underscores for all versions except Mac FR
		char *p = baseName_.begin();
		while ((p = strchr(p, '-')) != nullptr) {
			*p = '_';
			p++;
		}
	}

	if (extensions) {
		// We will rewrite the extension: strip the provided one
		// or append the extension dot
		int lastDotPos = baseName_.findLastOf('.');
		if (lastDotPos > -1) {
			baseName_.erase(lastDotPos + 1);
		} else {
			baseName_ += ".";
		}
	}

	// We are ready, build path and check
	Common::Path basePath;
	if (hasLevel) {
		basePath = Common::String::format(baseDir, _currentLevel);
	} else {
		basePath = baseDir;
	}

	Common::Path fullPath;
	if (checkFilePath(basePath, baseName_, extensions, fullPath)) {
		return fullPath;
	}

	if (!hasLevel) {
		warning("Failed to find file %s in %s", baseName.c_str(), baseDir);
		return Common::Path(baseName);
	}

	assert(baseName_.size() > 0);
	if (baseName_[0] < '1' ||
			baseName_[0] > '7' ||
			uint(baseName_[0] - '0') == _currentLevel) {
		warning("Failed to find file %s in %s (level %d)", baseName.c_str(), baseDir, _currentLevel);
		return Common::Path(baseName);
	}

	int fileLevel = baseName_[0] - '0';
	basePath = Common::String::format(baseDir, fileLevel);

	if (checkFilePath(basePath, baseName_, extensions, fullPath)) {
		return fullPath;
	}

	warning("Failed to find file %s in %s (levels %d and %d)", baseName.c_str(), baseDir, _currentLevel, fileLevel);
	return Common::Path(baseName);
}

void CryOmni3DEngine_Versailles::setupFonts() {
	Common::Array<Common::Path> fonts;

	// Explainations below are based on original binaries, debug is not used in this engine
	// Fonts loaded are not always the same: FR Mac and EN DOS don't use the same font for debug doc/unused
	// The important is that the loaded one is present in all versions

#define ADD_FONT(f) fonts.push_back(getFilePath(kFileTypeFont, f))

	if (getLanguage() == Common::ZH_TWN) {
		ADD_FONT("tw13.CRF"); // 0: Doc titles
		ADD_FONT("tw18.CRF"); // 1: Menu and T0 in credits
		ADD_FONT("tw13.CRF"); // 2: T1 and T3 in credits
		ADD_FONT("tw12.CRF"); // 3: Menu title, options messages boxes buttons
		ADD_FONT("tw12.CRF"); // 4: T2 in credits, text in docs
		ADD_FONT("tw12.CRF"); // 5: objects description in toolbar, options messages boxes text, T4 in credits
		ADD_FONT("tw12.CRF"); // 6: T5 in credits, doc subtitle
		ADD_FONT("tw12.CRF"); // 7: dialogs texts
		ADD_FONT("tw12.CRF"); // 8: unused
		ADD_FONT("tw12.CRF"); // 9: Warp messages texts
		ADD_FONT("tw12.CRF"); // 10: debug

		_fontManager.loadFonts(fonts, Common::kWindows950);
		return;
	} else if (getLanguage() == Common::JA_JPN) {
		_fontManager.loadTTFList(getFilePath(kFileTypeFont, "FONTS_JP.LST"), Common::kWindows932);
		return;
	} else if (getLanguage() == Common::KO_KOR) {
		_fontManager.loadTTFList(getFilePath(kFileTypeFont, "FONTS_KR.LST"), Common::kWindows949);
		return;
	}

	// Code below is for SBCS encodings (ie. non CJK)
	uint8 fontsSet = getFeatures() & GF_VERSAILLES_FONTS_MASK;
	switch (fontsSet) {
	case GF_VERSAILLES_FONTS_NUMERIC:
		ADD_FONT("font01.CRF"); // 0: Doc titles
		ADD_FONT("font02.CRF"); // 1: Menu and T0 in credits
		ADD_FONT("font03.CRF"); // 2: T1 and T3 in credits
		ADD_FONT("font04.CRF"); // 3: Menu title, options messages boxes buttons
		ADD_FONT("font05.CRF"); // 4: T2 in credits, text in docs
		ADD_FONT("font06.CRF"); // 5: objects description in toolbar, options messages boxes text, T4 in credits
		ADD_FONT("font07.CRF"); // 6: T5 in credits, doc subtitle
		ADD_FONT("font08.CRF"); // 7: dialogs texts
		ADD_FONT("font09.CRF"); // 8: unused
		ADD_FONT("font10.CRF"); // 9: Warp messages texts
		ADD_FONT("font11.CRF"); // 10: debug
		break;
	case GF_VERSAILLES_FONTS_SET_A:
		ADD_FONT("garamB18.CRF"); // 0: Doc titles
		ADD_FONT("garamB22.CRF"); // 1: Menu and T0 in credits
		//ADD_FONT("geneva15.CRF");
		ADD_FONT("geneva14.CRF"); // 3: T1 and T3 in credits
		ADD_FONT("geneva13.CRF"); // 4: Menu title, options messages boxes buttons
		ADD_FONT("geneva12.CRF"); // 5: T2 in credits, text in docs
		ADD_FONT("geneva10.CRF"); // 6: objects description in toolbar, options messages boxes text, T4 in credits
		ADD_FONT("geneva9.CRF");  // 7: T5 in credits, doc subtitle
		//ADD_FONT("helvet24.CRF");
		ADD_FONT("helvet16.CRF"); // 9: dialogs texts
		//ADD_FONT("helvet14.CRF");
		//ADD_FONT("helvet13.CRF");
		//ADD_FONT("helvet12.CRF");
		//ADD_FONT("helvet11.CRF");
		//ADD_FONT("helvet9.CRF");
		//ADD_FONT("fruitL9.CRF");
		ADD_FONT("fruitL10.CRF"); // 16: debug doc
		//ADD_FONT("fruitL11.CRF");
		//ADD_FONT("fruitL12.CRF");
		//ADD_FONT("fruitL13.CRF");
		//ADD_FONT("fruitL14.CRF");
		//ADD_FONT("fruitL16.CRF");
		ADD_FONT("fruitL18.CRF"); // 22: Warp messages texts
		//ADD_FONT("arial11.CRF");
		ADD_FONT("MPW12.CRF");    // 24: debug
		//ADD_FONT("MPW9.CRF");

		// This file isn't even loaded by MacOS executable
		//ADD_FONT("garamB20.CRF");
		break;
	case GF_VERSAILLES_FONTS_SET_B:
		ADD_FONT("garamB18.CRF"); // 0: Doc titles
		ADD_FONT("garamB22.CRF"); // 1: Menu and T0 in credits
		ADD_FONT("geneva14.CRF"); // 2: T1 and T3 in credits
		ADD_FONT("geneva13.CRF"); // 3: Menu title, options messages boxes buttons
		ADD_FONT("geneva12.CRF"); // 4: T2 in credits, text in docs
		ADD_FONT("geneva10.CRF"); // 5: objects description in toolbar, options messages boxes text, T4 in credits
		ADD_FONT("geneva9.CRF");  // 6: T5 in credits, doc subtitle
		ADD_FONT("helvet16.CRF"); // 7: dialogs texts
		ADD_FONT("helvet12.CRF"); // 8: debug doc
		ADD_FONT("fruitL18.CRF"); // 9: Warp messages texts
		ADD_FONT("MPW12.CRF");    // 10: debug
		break;
	case GF_VERSAILLES_FONTS_SET_C:
		ADD_FONT("garamB18.CRF"); // 0: Doc titles
		ADD_FONT("garamB22.CRF"); // 1: Menu and T0 in credits
		ADD_FONT("geneva14.CRF"); // 2: T1 and T3 in credits
		ADD_FONT("geneva13.CRF"); // 3: Menu title, options messages boxes buttons
		ADD_FONT("helvet12.CRF"); // 4: T2 in credits, text in docs
		ADD_FONT("geneva10.CRF"); // 5: objects description in toolbar, options messages boxes text, T4 in credits
		ADD_FONT("geneva9.CRF");  // 6: T5 in credits, doc subtitle
		ADD_FONT("helvet16.CRF"); // 7: dialogs texts
		ADD_FONT("helvet12.CRF"); // 8: debug doc
		ADD_FONT("fruitL18.CRF"); // 9: Warp messages texts
		ADD_FONT("MPW12.CRF");    // 10: debug
		break;
	default:
		error("Font set invalid");
	}

#undef ADD_FONT

	// Use a SBCS codepage as a placeholder, we won't convert characters anyway
	_fontManager.loadFonts(fonts, Common::kWindows1250);
}

void CryOmni3DEngine_Versailles::setupSprites() {
	Common::File file;

	if (!file.open(getFilePath(kFileTypeSprite, "all_spr.bin"))) {
		// Try with TW specific file
		if (getLanguage() == Common::ZH_TWN &&
			!file.open(getFilePath(kFileTypeSprite, "allsprtw.bin"))) {
			error("Failed to open all_spr.bin and allsprtw.bin file");
		} else {
			error("Failed to open all_spr.bin file");
		}
	}
	_sprites.loadSprites(file);

	for (uint i = 0; i < _sprites.getSpritesCount(); i++) {
		const Graphics::Cursor &cursor = _sprites.getCursor(i);
		if (cursor.getWidth() != 32 || cursor.getHeight() != 32) {
			_sprites.setSpriteHotspot(i, 8, 8);
		} else {
			_sprites.setSpriteHotspot(i, 16, 16);
		}
	}
	_sprites.setupMapTable(kSpritesMapTable, kSpritesMapTableSize);


	_sprites.setSpriteHotspot(181, 4, 0);
	// Replace 2-keys, 3-keys and 4-keys icons with 1-key ones
	_sprites.replaceSprite(80, 64);
	_sprites.replaceSprite(84, 66);
	_sprites.replaceSprite(93, 78);
	_sprites.replaceSprite(97, 82);
	_sprites.replaceSprite(92, 64);
	_sprites.replaceSprite(96, 66);
	_sprites.replaceSprite(116, 78);
	_sprites.replaceSprite(121, 82);
	_sprites.replaceSprite(115, 64);
	_sprites.replaceSprite(120, 66);
	_sprites.replaceSprite(135, 78);
	_sprites.replaceSprite(140, 82);
}

void CryOmni3DEngine_Versailles::loadCursorsPalette() {
	Image::BitmapDecoder bmpDecoder;

	Common::File file;

	if (!file.open(getFilePath(kFileTypeSpriteBmp, "bou1_cA.bmp"))) {
		error("Failed to open BMP file");
	}

	if (!bmpDecoder.loadStream(file)) {
		error("Failed to load BMP file");
	}

	const Graphics::Palette &palette = bmpDecoder.getPalette();
	_cursorPalette = new byte[3 * palette.size()]();
	palette.grab(_cursorPalette, 0, palette.size());
}

void CryOmni3DEngine_Versailles::setupPalette(const byte *palette, uint start, uint num,
		bool commit) {
	memcpy(_mainPalette + 3 * start, palette, 3 * num);
	copySubPalette(_mainPalette, _cursorPalette, 240, 8);

	calculateTransparentMapping();
	if (commit) {
		setPalette(_mainPalette, 0, 256);
	}
}

void CryOmni3DEngine_Versailles::setMainPaletteColor(byte color, byte red, byte green, byte blue) {
	_mainPalette[3 * color + 0] = red;
	_mainPalette[3 * color + 1] = green;
	_mainPalette[3 * color + 2] = blue;
	setPalette(_mainPalette, 0, 256);
}

struct transparentScore {
	uint score;
	byte redScaled;
	byte greenScaled;

	int dScore(transparentScore &other) { return abs((int)score - (int)other.score); }
	int dRed(transparentScore &other) { return abs((int)redScaled - (int)other.redScaled); }
	int dGreen(transparentScore &other) { return abs((int)greenScaled - (int)other.greenScaled); }
};

static transparentScore transparentCalculateScore(byte red, byte green, byte blue) {
	transparentScore ret;
	ret.score = 10 * (blue + 3 * (red + 2 * green)) / 30;
	if (ret.score) {
		ret.redScaled = ((uint)red) * 256 / ret.score;
		ret.greenScaled = ((uint)green) * 256 / ret.score;
	} else {
		ret.redScaled = 0;
		ret.greenScaled = 0;
	}
	return ret;
}

void CryOmni3DEngine_Versailles::calculateTransparentMapping() {
	// Calculate colors proximity array
	transparentScore *proximities = new transparentScore[256];

	for (uint i = _transparentSrcStart; i < _transparentSrcStop; i++) {
		proximities[i] = transparentCalculateScore(_mainPalette[3 * i + 0], _mainPalette[3 * i + 1],
		                 _mainPalette[3 * i + 2]);
	}

	uint newColorsNextId = _transparentNewStart;
	//uint newColorsCount = 0;
	for (uint i = _transparentDstStart; i < _transparentDstStop; i++) {
		byte transparentRed = ((uint)_mainPalette[3 * i + 0]) * 60 / 128;
		byte transparentGreen = ((uint)_mainPalette[3 * i + 1]) * 50 / 128;
		byte transparentBlue = ((uint)_mainPalette[3 * i + 2]) * 35 / 128;

		// Find nearest color
		transparentScore newColorScore = transparentCalculateScore(transparentRed, transparentGreen,
		                                 transparentBlue);
		uint distanceMin = uint(-1);
		uint nearestId = uint(-1);
		for (uint j = _transparentSrcStart; j < _transparentSrcStop; j++) {
			if (j != i && newColorScore.dScore(proximities[j]) < 15) {
				uint distance = newColorScore.dRed(proximities[j]) + newColorScore.dGreen(proximities[j]);
				if (distance < distanceMin) {
					distanceMin = distance;
					nearestId = j;
				}
			}
		}

		if (nearestId == uint(-1)) {
			// Couldn't find a good enough color, try to create one
			if (_transparentNewStart != uint(-1) && newColorsNextId <= _transparentNewStop) {
				_mainPalette[3 * newColorsNextId + 0] = transparentRed;
				_mainPalette[3 * newColorsNextId + 1] = transparentGreen;
				_mainPalette[3 * newColorsNextId + 2] = transparentBlue;
				nearestId = newColorsNextId;

				//newColorsCount++;
				newColorsNextId++;
			}
		}

		if (nearestId == uint(-1)) {
			// Couldn't allocate a new color, return the original one
			nearestId = i;
		}

		_transparentPaletteMap[i] = nearestId;
	}

	delete[] proximities;
}

void CryOmni3DEngine_Versailles::makeTranslucent(Graphics::Surface &dst,
		const Graphics::Surface &src) const {
	assert(dst.w == src.w && dst.h == src.h);

	const byte *srcP = (const byte *) src.getPixels();
	byte *dstP = (byte *) dst.getPixels();
	for (int y = 0; y < dst.h; y++) {
		for (int x = 0; x < dst.w; x++) {
			dstP[x] = _transparentPaletteMap[srcP[x]];
		}
		dstP += dst.pitch;
		srcP += src.pitch;
	}
}

bool CryOmni3DEngine_Versailles::hasPlaceDocumentation() {
	return _placeStates[_currentPlaceId].docImage != nullptr;
}

bool CryOmni3DEngine_Versailles::displayPlaceDocumentation() {
	if (!_placeStates[_currentPlaceId].docImage) {
		return false;
	}

	_docManager.handleDocInGame(_placeStates[_currentPlaceId].docImage);
	return true;
}

void CryOmni3DEngine_Versailles::syncOmni3DSettings() {
	ConfMan.registerDefault("omni3d_speed", 0);
	int confOmni3DSpeed = ConfMan.getInt("omni3d_speed");
	if (confOmni3DSpeed == 0) {
		_omni3dSpeed = 0;
	} else if (confOmni3DSpeed == 1) {
		_omni3dSpeed = 2;
	} else if (confOmni3DSpeed == 2) {
		_omni3dSpeed = 4;
	} else if (confOmni3DSpeed == 3) {
		_omni3dSpeed = -1;
	} else if (confOmni3DSpeed == 4) {
		_omni3dSpeed = -2;
	} else {
		// Invalid value
		_omni3dSpeed = 0;
	}
}

void CryOmni3DEngine_Versailles::syncSoundSettings() {
	CryOmni3DEngine::syncSoundSettings();

	int soundVolumeMusic = int(ConfMan.getInt("music_volume") / _musicVolumeFactor);

	bool mute = false;
	if (ConfMan.hasKey("mute")) {
		mute = ConfMan.getBool("mute");
	}

	bool musicMute = mute  || ConfMan.getBool("music_mute");

	_mixer->muteSoundType(Audio::Mixer::kMusicSoundType, musicMute);
	_mixer->setVolumeForSoundType(Audio::Mixer::kMusicSoundType, soundVolumeMusic);
}

void CryOmni3DEngine_Versailles::playTransitionEndLevel(int level) {
	musicStop();
	_gameVariables[GameVariables::kWarnedIncomplete] = 0;

	Common::String video;

	unlockPalette();
	switch (level) {
	case -2:
		video = "logo.hnm";
		break;
	case -1:
		video = "a0_vf.hns";
		break;
	case 0:
		video = "a1_vf.hns";
		break;
	case 1:
		video = "a2_vf.hns";
		break;
	case 2:
		video = "a3_vf.hns";
		_inventory.removeByNameID(96);
		_inventory.removeByNameID(104);
		break;
	case 3:
		video = "a4_vf.hns";
		break;
	case 4:
		video = "a5_vf.hns";
		_inventory.removeByNameID(101);
		_inventory.removeByNameID(127);
		_inventory.removeByNameID(129);
		_inventory.removeByNameID(130);
		_inventory.removeByNameID(131);
		_inventory.removeByNameID(132);
		_inventory.removeByNameID(126);
		break;
	case 5:
		video = "a6_vf.hns";
		_inventory.removeByNameID(115);
		break;
	case 6:
		video = "a7_vf.hns";
		break;
	case 7:
		video = "a9_vf.hns";
		break;
	case 8:
		video = "a8_vf.hns";
		break;
	default:
		error("Invalid level : %d", level);
	}

	fadeOutPalette();
	if (shouldAbort()) {
		return;
	}

	fillSurface(0);

	// In original game the HNM player just doesn't render the cursor
	bool cursorWasVisible = showMouse(false);

	if (level == -2 && getLanguage() == Common::DE_DEU) {
		Common::Path ravensbgPath(getFilePath(kFileTypeTransSceneI, "RAVENSBG"));
		if (Common::File::exists(ravensbgPath)) {
			// Display one more copyright
			if (displayHLZ(ravensbgPath, 5000)) {
				clearKeys();
				fadeOutPalette();
				if (shouldAbort()) {
					return;
				}
				// Display back cursor there once the palette has been zeroed
				showMouse(cursorWasVisible);

				fillSurface(0);
				return;
			}
		}
	}

	playSubtitledVideo(video);

	clearKeys();
	if (shouldAbort()) {
		return;
	}

	fadeOutPalette();
	if (shouldAbort()) {
		return;
	}

	if (level == -2 && getLanguage() == Common::JA_JPN) {
		Common::Path jvcPath(getFilePath(kFileTypeTransScene, "jvclogo.hnm"));
		if (Common::File::exists(jvcPath)) {
			// Display one more copyright
			playHNM(jvcPath, Audio::Mixer::kMusicSoundType);
			clearKeys();
			if (shouldAbort()) {
				return;
			}

			fadeOutPalette();
			if (shouldAbort()) {
				return;
			}
		}
	}

	// Display back cursor there once the palette has been zeroed
	showMouse(cursorWasVisible);

	fillSurface(0);

	if (level == 7 || level == 8) {
		_abortCommand = kAbortFinished;
	} else {
		_abortCommand = kAbortNextLevel;
	}
}

void CryOmni3DEngine_Versailles::changeLevel(int level) {
	_currentLevel = level;

	musicStop();
	_mixer->stopAll();

	if (_currentLevel == 1) {
		_dialogsMan.reinitVariables();
		for (Common::Array<uint>::iterator it = _gameVariables.begin(); it != _gameVariables.end();
		        it++) {
			*it = 0;
		}
		initCountdown();
		_inventory.clear();
	} else if (_currentLevel > 7) {
		error("New level %d is not implemented", level);
	}

	_gameVariables[GameVariables::kCurrentTime] = 1;

	// keep back place state for level 2
	int place8_state_backup = -1;
	if (level == 2) {
		place8_state_backup = _placeStates[8].state;
	}
	_nextPlaceId = uint(-1);
	initNewLevel(_currentLevel);
	// restore place state for level 2
	if (level == 2) {
		_placeStates[8].state = place8_state_backup;
	}
}

void CryOmni3DEngine_Versailles::initNewLevel(int level) {
	if (level < 1 || level > 8 ||
	        (level == 8 && !_isVisiting)) {
		error("Invalid level %d", level);
	}

	// Level 7 starts countdown
	_countingDown = (level == 7);
	initPlacesStates();
	initWhoSpeaksWhere();
	setupLevelWarps(level);
	updateGameTimeDialVariables();
	_dialogsMan["{JOUEUR-ESSAYE-OUVRIR-PORTE-SALON}"] = 'Y';
	_dialogsMan["{JOUEUR-ESSAYE-OUVRIR-PORTE-CHAMBRE}"] = 'Y';
	setupLevelActionsMask();
}

void CryOmni3DEngine_Versailles::setupLevelWarps(int level) {
	Common::File wamFile;
	Common::Path wamPath = getFilePath(kFileTypeWAM, Common::String::format("level%d.wam", level));
	if (!wamFile.open(wamPath)) {
		error("Can't open WAM file '%s'", wamPath.toString(Common::Path::kNativeSeparator).c_str());
	}
	_wam.loadStream(wamFile);

	const LevelInitialState &initialState = kLevelInitialStates[level - 1];

	if (_nextPlaceId == uint(-1)) {
		_nextPlaceId = initialState.placeId;
	}
	_omni3dMan.setAlpha(initialState.alpha);
	_omni3dMan.setBeta(initialState.beta);
}

void CryOmni3DEngine_Versailles::setGameTime(uint newTime, uint level) {
	if (_currentLevel != level) {
		error("Level %u != current level %u", level, _currentLevel);
	}

	_gameVariables[GameVariables::kCurrentTime] = newTime;
	updateGameTimeDialVariables();

	if (_currentLevel == 1) {
		if (newTime == 2) {
			setPlaceState(1, 1);
			setPlaceState(2, 1);
			_whoSpeaksWhere[PlaceActionKey(2, 11201)] = "12E_HUI";
			setPlaceState(3, 1);
		} else if (newTime == 3) {
			setPlaceState(2, 2);
		}
	} else if (_currentLevel == 2) {
		if (newTime == 2) {
			setPlaceState(9, 1);
			_whoSpeaksWhere[PlaceActionKey(9, 52902)] = "22G_DAU";
		} else if (newTime == 4) {
			setPlaceState(10, 1);
			setPlaceState(11, 1);
			setPlaceState(12, 1);
			setPlaceState(13, 1);
		}
	} else if (_currentLevel == 3) {
		if (newTime == 2) {
			if (_placeStates[13].state) {
				setPlaceState(13, 3);
			} else {
				setPlaceState(13, 2);
			}
			setPlaceState(15, 1);
			setPlaceState(17, 1);
		} else if (newTime == 3) {
			setPlaceState(10, 1);
			setPlaceState(14, 1);
		}
	} else if (_currentLevel == 4) {
		if (newTime == 2) {
			setPlaceState(7, 1);
			setPlaceState(8, 1);
			setPlaceState(10, 1);
			setPlaceState(16, 1);
		} else if (newTime == 3) {
			setPlaceState(10, 2);
			setPlaceState(9, 1);
		} else if (newTime == 4) {
			setPlaceState(9, 2);
			_whoSpeaksWhere[PlaceActionKey(9, 54091)] = "4_MAI";
			_whoSpeaksWhere[PlaceActionKey(9, 14091)] = "4_MAI";
		}
	} else if (_currentLevel == 5) {
		if (newTime == 2) {
			setPlaceState(9, 1);
			setPlaceState(13, 1);
		} else if (newTime == 3) {
			if (!_placeStates[16].state) {
				setPlaceState(16, 2);
			}
		} else if (newTime == 4) {
			_whoSpeaksWhere[PlaceActionKey(9, 15090)] = "54I_BON";
		}
	} else if (_currentLevel == 6) {
		if (newTime == 2) {
			setPlaceState(14, 1);
			setPlaceState(19, 2);
		}
	}
}

void CryOmni3DEngine_Versailles::gameStep() {
	while (!_abortCommand) {
		if (_nextPlaceId != uint(-1)) {
			if (_placeStates[_nextPlaceId].initPlace) {
				(this->*_placeStates[_nextPlaceId].initPlace)();
			}
			doPlaceChange();
			musicUpdate();
		}
		if (_forcePaletteUpdate) {
			redrawWarp();
		}
		uint actionId = handleWarp();
		debug("handleWarp returned %u", actionId);
		// Don't handle keyboard for levels 4 and 5, it was a debug leftover

		// Get selected object there to detect when it has just been deselected
		Object *selectedObject = _inventory.selectedObject();

		_nextPlaceId = uint(-1);
		bool doEvent;
		if (_placeStates[_currentPlaceId].filterEvent && !_isVisiting) {
			doEvent = (this->*_placeStates[_currentPlaceId].filterEvent)(&actionId);
		} else {
			doEvent = true;
		}

		if (_abortCommand) {
			break;
		}

		if (!selectedObject) {
			// Execute only when no object was used before filtering event
			if (actionId >= 1 && actionId < 10000) {
				// Move to another place
				if (doEvent) {
					executeTransition(actionId);
				}
			} else if (actionId >= 10000 && actionId < 20000) {
				// Speak
				if (doEvent) {
					executeSpeakAction(actionId);
					// Force refresh of place
					if (_nextPlaceId == uint(-1)) {
						_nextPlaceId = _currentPlaceId;
					}
				}
			} else if (actionId >= 20000 && actionId < 30000) {
				// Documentation
				executeDocAction(actionId);
			} else if (actionId >= 30000 && actionId < 40000) {
				// Use
				// In original game there is a handler for use actions but it's
				// only for some events, we will implement them in the filterEvent handler
				if (doEvent) {
					error("Not implemented yet");
				}
			} else if (actionId >= 40000 && actionId < 50000) {
				// See
				executeSeeAction(actionId);
			} else if (actionId >= 50000 && actionId < 60000) {
				// Listening
				// never filtered
				executeSpeakAction(actionId);
				// Force refresh of place
				if (_nextPlaceId == uint(-1)) {
					_nextPlaceId = _currentPlaceId;
				}
			} else if (actionId == 66666) {
				// Abort
				assert(_abortCommand != kAbortNoAbort);
				return;
			}
		} else if (!actionId) {
			// Click on nothing with an object: deselect it
			_inventory.setSelectedObject(nullptr);
		}
	}
}

void CryOmni3DEngine_Versailles::doGameOver() {
	musicStop();
	fadeOutPalette();
	fillSurface(0);
	// This test is not really relevant because it's for 2CDs edition but let's follow the code
	if (_currentLevel < 4) {
		playInGameAnimVideo("1gameove");
	} else {
		playInGameAnimVideo("4gameove");
	}
	fillSurface(0);
	_abortCommand = kAbortGameOver;
}

void CryOmni3DEngine_Versailles::doPlaceChange() {
	const Place *nextPlace = _wam.findPlaceById(_nextPlaceId);
	uint state = _placeStates[_nextPlaceId].state;
	if (state == uint(-1)) {
		state = 0;
	}

	if (state >= nextPlace->warps.size()) {
		error("invalid warp %d/%d/%d", _currentLevel, _nextPlaceId, state);
	}

	Common::String warpFile = nextPlace->warps[state];
	warpFile.toUppercase();
	if (warpFile.size() > 0) {
		if (warpFile.hasPrefix("NOT_MOVE")) {
			// Don't move so do nothing but cancel place change
			_nextPlaceId = uint(-1);
		} else {
			_currentPlace = nextPlace;
			if (!warpFile.hasPrefix("NOT_STOP")) {
				if (_currentWarpImage) {
					delete _currentWarpImage;
				}
				debug("Loading warp %s", warpFile.c_str());
				_currentWarpImage = loadHLZ(getFilePath(kFileTypeWarpCyclo, warpFile));
				if (!_currentWarpImage) {
					error("Can't load warp %s", warpFile.c_str());
				}
#if 0
				// This is not correct but to debug zones I think it's OK
				Graphics::Surface *tmpSurf = (Graphics::Surface *) _currentWarpImage->getSurface();
				for (Common::Array<Zone>::const_iterator it = _currentPlace->zones.begin();
				        it != _currentPlace->zones.end(); it++) {
					Common::Rect tmp = it->rct;
					tmp.bottom = tmpSurf->h - it->rct.top;
					tmp.top = tmpSurf->h - it->rct.bottom;
					tmpSurf->frameRect(tmp, 244);
				}
#endif
				_currentPlace->setupWarpConstraints(_omni3dMan);
				_omni3dMan.setSourceSurface(_currentWarpImage->getSurface());

				setupPalette(_currentWarpImage->getPalette().data(), 0,
				             _currentWarpImage->getPalette().size(), !_fadedPalette);

				setMousePos(Common::Point(320, 240)); // Center of screen

				_currentPlaceId = _nextPlaceId;
				_nextPlaceId = uint(-1);
			}
		}
	} else {
		error("invalid warp %d/%d/%d", _currentLevel, _nextPlaceId, state);
	}
}

void CryOmni3DEngine_Versailles::setPlaceState(uint placeId, uint newState) {
	uint numStates = _wam.findPlaceById(placeId)->getNumStates();
	uint oldState = _placeStates[placeId].state;

	if (newState > numStates) {
		warning("CryOmni3DEngine_Versailles::setPlaceState: newState '%d' > numStates '%d'",
		        newState, numStates);
		return;
	}
	_placeStates[placeId].state = newState;

	if (_currentPlaceId == placeId && oldState != newState) {
		// force reload of the place
		_nextPlaceId = _currentPlaceId;
	}
}

void CryOmni3DEngine_Versailles::executeTransition(uint nextPlaceId) {
	const Transition *transition;
	uint animationId = determineTransitionAnimation(_currentPlaceId, nextPlaceId, &transition);

	_nextPlaceId = nextPlaceId;

	Common::String animation = (animationId == uint(-1)) ? "" : transition->animations[animationId];
	animation.toUppercase();
	debug("Transition animation: %s", animation.c_str());
	if (animation.hasPrefix("NOT_FLI")) {
		return;
	}

	if (_transitionAnimateWarp) {
		animateWarpTransition(transition);
	} else {
		_transitionAnimateWarp = true;
	}
	if (musicWouldChange(_currentLevel, _nextPlaceId)) {
		musicStop();
	}
	if (animation.hasPrefix("FADE_PAL")) {
		_fadedPalette = true;
		fadeOutPalette();
	} else if (animation != "") {
		_fadedPalette = false;
		// Normally transitions don't overwrite the cursors colors and game doesn't restore palette
		playInGameVideo(getFilePath(kFileTypeWarpHNM, animation), false);
	}

	_omni3dMan.setAlpha(transition->dstAlpha);
	_omni3dMan.setBeta(-transition->dstBeta);

	uint nextState = _placeStates[nextPlaceId].state;
	if (nextState == uint(-1)) {
		nextState = 0;
	}
	const Place *nextPlace = _wam.findPlaceById(nextPlaceId);
	Common::String warpFile = nextPlace->warps[nextState];
	warpFile.toUppercase();
	if (warpFile.hasPrefix("NOT_STOP")) {
		debug("Got not stop");
		uint transitionNum;
		// Determine transition to take
		if (nextPlace->getNumTransitions() == 1) {
			// Only one
			transitionNum = 0;
		} else if (nextPlace->findTransition(_currentPlaceId) == &nextPlace->transitions[0]) {
			// Don't take the transition returning to where we come from
			transitionNum = 1;
		} else {
			transitionNum = 0;
		}
		uint nextNextPlaceId = nextPlace->transitions[transitionNum].dstId;

		animationId = determineTransitionAnimation(nextPlaceId, nextNextPlaceId, &transition);
		animation = (animationId == uint(-1)) ? "" : transition->animations[animationId];
		animation.toUppercase();

		debug("Transition animation: %s", animation.c_str());
		if (animation.hasPrefix("NOT_FLI")) {
			return;
		}
		if (animation.hasPrefix("FADE_PAL")) {
			_fadedPalette = true;
			fadeOutPalette();
		} else if (animation != "") {
			_fadedPalette = false;
			// Normally transitions don't overwrite the cursors colors and game doesn't restore palette
			playInGameVideo(getFilePath(kFileTypeWarpHNM, animation), false);
		}

		_nextPlaceId = nextNextPlaceId;

		_omni3dMan.setAlpha(transition->dstAlpha);
		_omni3dMan.setBeta(-transition->dstBeta);
	}
}

void CryOmni3DEngine_Versailles::fakeTransition(uint dstPlaceId) {
	// No need of animation, caller will take care
	// We just setup the camera in good place for the caller
	const Place *srcPlace = _wam.findPlaceById(_currentPlaceId);
	const Transition *transition = srcPlace->findTransition(dstPlaceId);

	animateWarpTransition(transition);

	_omni3dMan.setAlpha(transition->dstAlpha);
	_omni3dMan.setBeta(-transition->dstBeta);
}

uint CryOmni3DEngine_Versailles::determineTransitionAnimation(uint srcPlaceId,
		uint dstPlaceId, const Transition **transition_) {
	const Place *srcPlace = _wam.findPlaceById(srcPlaceId);
	const Place *dstPlace = _wam.findPlaceById(dstPlaceId);
	const Transition *transition = srcPlace->findTransition(dstPlaceId);

	if (transition_) {
		*transition_ = transition;
	}

	uint srcNumStates = srcPlace->getNumStates();
	uint dstNumStates = dstPlace->getNumStates();
	uint animsNum = transition->getNumAnimations();

	uint srcState = _placeStates[srcPlaceId].state;
	uint dstState = _placeStates[dstPlaceId].state;

	if (srcState >= srcNumStates) {
		error("Invalid src state");
	}

	if (dstState >= dstNumStates) {
		error("Invalid dst state");
	}

	if (animsNum == 0) {
		return uint(-1);
	}

	if (animsNum == 1) {
		return 0;
	}

	if (srcNumStates == 2 && dstNumStates == 2) {
		if (animsNum == 2) {
			return dstState;
		} else if (animsNum == 4) {
			return srcState * 2 + dstState;
		}
	}

	if (animsNum == dstNumStates) {
		return dstState;
	}

	if (animsNum == srcNumStates) {
		return srcState;
	}

	// Other case
	return 0;
}

int CryOmni3DEngine_Versailles::handleWarp() {
	bool exit = false;
	bool leftButtonPressed = false;
	bool firstDraw = true;
	bool moving = true;
	uint actionId = uint(-1);
	showMouse(true);
	_canLoadSave = true;
	while (!leftButtonPressed && !exit) {
		int xDelta = 0, yDelta = 0;
		uint movingCursor = uint(-1);

		pollEvents();
		// The OMNI3D panorama spans the full physical width: use raw coords.
		Common::Point mouse = getRawMousePos();
		// Right-edge scroll threshold scales with the physical width (width-100).
		const int rightEdge = g_system->getWidth() - 100;

		if (mouse.y < 100) {
			movingCursor = 245;
			yDelta = 100 - mouse.y;
		} else if (mouse.y > 380) {
			movingCursor = 224;
			yDelta = 380 - mouse.y;
		}
		if (mouse.x < 100) {
			movingCursor = 241;
			xDelta = 100 - mouse.x;
		} else if (mouse.x > rightEdge) {
			movingCursor = 228;
			xDelta = rightEdge - mouse.x;
		}
		if (_omni3dSpeed > 0) {
			xDelta <<= _omni3dSpeed;
			yDelta <<= _omni3dSpeed;
		} else if (_omni3dSpeed < 0) {
			xDelta >>= -_omni3dSpeed;
			yDelta >>= -_omni3dSpeed;
		}
		// This correction factor is to slow down movements for modern CPUs
		xDelta /= 5;
		yDelta /= 5;
		leftButtonPressed = (getCurrentMouseButton() == 1);

		Common::Point mouseRev = _omni3dMan.mapMouseCoords(mouse);
		mouseRev.y = 768 - mouseRev.y;

		actionId = _currentPlace->hitTest(mouseRev);

		exit = handleWarpMouse(&actionId, movingCursor);
		if (shouldAbort()) {
			// We abort if we quit or if we load from GMM
			exit = true;
		}
		if (exit) {
			actionId = 66666;
		}

		if (firstDraw || xDelta || yDelta || _omni3dMan.hasSpeed()) {
			bool useOldSpeed = false;
			if (_omni3dSpeed <= 2) {
				useOldSpeed = true;
			}
			_omni3dMan.updateCoords(xDelta, -yDelta, useOldSpeed);

			const Graphics::Surface *result = _omni3dMan.getSurface();
			g_system->copyRectToScreen(result->getPixels(), result->pitch, 0, 0, result->w, result->h);
			if (!exit) {
				drawCountdown();
				g_system->updateScreen();
				if (firstDraw && _fadedPalette) {
					fadeInPalette(_mainPalette);
					_fadedPalette = false;
				}
			}
			moving = true;
			firstDraw = false;
		} else if (moving) {
			const Graphics::Surface *result = _omni3dMan.getSurface();
			g_system->copyRectToScreen(result->getPixels(), result->pitch, 0, 0, result->w, result->h);
			if (!exit) {
				drawCountdown();
				g_system->updateScreen();
			}
			moving = false;
		} else {
			if (!exit) {
				// Countdown is updated as soon as it changes
				g_system->updateScreen();
			}
		}

		// Slow down loop but after updating screen
		g_system->delayMillis(10);
	}
	_canLoadSave = false;
	showMouse(false);
	return actionId;
}

bool CryOmni3DEngine_Versailles::handleWarpMouse(uint *actionId,
		uint movingCursor) {
	fixActionId(actionId);

	if (getCurrentMouseButton() == 2 ||
	        getNextKey().keycode == Common::KEYCODE_SPACE) {
		// Prepare background using alpha
		const Graphics::Surface *original = _omni3dMan.getSurface();

		// Display surface with countdown before showing toolbar just to be sure
		g_system->copyRectToScreen(original->getPixels(), original->pitch, 0, 0, original->w, original->h);
		drawCountdown();

		// Fade in palette to avoid displaying toolbar on a black screen
		if (_fadedPalette) {
			fadeInPalette(_mainPalette);
			_fadedPalette = false;
		}

		bool mustRedraw = displayToolbar(original);
		// Don't redraw if we abort game
		if (shouldAbort()) {
			return true;
		}
		if (mustRedraw) {
			_forceRedrawWarp = true;
			redrawWarp();
		}
		// Force a cycle to recalculate the correct mouse cursor
		return false;
	}

	if (countDown()) {
		// Time has changed: need redraw
		// Don't redraw if we abort game
		if (shouldAbort()) {
			return true;
		}

		_forceRedrawWarp = true;
		redrawWarp();
	}

	const Object *selectedObj = _inventory.selectedObject();
	if (selectedObj) {
		if (*actionId != 0) {
			setCursor(selectedObj->idSA());
		} else {
			setCursor(selectedObj->idSl());
		}
	} else if (*actionId >= 1 && *actionId < 10000) {
		setCursor(243);
	} else if (*actionId >= 10000 && *actionId < 20000) {
		setCursor(113);
	} else if (*actionId >= 20000 && *actionId < 30000) {
		setCursor(198);
	} else if (*actionId >= 30000 && *actionId < 40000) {
		setCursor(99);
	} else if (*actionId >= 40000 && *actionId < 50000) {
		setCursor(145);
	} else if (*actionId >= 50000 && *actionId < 60000) {
		setCursor(136);
	} else if (movingCursor != uint(-1)) {
		setCursor(movingCursor);
	} else {
		setCursor(45);
	}
	return false;
}

void CryOmni3DEngine_Versailles::fixActionId(uint *actionId) const {
	PlaceStateActionKey mask = PlaceStateActionKey(_currentPlaceId, _placeStates[_currentPlaceId].state,
	                           *actionId);
	Common::HashMap<PlaceStateActionKey, uint>::const_iterator it = _actionMasks.find(mask);
	if (it != _actionMasks.end()) {
		*actionId = it->_value;
		return;
	}

	// Special case for level 3 taking dialogs variables into account
	if (_currentLevel == 3) {
		if (_dialogsMan["{LE JOUEUR-A-TENTE-OUVRIR-PETITE-PORTE}"] == 'N') {
			if (*actionId == 13060) {
				*actionId = 23060;
			} else if (*actionId == 13100) {
				if (currentGameTime() != 4) {
					*actionId = 23100;
				}
			} else if (*actionId == 13130) {
				*actionId = 23130;
			} else if (*actionId == 13150) {
				*actionId = 23150;
			}
		} else if (_dialogsMan["{JOUEUR-POSSEDE-CLE}"] == 'Y') {
			if (*actionId == 13100) {
				if (currentGameTime() != 4) {
					*actionId = 23100;
				}
			} else if (*actionId == 13130) {
				*actionId = 23130;
			} else if (*actionId == 13150) {
				*actionId = 23150;
			}

		}
	}
}

void CryOmni3DEngine_Versailles::animateWarpTransition(const Transition *transition) {
	double srcAlpha = transition->srcAlpha;
	double srcBeta = transition->srcBeta;

	double oldDeltaAlpha = 1000., oldDeltaBeta = 1000.;

	clearKeys();

	bool exit = false;
	while (!exit) {
		double deltaAlpha = 2.*M_PI - srcAlpha + _omni3dMan.getAlpha();
		if (deltaAlpha >= 2.*M_PI) {
			deltaAlpha -= 2.*M_PI;
		} else if (deltaAlpha < 0) {
			deltaAlpha += 2.*M_PI;
		}

		// We divide by 5 to slow down movement for modern CPUs
		int deltaAlphaI;
		if (deltaAlpha < M_PI) {
			deltaAlphaI = int(-(deltaAlpha * 512. / 5.));
		} else {
			deltaAlphaI = int((2.*M_PI - deltaAlpha) * 512. / 5.);
		}

		double deltaBeta = -srcBeta - _omni3dMan.getBeta();
		int deltaBetaI = int(-(deltaBeta * 512. / 5.));

		if (_omni3dSpeed > 0) {
			deltaAlphaI <<= 2;
			deltaBetaI <<= 2;
		} else if (_omni3dSpeed < 0) {
			deltaAlphaI >>= 2;
			deltaBetaI >>= 2;
		}

		_omni3dMan.updateCoords(deltaAlphaI, -deltaBetaI, false);

		const Graphics::Surface *result = _omni3dMan.getSurface();
		g_system->copyRectToScreen(result->getPixels(), result->pitch, 0, 0, result->w, result->h);
		drawCountdown();
		g_system->updateScreen();

		// Slow down transition
		g_system->delayMillis(10);

		if (fabs(oldDeltaAlpha - deltaAlpha) < 0.001 && fabs(oldDeltaBeta - deltaBeta) < 0.001) {
			exit = true;
		}
		oldDeltaAlpha = deltaAlpha;
		oldDeltaBeta = deltaBeta;

		if (pollEvents() && checkKeysPressed(2, Common::KEYCODE_ESCAPE, Common::KEYCODE_SPACE)) {
			exit = true;
		}
	}
}

void CryOmni3DEngine_Versailles::redrawWarp() {
	setupPalette(_currentWarpImage->getPalette().data(), 0,
	             _currentWarpImage->getPalette().size(), true);
	if (_forceRedrawWarp) {
		const Graphics::Surface *result = _omni3dMan.getSurface();
		g_system->copyRectToScreen(result->getPixels(), result->pitch, 0, 0, result->w, result->h);
		drawCountdown();
		g_system->updateScreen();
		_forceRedrawWarp = false;
	}
	_forcePaletteUpdate = false;
}

void CryOmni3DEngine_Versailles::warpMsgBoxCB() {
	pollEvents();

	g_system->updateScreen();
	g_system->delayMillis(10);
}

void CryOmni3DEngine_Versailles::animateCursor(const Object *obj) {
	if (obj == nullptr) {
		return;
	}

	bool cursorWasVisible = showMouse(true);

	for (uint i = 4; i > 0; i--) {
		// Wait 100ms
		for (uint j = 10; j > 0; j--) {
			pollEvents();
			g_system->updateScreen();
			g_system->delayMillis(10);
		}
		setCursor(obj->idSA());
		g_system->updateScreen();
		// Wait 100ms
		for (uint j = 10; j > 0; j--) {
			pollEvents();
			g_system->updateScreen();
			g_system->delayMillis(10);
		}
		setCursor(obj->idSl());
		g_system->updateScreen();
	}

	showMouse(cursorWasVisible);
}

void CryOmni3DEngine_Versailles::collectObject(Object *obj, const ZonFixedImage *fimg,
		bool showObject) {
	_inventory.add(obj);
	Object::ViewCallback cb = obj->viewCallback();
	if (showObject && cb) {
		(*cb)();
		if (fimg) {
			fimg->display();
		} else {
			_forceRedrawWarp = true;
			redrawWarp();
		}
	}
	animateCursor(obj);
}

void CryOmni3DEngine_Versailles::displayObject(const Common::String &imgName,
		DisplayObjectHook hook) {
	Image::ImageDecoder *imageDecoder = loadHLZ(getFilePath(kFileTypeObject, imgName));
	if (!imageDecoder) {
		error("Can't display object");
	}

	if (imageDecoder->hasPalette()) {
		// We don't need to calculate transparency but it's simpler to call this function
		setupPalette(imageDecoder->getPalette().data(), 0,
		             imageDecoder->getPalette().size());
	}

	const Graphics::Surface *image = imageDecoder->getSurface();

	// Duplicate image to let hook modify it
	Graphics::ManagedSurface dstSurface(image->w, image->h, image->format);
	dstSurface.blitFrom(*image);

	delete imageDecoder;
	imageDecoder = nullptr;

	if (hook) {
		(this->*hook)(dstSurface);
	}

	copyRectToScreen2D(dstSurface.getPixels(), dstSurface.pitch, 0, 0,
	                           dstSurface.w, dstSurface.h);
	g_system->updateScreen();

	setMousePos(Common::Point(320, 240)); // Center of screen
	setCursor(181);

	bool cursorWasVisible = showMouse(true);

	bool exitImg = false;
	while (!shouldAbort() && !exitImg) {
		if (pollEvents()) {
			if (getCurrentMouseButton() == 1) {
				exitImg = true;
			}
		}
		g_system->updateScreen();
		g_system->delayMillis(10);
	}
	waitMouseRelease();
	clearKeys();

	showMouse(cursorWasVisible);
	setMousePos(Common::Point(320, 240)); // Center of screen
}

void CryOmni3DEngine_Versailles::executeSeeAction(uint actionId) {
	if (_currentLevel == 7 && _currentPlaceId != 20) {
		// Don't display fixed images unless it's the bomb
		// Not enough time for paintings
		displayMessageBoxWarp(14);
		return;
	}

	const FixedImgCallback cb = _imgScripts.getVal(actionId);
	handleFixedImg(cb);
}

void CryOmni3DEngine_Versailles::executeSpeakAction(uint actionId) {
	PlaceActionKey key(_currentPlaceId, actionId);
	Common::HashMap<PlaceActionKey, Common::String>::iterator it = _whoSpeaksWhere.find(key);
	showMouse(true);
	bool doneSth = false;
	if (it != _whoSpeaksWhere.end()) {
		doneSth = _dialogsMan.play(it->_value);
	}
	showMouse(false);
	_forcePaletteUpdate = true;
	if (doneSth) {
		setMousePos(Common::Point(320, 240)); // Center of screen
	}
}

void CryOmni3DEngine_Versailles::executeDocAction(uint actionId) {
	if (_currentLevel == 7) {
		// Not enough time for doc
		displayMessageBoxWarp(13);
		return;
	}

	Common::HashMap<uint, const char *>::iterator it = _docPeopleRecord.find(actionId);
	if (it == _docPeopleRecord.end() || !it->_value) {
		warning("Missing documentation record for action %u", actionId);
		return;
	}

	_docManager.handleDocInGame(it->_value);

	_forcePaletteUpdate = true;
	setMousePos(Common::Point(320, 240)); // Center of screen
}

void CryOmni3DEngine_Versailles::handleFixedImg(const FixedImgCallback &callback) {
	if (!callback) {
		return;
	}

	// Fixed images / close-ups (paintings, objects). Static, so ambient uses
	// solid colour bars. Default mode = ambient.
	Screen2DBarModeGuard _barMode(barModeForCategory("bars_fixedimage", kScreen2DBarModeAmbient), true);

	ZonFixedImage::CallbackFunctor *functor =
	    new Common::Functor1Mem<ZonFixedImage *, void, CryOmni3DEngine_Versailles>(this, callback);
	_fixedImage->run(functor);
	// functor is deleted in ZoneFixedImage
	functor = nullptr;

	if (_nextPlaceId == uint(-1)) {
		_forcePaletteUpdate = true;
	}
}

uint CryOmni3DEngine_Versailles::getFakeTransition(uint actionId) const {
	for (const FakeTransitionActionPlace *ft = kFakeTransitions; ft->actionId != 0; ft++) {
		if (ft->actionId == actionId) {
			return ft->placeId;
		}
	}
	return 0;
}

void CryOmni3DEngine_Versailles::playInGameVideo(const Common::Path &filename,
		bool restoreCursorPalette) {
	if (!_isPlaying) {
		return;
	}

	if (restoreCursorPalette) {
		// WORKAROUND: Don't mess with mouse when not restoring cursors palette
		showMouse(false);
	}
	lockPalette(0, 241);
	// In-game videos = room transitions / character animations. Default to the
	// ambient blurred side bars (user-configurable per category).
	Screen2DBarModeGuard _barMode(barModeForCategory("bars_transition", kScreen2DBarModeAmbient));
	// Videos are like music because if you mute music in game it will mute videos soundtracks
	playHNM(filename, Audio::Mixer::kMusicSoundType, nullptr,
	        static_cast<HNMCallback>(&CryOmni3DEngine_Versailles::drawCountdownVideo));
	clearKeys();
	unlockPalette();
	if (restoreCursorPalette) {
		// Restore cursors colors as 2 first ones may have been erased by the video
		setPalette(&_cursorPalette[3 * 240], 240, 8);
		// WORKAROUND: Don't mess with mouse when not restoring cursors palette
		showMouse(true);
	}
}

void CryOmni3DEngine_Versailles::playSubtitledVideo(const Common::String &filename) {
	Common::HashMap<Common::String, Common::Array<SubtitleEntry> >::const_iterator it;

	// Cinematics. Default = ambient bars; the intro logos (static) use solid
	// colour bars, the motion videos use the blurred ambient bars.
	Common::String lf = filename;
	lf.toLowercase();
	bool isLogo = lf.contains("logo");
	Screen2DBarModeGuard _barMode(barModeForCategory("bars_cinematic", kScreen2DBarModeAmbient), isLogo);

	if (!showSubtitles() ||
	        (it = _subtitles.find(filename)) == _subtitles.end() ||
	        it->_value.size() == 0) {
		// No subtitle, don't try to handle them frame by frame
		// Videos are like music because if you mute music in game it will mute videos soundtracks
		playHNM(getFilePath(kFileTypeTransScene, filename), Audio::Mixer::kMusicSoundType);
		return;
	}

	// Keep 2 colors for background and text
	setPalette(&_cursorPalette[3 * 242], 254, 1);
	setPalette(&_cursorPalette[3 * 247], 255, 1);
	lockPalette(0, 253);

	_currentSubtitleSet = &it->_value;
	_currentSubtitle = _currentSubtitleSet->begin();

	_fontManager.setCurrentFont(8);
	_fontManager.setTransparentBackground(true);
	_fontManager.setForeColor(254u);
	_fontManager.setLineHeight(22);
	_fontManager.setSpaceWidth(2);
	_fontManager.setCharSpacing(1);

	// Videos are like music because if you mute music in game it will mute videos soundtracks
	playHNM(getFilePath(kFileTypeTransScene, filename), Audio::Mixer::kMusicSoundType,
	        static_cast<HNMCallback>(&CryOmni3DEngine_Versailles::drawVideoSubtitles), nullptr);

	clearKeys();
	unlockPalette();
}

void CryOmni3DEngine_Versailles::drawVideoSubtitles(uint frameNum) {
	if (_currentSubtitle == _currentSubtitleSet->end()) {
		// No next subtitle to draw, just return
		return;
	}

	if (frameNum < _currentSubtitle->frameStart) {
		// Not yet the good frame, just return
		return;
	}

	const Common::String &text = _currentSubtitle->text;
	_currentSubtitle++;

	if (text.size() == 0) {
		// Empty text, reset clipping
		unsetHNMClipping();
		return;
	}

	uint lines = _fontManager.getLinesCount(text, 640 - 8);
	uint top = 480 - (2 * 4) - _fontManager.lineHeight() * lines;

	Graphics::ManagedSurface tmp(640, 480 - top, Graphics::PixelFormat::createFormatCLUT8());

	tmp.clear(255u);

	_fontManager.setSurface(&tmp);
	_fontManager.setupBlock(Common::Rect(4, 4, tmp.w - 4,
	                                     tmp.h - 4)); // +1 because bottom,right is excluded

	_fontManager.displayBlockText(text);

	// Enable clipping to avoid refreshing text at every frame
	setHNMClipping(Common::Rect(0, 0, 640, top));

	copyRectToScreen2D(tmp.getPixels(), tmp.pitch, 0, top, tmp.w, tmp.h);
	g_system->updateScreen();
}

void CryOmni3DEngine_Versailles::loadBMPs(const char *pattern, Graphics::Surface *bmps,
		uint count) {
	Image::BitmapDecoder bmpDecoder;
	Common::File file;

	for (uint i = 0; i < count; i++) {
		Common::Path bmp = getFilePath(kFileTypeSpriteBmp, Common::String::format(pattern, i));

		if (!file.open(bmp)) {
			error("Failed to open BMP file: %s", bmp.toString(Common::Path::kNativeSeparator).c_str());
		}
		if (!bmpDecoder.loadStream(file)) {
			error("Failed to load BMP file: %s", bmp.toString(Common::Path::kNativeSeparator).c_str());
		}
		bmps[i].copyFrom(*bmpDecoder.getSurface());
		bmpDecoder.destroy();
		file.close();
	}
}

// ---------------------------------------------------------------------------
// Widescreen standalone: in-game language switch
// ---------------------------------------------------------------------------

// Find a child node by name, case-insensitively (folder names may differ in
// case across platforms/dumps).
static Common::FSNode childCaseless(const Common::FSNode &parent, const Common::String &name) {
	Common::FSNode direct = parent.getChild(name);
	if (direct.exists()) {
		return direct;
	}
	Common::FSList list;
	if (parent.getChildren(list, Common::FSNode::kListAll)) {
		for (Common::FSList::iterator it = list.begin(); it != list.end(); it++) {
			if (it->getName().equalsIgnoreCase(name)) {
				return *it;
			}
		}
	}
	return direct; // non-existent
}

const char *CryOmni3DEngine_Versailles::languageCode(Common::Language lang) {
	switch (lang) {
	case Common::EN_ANY:
		return "en";
	case Common::DE_DEU:
		return "de";
	case Common::ZH_TWN:
		return "zh";
	case Common::FR_FRA:
	default:
		return nullptr; // French is the base data, no overlay
	}
}

const char *CryOmni3DEngine_Versailles::languageLabel(Common::Language lang) {
	switch (lang) {
	case Common::EN_ANY:
		return "English";
	case Common::DE_DEU:
		return "Deutsch";
	case Common::ZH_TWN:
		return "Chinese";
	case Common::FR_FRA:
	default:
		return "Francais";
	}
}

Common::Language CryOmni3DEngine_Versailles::nextLanguage(Common::Language lang) {
	switch (lang) {
	case Common::FR_FRA:
		return Common::EN_ANY;
	case Common::EN_ANY:
		return Common::DE_DEU;
	case Common::DE_DEU:
		return Common::ZH_TWN;
	case Common::ZH_TWN:
	default:
		return Common::FR_FRA;
	}
}

Common::Language CryOmni3DEngine_Versailles::nextAudioLanguage(Common::Language lang) {
	// Chinese has no dub (its "voices" are the English track), so the audio
	// cycle skips it: French -> English -> German -> French.
	switch (lang) {
	case Common::FR_FRA:
		return Common::EN_ANY;
	case Common::EN_ANY:
		return Common::DE_DEU;
	case Common::DE_DEU:
	default:
		return Common::FR_FRA;
	}
}

static Common::Language parseLanguageCode(const Common::String &code, Common::Language fallback) {
	if (code == "fr") {
		return Common::FR_FRA;
	} else if (code == "en") {
		return Common::EN_ANY;
	} else if (code == "de") {
		return Common::DE_DEU;
	} else if (code == "zh") {
		return Common::ZH_TWN;
	}
	return fallback;
}

static Common::Language osDefaultLanguage() {
	// g_system->getSystemLanguage() returns a POSIX-style locale ("fr_FR",
	// "de_DE", "zh_TW", "en_US", ...) on both Windows and Linux. Map its language
	// part to a supported language; anything else falls back to English.
	Common::String loc = g_system->getSystemLanguage();
	Common::String code2 = (loc.size() >= 2) ? Common::String(loc.c_str(), 2) : Common::String("en");
	code2.toLowercase();
	if (code2 == "fr") {
		return Common::FR_FRA;
	} else if (code2 == "de") {
		return Common::DE_DEU;
	} else if (code2 == "zh") {
		return Common::ZH_TWN;
	}
	return Common::EN_ANY;
}

// Mount a lang/<code>/<subdir> as a SearchMan directory under a stable name so
// it can be swapped later. Absent directories are simply skipped.
void CryOmni3DEngine_Versailles::applyLanguageOverlays() {
	static const char *const kNames[] = {
		"versailles_text_datasv", "versailles_text_install", "versailles_audio_datasv"
	};
	for (int i = 0; i < 3; i++) {
		if (SearchMan.hasArchive(kNames[i])) {
			SearchMan.remove(kNames[i]);
		}
	}

	// Text overlay (messages come from the .dat, but documents, dialog text,
	// menu/object images and fonts live in text_datasv/ + text_install/).
	const char *textCode = languageCode(_currentLanguage);
	if (textCode) {
		Common::FSNode root = childCaseless(childCaseless(_gamePath, "lang"), textCode);
		Common::FSNode td = childCaseless(root, "text_datasv");
		if (td.exists()) {
			SearchMan.addDirectory("versailles_text_datasv", td, 100, 4, false);
		}
		Common::FSNode ti = childCaseless(root, "text_install");
		if (ti.exists()) {
			SearchMan.addDirectory("versailles_text_install", ti, 100, 3, false);
		}
	}

	// Audio overlay (voices + dubbed cinematics).
	const char *audioCode = languageCode(_audioLanguage);
	if (audioCode) {
		Common::FSNode root = childCaseless(childCaseless(_gamePath, "lang"), audioCode);
		Common::FSNode ad = childCaseless(root, "audio_datasv");
		if (ad.exists()) {
			SearchMan.addDirectory("versailles_audio_datasv", ad, 100, 4, false);
		}
	}
}

void CryOmni3DEngine_Versailles::reloadTextData() {
	// Reload every TEXT-language-dependent resource live. On-demand assets
	// (voices, cinematics, localized images) come through SearchMan, already
	// re-pointed, so only the preloaded data needs refreshing.
	loadStaticData();     // messages, localized filenames, painting titles, subtitles
	setupFonts();         // Latin vs CJK fonts follow the text language
	// NB: do NOT call setupObjects() here — it appends ~50 objects without
	// clearing (would overflow the inventory and crash). Object names are
	// _messages[] indices, so they follow the reloaded messages automatically.
	_dialogsMan.init(138, _messages[22]);
	// Dialog text (GTO) follows the text language; voice names inside it use the
	// audio language's padding.
	_dialogsMan.setPadAudioFileName(_audioLanguage != Common::EN_ANY);
	_dialogsMan.loadGTO(getFilePath(kFileTypeGTO, _localizedFilenames[LocalizedFilenames::kDialogs]));

	_docManager.init(&_sprites, &_fontManager, &_messages, this,
	                 getFilePath(kFileTypeText, _localizedFilenames[LocalizedFilenames::kAllDocs]),
	                 getFilePath(kFileTypeText, getFeatures() & GF_VERSAILLES_LINK_LOCALIZED
	                             ? _localizedFilenames[LocalizedFilenames::kLinksDocs]
	                             : Common::String("lien_doc.txt")));
}

void CryOmni3DEngine_Versailles::changeTextLanguage(Common::Language lang) {
	if (lang == _currentLanguage) {
		return;
	}
	setCurrentLanguage(lang);
	applyLanguageOverlays();

	const char *code = languageCode(lang);
	// Persist in the global (application) domain so it survives relaunch: the
	// standalone auto-detects the game into a transient domain that is not saved.
	ConfMan.set("versailles_language", code ? code : "fr", Common::ConfigManager::kApplicationDomain);
	ConfMan.flushToDisk();

	reloadTextData();
}

void CryOmni3DEngine_Versailles::changeAudioLanguage(Common::Language lang) {
	if (lang == _audioLanguage) {
		return;
	}
	_audioLanguage = lang;
	applyLanguageOverlays();

	const char *code = languageCode(lang);
	ConfMan.set("versailles_audio_language", code ? code : "fr", Common::ConfigManager::kApplicationDomain);
	ConfMan.flushToDisk();

	// Only the voice-name padding needs updating; voices/cinematics are loaded
	// on demand through the re-pointed SearchMan overlay.
	_dialogsMan.setPadAudioFileName(_audioLanguage != Common::EN_ANY);
}

// ---- Localized UI labels for the custom menu entries (current text language) ----
// ASCII only, so they render with any font (including the CJK menu font).

// The Chinese (ZH_TWN) strings are Big5 / CP950 byte sequences, matching the
// encoding of the game's own Chinese menu strings (setupFonts loads the CJK
// font with kWindows950), so they render with the tw12 font.
int CryOmni3DEngine_Versailles::barModeForCategory(const char *confKey, int defMode) const {
	int m = ConfMan.hasKey(confKey) ? ConfMan.getInt(confKey) : defMode;
	if (m < kScreen2DBarModeAmbient || m > kScreen2DBarModeStretch) {
		m = defMode;
	}
	return m;
}

const char *CryOmni3DEngine_Versailles::uiLabelFilter() const {
	switch (getLanguage()) {
	case Common::FR_FRA: return "Filtrage image";
	case Common::DE_DEU: return "Bildfilter";
	case Common::ZH_TWN: return "\xbc\x76\xb9\xb3\xc2\x6f\xc3\xe8"; // 影像濾鏡
	default:             return "Image filter";
	}
}

const char *CryOmni3DEngine_Versailles::uiLabelVoiceLang() const {
	switch (getLanguage()) {
	case Common::FR_FRA: return "Langue des voix";
	case Common::DE_DEU: return "Sprache Stimmen";
	case Common::ZH_TWN: return "\xbb\x79\xad\xb5\xbb\x79\xa8\xa5"; // 語音語言
	default:             return "Voice language";
	}
}

const char *CryOmni3DEngine_Versailles::uiLabelTextLang() const {
	switch (getLanguage()) {
	case Common::FR_FRA: return "Langue des textes";
	case Common::DE_DEU: return "Sprache Texte";
	case Common::ZH_TWN: return "\xa4\xe5\xa6\x72\xbb\x79\xa8\xa5"; // 文字語言
	default:             return "Text language";
	}
}

const char *CryOmni3DEngine_Versailles::uiLabelOnOff(bool on) const {
	switch (getLanguage()) {
	case Common::FR_FRA: return on ? "OUI" : "NON";
	case Common::DE_DEU: return on ? "JA" : "NEIN";
	case Common::ZH_TWN: return on ? "\xb6\x7d" : "\xc3\xf6"; // 開 / 關
	default:             return on ? "ON" : "OFF";
	}
}

const char *CryOmni3DEngine_Versailles::languageNameLocalized(Common::Language named) const {
	// Name of 'named' written in the current text language.
	switch (getLanguage()) {
	case Common::FR_FRA:
		switch (named) {
		case Common::EN_ANY: return "Anglais";
		case Common::DE_DEU: return "Allemand";
		case Common::ZH_TWN: return "Chinois";
		default:             return "Francais";
		}
	case Common::DE_DEU:
		switch (named) {
		case Common::EN_ANY: return "Englisch";
		case Common::DE_DEU: return "Deutsch";
		case Common::ZH_TWN: return "Chinesisch";
		default:             return "Franzoesisch";
		}
	case Common::ZH_TWN: // Big5: 法文 / 英文 / 德文 / 中文
		switch (named) {
		case Common::EN_ANY: return "\xad\x5e\xa4\xe5";
		case Common::DE_DEU: return "\xbc\x77\xa4\xe5";
		case Common::ZH_TWN: return "\xa4\xa4\xa4\xe5";
		default:             return "\xaa\x6b\xa4\xe5";
		}
	default: // EN
		switch (named) {
		case Common::EN_ANY: return "English";
		case Common::DE_DEU: return "German";
		case Common::ZH_TWN: return "Chinese";
		default:             return "French";
		}
	}
}

} // End of namespace Versailles
} // End of namespace CryOmni3D
