#include <tamashii/engine/scene/asset.hpp>
#include <tiny_gltf.h>
#include <utility>

T_USE_NAMESPACE

Value::Value() : mType(Type::EMPTY), mIntValue(0), mBoolValue(false), mFloatValue(0) {}
Value::Value(const int aInt) : mType(Type::INT), mIntValue(aInt), mBoolValue(false), mFloatValue(0) {}
Value::Value(const bool aBool) : mType(Type::BOOL), mIntValue(0), mBoolValue(aBool), mFloatValue(0) {}
Value::Value(const float aFloat) : mType(Type::FLOAT), mIntValue(0), mBoolValue(false), mFloatValue(aFloat) {}
Value::Value(std::string aString) : mType(Type::STRING), mIntValue(0), mBoolValue(false), mFloatValue(0), mStringValue(std::move(aString)) {}
Value::Value(std::vector<unsigned char> aBinary) : mType(Type::BINARY), mIntValue(0), mBoolValue(false), mFloatValue(0), mBinaryValue(std::move(aBinary)) {}
Value::Value(std::vector<Value> aArray) : mType(Type::ARRAY), mIntValue(0), mBoolValue(false), mFloatValue(0), mArrayValue(std::move(aArray)) {}
Value::Value(std::map<std::string, Value> aMap) : mType(Type::MAP), mIntValue(0), mBoolValue(false), mFloatValue(0), mMapValue(std::move(aMap)) {}

bool Value::isEmpty() const
{
	return mType == Type::EMPTY;
}

bool Value::isInt() const
{
	return mType == Type::INT;
}

bool Value::isBool() const
{
	return mType == Type::BOOL;
}

bool Value::isFloat() const
{
	return mType == Type::FLOAT;
}

bool Value::isString() const
{
	return mType == Type::STRING;
}

bool Value::isBinary() const
{
	return mType == Type::BINARY;
}

bool Value::isArray() const
{
	return mType == Type::ARRAY;
}

bool Value::isMap() const
{
	return mType == Type::MAP;
}

Value::Type Value::getType() const
{
	return mType;
}

int Value::getInt() const
{
	return mIntValue;
}

bool Value::getBool() const
{
	return mBoolValue;
}

float Value::getFloat() const
{
	return mFloatValue;
}

std::string Value::getString() const
{
	return mStringValue;
}

std::vector<unsigned char> Value::getBinary() const
{
	return mBinaryValue;
}

std::vector<Value> Value::getArray() const
{
	return mArrayValue;
}

std::map<std::string, Value> Value::getMap() const
{
	return mMapValue;
}

void Value::setInt(const int aInt)
{
	mType = Type::INT; mIntValue = aInt;
}

void Value::setBool(const bool aBool)
{
	mType = Type::BOOL; mBoolValue = aBool;
}

void Value::setFloat(const float aFloat)
{
	mType = Type::FLOAT; mFloatValue = aFloat;
}

void Value::setString(const std::string& aString)
{
	mType = Type::STRING; mStringValue = aString;
}

void Value::setBinary(const std::vector<unsigned char>& aBinary)
{
	mType = Type::BINARY; mBinaryValue = aBinary;
}

void Value::setArray(const std::vector<Value>& aArray)
{
	mType = Type::ARRAY; mArrayValue = aArray;
}

void Value::setMap(const std::map<std::string, Value>& aMap)
{
	mType = Type::MAP; mMapValue = aMap;
}

Asset::Asset(const Type aType) : mAssetType(aType) {}
Asset::Asset(const Type aType, std::string aName) : mName(std::move(aName)), mAssetType(aType) {}

std::string Asset::getName() const
{
	return mName;
}

std::string Asset::getFilepath() const
{
	return mFilepath;
}

Asset::Type Asset::getAssetType() const
{
	return mAssetType;
}

Value Asset::getCustomProperty(const std::string& aKey)
{
	const auto search = mCustomProperties.find(aKey);
	return (search != mCustomProperties.end()) ? search->second : Value();
}

std::map<std::string, Value>* Asset::getCustomPropertyMap()
{
	return &mCustomProperties;
}

void Asset::setName(const std::string& aName)
{
	mName = aName;
}

void Asset::setFilepath(const std::string& aFilepath)
{
	mFilepath = aFilepath;
}

void Asset::setAssetType(const Type aType)
{
	mAssetType = aType;
}

void Asset::addCustomProperty(const std::string& aKey, const Value& aValue)
{
	// overwrite existing
	mCustomProperties[aKey] = aValue;
}
