#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <bcrypt.h>
#include <string>
#include <vector>
#include <cstdio>

static std::wstring directory() {
    wchar_t path[32768]; DWORD n=GetModuleFileNameW(nullptr,path,32768);
    if(!n || n>=32768)return L"";
    std::wstring p(path,n);return p.substr(0,p.find_last_of(L"\\/"));
}
static std::wstring quoted(const std::wstring& s) {
    std::wstring out=L"\"";size_t slashes=0;
    for(wchar_t c:s){
        if(c==L'\\'){slashes++;continue;}
        if(c==L'"')out.append(slashes*2+1,L'\\');
        else out.append(slashes,L'\\');
        slashes=0;out+=c;
    }
    out.append(slashes*2,L'\\');out+=L'"';return out;
}
static bool compatible(const std::wstring& path) {
    FILE *f=_wfopen(path.c_str(),L"rb");if(!f)return false;
    if(fseek(f,0,SEEK_END)!=0){fclose(f);return false;}
    long size=ftell(f);rewind(f);
    if(size<16 || size>8*1024*1024){fclose(f);return false;}
    std::vector<unsigned char> data(static_cast<size_t>(size));
    size_t got=fread(data.data(),1,data.size(),f);fclose(f);
    if(got!=data.size() || data[0]!='N' || data[1]!='E' || data[2]!='S' || data[3]!=0x1a)return false;
    size_t offset=16+((data[6]&4)?512:0);if(offset>=data.size())return false;
    BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    DWORD object_size=0,bytes=0;unsigned char result[32];bool ok=false;
    if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
    if(BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&object_size),sizeof object_size,&bytes,0)>=0){
        std::vector<unsigned char> object(object_size);
        if(BCryptCreateHash(alg,&hash,object.data(),object_size,nullptr,0,0)>=0){
            if(BCryptHashData(hash,data.data()+offset,static_cast<ULONG>(data.size()-offset),0)>=0 && BCryptFinishHash(hash,result,32,0)>=0){
                char hex[65];for(int i=0;i<32;i++)snprintf(hex+i*2,3,"%02x",result[i]);
                ok=std::string(hex)==ROSE_PAYLOAD_SHA256;
            }
            BCryptDestroyHash(hash);
        }
    }
    BCryptCloseAlgorithmProvider(alg,0);return ok;
}
static bool choose(std::wstring& path) {
    std::vector<wchar_t> buffer(32768,0);
    OPENFILENAMEW dialog={};dialog.lStructSize=sizeof dialog;
    dialog.lpstrTitle=L"Rose Recompiled - Selecione sua ROM compativel";
    dialog.lpstrFilter=L"ROM NES (*.nes)\0*.nes\0Todos os arquivos\0*.*\0\0";
    dialog.lpstrFile=buffer.data();dialog.nMaxFile=static_cast<DWORD>(buffer.size());
    dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_EXPLORER;
    if(!GetOpenFileNameW(&dialog))return false;
    path=buffer.data();return true;
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    int count=0;wchar_t **args=CommandLineToArgvW(GetCommandLineW(),&count);if(!args)return 2;
    std::vector<std::wstring> arguments;for(int i=1;i<count;i++)arguments.emplace_back(args[i]);LocalFree(args);
    if(arguments.size()==2 && arguments[0]==L"--check-rom")return compatible(arguments[1])?0:2;
    const std::wstring root=directory(),ini=root+L"\\launcher.ini";
    if(root.empty())return 2;
    bool quiet=false;for(const auto& s:arguments)if(s==L"--frames"||s==L"--headless"||s==L"--hidden")quiet=true;
    if(arguments.empty()){
        wchar_t saved[32768]={};GetPrivateProfileStringW(L"Launcher",L"ROM",L"",saved,32768,ini.c_str());
        std::wstring rom=saved;
        while(!compatible(rom)){
            if(!rom.empty())MessageBoxW(nullptr,L"Esta ROM nao e compativel com esta build beta. Selecione a revisao indicada no LEIA-ME.",L"Rose Recompiled",MB_OK|MB_ICONINFORMATION);
            if(!choose(rom))return 0;
        }
        if(!WritePrivateProfileStringW(L"Launcher",L"ROM",rom.c_str(),ini.c_str()))
            MessageBoxW(nullptr,L"Nao foi possivel lembrar a ROM. Extraia o pacote para uma pasta com permissao de escrita.",L"Rose Recompiled",MB_OK|MB_ICONINFORMATION);
        arguments.push_back(rom);
    }else if(arguments[0].empty() || arguments[0][0]==L'-' || !compatible(arguments[0])){
        if(!quiet)MessageBoxW(nullptr,L"ROM ausente ou incompativel com esta build beta.",L"Rose Recompiled",MB_OK|MB_ICONERROR);
        return 2;
    }
    const std::wstring runtime=root+L"\\runtime\\RoseRuntime.exe";
    std::wstring command=quoted(runtime);for(const auto& s:arguments)command+=L" "+quoted(s);
    SECURITY_ATTRIBUTES sa={sizeof sa,nullptr,TRUE};
    HANDLE log=CreateFileW((root+L"\\beta.log").c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    HANDLE input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr);
    if(log==INVALID_HANDLE_VALUE || input==INVALID_HANDLE_VALUE){
        if(log!=INVALID_HANDLE_VALUE)CloseHandle(log);if(input!=INVALID_HANDLE_VALUE)CloseHandle(input);
        if(!quiet)MessageBoxW(nullptr,L"Nao foi possivel criar beta.log. Extraia o pacote para uma pasta com permissao de escrita.",L"Rose Recompiled",MB_OK|MB_ICONERROR);
        return 2;
    }
    STARTUPINFOW startup={};startup.cb=sizeof startup;startup.dwFlags=STARTF_USESTDHANDLES;
    startup.hStdOutput=log;startup.hStdError=log;startup.hStdInput=input;
    PROCESS_INFORMATION process={};
    BOOL started=CreateProcessW(runtime.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,root.c_str(),&startup,&process);
    CloseHandle(log);CloseHandle(input);
    if(!started){if(!quiet)MessageBoxW(nullptr,L"Nao foi possivel iniciar. Extraia o ZIP completo, incluindo a pasta runtime.",L"Rose Recompiled",MB_OK|MB_ICONERROR);return 2;}
    CloseHandle(process.hThread);WaitForSingleObject(process.hProcess,INFINITE);
    DWORD code=2;GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hProcess);
    if(code && !quiet)MessageBoxW(nullptr,L"O jogo encerrou com erro. Envie o arquivo beta.log ao responsavel pelo teste.",L"Rose Recompiled",MB_OK|MB_ICONERROR);
    return static_cast<int>(code);
}
