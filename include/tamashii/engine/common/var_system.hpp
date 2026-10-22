#pragma once
#include <tamashii/public.hpp>

#include <string>
#include <deque>

T_BEGIN_NAMESPACE
// #todo: use templates?
class Var
{
public:
								Var() = delete;
								Var(const std::string& aName, const std::string& aValue, uint32_t aFlags, const std::string&
								    aDescription, const std::string& aCmd);
								Var(const std::string& aName, const std::string& aValue, float aValueMin, float aValueMax, uint32_t aFlags,
								    const std::string& aDescription, const std::string& aCmd);
								Var(const std::string& aName, const std::string& aValue, const std::vector<std::string>& aChoices, uint32_t aFlags, 
									const std::string& aDescription, const std::string& aCmd);

	void						setValue(const std::string& aValue);
	std::string					getValue();

	std::string					getString() const;
	void						setString(const std::string& aValue);

	int							getInt() const;
	glm::ivec2					getInt2() const;
	glm::ivec3					getInt3() const;
	glm::ivec4					getInt4() const;

	void						setInt(int aValue);
	void						setInt2(glm::ivec2 aValue);
	void						setInt3(glm::ivec3 aValue);
	void						setInt4(glm::ivec4 aValue);
	void						setIntArray(const int *aValue, int aSize);

	float						getFloat() const;
	glm::vec2					getFloat2() const;
	glm::vec3					getFloat3() const;
	glm::vec4					getFloat4() const;

	void						setFloat(float aValue);
	void						setFloat2(glm::vec2 aValue);
	void						setFloat3(glm::vec3 aValue);
	void						setFloat4(glm::vec4 aValue);
	void						setFloatArray(const float* aValue, int aSize);

	bool						getBool() const;
	void						setBool(bool aValue);

	float						getMin() const;
	void						setMin(bool aValue);

	float						getMax() const;
	void						setMax(bool aValue);

	std::vector<std::string>	getChoices() const;
	void						setChoices(const std::vector<std::string>& aChoices);

	uint32_t					getFlags() const;

	std::string					getName() const;
	std::string					getDescription() const;
	std::string					getCmd() const;

	static void					registerStaticVars();

	enum Flag {
		BOOL					= (1 << 0),		// variable is a boolean
		INTEGER					= (1 << 1),		// variable is an integer
		FLOAT					= (1 << 2),		// variable is a float
		STRING					= (1 << 3),		// variable is a string
		INIT					= (1 << 4),		// can only be set from the command-line
		ROM						= (1 << 5),		// display only, cannot be set at all
		CONFIG_RD				= (1 << 6),		// load variable from config file
		CONFIG_RDWR				= (3 << 6)		// load variable from config file and save changes back to config file when application is closed
	};
private:
	friend class							VarSystem;

	void									init(const std::string& aName, const std::string& aValue, float aValueMin, float aValueMax,
	                                             const std::vector<std::string>& aChoices, uint32_t aFlags, 
												 const std::string& aDescription, const std::string& aCmd);
	bool									checkChoices(const std::string& aString);
	void									toValue();
                                            // collect all tvars until VarSystem is initialized
    static std::deque<Var*>*				getStaticVars();

	std::string								mName;
	std::string								mValue;
	std::string								mDescription;
	uint32_t								mFlags;
	float									mValueMin;
	float									mValueMax;
	std::vector<std::string>				mChoices;
	std::vector<int>						mIntegerValue;
	std::vector<float>						mFloatValue;

	std::string								mCmd;
};

class VarSystem
{
public:
															VarSystem();
	bool													isInit() const;

	void													init();
	void													shutdown();

	void													registerVar(Var* aVar);
	Var*													find(const std::string& aName);

	// iterator
	std::unordered_map<std::string, Var*>::iterator			begin();
	std::unordered_map<std::string, Var*>::const_iterator	begin() const;
	std::unordered_map<std::string, Var*>::iterator			end();
	std::unordered_map<std::string, Var*>::const_iterator	end() const;
private:
	bool													mInitialized = false;
	std::unordered_map<std::string, Var*>					mVars;
};
T_END_NAMESPACE
