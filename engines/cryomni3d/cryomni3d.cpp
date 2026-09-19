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

#include "common/scummsys.h"
#include "common/error.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "common/debug-channels.h"

#include "common/events.h"
#include "common/file.h"

#include "engines/util.h"

#include "audio/mixer.h"
#include "graphics/paletteman.h"

#include "cryomni3d/cryomni3d.h"
#include "cryomni3d/datstream.h"

#include "cryomni3d/image/hlz.h"
#include "cryomni3d/image/hnm.h"
#include "video/hnm_decoder.h"

namespace CryOmni3D {

CryOmni3DEngine::CryOmni3DEngine(OSystem *syst,
								 const CryOmni3DGameDescription *gamedesc) : Engine(syst), _gameDescription(gamedesc),
	_canLoadSave(false), _fontManager(), _sprites(), _dragStatus(kDragStatus_NoDrag), _lastMouseButton(0),
	_autoRepeatNextEvent(uint(-1)), _hnmHasClip(false) {
	if (!_mixer->isReady()) {
		error("Sound initialization failed");
	}

	// Setup mixer
	syncSoundSettings();

	unlockPalette();
}

CryOmni3DEngine::~CryOmni3DEngine() {
}

Common::Error CryOmni3DEngine::run() {
	return Common::kNoError;
}

void CryOmni3DEngine::pauseEngineIntern(bool pause) {
	Engine::pauseEngineIntern(pause);

	/*
	if (pause) {
	    _video->pauseVideos();
	} else {
	    _video->resumeVideos();
	    _system->updateScreen();
	}
	*/
}

DATSeekableStream *CryOmni3DEngine::getStaticData(uint32 gameId, uint16 version) const {
	Common::File *datFile = new Common::File();

	if (!datFile->open("cryomni3d.dat")) {
		delete datFile;
		error("Failed to open cryomni3d.dat file");
		return nullptr;
	}

	DATSeekableStream *gameStream = DATSeekableStream::getGame(datFile, gameId, version, getLanguage(),
	                                getPlatform());
	if (!gameStream) {
		delete datFile;
		error("Failed to find game in cryomni3d.dat file");
		return nullptr;
	}

	return gameStream;
}

void CryOmni3DEngine::playHNM(const Common::Path &filepath, Audio::Mixer::SoundType soundType,
							  HNMCallback beforeDraw, HNMCallback afterDraw) {
	Graphics::PixelFormat screenFormat = g_system->getScreenFormat();
	byte *currentPalette = nullptr;
	if (screenFormat.bytesPerPixel == 1) {
		currentPalette = new byte[256 * 3];
		g_system->getPaletteManager()->grabPalette(currentPalette, 0, 256);
	}

	// Pass the ownership of currentPalette to HNMDecoder
	Video::VideoDecoder *videoDecoder = new Video::HNMDecoder(screenFormat, false, currentPalette);
	videoDecoder->setSoundType(soundType);

	if (!videoDecoder->loadFile(filepath)) {
		warning("Failed to open movie file %s", filepath.toString(Common::Path::kNativeSeparator).c_str());
		delete videoDecoder;
		return;
	}

	videoDecoder->start();

	uint16 width = videoDecoder->getWidth();
	uint16 height = videoDecoder->getHeight();

	bool skipVideo = false;
	uint frameNum = 0;
	while (!shouldAbort() && !videoDecoder->endOfVideo() && !skipVideo) {
		if (videoDecoder->needsUpdate()) {
			const Graphics::Surface *frame = videoDecoder->decodeNextFrame();

			if (frame) {
				if (videoDecoder->hasDirtyPalette()) {
					const byte *palette = videoDecoder->getPalette();
					setPalette(palette, 0, 256);
				}

				if (beforeDraw) {
					(this->*beforeDraw)(frameNum);
				}

				if (_hnmHasClip) {
					Common::Rect rct(width, height);
					rct.clip(_hnmClipping);
					copyRectToScreen2D(frame->getPixels(), frame->pitch, rct.left, rct.top, rct.width(),
					                           rct.height());
				} else {
					copyRectToScreen2D(frame->getPixels(), frame->pitch, 0, 0, width, height);
				}

				if (afterDraw) {
					(this->*afterDraw)(frameNum);
				}

				frameNum++;
			}
		}
		g_system->updateScreen();
		g_system->delayMillis(10);

		if (pollEvents() && checkKeysPressed()) {
			skipVideo = true;
		}
	}

	delete videoDecoder;
}

Image::ImageDecoder *CryOmni3DEngine::loadHLZ(const Common::Path &filepath) {
	Common::File file;

	if (!file.open(filepath)) {
		warning("Failed to open hlz file %s", filepath.toString(Common::Path::kNativeSeparator).c_str());
		return nullptr;
	}

	Image::ImageDecoder *imageDecoder = new Image::HLZFileDecoder();

	if (!imageDecoder->loadStream(file)) {
		warning("Failed to load hlz file %s", filepath.toString(Common::Path::kNativeSeparator).c_str());
		delete imageDecoder;
		imageDecoder = nullptr;
		return nullptr;
	}

	return imageDecoder;
}

bool CryOmni3DEngine::displayHLZ(const Common::Path &filepath, uint32 timeout) {
	Image::ImageDecoder *imageDecoder = loadHLZ(filepath);

	if (!imageDecoder) {
		return false;
	}

	if (imageDecoder->hasPalette()) {
		setPalette(imageDecoder->getPalette().data(), 0, imageDecoder->getPalette().size());
	}

	const Graphics::Surface *frame = imageDecoder->getSurface();
	copyRectToScreen2D(frame->getPixels(), frame->pitch, 0, 0, frame->w, frame->h);
	g_system->updateScreen();

	uint32 end;
	if (timeout == uint(-1)) {
		end = uint(-1);
	} else {
		end = g_system->getMillis() + timeout;
	}
	bool exitImg = false;
	while (!shouldAbort() && !exitImg && g_system->getMillis() < end) {
		if (pollEvents()) {
			if (checkKeysPressed() || getCurrentMouseButton() == 1) {
				exitImg = true;
			}
		}
		g_system->updateScreen();
		g_system->delayMillis(10);
	}

	delete imageDecoder;

	return exitImg || shouldAbort();
}

void CryOmni3DEngine::setCursor(const Graphics::Cursor &cursor) const {
	CursorMan.replaceCursor(&cursor);
}

void CryOmni3DEngine::setCursor(uint cursorId) const {
	const Graphics::Cursor &cursor = _sprites.getCursor(cursorId);
	CursorMan.replaceCursor(&cursor);
}

bool CryOmni3DEngine::pollEvents() {
	Common::Event event;
	int buttonMask;
	bool hasEvents = false;

	// Don't take into transitional clicks for the drag
	buttonMask = g_system->getEventManager()->getButtonState();
	uint oldMouseButton;
	if (buttonMask & 0x1) {
		oldMouseButton = 1;
	} else if (buttonMask & 0x2) {
		oldMouseButton = 2;
	} else {
		oldMouseButton = 0;
	}

	int transitionalMask = 0;
	while (g_system->getEventManager()->pollEvent(event)) {
		if (event.type == Common::EVENT_KEYDOWN) {
			_keysPressed.push(event.kbd);
		} else if (event.type == Common::EVENT_LBUTTONDOWN) {
			transitionalMask |= Common::EventManager::LBUTTON;
		} else if (event.type == Common::EVENT_RBUTTONDOWN) {
			transitionalMask |= Common::EventManager::RBUTTON;
		}
		hasEvents = true;
	}

	// Merge current button state with any buttons pressed since last poll
	// That's to avoid missed clicks
	buttonMask = g_system->getEventManager()->getButtonState() |
	             transitionalMask;
	if (buttonMask & 0x1) {
		_lastMouseButton = 1;
	} else if (buttonMask & 0x2) {
		_lastMouseButton = 2;
	} else {
		_lastMouseButton = 0;
	}

	_dragStatus = kDragStatus_NoDrag;
	uint currentMouseButton = getCurrentMouseButton();
	if (!oldMouseButton && currentMouseButton == 1) {
		// Starting the drag
		_dragStatus = kDragStatus_Pressed;
		_dragStart = getMousePos();
	} else if (oldMouseButton == 1) {
		// We were already pressing
		if (currentMouseButton == 1) {
			// We are still pressing
			Common::Point delta = _dragStart - getMousePos();
			if (ABS(delta.x) > 2 || ABS(delta.y) > 2) {
				// We moved from the start point
				_dragStatus = kDragStatus_Dragging;
			} else if (_autoRepeatNextEvent != uint(-1)) {
				// Check for auto repeat duration
				if (_autoRepeatNextEvent < g_system->getMillis()) {
					_dragStatus = kDragStatus_Pressed;
				}
			}
		} else {
			// We just finished dragging
			_dragStatus = kDragStatus_Finished;
			// Cancel auto repeat
			_autoRepeatNextEvent = uint(-1);
		}
	}
	// Else we weren't dragging and still aren't

	return hasEvents;
}

void CryOmni3DEngine::setAutoRepeatClick(uint millis) {
	_autoRepeatNextEvent = g_system->getMillis() + millis;
}

void CryOmni3DEngine::waitMouseRelease() {
	while (getCurrentMouseButton() != 0 && !shouldAbort()) {
		pollEvents();
		g_system->updateScreen();
		g_system->delayMillis(10);
	}
}

// Widescreen 2D centering offset (physical - 2D). Set by the engine after
// initGraphics(); 0 means no widescreen (native 640 layout).
int g_screen2DOffsetX = 0;

// Side-bar style for full-width 2D content. Default = ambient blur everywhere;
// set to false (crisp solid edge color) only for the exceptions: the intro
// logos and the title/main-menu screen.
bool g_screen2DBlurBars = true;

// --- Widescreen "ambient" blurred side bars (TikTok/Shorts-style background) ---
// The game renders in 8-bit paletted mode, so we blur in RGB (via the current
// palette) and remap to the nearest palette index using a precomputed LUT.
static byte s_nearestLUT[4096];   // RGB444 -> palette index
static byte s_lutPalette[768];
static bool s_lutValid = false;

static void buildNearestLUT(const byte *pal) {
	for (int r = 0; r < 16; r++) {
		for (int g = 0; g < 16; g++) {
			for (int b = 0; b < 16; b++) {
				int R = r * 17, G = g * 17, B = b * 17; // 4-bit -> 8-bit
				int best = 0;
				long bestd = 0x7fffffffL;
				for (int i = 0; i < 256; i++) {
					int dr = R - pal[i * 3 + 0];
					int dg = G - pal[i * 3 + 1];
					int db = B - pal[i * 3 + 2];
					long d = (long)dr * dr + (long)dg * dg + (long)db * db;
					if (d < bestd) { bestd = d; best = i; }
				}
				s_nearestLUT[(r << 8) | (g << 4) | b] = (byte)best;
			}
		}
	}
	memcpy(s_lutPalette, pal, 768);
	s_lutValid = true;
}

static void drawBlurredSideBars(const byte *src, int pitch, int sw, int sh, int contentW) {
	int screenW = g_system->getWidth();
	int screenH = g_system->getHeight();
	if (g_screen2DOffsetX <= 0 || sw <= 0 || sh <= 0)
		return;

	// Crisp path (logos, title, stills): fill each side bar with ONE solid color
	// = the dominant color of the source column that touches that bar (the pixel
	// adjacent to the bar edge). E.g. a plain white intro screen -> white bars.
	if (!g_screen2DBlurBars) {
		for (int pass = 0; pass < 2; pass++) {
			int x0 = (pass == 0) ? 0 : (g_screen2DOffsetX + contentW);
			int x1 = (pass == 0) ? g_screen2DOffsetX : screenW;
			if (x1 <= x0)
				continue;
			int edgeX = (pass == 0) ? 0 : (sw - 1);
			int hist[256];
			memset(hist, 0, sizeof(hist));
			for (int y = 0; y < sh; y++)
				hist[src[y * pitch + edgeX]]++;
			int domIdx = 0, domCnt = -1;
			for (int i = 0; i < 256; i++)
				if (hist[i] > domCnt) { domCnt = hist[i]; domIdx = i; }
			g_system->fillScreen(Common::Rect(x0, 0, x1, screenH), domIdx);
		}
		return;
	}

	byte pal[768];
	g_system->getPaletteManager()->grabPalette(pal, 0, 256);
	if (!s_lutValid || memcmp(pal, s_lutPalette, 768) != 0)
		buildNearestLUT(pal);

	// Small blurred RGB thumbnail of the source (box average per cell).
	const int TW = 64, TH = 48;
	static byte thumb[TW * TH * 3];
	for (int ty = 0; ty < TH; ty++) {
		int sy0 = ty * sh / TH, sy1 = (ty + 1) * sh / TH;
		if (sy1 <= sy0) sy1 = sy0 + 1;
		for (int tx = 0; tx < TW; tx++) {
			int sx0 = tx * sw / TW, sx1 = (tx + 1) * sw / TW;
			if (sx1 <= sx0) sx1 = sx0 + 1;
			long ar = 0, ag = 0, ab = 0, n = 0;
			for (int yy = sy0; yy < sy1; yy++) {
				const byte *row = src + yy * pitch;
				for (int xx = sx0; xx < sx1; xx++) {
					byte idx = row[xx];
					ar += pal[idx * 3 + 0];
					ag += pal[idx * 3 + 1];
					ab += pal[idx * 3 + 2];
					n++;
				}
			}
			if (!n) n = 1;
			byte *t = &thumb[(ty * TW + tx) * 3];
			t[0] = (byte)(ar / n);
			t[1] = (byte)(ag / n);
			t[2] = (byte)(ab / n);
		}
	}

	const int DARK = 150; // /256 ~ 0.59 dimming for the ambient look
	static byte bar[288 * 512];
	for (int pass = 0; pass < 2; pass++) {
		int x0, x1;
		if (pass == 0) { x0 = 0; x1 = g_screen2DOffsetX; }
		else { x0 = g_screen2DOffsetX + contentW; x1 = screenW; }
		if (x1 <= x0 || screenH > 512 || (x1 - x0) > 288)
			continue;
		int bw = x1 - x0;

		for (int y = 0; y < screenH; y++) {
			float fv = (float)y / screenH * (TH - 1);
			int v0 = (int)fv, v1 = v0 + 1;
			if (v1 >= TH) v1 = TH - 1;
			float vf = fv - v0;
			byte *brow = bar + y * bw;
			for (int xx = 0; xx < bw; xx++) {
				int X = x0 + xx;
				float fu = (float)X / screenW * (TW - 1);
				int u0 = (int)fu, u1 = u0 + 1;
				if (u1 >= TW) u1 = TW - 1;
				float uf = fu - u0;
				const byte *t00 = &thumb[(v0 * TW + u0) * 3];
				const byte *t10 = &thumb[(v0 * TW + u1) * 3];
				const byte *t01 = &thumb[(v1 * TW + u0) * 3];
				const byte *t11 = &thumb[(v1 * TW + u1) * 3];
				int rgb[3];
				for (int c = 0; c < 3; c++) {
					float top = t00[c] * (1 - uf) + t10[c] * uf;
					float bot = t01[c] * (1 - uf) + t11[c] * uf;
					int v = (int)(top * (1 - vf) + bot * vf);
					v = (v * DARK) >> 8;
					rgb[c] = v;
				}
				brow[xx] = s_nearestLUT[((rgb[0] >> 4) << 8) | ((rgb[1] >> 4) << 4) | (rgb[2] >> 4)];
			}
		}
		g_system->copyRectToScreen(bar, bw, x0, 0, bw, screenH);
	}
}

void copyRectToScreen2D(const void *buf, int pitch, int x, int y, int w, int h) {
	if (g_screen2DOffsetX != 0 && x == 0) {
		// Full-width 2D content: fill the pillarbox side bars with an ambient
		// blurred version of the image instead of leaving stale content.
		drawBlurredSideBars((const byte *)buf, pitch, w, h, w);
	}
	g_system->copyRectToScreen(buf, pitch, x + g_screen2DOffsetX, y, w, h);
}

void CryOmni3DEngine::setMousePos(const Common::Point &point) {
	// point is given in 2D virtual-screen coords: warp to physical.
	g_system->warpMouse(point.x + g_screen2DOffsetX, point.y);
	// Ensure to update mouse position in event manager
	pollEvents();
}

Common::Point CryOmni3DEngine::getMousePos() {
	// Return 2D virtual-screen coords (used by all 2D UI hit-testing).
	Common::Point p = g_system->getEventManager()->getMousePos();
	p.x -= g_screen2DOffsetX;
	return p;
}

Common::Point CryOmni3DEngine::getRawMousePos() {
	// Raw physical coords (used by the full-width OMNI3D panorama).
	return g_system->getEventManager()->getMousePos();
}

Common::KeyState CryOmni3DEngine::getNextKey() {
	if (_keysPressed.empty()) {
		return Common::KeyState();
	} else {
		return _keysPressed.pop();
	}
}

bool CryOmni3DEngine::checkKeysPressed() {
	Common::KeyCode kc = getNextKey().keycode;
	if (kc != Common::KEYCODE_INVALID) {
		clearKeys();
		return true;
	} else {
		return false;
	}
}

bool CryOmni3DEngine::checkKeysPressed(uint numKeys, ...) {
	bool found = false;
	Common::KeyCode kc = getNextKey().keycode;
	while (!found && kc != Common::KEYCODE_INVALID) {
		va_list va;
		va_start(va, numKeys);
		for (uint i = 0; i < numKeys; i++) {
			// Compiler says that KeyCode is promoted to int, so we need this ugly cast
			Common::KeyCode match = (Common::KeyCode) va_arg(va, int);
			if (match == kc) {
				found = true;
				break;
			}
		}
		va_end(va);
		kc = getNextKey().keycode;
	}
	clearKeys();
	return found;
}

void CryOmni3DEngine::copySubPalette(byte *dst, const byte *src, uint start, uint num) {
	assert(start < 256);
	assert(start + num < 256);
	memcpy(&dst[3 * start], &src[3 * start], 3 * num * sizeof(*dst));
}

void CryOmni3DEngine::setPalette(const byte *colors, uint start, uint num) {
	if (start < _lockPaletteStartRW) {
		colors = colors + 3 * (_lockPaletteStartRW - start);
		start = _lockPaletteStartRW;
	}
	uint end = start + num - 1;
	if (end > _lockPaletteEndRW) {
		num = num - (end - _lockPaletteEndRW);
		end = _lockPaletteEndRW;
	}
	g_system->getPaletteManager()->setPalette(colors, start, num);
	// Don't update screen there: palette will be updated with next updateScreen call
}

void CryOmni3DEngine::fadeOutPalette() {
	byte palOut[256 * 3];
	uint16 palWork[256 * 3];
	uint16 delta[256 * 3];

	g_system->getPaletteManager()->grabPalette(palOut, 0, 256);
	for (uint i = 0; i < 256 * 3; i++) {
		palWork[i] = palOut[i] << 8;
		delta[i] = palWork[i] / 25;
	}

	for (uint step = 0; step < 25 && !shouldAbort(); step++) {
		for (uint i = 0; i < 256 * 3; i++) {
			palWork[i] -= delta[i];
			palOut[i] = palWork[i] >> 8;
		}
		setPalette(palOut, 0, 256);
		// Wait 50ms between each steps but refresh screen every 10ms
		for (uint i = 0; i < 5; i++) {
			pollEvents();
			g_system->updateScreen();
			g_system->delayMillis(10);
		}
	}
	setBlackPalette();
	pollEvents();
	g_system->updateScreen();
	clearKeys();
}

void CryOmni3DEngine::fadeInPalette(const byte *palette) {
	byte palOut[256 * 3];
	uint16 palWork[256 * 3];
	uint16 delta[256 * 3];

	memset(palOut, 0, sizeof(palOut));
	memset(palWork, 0, sizeof(palWork));
	for (uint i = 0; i < 256 * 3; i++) {
		delta[i] = (palette[i] << 8) / 25;
	}

	setBlackPalette();
	for (uint step = 0; step < 25 && !shouldAbort(); step++) {
		for (uint i = 0; i < 256 * 3; i++) {
			palWork[i] += delta[i];
			palOut[i] = palWork[i] >> 8;
		}
		setPalette(palOut, 0, 256);
		// Wait 50ms between each steps but refresh screen every 10ms
		for (uint i = 0; i < 5; i++) {
			pollEvents();
			g_system->updateScreen();
			g_system->delayMillis(10);
		}
	}
	setPalette(palette, 0, 256);
	pollEvents();
	g_system->updateScreen();
	clearKeys();
}

void CryOmni3DEngine::setBlackPalette() {
	byte pal[256 * 3];
	memset(pal, 0, 256 * 3);
	g_system->getPaletteManager()->setPalette(pal, 0, 256);
	g_system->updateScreen();
}

void CryOmni3DEngine::fillSurface(byte color) {
	g_system->fillScreen(color);
	g_system->updateScreen();
}

Common::Error CryOmni3DEngine_HNMPlayer::run() {
	CryOmni3DEngine::run();

	initGraphics(640, 480);

	syncSoundSettings();

	for (int i = 0; _gameDescription->desc.filesDescriptions[i].fileName; i++) {
		playHNM(_gameDescription->desc.filesDescriptions[i].fileName, Audio::Mixer::kMusicSoundType);
	}

	return Common::kNoError;
}

} // End of namespace CryOmni3D
