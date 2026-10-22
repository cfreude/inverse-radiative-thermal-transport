#pragma once
#include <tamashii/public.hpp>

#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <deque>

T_BEGIN_NAMESPACE
class CmdArgs
{
public:
													CmdArgs();

													// number of arguments
	size_t											argc() const;
													// argument at pos (arg)
	std::string										argv(int aArg) const;

													// populate with argc and argv
	void											setArgs(int aArgc, char * const aArgv[]);
													// populate with a string (individual args are seperated by whitespaces)
	void											setArgs(const std::string& aArgs);

	void											appendArg(const std::string& aArg);
	void											clear();
	std::vector<std::string>&						getArgs();

													// iterator
	std::vector<std::string>::iterator				begin();
	std::vector<std::string>::const_iterator		begin() const;
	std::vector<std::string>::iterator				end();
	std::vector<std::string>::const_iterator		end() const;
private:
	std::vector<std::string>						mArgs;
};

class CmdSystem
{
public:
													CmdSystem();
	void											init();
	void											shutdown();

	typedef std::function<void(const CmdArgs&)>		cmdFunction;
	typedef std::function<void(const CmdArgs&)>		cmdCompletion;
	enum class Execute
	{
		NOW,
		APPEND
	};
	struct commandDef
	{
		std::string									name;
		cmdFunction									function;
		cmdCompletion								completion;
		std::string									description;
	};
													// add the commands that this cmd system supports
	void											addCommand(const std::string& aCmdName, const std::function<void(const CmdArgs&)>& aFunction,
	                                                           const std::string& aDescription, const cmdCompletion&
		                                                           aCompletionCb = nullptr);
													// add a cmd/argument to the queue or execute it right away
	void											cmdArg(Execute aExecute, const CmdArgs &aArgs);
													// execute all cmds in the queue and clear it
	void											execute();

	size_t											cmdQueueSize() const;

												private:
													// execute a command based on an argument
													// eg: set window_width 1280
													// call set cmd with the arguments window_width and 1280
	void											executeCmdArg(const CmdArgs& aArgs);

	std::unordered_map<std::string, commandDef>		mCmds;
	std::deque<CmdArgs>								mCmdQueue;
};

T_END_NAMESPACE