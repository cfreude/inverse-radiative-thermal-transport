#pragma once
#include <tamashii/public.hpp>

#include <thread>
#include <mutex>

T_BEGIN_NAMESPACE
/**
* FileWatcher
* Use native api provided by the OS for best performance
**/

class FileWatcher {
public:
    static FileWatcher&                             getInstance()
                                                    {
                                                        static FileWatcher instance;
                                                        return instance;
                                                    }
                                                    FileWatcher(FileWatcher const&) = delete;
    void                                            operator=(FileWatcher const&) = delete;
													// set a function the thread should call in the beginning
    void                                            setInitCallback(const std::function<void()>& aCallback) { mInitCallback = aCallback; }
													// set a function the thread should call in the end
    void                                            setShutdownCallback(const std::function<void()>& aCallback) { mShutdownCallback = aCallback; }
													// add file to watch
    void                                            watchFile(std::string aFilePath, std::function<void()> aCallback);
    void                                            removeFile(std::string aFilePath);
													// print current watch list
    void                                            print() const;

    void                                            clear() { mWatchHashList.clear(); }
													// spawn a thread which calls check
    std::thread                                     spawn();
													// terminate the thread
    void                                            terminate();

private:
                                                    FileWatcher();
                                                    ~FileWatcher();
    // check for file changes (blocking, therefore use separate thread)
    void                                            check();

    struct                                          triple_s
    {
        std::string                                 filePath;
        uint64_t                                    lastWrite;
        std::function<void()>                       callback;
    };

    std::mutex                                      mMutex;

#if defined( _WIN32 )
    void* hDir; // HANDLE
#elif defined( __linux__)
    int                                             mFd = -1;
    std::unordered_map<int, std::string>            mWdList;
#endif
    std::unordered_map<std::string, triple_s>       mWatchHashList;
    std::function<void()>                           mInitCallback;
    std::function<void()>                           mShutdownCallback;
};
T_END_NAMESPACE
