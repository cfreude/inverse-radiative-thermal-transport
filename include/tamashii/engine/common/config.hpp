#pragma once
#include <tamashii/public.hpp>

#include <string>
#include <map>

T_BEGIN_NAMESPACE
class Config {
public:
														Config();

														// load cfg from file
	void												load(const std::string& aCfgFile);
														// write cfg to file
	void												write(const std::string& aCfgFile) const;

	bool												inside(const std::string& aToken);
	std::string											get(const std::string& aToken);
	void												add(const std::string& aToken, const std::string& aValue);
	void												update(const std::string& aToken, const std::string& aValue);
	void												remove(const std::string& aToken);

														// has anything changed since the loading of the cfgFile
	bool												wasUpdated() const;

														// iterator
	std::map<std::string, std::string>::iterator		begin();
	std::map<std::string, std::string>::const_iterator	begin() const;
	std::map<std::string, std::string>::iterator		end();
	std::map<std::string, std::string>::const_iterator	end() const;
private:
	const std::string									mDelimiter;
	std::map<std::string, std::string>					mConfigPairs;
	bool												mUpdated;

};
T_END_NAMESPACE