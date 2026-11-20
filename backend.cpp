#include "backend.h"
#include <QObject>
#include <QDebug>
#include <iostream>
#include <windows.h>
#include <winuser.h>

int d =0;

QTextStream& qStdOut()
{
    static QTextStream ts(stdout);
    return ts;
}

void BackEnd::handleButtonClick()
{   
    d++;
    qDebug()<<"\nMethuk: " <<d;
}

void BackEnd::testSS()
{
    int x1, y1, width, height;

    
    x1  = GetSystemMetrics(SM_XVIRTUALSCREEN);
    y1  = GetSystemMetrics(SM_YVIRTUALSCREEN);
    width  = GetSystemMetrics(SM_CXVIRTUALSCREEN) -x1;
    height = GetSystemMetrics(SM_CYVIRTUALSCREEN) -y1;

    HDC hScreen = GetDC(nullptr);
    HDC hMem = CreateCompatibleDC(hScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen,width,height);
    HGDIOBJ old_obj = SelectObject(hMem,hBitmap);
    BOOL    bRet    = BitBlt(hMem, 0, 0, width, height, hScreen, x1, y1, SRCCOPY);

    OpenClipboard(NULL);
    EmptyClipboard();
    SetClipboardData(CF_BITMAP, hBitmap);
    CloseClipboard();   

    
    SelectObject(hScreen, old_obj);
    ReleaseDC(NULL, hScreen);
    DeleteDC(hScreen);
    DeleteObject(hBitmap);
}


#include "backend.moc"