#include "hostfxr.h"
#include "coreclr_delegates.h"

#include <dlfcn.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/sysctl.h>

#include <iostream>
#include <string_view>
#include <filesystem>
#include <cassert>
#include <chrono>
#include <thread>

using EntryPoint = void(*)();
using std::filesystem::path;

using namespace std::chrono_literals;

constexpr std::string_view RuntimeName = "dotnet-runtime-6.0.6-osx-arm64";
constexpr std::string_view RuntimeConfig = "HostingOsx.runtimeconfig.json";

path getModulePath()
{
    Dl_info info;
    auto res = dladdr(reinterpret_cast<const void*>(&getModulePath), &info);
    assert(res);
    return path(info.dli_fname);
}

path findRoot()
{
    auto dir = getModulePath().parent_path();

    while (!exists(dir / RuntimeConfig))
        dir = dir.parent_path();

    return dir;
}

EntryPoint loadClr()
{
    auto rootDir = findRoot();
    auto hostfxr = rootDir / RuntimeName / "host/fxr/6.0.6/libhostfxr.dylib";
    auto runtimeConfig = rootDir / RuntimeConfig;

    void *lib = dlopen(hostfxr.c_str(), RTLD_LAZY | RTLD_LOCAL);

    auto init = hostfxr_initialize_for_runtime_config_fn(dlsym(lib, "hostfxr_initialize_for_runtime_config"));
    auto getRuntimeDelegate = hostfxr_get_runtime_delegate_fn(dlsym(lib, "hostfxr_get_runtime_delegate"));
    auto close = hostfxr_close_fn(dlsym(lib, "hostfxr_close"));

    assert(init && getRuntimeDelegate && close);

    // Load .NET Core
    hostfxr_handle cxt = nullptr;

    int error = init(absolute(runtimeConfig).c_str(), nullptr, &cxt);

    assert(error == 0 && cxt != nullptr);

    // Get the load assembly function pointer
    void *loadFunc = nullptr;
    error = getRuntimeDelegate(cxt, hdt_load_assembly_and_get_function_pointer, &loadFunc);

    assert(error == 0 && loadFunc != nullptr);

    auto load = load_assembly_and_get_function_pointer_fn(loadFunc);

    auto dotnetLibPath = rootDir / "dotnet/bin/Debug/net6.0/HostingOsx.dll";

    void *method = nullptr;
    error = load(dotnetLibPath.c_str(), "HostingOsx.Class1, HostingOsx", "EntryPoint", UNMANAGEDCALLERSONLY_METHOD, nullptr, &method);

    assert(error == 0);

    return EntryPoint(method);
}

bool IsDebuggerPresent()
{
    int junk;
    int mib[4];
    struct kinfo_proc info;
    size_t size;

    info.kp_proc.p_flag = 0;

    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID;
    mib[3] = getpid();

    size = sizeof(info);
    junk = sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, NULL, 0);
    assert(junk == 0);

    return ((info.kp_proc.p_flag & P_TRACED) != 0);
}

void waitForDebugger()
{
    std::puts("waiting for debugger to attach...\n");

    while (!IsDebuggerPresent())
        std::this_thread::sleep_for(200ms);
}

int main(int, char **)
{
    std::cout << "Starting...\n";
    auto entryPoint = loadClr();
    std::cout << "clr started\n";
    // waitForDebugger();
    entryPoint();
    std::cout << "entry point called\n";
}
