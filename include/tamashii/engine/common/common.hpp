#pragma once
#include <tamashii/public.hpp>
#include <tamashii/engine/common/cmd_system.hpp>
#include <tamashii/engine/common/var_system.hpp>
#include <tamashii/engine/render/render_system.hpp>
#include <tamashii/engine/common/config.hpp>
#include <tamashii/engine/common/window_thread.hpp>
#include <tamashii/engine/platform/system.hpp>

#include <thread>
#include <optional>

T_BEGIN_NAMESPACE
class Common
{
public:
                                static Common& getInstance()
                                {
                                    static Common instance;
                                    return instance;
                                }
                                Common(Common const&) = delete;
    void                        operator=(Common const&) = delete;

	void				        init(int aArgc, char* aArgv[], char* aCmdline);
	void				        shutdown();

    bool                        frame();

                                // actions
    void                        newScene();
    void                        openScene(const std::string& aFile);
    void                        addScene(const std::string& aFile) const;
    void                        addModel(const std::string& aFile) const;
    void                        addLight(const std::string& aFile) const;
    void                        exportScene(const std::string& aOutputFile, uint32_t aSettings) const;
    void                        reloadBackendImplementation() const;
    void                        changeBackendImplementation(int aIdx);
    void                        clearCache() const;
    void                        takeScreenshot(const std::filesystem::path& aOut, bool aScreenshotNoUi = false);

								// intersect scene with objects under current mouse pos
    void                        intersectScene(IntersectionSettings aSettings, HitInfo_s* aHitInfo);

    CmdSystem*                  getCmdSystem();
	VarSystem*                  getVarSystem();
	RenderSystem*               getRenderSystem();
	CmdArgs*                    getCommandLineArguments();

	WindowThread*               getMainWindow();
    IntersectionSettings&       intersectionSettings();

								// File dialog helper
    static void                 openFileDialogOpenScene();
    static void                 openFileDialogAddScene();
    static void                 openFileDialogOpenModel();
    static void                 openFileDialogOpenLight();
    static void                 openFileDialogExportScene(uint32_t aSettings);
private:
								Common();
                                ~Common() = default;

	void				        shutdownIntern();
    bool                        frameIntern();

    void                        parseCommandLine(CmdArgs const &aArgs);
    void                        setStartupVariables(const std::string& aMatch = "");
    void                        addStartupCommands();

    void                        loadConfig();
    void                        writeConfig();

    void                        processInputs();

    void                        paintOnMesh(const HitInfo_s *aHitInfo) const;

    SystemInfo_s                mSystemInfo;

    std::thread                 mFileWatcherThread;
    std::thread                 mSceneLoaderThread;
    std::thread                 mFrameThread;
    std::thread                 mRenderThread;
    WindowThread                mWindow;
    bool                        mShutdown;

    CmdArgs                     mCmdArgs;
    std::deque<CmdArgs>         mCommandLineArgs;
    Config                      mCfg;

    CmdSystem					mCmdSystem;
    VarSystem                   mVarSystem;
    RenderSystem                mRenderSystem;

    IntersectionSettings        mIntersectionSettings;
    DrawInfo_s			        mDrawInfo;
    std::filesystem::path       mScreenshotPath;
    bool                        mScreenshotNoUi;

    static std::atomic_bool     mFileDialogRunning;
};

T_END_NAMESPACE