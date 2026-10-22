#pragma once
// for runtime shader compilation
// call in each thread that is used to compile shaders
namespace scomp {
	void getGlslangVersion(int &aMajor, int &aMinor, int &aPatch);
	void getDxcVersion(int &aMajor, int &aMinor, int& aPatch);

	void init();
	void finalize();
}