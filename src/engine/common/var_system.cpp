#include <tamashii/engine/common/var_system.hpp>
#include <tamashii/engine/common/common.hpp>

#include <sstream>

T_USE_NAMESPACE

Var::Var(const std::string& aName, const std::string& aValue, const uint32_t aFlags, 
	const std::string& aDescription, const std::string& aCmd) :
	mFlags(aFlags), mValueMin(1), mValueMax(-1)
{
	init(aName, aValue, mValueMin, mValueMax, {}, aFlags, aDescription, aCmd);
}

Var::Var(const std::string& aName, const std::string& aValue, const float aValueMin, const float aValueMax, 
	const uint32_t aFlags, const std::string& aDescription, const std::string& aCmd) :
	mFlags(aFlags), mValueMin(aValueMin), mValueMax(aValueMax)
{
	init(aName, aValue, aValueMin, aValueMax, {}, aFlags, aDescription, aCmd);
}

Var::Var(const std::string& aName, const std::string& aValue,
	const std::vector<std::string>& aChoices, const uint32_t aFlags, const std::string& aDescription, const std::string& aCmd) :
	mFlags(aFlags), mValueMin(1), mValueMax(-1)
{
	init(aName, aValue, mValueMin, mValueMax, aChoices, aFlags, aDescription, aCmd);
}

std::string Var::getValue()
{ return mValue; }

std::string Var::getString() const
{ return mValue; }

void Var::setString(const std::string& aValue)
{ mValue = aValue; }

int Var::getInt() const
{ return mIntegerValue[0]; }

glm::ivec2 Var::getInt2() const
{ return glm::make_vec2(mIntegerValue.data());}

glm::ivec3 Var::getInt3() const
{ return glm::make_vec3(mIntegerValue.data());}

glm::ivec4 Var::getInt4() const
{ return glm::make_vec4(mIntegerValue.data());}

float Var::getFloat() const
{ return mFloatValue[0]; }

glm::vec2 Var::getFloat2() const
{ return glm::make_vec2(mFloatValue.data()); }

glm::vec3 Var::getFloat3() const
{ return glm::make_vec3(mFloatValue.data()); }

glm::vec4 Var::getFloat4() const
{ return glm::make_vec4(mFloatValue.data()); }

bool Var::getBool() const
{ return mIntegerValue[0] > 0; }

float Var::getMin() const
{ return mValueMin; }

float Var::getMax() const
{ return mValueMax; }

std::vector<std::string> Var::getChoices() const
{ return mChoices; }

uint32_t Var::getFlags() const
{ return mFlags; }

std::string Var::getName() const
{ return mName; }

std::string Var::getDescription() const
{ return mDescription; }

std::string Var::getCmd() const
{ return mCmd; }

void Var::setValue(const std::string& aValue)
{
	if (isBitSet<uint32_t>(mFlags, Var::Flag::ROM)) return;
	mIntegerValue.clear(); mFloatValue.clear();

	const size_t cv = std::count(aValue.begin(), aValue.end(), ',') + 1;
	if (mFlags & Flag::INTEGER || mFlags & Flag::BOOL) mIntegerValue.reserve(cv);
	else if (mFlags & Flag::FLOAT) mFloatValue.reserve(cv);
	else if (mFlags & Flag::STRING) {
		mValue = aValue;
		return;
	}

	const std::vector<int> prevIntegerValue = mIntegerValue;
	const std::vector<float> prevFloatValue = mFloatValue;
	std::istringstream ss(aValue);
	std::string token;
	bool changes = false;
	while (std::getline(ss, token, ',')) {
		// if we have choices, check if it is a valid choice
		bool valid = mChoices.empty();
		if (!valid) valid = checkChoices(token);
		
		try {
			if (mFlags & Flag::INTEGER || mFlags & Flag::BOOL) {
				int v = std::stoi(token);
				if (mValueMin <= mValueMax) v = std::min(std::max(v, static_cast<int>(mValueMin)), static_cast<int>(mValueMax));
				if(valid) mIntegerValue.push_back(v);
				else if (mIntegerValue.size() < prevIntegerValue.size())  mIntegerValue.push_back(prevIntegerValue[mIntegerValue.size()]);
				else mIntegerValue.push_back(std::stoi(mChoices.front()));
			}
			if (mFlags & Flag::FLOAT) {
				float v = std::stof(token);
				if (mValueMin <= mValueMax) v = std::min(std::max(v, mValueMin), mValueMax);
				if (valid) mFloatValue.push_back(v);
				else if (mFloatValue.size() < prevFloatValue.size()) mFloatValue.push_back(prevFloatValue[mFloatValue.size()]);
				else mFloatValue.push_back(std::stof(mChoices.front()));
			}
		}
		catch (const std::invalid_argument& ia)
		{
			if (mFlags & Flag::BOOL) {
				bool b;
				std::istringstream(token) >> std::boolalpha >> b;
				mIntegerValue.push_back(b);
			}
		}
		catch (const std::out_of_range& oor) {}
	}
	toValue();
}

void Var::setInt(const int aValue)
{
	setIntArray(&aValue, 1);
}

void Var::setInt2(const glm::ivec2 aValue)
{
	setIntArray(&aValue[0], 2);
}

void Var::setInt3(const glm::ivec3 aValue)
{
	setIntArray(&aValue[0], 3);
}

void Var::setInt4(const glm::ivec4 aValue)
{
	setIntArray(&aValue[0], 4);
}

void Var::setIntArray(const int* aValue, const int aSize)
{
	if (isBitSet<uint32_t>(mFlags, Var::Flag::ROM)) return;
	for (int i = 0; i < aSize; i++) {
		if (mValueMin < mValueMax && !inBounds<float>(mValueMin, mValueMax, static_cast<float>(aValue[i]))) return;
	}
	mIntegerValue.clear();
	for (int i = 0; i < aSize; i++) mIntegerValue.push_back(aValue[i]);
	toValue();
	
}

void Var::setFloat(const float aValue)
{
	setFloatArray(&aValue, 1);
}

void Var::setFloat2(const glm::vec2 aValue)
{
	setFloatArray(&aValue[0], 2);
}

void Var::setFloat3(const glm::vec3 aValue)
{
	setFloatArray(&aValue[0], 3);
}

void Var::setFloat4(const glm::vec4 aValue)
{
	setFloatArray(&aValue[0], 4);
}

void Var::setFloatArray(const float* aValue, const int aSize)
{
	if (isBitSet<uint32_t>(mFlags, Var::Flag::ROM)) return;
	for (int i = 0; i < aSize; i++) {
		if (mValueMin < mValueMax && !inBounds<float>(mValueMin, mValueMax, aValue[i])) return;
	}
	mFloatValue.clear();
	for (int i = 0; i < aSize; i++) mFloatValue.push_back(aValue[i]);
	toValue();
}

void Var::setBool(const bool aValue)
{
	if (isBitSet<uint32_t>(mFlags, Var::Flag::ROM)) return;
	if (mValueMin < mValueMax && !inBounds<float>(mValueMin, mValueMax, aValue)) return;
	mIntegerValue[0] = aValue;
	mValue = std::to_string(aValue);
}

void Var::setMin(const bool aValue)
{
	if (isBitSet<uint32_t>(mFlags, Var::Flag::ROM)) return;
	mValueMin = aValue;
}

void Var::setMax(const bool aValue)
{
	if (isBitSet<uint32_t>(mFlags, Var::Flag::ROM)) return;
	mValueMax = aValue;
}

void Var::setChoices(const std::vector<std::string>& aChoices)
{
	if (isBitSet<uint32_t>(mFlags, Var::Flag::ROM)) return;
	mChoices = aChoices;
}

void Var::registerStaticVars()
{
    for (Var* var : *getStaticVars()) {
		Common::getInstance().getVarSystem()->registerVar(var);
	}
    getStaticVars()->clear();
}

void Var::init(const std::string& aName, const std::string& aValue, const float aValueMin, const float aValueMax, 
	const std::vector<std::string>& aChoices, const uint32_t aFlags, 
	const std::string& aDescription, const std::string& aCmd)
{
	mName = aName;
	mDescription = aDescription;
	mFlags = aFlags;
	mValueMin = aValueMin;
	mValueMax = aValueMax;
	mChoices = aChoices;
	setValue(aValue);
	mCmd = aCmd;

	// if var system is not init yet, save tVar to a static list and add it later
	if (Common::getInstance().getVarSystem()->isInit()) Common::getInstance().getVarSystem()->registerVar(this);
    else { getStaticVars()->push_back(this); }
}
bool Var::checkChoices(const std::string& aString)
{
	for (const std::string& s : mChoices) {
		if (s == aString) return true;
	}
	spdlog::warn(R"("{}" is not a valid value for Var "{}")", aString, mName);
	return false;
}

void tamashii::Var::toValue()
{
	mValue.clear();
	if(!mIntegerValue.empty())
	{
		uint32_t chars = 0;
		for (uint32_t i = 0; i < mIntegerValue.size(); i++) {
			mValue += std::to_string(mIntegerValue[i]);
			if (i != mIntegerValue.size() - 1) mValue += ",";
		}
	}
	else if (!mFloatValue.empty())
	{
		uint32_t chars = 0;
		for (uint32_t i = 0; i < mFloatValue.size(); i++) {
			mValue += std::to_string(mFloatValue[i]);
			if (i != mFloatValue.size() - 1) mValue += ",";
		}
	}
}

std::deque<Var*>* Var::getStaticVars(){
    static std::deque<Var*> staticVars;
    return &staticVars;
}

VarSystem::VarSystem()
{ mVars.reserve(VAR_SYSTEM_BUCKET_SIZE); }

bool VarSystem::isInit() const
{ return mInitialized; }

std::unordered_map<std::string, Var*>::iterator VarSystem::begin()
{ return mVars.begin(); }

std::unordered_map<std::string, Var*>::const_iterator VarSystem::begin() const
{ return mVars.begin(); }

std::unordered_map<std::string, Var*>::iterator VarSystem::end()
{ return mVars.end(); }

std::unordered_map<std::string, Var*>::const_iterator VarSystem::end() const
{ return mVars.end(); }

void VarSystem::init()
{
	mInitialized = true;
}
void VarSystem::shutdown()
{
	mVars.clear();
	mInitialized = false;
}

void VarSystem::registerVar(Var* aVar)
{
	const auto search = mVars.find(aVar->mName);
	if (search != mVars.end()) {
		spdlog::warn("tVarSystem: variable {} already registered", aVar->mName);
		return;
	}
	mVars.insert(std::pair<std::string, Var*>(aVar->mName, aVar));
}

Var* VarSystem::find(const std::string& aName)
{
	const auto search = mVars.find(aName);
	if (search != mVars.end()) {
		return search->second;
	}
	return nullptr;
}

