#include "AutoUpdate.h"
#include "UpdatePolicy.h"
#include <shlobj.h>
#include <shellapi.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <winver.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <memory>
#include <vector>
#include <chrono>
#include <stdexcept>
#include <algorithm>
namespace suite {
namespace fs=std::filesystem;
namespace {
constexpr wchar_t preferencesKey[]=L"Software\\DesktopSuite";
const std::wstring api=L"https://api.github.com/repos/Manixpcshot/Desktop-Widget-Suite/releases/latest";
const std::string assetPrefix="https://github.com/Manixpcshot/Desktop-Widget-Suite/releases/download/";
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
struct Internet { HINTERNET h=nullptr; ~Internet(){ if(h) WinHttpCloseHandle(h); } };
std::vector<unsigned char> download(const std::wstring& url,size_t limit) {
    URL_COMPONENTS parts{sizeof(parts)}; parts.dwHostNameLength=parts.dwUrlPathLength=parts.dwExtraInfoLength=static_cast<DWORD>(-1);
    require(WinHttpCrackUrl(url.c_str(),0,0,&parts)!=FALSE,"Invalid update URL");
    require(parts.nScheme==INTERNET_SCHEME_HTTPS,"Update URL must be HTTPS");
    std::wstring host(parts.lpszHostName,parts.dwHostNameLength),path(parts.lpszUrlPath,parts.dwUrlPathLength);
    if(parts.dwExtraInfoLength) path.append(parts.lpszExtraInfo,parts.dwExtraInfoLength);
    Internet session{WinHttpOpen(L"DesktopSuite/" SUITE_VERSION_W,WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
    require(session.h!=nullptr,"Cannot initialize update network session");
    WinHttpSetTimeouts(session.h,10000,10000,10000,15000);
    Internet connection{WinHttpConnect(session.h,host.c_str(),parts.nPort,0)};
    require(connection.h!=nullptr,"Cannot connect to GitHub");
    Internet request{WinHttpOpenRequest(connection.h,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};
    require(request.h!=nullptr,"Cannot create GitHub request");
    DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    WinHttpSetOption(request.h,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect));
    require(WinHttpSendRequest(request.h,L"Accept: application/vnd.github+json\r\n",static_cast<DWORD>(-1),nullptr,0,0,0) && WinHttpReceiveResponse(request.h,nullptr),"GitHub request failed (network, proxy or TLS)");
    DWORD status=0,size=sizeof(status);
    require(WinHttpQueryHeaders(request.h,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX)!=FALSE,"Missing HTTP status");
    require(status==200,"GitHub returned a non-200 response (possibly rate limited)");
    std::vector<unsigned char> data; unsigned char buffer[32768]; DWORD read=0;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(120);
    for(;;) {
        require(std::chrono::steady_clock::now()<deadline,"Update download timed out");
        require(WinHttpReadData(request.h,buffer,sizeof(buffer),&read)!=FALSE,"Update download interrupted");
        if(!read) break;
        require(data.size()+read<=limit,"Update response exceeds size limit");
        data.insert(data.end(),buffer,buffer+read);
    }
    return data;
}
std::string sha256(const std::vector<unsigned char>& bytes) {
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    require(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"Cannot initialize SHA256");
    unsigned char hash[32];
    auto status=BCryptHash(algorithm,nullptr,0,const_cast<PUCHAR>(bytes.data()),static_cast<ULONG>(bytes.size()),hash,sizeof(hash));
    BCryptCloseAlgorithmProvider(algorithm,0);
    require(status>=0,"SHA256 failed");
    std::string text; const char* hex="0123456789abcdef";
    for(auto b:hash) { text+=hex[b>>4]; text+=hex[b&15]; } return text;
}
bool launch(const fs::path& exe,const std::wstring& arguments=L"") {
    std::wstring command=L"\""+exe.wstring()+L"\" "+arguments;
    STARTUPINFOW si{sizeof(si)}; PROCESS_INFORMATION pi{};
    if(!CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,exe.parent_path().c_str(),&si,&pi)) return false;
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess); return true;
}
unsigned long long fileVersion(const fs::path& path) {
    DWORD unused=0,size=GetFileVersionInfoSizeW(path.c_str(),&unused);
    if(!size) return 0;
    std::vector<unsigned char> buffer(size); VS_FIXEDFILEINFO* info=nullptr; UINT length=0;
    if(!GetFileVersionInfoW(path.c_str(),0,size,buffer.data()) || !VerQueryValueW(buffer.data(),L"\\",reinterpret_cast<void**>(&info),&length) || length<sizeof(*info)) return 0;
    return (static_cast<unsigned long long>(info->dwFileVersionMS)<<32)|info->dwFileVersionLS;
}
}
fs::path executablePath() {
    std::wstring buffer(32768,L'\0'); DWORD size=GetModuleFileNameW(nullptr,buffer.data(),static_cast<DWORD>(buffer.size()));
    require(size>0 && size<buffer.size(),"Cannot locate executable"); buffer.resize(size); return fs::path(buffer);
}
fs::path userDirectory() {
    PWSTR value=nullptr;
    require(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&value)),"Cannot locate LocalAppData");
    fs::path path(value); CoTaskMemFree(value); path/=L"DesktopSuite";
    fs::create_directories(path); return path;
}
bool preference(const wchar_t* name,bool fallback) {
    DWORD value=0,size=sizeof(value);
    return RegGetValueW(HKEY_CURRENT_USER,preferencesKey,name,RRF_RT_REG_DWORD,nullptr,&value,&size)==ERROR_SUCCESS?value!=0:fallback;
}
void setPreference(const wchar_t* name,bool enabled) {
    HKEY key=nullptr; require(RegCreateKeyExW(HKEY_CURRENT_USER,preferencesKey,0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr)==ERROR_SUCCESS,"Cannot save preference");
    DWORD value=enabled?1:0; auto code=RegSetValueExW(key,name,0,REG_DWORD,reinterpret_cast<const BYTE*>(&value),sizeof(value)); RegCloseKey(key);
    require(code==ERROR_SUCCESS,"Cannot save preference");
}
void setAutoRun(bool enabled) {
    HKEY key=nullptr;
    require(RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr)==ERROR_SUCCESS,"Cannot open startup settings");
    std::wstring command=L"\""+(userDirectory()/L"bin"/L"DesktopSuite.exe").wstring()+L"\"";
    auto code=enabled?RegSetValueExW(key,L"DesktopSuite",0,REG_SZ,reinterpret_cast<const BYTE*>(command.c_str()),static_cast<DWORD>((command.size()+1)*sizeof(wchar_t))):RegDeleteValueW(key,L"DesktopSuite");
    RegCloseKey(key); require(code==ERROR_SUCCESS || (!enabled && code==ERROR_FILE_NOT_FOUND),"Cannot change startup setting");
    setPreference(L"AutoRun",enabled);
}
bool bootstrapUserInstall() {
    auto current=executablePath(),target=userDirectory()/L"bin"/L"DesktopSuite.exe";
    fs::create_directories(target.parent_path());
    if(fs::equivalent(current.parent_path(),target.parent_path())) return false;
    if(!fs::exists(target) || fileVersion(current)>fileVersion(target)) {
        HANDLE existing=OpenMutexW(SYNCHRONIZE,FALSE,L"Local\\DesktopSuite.Milestone1");
        if(existing) { CloseHandle(existing); throw std::runtime_error("Please quit Desktop Suite from the tray before installing a newer version."); }
        fs::copy_file(current,target,fs::copy_options::overwrite_existing);
    }
    require(launch(target),"Cannot start the per-user installed application"); return true;
}
bool handleUpdateCommand() {
    int count=0; auto argv=CommandLineToArgvW(GetCommandLineW(),&count);
    if(!argv) return false;
    bool mode=count==3 && std::wstring_view(argv[1])==L"--apply-update";
    DWORD pid=mode?wcstoul(argv[2],nullptr,10):0; LocalFree(argv);
    if(!mode) return false;
    require(pid!=0,"Invalid update parent PID");
    HANDLE parent=OpenProcess(SYNCHRONIZE,FALSE,pid);
    if(parent) { auto wait=WaitForSingleObject(parent,60000); CloseHandle(parent); require(wait==WAIT_OBJECT_0,"Application did not exit for update"); }
    else require(GetLastError()==ERROR_INVALID_PARAMETER,"Cannot wait for the update parent process");
    auto target=userDirectory()/L"bin"/L"DesktopSuite.exe",backup=target; backup+=L".bak";
    // Both target and backup are fixed per-user paths; no arbitrary shell/target arguments.
    require(MoveFileExW(target.c_str(),backup.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE,"Cannot back up current executable");
    if(!CopyFileW(executablePath().c_str(),target.c_str(),FALSE)) {
        MoveFileExW(backup.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING); launch(target);
        throw std::runtime_error("Update replacement failed; previous version restored");
    }
    if(!launch(target)) {
        DeleteFileW(target.c_str()); MoveFileExW(backup.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING); launch(target);
        throw std::runtime_error("New executable could not launch; previous version restored");
    }
    return true;
}
AutoUpdate::AutoUpdate(HWND owner):owner_(owner),worker_([this](std::stop_token stop){run(stop);}) {}
AutoUpdate::~AutoUpdate() { worker_.request_stop(); wake_.notify_all(); }
void AutoUpdate::check(bool manual) {
    {std::lock_guard lock(mutex_); requested_=true; manual_=manual;} wake_.notify_all();
}
AutoUpdate::Result AutoUpdate::result() { std::lock_guard lock(mutex_); return result_; }
void AutoUpdate::run(std::stop_token stop) {
    bool first=true;
    while(!stop.stop_requested()) {
        bool manual=false,runCheck=false;
        {
            std::unique_lock lock(mutex_);
            if(!first) wake_.wait_for(lock,stop,std::chrono::hours(1),[this]{return requested_;});
            if(stop.stop_requested()) break;
            manual=manual_; runCheck=requested_ || preference(L"AutoUpdate"); requested_=false; manual_=false; first=false;
        }
        if(!runCheck) continue;
        Result output;
        try { output=fetch(manual); } catch(const std::exception& e) { output={e.what(),false,manual}; }
        {std::lock_guard lock(mutex_); result_=output;}
        if(!stop.stop_requested()) PostMessageW(owner_,WM_SUITE_UPDATE,0,0);
    }
}
AutoUpdate::Result AutoUpdate::fetch(bool manual) {
    auto bytes=download(api,1024*1024);
    auto release=nlohmann::json::parse(bytes);
    if(release.value("draft",true) || release.value("prerelease",true)) return {"No stable release available.",false,manual};
    std::string tag=release.at("tag_name").get<std::string>();
    if(!newer(tag,SUITE_VERSION)) return {"You are running the latest stable version.",false,manual};
    for(const auto& asset:release.at("assets")) {
        if(asset.value("name","")!="DesktopSuite.exe") continue;
        std::string url=asset.at("browser_download_url").get<std::string>();
        std::string digest=asset.value("digest",nlohmann::json()).is_string()?asset["digest"].get<std::string>():"";
        require(url==assetPrefix+tag+"/DesktopSuite.exe","Untrusted update download URL");
        require(validDigest(digest),"Release is missing a valid GitHub SHA256 digest; update refused");
        require(asset.at("size").get<size_t>()>0 && asset.at("size").get<size_t>()<=64*1024*1024,"Invalid executable size");
        auto exe=download(std::wstring(url.begin(),url.end()),64*1024*1024);
        require(exe.size()==asset.at("size").get<size_t>() && exe.size()>2 && exe[0]=='M' && exe[1]=='Z',"Downloaded executable is incomplete or invalid");
        require("sha256:"+sha256(exe)==digest,"Update SHA256 verification failed");
        auto dir=userDirectory()/L"updates"; fs::create_directories(dir);
        auto path=dir/L"DesktopSuite-next.exe";
        std::ofstream out(path,std::ios::binary|std::ios::trunc); out.write(reinterpret_cast<const char*>(exe.data()),static_cast<std::streamsize>(exe.size())); out.close();
        require(out.good(),"Cannot stage update on disk");
        require(fileVersion(path)>fileVersion(executablePath()),"Downloaded executable version does not advance the current version");
        return {"Verified update "+tag+" ready to install.",true,manual};
    }
    return {"The newer release has no DesktopSuite.exe asset yet. Will retry later.",false,manual};
}
bool AutoUpdate::apply() {
    return launch(userDirectory()/L"updates"/L"DesktopSuite-next.exe",L"--apply-update "+std::to_wstring(GetCurrentProcessId()));
}
}
