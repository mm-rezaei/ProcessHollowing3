#include <windows.h>
#include <tchar.h>
#include <string>

using namespace std;

wstring GetLocationMessage()
{
    wchar_t path[MAX_PATH];

    GetModuleFileName(NULL, path, MAX_PATH);

    wstring pathStr = path;

    wstring message = L"I was injected to '" + pathStr + L"'!";

    return message;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    wstring message = GetLocationMessage();

    MessageBox(NULL, message.c_str(), L"Location Message", MB_OK | MB_ICONINFORMATION);

    return 0;
}
