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

#ifndef CRYOMNI3D_OMNI3D_H
#define CRYOMNI3D_OMNI3D_H

#include "graphics/surface.h"

namespace CryOmni3D {

class Omni3DManager {
public:
	Omni3DManager() : _vfov(0), _alpha(0), _beta(0), _xSpeed(0), _ySpeed(0), _alphaMin(0), _alphaMax(0),
		_betaMin(0), _betaMax(0), _helperValue(0), _dirty(true), _dirtyCoords(true),
		_sourceSurface(nullptr), _tilesX(54), _halfX(27), _rowstride(110), _w(864) {}
	virtual ~Omni3DManager();

	// widescreen: 54 tiles (864 px, 16:9) vs 40 tiles (640 px, 4:3).
	void init(double hfov, bool widescreen = true);

	int getWidth() const { return _w; }

	void setSourceSurface(const Graphics::Surface *surface) { _sourceSurface = surface; _dirty = true; }

	void clearConstraints();
	void setAlphaConstraints(double alphaMin, double alphaMax) { _alphaMin = alphaMin; _alphaMax = alphaMax; }
	void setBetaMinConstraint(double betaMin) { _betaMin = betaMin; }
	void setBetaMaxConstraint(double betaMax) { _betaMax = betaMax; }

	void setAlpha(double alpha) { _alpha = alpha; _dirtyCoords = true; }
	void setBeta(double beta) { _beta = beta; _dirtyCoords = true; }
	void updateCoords(int xDelta, int yDelta, bool useOldSpeed);

	double getAlpha() const { return _alpha; }
	double getBeta() const { return _beta; }

	Common::Point mapMouseCoords(const Common::Point &mouse);

	bool hasSpeed() { return _xSpeed != 0. || _ySpeed != 0.; }
	bool needsUpdate() { return _dirty || _dirtyCoords; }
	const Graphics::Surface *getSurface();

private:
	void updateImageCoords();

	double _vfov;

	double _alpha, _beta;
	double _xSpeed, _ySpeed;

	double _alphaMin, _alphaMax;
	double _betaMin, _betaMax;

	// Arrays sized for the widest configuration (54 horizontal tiles).
	int _imageCoords[3520];
	double _squaresCoords[31][28];
	double _hypothenusesH[31];
	double _anglesH[31];
	double _oppositeV[28];
	double _helperValue;

	// Horizontal geometry (4:3 vs 16:9); vertical stays 30 tiles / 480 px.
	int _tilesX;     // 40 or 54
	int _halfX;      // _tilesX / 2
	int _rowstride;  // 2 * (_tilesX + 1)
	int _w;          // _tilesX * 16

	bool _dirty;
	bool _dirtyCoords;
	const Graphics::Surface *_sourceSurface;
	Graphics::Surface _surface;
};

} // End of namespace CryOmni3D

#endif
