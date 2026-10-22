#include <tamashii/engine/common/cmd_system.hpp>
#include <tamashii/engine/common/common.hpp>
#include <tamashii/engine/common/vars.hpp>
#include <tamashii/engine/common/input.hpp>

#include <sstream>

T_USE_NAMESPACE

// cmds for cmdSystem
namespace {
	// cmd for loading a scene
	void _cmd_load_scene(const CmdArgs& aArgs) {
		if (aArgs.argc() != 2) {
			spdlog::warn("CmdSystem: could not execute load scene cmd");
			return;
		}
		std::filesystem::path path(aArgs.argv(1));
		if (path.is_absolute()) path = path.lexically_relative(std::filesystem::current_path());
		EventSystem::queueEvent(EventType::ACTION, Input::A_OPEN_SCENE, 0, 0, 0, path.make_preferred().string());
		//Common::getInstance().openScene(path.make_preferred().string());
	}
	// cmd for setting a var
	void _cmd_set(const CmdArgs& aArgs)
	{
        if (aArgs.argc() < 2) {
			spdlog::warn("CmdSystem: could not set var");
			return;
		}
		std::string varName = aArgs.argv(0);
        const std::string varValue = aArgs.argv(1);
		Var* tvar = Common::getInstance().getVarSystem()->find(varName);
		if (tvar == nullptr) {
			spdlog::warn("CmdSystem: _cmd_set could not find var '{}'", varName);
			return;
		}
		tvar->setValue(varValue);
	}
}

CmdArgs::CmdArgs()
{ mArgs.reserve(ARG_ARGV_RESERVE_COUNT); }

size_t CmdArgs::argc() const
{ return mArgs.size(); }

std::string CmdArgs::argv(const int aArg) const
{ return mArgs.at(aArg); }

void CmdArgs::appendArg(const std::string& aArg)
{ mArgs.push_back(aArg); }

void CmdArgs::clear()
{ mArgs.clear(); }

std::vector<std::string>& CmdArgs::getArgs()
{ return mArgs; }

std::vector<std::string>::iterator CmdArgs::begin()
{ return mArgs.begin(); }

std::vector<std::string>::const_iterator CmdArgs::begin() const
{ return mArgs.begin(); }

std::vector<std::string>::iterator CmdArgs::end()
{ return mArgs.end(); }

std::vector<std::string>::const_iterator CmdArgs::end() const
{ return mArgs.end(); }

void CmdArgs::setArgs(const int aArgc, char * const aArgv[])
{
	mArgs.reserve(aArgc);
	for (int i = 0; i < aArgc; i++) {
		mArgs.emplace_back(aArgv[i]);
	}
	// remove quotes
	for (std::string& s : mArgs) {
		s.erase(std::remove(s.begin(), s.end(), '\"'), s.end());
		s.erase(std::remove(s.begin(), s.end(), '\''), s.end());
	}
}

void CmdArgs::setArgs(const std::string& aArgs)
{
	std::istringstream iss(aArgs);
	mArgs = std::vector<std::string>(std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>());
	// remove quotes
	for (std::string& s : mArgs) {
		s.erase(std::remove(s.begin(), s.end(), '\"'), s.end());
		s.erase(std::remove(s.begin(), s.end(), '\''), s.end());
	}
}

CmdSystem::CmdSystem()
{ mCmds.reserve(CMD_SYSTEM_BUCKET_SIZE); }

void CmdSystem::init()
{
	addCommand("load_scene", _cmd_load_scene, "load a scene files");
	addCommand("set_var", _cmd_set, "set a var");
}

void CmdSystem::shutdown()
{
	mCmds.clear();
	mCmdQueue.clear();
}

void CmdSystem::addCommand(const std::string& aCmdName, const std::function<void(const CmdArgs&)>& aFunction, const std::string
                           & aDescription, const cmdCompletion& aCompletionCb)
{
	// check if cmd already defined
	const auto search = mCmds.find(aCmdName);
	if (search != mCmds.end()) {
		return;
	}

	commandDef cmd;
	cmd.name = aCmdName;
	cmd.function = aFunction;
	cmd.description = aDescription;
	cmd.completion = aCompletionCb;
	mCmds.insert(std::pair<std::string, commandDef>(aCmdName, cmd));
}

void CmdSystem::cmdArg(const Execute aExecute, const CmdArgs& aArgs)
{
	switch (aExecute)
	{
	case Execute::NOW:
		executeCmdArg(aArgs);
		break;
	case Execute::APPEND:
		mCmdQueue.push_back(aArgs);
		break;
	}
}

void CmdSystem::execute()
{
	for (const CmdArgs& args : mCmdQueue) {
		executeCmdArg(args);
	}
	mCmdQueue.clear();
}

size_t CmdSystem::cmdQueueSize() const
{ return mCmdQueue.size(); }

void CmdSystem::executeCmdArg(const CmdArgs& aArgs)
{
	if (!aArgs.argc()) {
		spdlog::warn("CmdSystem: empty command");
		return;
	}

	const Var *var = Common::getInstance().getVarSystem()->find(aArgs.argv(0));
	const std::string cmd = var->getCmd();

	const auto search = mCmds.find(cmd);
	if (search != mCmds.end()) {
		search->second.function(aArgs);
	} else {
		spdlog::warn("CmdSystem: cmd '{}' not found", cmd);
	}
}

