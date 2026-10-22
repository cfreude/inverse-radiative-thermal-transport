#include <tamashii/engine/common/config.hpp>
#include <tamashii/engine/exporter/exporter.hpp>

#include <fstream>

T_USE_NAMESPACE

Config::Config(): mDelimiter("="), mUpdated(false)
{}

void Config::load(const std::string& aCfgFile)
{
	mConfigPairs.clear();
	mUpdated = false;
	// check if file exists
	std::ifstream f(aCfgFile);
	// load config line by line
	while (f.good()) {
		std::string line;
		std::getline(f, line);
		// remove quotation marks etc
		line.erase(std::remove(line.begin(), line.end(), '\"'), line.end());
		line.erase(std::remove(line.begin(), line.end(), '\''), line.end());
		if (size_t pos; (pos = line.find(mDelimiter)) != std::string::npos) {
			std::string token = line.substr(0, pos);
			std::string value = line.substr(pos + 1, line.size());
			mConfigPairs.insert({ token, value });
		}
	}
	f.close();
}

void Config::write(const std::string& aCfgFile) const
{
	// create output string
	std::string out;
	for (auto const& cfg : mConfigPairs) {
		out += cfg.first + mDelimiter + "\"" + cfg.second + "\"\n";
	}
	// write config
	if (!out.empty()) Exporter::write_file(aCfgFile, out);
}

bool Config::inside(const std::string& aToken)
{
	const auto it = mConfigPairs.find(aToken);
	if (it != mConfigPairs.end()) return true;
	else return false;
}

std::string Config::get(const std::string& aToken)
{
	const auto it = mConfigPairs.find(aToken);
	if (it != mConfigPairs.end()) return it->second;
	else return "";
}

void Config::add(const std::string& aToken, const std::string& aValue)
{
	mConfigPairs.insert({ aToken, aValue });
	mUpdated = true;
}

void Config::update(const std::string& aToken, const std::string& aValue)
{
	const auto it = mConfigPairs.find(aToken);
	if (it != mConfigPairs.end()) {
		it->second = aValue;
		mUpdated = true;
	}
}

void Config::remove(const std::string& aToken)
{
	const auto it = mConfigPairs.find(aToken);
	if (it != mConfigPairs.end()) {
		mConfigPairs.erase(it);
		mUpdated = true;
	}
}

bool Config::wasUpdated() const
{ return mUpdated; }

std::map<std::string, std::string>::iterator Config::begin()
{ return mConfigPairs.begin(); }

std::map<std::string, std::string>::const_iterator Config::begin() const
{ return mConfigPairs.begin(); }

std::map<std::string, std::string>::iterator Config::end()
{ return mConfigPairs.end(); }

std::map<std::string, std::string>::const_iterator Config::end() const
{ return mConfigPairs.end(); }
