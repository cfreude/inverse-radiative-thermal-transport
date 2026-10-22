#pragma once
#include <tamashii/public.hpp>

#include <string>
#include <vector>
#include <map>

T_BEGIN_NAMESPACE
class Value {
public:
	enum class Type {
		EMPTY, INT, BOOL, FLOAT, STRING, BINARY, ARRAY, MAP
	};

								Value();
	explicit					Value(int aInt);
	explicit					Value(bool aBool);
	explicit					Value(float aFloat);
	explicit					Value(std::string aString);
	explicit					Value(std::vector<unsigned char> aBinary);
	explicit					Value(std::vector<Value> aArray);
	explicit					Value(std::map<std::string, Value> aMap);

	bool						isEmpty() const;
	bool						isInt() const;
	bool						isBool() const;
	bool						isFloat() const;
	bool						isString() const;
	bool						isBinary() const;
	bool						isArray() const;
	bool						isMap() const;

	Type						getType() const;
	int							getInt() const;
	bool						getBool() const;
	float						getFloat() const;
	std::string					getString() const;
	std::vector<unsigned char>	getBinary() const;
	std::vector<Value>			getArray() const;
	std::map<std::string, Value>getMap() const;

	void						setInt(int aInt);
	void						setBool(bool aBool);
	void						setFloat(float aFloat);
	void						setString(const std::string& aString);
	void						setBinary(const std::vector<unsigned char>& aBinary);
	void						setArray(const std::vector<Value>& aArray);
	void						setMap(const std::map<std::string, Value>& aMap);
private:
	Type						mType;
	int							mIntValue;
	bool						mBoolValue;
	float						mFloatValue;
	std::string					mStringValue;
	std::vector<unsigned char>	mBinaryValue;
	std::vector<Value>			mArrayValue;
	std::map<std::string, Value>mMapValue;
};

// all assets in AssetManager must have Asset as a base class
class Asset {
public:

	enum class Type {
		UNDEFINED, MESH, MODEL, CAMERA, LIGHT, IMAGE, MATERIAL, NODE, SCENE
	};

												Asset(Type aType);
												Asset(Type aType, std::string aName);
												~Asset() = default;

	std::string									getName() const;
	std::string									getFilepath() const;
	Type										getAssetType() const;
	Value										getCustomProperty(const std::string& aKey);
	std::map<std::string, Value>*				getCustomPropertyMap();

	void										setName(const std::string& aName);
	void										setFilepath(const std::string& aFilepath);
	void										setAssetType(Type aType);
	void										addCustomProperty(const std::string& aKey, const Value& aValue);
protected:

	std::string									mName;
	std::string									mFilepath;
	Type										mAssetType;
	std::map<std::string, Value>				mCustomProperties;
};
T_END_NAMESPACE
