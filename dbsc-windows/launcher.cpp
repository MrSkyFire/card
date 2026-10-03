#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <sstream>
#pragma comment(lib,"ws2_32.lib")

static HWND gOut,gInput,gStatus,gStart,gStop,gRestart,gSend;
static SOCKET gSock=INVALID_SOCKET;
static PROCESS_INFORMATION gPi{};
static HANDLE gPipeRead=nullptr;
static std::atomic<bool> gRunning{false};
static const UINT WM_APPEND_TEXT=WM_APP+1;
static const UINT WM_STATUS_TEXT=WM_APP+2;

static std::string exeDir(){
    char p[MAX_PATH]; GetModuleFileNameA(nullptr,p,MAX_PATH);
    std::string s=p; auto x=s.find_last_of("\\/"); return x==std::string::npos?".":s.substr(0,x);
}
static void postText(const std::string&s){
    auto *p=new std::string(s); PostMessageA(GetParent(gOut),WM_APPEND_TEXT,0,(LPARAM)p);
}
static void postStatus(const std::string&s){
    auto *p=new std::string(s); PostMessageA(GetParent(gOut),WM_STATUS_TEXT,0,(LPARAM)p);
}
static std::string stripAnsi(const std::string& in){
    std::string o; bool esc=false,csi=false;
    for(unsigned char c:in){
        if(!esc && c==27){esc=true;continue;}
        if(esc){
            if(!csi && c=='['){csi=true;continue;}
            if(csi && ((c>='0'&&c<='9')||c==';'||c=='?'||c==' ')) continue;
            esc=false;csi=false;continue;
        }
        if(c!='\0') o.push_back((char)c);
    }
    return o;
}
static void pumpServerLog(){
    char buf[2048]; DWORD n;
    while(gPipeRead && ReadFile(gPipeRead,buf,sizeof(buf)-1,&n,nullptr) && n){
        buf[n]=0; postText(std::string("[SERVER] ")+buf);
    }
}
static bool startServer(){
    if(gRunning) return true;
    std::string dir=exeDir();
    std::string server=dir+"\\dbsaga-server.exe";
    DWORD a=GetFileAttributesA(server.c_str());
    if(a==INVALID_FILE_ATTRIBUTES){postStatus("Server executable missing");postText("[LAUNCHER] dbsaga-server.exe not found.\r\n");return false;}

    SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};
    HANDLE wr=nullptr;
    if(!CreatePipe(&gPipeRead,&wr,&sa,0)) return false;
    SetHandleInformation(gPipeRead,HANDLE_FLAG_INHERIT,0);

    STARTUPINFOA si{}; si.cb=sizeof(si);
    si.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;
    si.hStdOutput=wr; si.hStdError=wr; si.hStdInput=GetStdHandle(STD_INPUT_HANDLE); si.wShowWindow=SW_HIDE;
    std::string cmd="\""+server+"\" /run";
    std::vector<char> mutableCmd(cmd.begin(),cmd.end()); mutableCmd.push_back(0);
    BOOL ok=CreateProcessA(server.c_str(),mutableCmd.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,dir.c_str(),&si,&gPi);
    CloseHandle(wr);
    if(!ok){CloseHandle(gPipeRead);gPipeRead=nullptr;postStatus("Server failed to launch");return false;}
    gRunning=true; postStatus("Server starting on port 4000...");
    std::thread(pumpServerLog).detach();
    return true;
}
static void closeClient(){
    if(gSock!=INVALID_SOCKET){shutdown(gSock,SD_BOTH);closesocket(gSock);gSock=INVALID_SOCKET;}
}
static void stopServer(){
    closeClient();
    if(gRunning){
        TerminateProcess(gPi.hProcess,0);
        WaitForSingleObject(gPi.hProcess,3000);
        CloseHandle(gPi.hThread); CloseHandle(gPi.hProcess);
        ZeroMemory(&gPi,sizeof(gPi)); gRunning=false;
    }
    if(gPipeRead){CloseHandle(gPipeRead);gPipeRead=nullptr;}
    postStatus("Stopped");
}
static void clientReader(){
    char b[4096];
    while(gSock!=INVALID_SOCKET){
        int n=recv(gSock,b,sizeof(b),0); if(n<=0) break;
        std::string s(b,b+n);
        // minimal telnet IAC filtering
        std::string o;
        for(size_t i=0;i<s.size();++i){
            unsigned char c=(unsigned char)s[i];
            if(c==255){
                if(i+1<s.size() && (unsigned char)s[i+1]==255){o.push_back((char)255);++i;continue;}
                if(i+2<s.size()){i+=2;continue;}
                continue;
            }
            o.push_back((char)c);
        }
        postText(stripAnsi(o));
    }
    postStatus("Local client disconnected (server may still be running)");
}
static void connectLocal(){
    std::thread([]{
        if(!gRunning && !startServer()) return;
        postStatus("Waiting for local server...");
        for(int i=0;i<60;i++){
            SOCKET s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
            sockaddr_in a{}; a.sin_family=AF_INET; a.sin_port=htons(4000); inet_pton(AF_INET,"127.0.0.1",&a.sin_addr);
            if(connect(s,(sockaddr*)&a,sizeof(a))==0){gSock=s;postStatus("DBSC running • local client connected • port 4000 open");std::thread(clientReader).detach();return;}
            closesocket(s); Sleep(250);
        }
        postStatus("Server did not open port 4000");
    }).detach();
}
static void sendLine(){
    if(gSock==INVALID_SOCKET) return;
    int len=GetWindowTextLengthA(gInput); std::string s(len,'\0'); GetWindowTextA(gInput,s.data(),len+1);
    s+="\r\n"; send(gSock,s.c_str(),(int)s.size(),0); SetWindowTextA(gInput,"");
}
static LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_CREATE:{
        gStatus=CreateWindowA("STATIC","Stopped",WS_CHILD|WS_VISIBLE,10,8,760,22,h,nullptr,nullptr,nullptr);
        gOut=CreateWindowExA(WS_EX_CLIENTEDGE,"EDIT","",WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY,10,35,760,440,h,nullptr,nullptr,nullptr);
        gInput=CreateWindowExA(WS_EX_CLIENTEDGE,"EDIT","",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,10,485,600,28,h,(HMENU)101,nullptr,nullptr);
        gSend=CreateWindowA("BUTTON","Send",WS_CHILD|WS_VISIBLE,620,485,150,28,h,(HMENU)102,nullptr,nullptr);
        gStart=CreateWindowA("BUTTON","Start + Connect",WS_CHILD|WS_VISIBLE,10,525,180,30,h,(HMENU)103,nullptr,nullptr);
        gStop=CreateWindowA("BUTTON","Stop Server",WS_CHILD|WS_VISIBLE,200,525,180,30,h,(HMENU)104,nullptr,nullptr);
        gRestart=CreateWindowA("BUTTON","Restart + Connect",WS_CHILD|WS_VISIBLE,390,525,190,30,h,(HMENU)105,nullptr,nullptr);
        HFONT f=(HFONT)GetStockObject(ANSI_FIXED_FONT); SendMessageA(gOut,WM_SETFONT,(WPARAM)f,TRUE); SendMessageA(gInput,WM_SETFONT,(WPARAM)f,TRUE);
        break;}
    case WM_COMMAND:
        switch(LOWORD(w)){
            case 102: sendLine(); break;
            case 103: connectLocal(); break;
            case 104: stopServer(); break;
            case 105: stopServer(); Sleep(300); connectLocal(); break;
        } break;
    case WM_APPEND_TEXT:{
        std::string *s=(std::string*)l; int n=GetWindowTextLengthA(gOut); SendMessageA(gOut,EM_SETSEL,n,n); SendMessageA(gOut,EM_REPLACESEL,FALSE,(LPARAM)s->c_str()); delete s; break;}
    case WM_STATUS_TEXT:{std::string*s=(std::string*)l;SetWindowTextA(gStatus,s->c_str());delete s;break;}
    case WM_SIZE:{
        int W=LOWORD(l),H=HIWORD(l);
        MoveWindow(gStatus,10,8,W-20,22,TRUE);
        MoveWindow(gOut,10,35,W-20,H-150,TRUE);
        MoveWindow(gInput,10,H-105,W-180,28,TRUE);
        MoveWindow(gSend,W-160,H-105,150,28,TRUE);
        MoveWindow(gStart,10,H-65,180,30,TRUE);MoveWindow(gStop,200,H-65,180,30,TRUE);MoveWindow(gRestart,390,H-65,190,30,TRUE);
        break;}
    case WM_CLOSE: stopServer(); DestroyWindow(h); break;
    case WM_DESTROY: PostQuitMessage(0); break;
    default:return DefWindowProcA(h,m,w,l);
    } return 0;
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE,LPSTR,int){
    WSADATA wd; WSAStartup(MAKEWORD(2,2),&wd);
    WNDCLASSA wc{};wc.lpfnWndProc=WndProc;wc.hInstance=hi;wc.lpszClassName="DBSCLauncher";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);RegisterClassA(&wc);
    HWND h=CreateWindowA("DBSCLauncher","DragonBall Saga - Local Server",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,820,640,nullptr,nullptr,hi,nullptr);
    ShowWindow(h,SW_SHOW);UpdateWindow(h);
    connectLocal();
    MSG msg; while(GetMessageA(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageA(&msg);}
    WSACleanup();return 0;
}
