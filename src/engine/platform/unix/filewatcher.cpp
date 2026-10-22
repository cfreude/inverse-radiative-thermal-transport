#include <tamashii/engine/platform/filewatcher.hpp>

#include <sys/inotify.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>
#include <filesystem>

T_USE_NAMESPACE
FileWatcher::FileWatcher() {
    mWatchHashList.reserve(16);
    mWdList.reserve(8);
    mFd = inotify_init();
    if (mFd < 0) spdlog::error("Encountered a problem creating FileWatcher");
}
FileWatcher::~FileWatcher() {
    for(const auto& p : mWdList) inotify_rm_watch( mFd, p.first );
    close( mFd );
}
void FileWatcher::watchFile(std::string aFilePath, std::function<void()> aCallback)
{
    const std::lock_guard<std::mutex> lock(mMutex);
    if (aFilePath.empty()) return;
    // remove leading ./ if present
    std::string remove = "./";
    size_t index;
    if((index = aFilePath.find(remove)) != std::string::npos && index == 0) aFilePath.erase(0, remove.size());
    aFilePath = std::filesystem::path(aFilePath).make_preferred().string();

    // return if already watching
    if (mWatchHashList.find(aFilePath) != mWatchHashList.end()) return;

    // get last mod time
    struct stat attr;
    stat(aFilePath.c_str(), &attr);
    uint64_t time = (attr.st_mtime * 1000) + (attr.st_mtim.tv_nsec / 1000000);

    // add file with its time and aCallback function
    std::pair<std::string, triple_s> pair(aFilePath, { aFilePath, time, aCallback });
    mWatchHashList.insert(pair);

    // get dir from file
    std::filesystem::path dir = std::filesystem::path(aFilePath).parent_path();
    //watch the dir for changes
    int wd = inotify_add_watch(mFd, dir.c_str(), IN_MODIFY);
    if (wd < 0) spdlog::error("Could not add watch for: {}", aFilePath);
    // check if this directory is already watched, if not insert
    if (mWdList.find(wd) == mWdList.end()) {
        mWdList.insert({wd, dir});
    }
}
void FileWatcher::removeFile(std::string aFilePath)
{
    const std::lock_guard<std::mutex> lock(mMutex);
    if (aFilePath.empty()) return;
    // remove leading ./ if present
    std::string remove = "./";
    size_t index;
    if ((index = aFilePath.find(remove)) != std::string::npos && index == 0) aFilePath.erase(0, remove.size());
    aFilePath = std::filesystem::path(aFilePath).make_preferred().string();

    mWatchHashList.erase(aFilePath);
}

void FileWatcher::check()
{
    if(mInitCallback) mInitCallback();

    const int BUF_LEN = ( 1024 * ( sizeof(inotify_event) + 16 ) );
    char buffer[BUF_LEN];

    while(true){
        int length = read( mFd, buffer, BUF_LEN );
        if ( length < 0 ) break; //spdlog::error("tFileWatcher: empty read");

        int i = 0;
        while (i < length ) {
            struct inotify_event *event = ( struct inotify_event *) &buffer[ i ];
            if ( event->len ) {
                std::string completePath = mWdList[event->wd] + "/" + event->name;
                if ( event->mask & IN_MODIFY ) {
                    if ( !(event->mask & IN_ISDIR) ) {
                        //spdlog::info("The file {} was modified.", event->name );
                        auto search = mWatchHashList.find(completePath);
                        if (search != mWatchHashList.end()) {
                            struct stat attr;
                            stat(completePath.c_str(), &attr);
                            uint64_t time = (attr.st_mtime * 1000) + (attr.st_mtim.tv_nsec / 1000000);

                            // compare, update, execute
                            triple_s& f = search->second;
                            if (time > f.lastWrite) {
                                f.lastWrite = time + 10;  // add 10 milliseconds so it does not get called again
                                std::this_thread::sleep_for(std::chrono::milliseconds(2));  // wait 2 milliseconds to ensure writes are completed
                                f.callback();
                            }
                            break;
                        }

                    }
                }
            }
            i += sizeof(inotify_event) + event->len;
        }
    }
    if (mShutdownCallback) mShutdownCallback();

    // since we had to close inotify for breaking the loop
    // restart it, just in case
    mFd = inotify_init();
    std::unordered_map<int, std::string> wd_list_new;
    wd_list_new.reserve(mWdList.size());
    for (const auto& p : mWdList) {
        wd_list_new.insert({inotify_add_watch(mFd, p.second.c_str(), IN_MODIFY), p.second});
    }
    mWdList.clear();
    mWdList = wd_list_new;
}
void FileWatcher::print() const
{
    spdlog::info("tFileWatcher content:");
    for (const auto& f : mWatchHashList) {
        spdlog::info("   watching: {}", f.second.filePath);
    }
}
std::thread FileWatcher::spawn()
{
    std::thread t(&FileWatcher::check, this);
    spdlog::info("FileWatcher thread spawned");
    return t;
}
void FileWatcher::terminate() {
    spdlog::info("...terminating FileWatcher thread");
    // if no file is watched, closing fd does not unblock read
    // hack to unblock read when fd is closed
    if(mWdList.empty()) watchFile(".", []{});

    // hack to stop reading block
    for(const auto& p : mWdList) inotify_rm_watch( mFd, p.first );
    close( mFd );
}
