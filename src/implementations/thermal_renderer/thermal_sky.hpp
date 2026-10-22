#pragma once

#include "thermal_common.hpp"
#include <tamashii/engine/scene/render_scene.hpp>

#include"sky/skymatrix.hpp"

using namespace tamashii;

class ThermalSky {

public:
	ThermalSky(const string _epw_path = "./assets/misc/epw/AUT_Vienna.Schwechat.110360_IWEC.epw")
	{
		mSkyMatrix.set_epw(_epw_path);
	};

	~ThermalSky() {};

	void ThermalSky::loadGeometry(RenderScene * const _scene);

private:

	SkyMatrix mSkyMatrix;

};