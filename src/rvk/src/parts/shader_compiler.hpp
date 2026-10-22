#pragma once

#include <rvk/parts/shader.hpp>
#include <string>

namespace scomp {
	// needs to be done per process
	void init();
	void finalize();
	void cleanup();
	// preprocess shader
    bool preprocess(rvk::Shader::Source aRvkSource, rvk::Shader::Stage aRvkStage, std::string const& aPath, std::string const& aCode, std::vector<std::string> const& aDefines, std::string const& aEntryPoint);
	std::size_t getHash();
    bool compile(std::string& aSpv, bool aCache = false, std::string const& aCacheDir = "");

	bool preprocessDXC(rvk::Shader::Source aRvkSource, rvk::Shader::Stage aRvkStage, std::string const& aPath, std::string const& aCode, std::vector<std::string> const& aDefines, std::string const& aEntryPoint);
	bool compileDXC(std::string& aSpv, bool aCache = false, std::string const& aCacheDir = "");
}
