// This place only defines the GUI logic
// It also has a header but that one only includes the libraries ShlObj.h (for folder selection), commdlg.h (for file selection) and of course ISDM.h

#include "framework.h"
#include "UI.h"


#define MAX_LOADSTRING 100
#define SELECTFILE 	1000
#define SETDEST 1001

// global variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // Title Text
WCHAR szWindowClass[MAX_LOADSTRING];            // Der Klassenname des Hauptfensters.
HWND hTitle, hBtn1, hBtn2;						// status text (which mod is loaded), first button, lower button
ISDM* dtaFileInst;								// global and single instance from ISDM class to use anywhere here
HANDLE hThread;									// thread handle

static uint32_t fileCounter;					// will be passed to ISDM class to count up files when writing
static bool userCancel;							// bool will inform ISDM if file writing should be cancelled or not

// function declarations:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);	// main window
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);		// about info window
INT_PTR CALLBACK    Progress(HWND, UINT, WPARAM, LPARAM);	// progress window
DWORD WINAPI		thrOutputAllFiles(LPVOID lpParam);		// extra thread for unpacking process

void DrawWindows(HWND);
static std::string OpenFile(HWND, const char*);
static std::string SaveSingleFile(HWND, const char*);
static std::string ToSavePath(HWND);

// define little struct for thread data to passed by
typedef struct threadData {
	HWND hProgressBar;
	HWND hProgressTitle;
	std::string str_savePath;

} THREADDTA, *PTR_THREADDTA;
// and create one instance of it:
PTR_THREADDTA pThreadData;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Hier Code einfügen.

    // Globale Zeichenfolgen initialisieren
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_HD2UNPACKERW, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Anwendungsinitialisierung ausführen:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_HD2UNPACKERW));

    MSG msg;

    // Hauptnachrichtenschleife:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}


// callback proc for browsing folder window
INT CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lp, LPARAM pData)
{
	if (uMsg == BFFM_INITIALIZED) {
		SendMessageA(hwnd, BFFM_SETSELECTIONA, TRUE, (LPARAM)pData);
	}
	return 0;
}




// register window class
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_HD2UNPACKERW));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_HD2UNPACKERW);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}



BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // save instance into global var

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
	   CW_USEDEFAULT, 0, 400, 183, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}


// callback loop main window
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
			case SELECTFILE:
			{
				
				std::string filePath(OpenFile(hWnd, "HD2 DTA file (*.dta)\0*.dta\0"));
				if (!filePath.empty()) {
					

					delete dtaFileInst;
					dtaFileInst = new ISDM(filePath);
					// start verification
					if (dtaFileInst->GetLastError()) {
						char errorMsg[250];
						dtaFileInst->GetLastError(errorMsg);
						dtaFileInst->CloseFile();
						MessageBoxA(NULL, errorMsg, "ERROR", MB_OK | MB_ICONERROR);
					}
					else {
						std::string titleBuff("\"");
						titleBuff.append(dtaFileInst->GetFileName());
						titleBuff.append("\" archive selected.");

						SetWindowTextA(hTitle, titleBuff.c_str());
						EnableWindow(hBtn2, true);
					}

				}
			}
				break;

			case SETDEST:

				
				DialogBox(hInst, MAKEINTRESOURCE(IDD_PROPPAGE_SMALL), hWnd, Progress);

				if (pThreadData != NULL) {
					HeapFree(GetProcessHeap(), 0, pThreadData);
					pThreadData = NULL;

					if (hThread != NULL) {
						WaitForSingleObject(hThread, NULL);	// INFINITE doesent work for some reason
						CloseHandle(hThread);
					}
					hThread = NULL;
				}

				break;
            case IDM_EXIT:
				delete dtaFileInst;
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;

	case WM_CREATE:
		
		DrawWindows(hWnd);
		break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:

        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// About window callback loop
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}


// Progress message callback loop
INT_PTR CALLBACK Progress(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{

	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:

		fileCounter = 0;
		pThreadData = (PTR_THREADDTA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(THREADDTA));
		DWORD dwThread;

		pThreadData->str_savePath.assign(ToSavePath(hDlg));
		if (pThreadData->str_savePath.empty()) {
			HeapFree(GetProcessHeap(), 0, pThreadData);
			pThreadData = NULL;
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}

		pThreadData->str_savePath.push_back('\\');

		pThreadData->hProgressBar = GetDlgItem(hDlg, IDC_PROGRESS1);
		pThreadData->hProgressTitle = GetDlgItem(hDlg, IDC_STATIC2);

		SendMessage(pThreadData->hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, dtaFileInst->GetFileCount()));
		SendMessage(pThreadData->hProgressBar, PBM_SETSTEP, (WPARAM)1, 0);

		if (pThreadData == NULL) {
			// if allocation fails, system is out of memory. stop further execution:
			// (also no point in outputting an error message)
			ExitProcess(2);
		}

		// I set the bool here, so the other thead only reads this, while main thread can modify
		userCancel = false;
		hThread = CreateThread(NULL, 0, thrOutputAllFiles, pThreadData, 0, &dwThread);

		if (hThread == NULL) {
			MessageBoxA(hDlg, "Thread creation failed! Program will be closed", "Error", MB_OK | MB_ICONERROR);
			ExitProcess(3);
			return (INT_PTR)FALSE;
		}

		return (INT_PTR)TRUE;

	case WM_COMMAND:

		int wmId = LOWORD(wParam);

		switch (wmId) {

		case IDCANCEL:

			userCancel = true;
			WaitForSingleObject(hThread, 100);
			CloseHandle(hThread);
			if (pThreadData != NULL) {
				HeapFree(GetProcessHeap(), 0, pThreadData);
				pThreadData = NULL;
				hThread = NULL;
			}

			std::string finalMsg("File extraction cancelled!\nOnly ");
			finalMsg.append(std::to_string(fileCounter));
			finalMsg.append(" files of ");
			finalMsg.append(std::to_string(dtaFileInst->GetFileCount()));
			finalMsg.append("\nfiles unpacked!");
			MessageBoxA(hDlg, finalMsg.c_str(), "Info", MB_OK | MB_ICONWARNING);

			EndDialog(hDlg, LOWORD(wParam));


			return (INT_PTR)TRUE;
			break;
		}
		break;
	}
	
	return (INT_PTR)FALSE;
}


void DrawWindows(HWND hWnd) {
	
	HFONT titleFont = CreateFont(16, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
		CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, TEXT("MS Shell Dlg"));

	SendMessage(hTitle = CreateWindow(L"Static", L"Please select the .dta file you want to unpack", WS_VISIBLE | WS_CHILD | SS_CENTER, 0, 0, 390, 18, hWnd, NULL, NULL, NULL), WM_SETFONT, WPARAM(titleFont), TRUE);
	HFONT defaultFont = CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
		CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, TEXT("MS Shell Dlg"));
	SendMessage(hBtn1 = CreateWindow(L"Button", L"SELECT FILE", WS_VISIBLE | WS_CHILD, 5, 25, 375, 30, hWnd, (HMENU)SELECTFILE, NULL, NULL), WM_SETFONT, WPARAM(defaultFont), TRUE);
	//SendMessage(hEdit = CreateWindow(L"Edit", L"", WS_DISABLED | ES_NUMBER | WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_BORDER | SS_CENTER, 5, 60, 375, 20, hWnd, NULL, NULL, NULL), WM_SETFONT, WPARAM(defaultFont), TRUE);
	SendMessage(hBtn2 = CreateWindow(L"Button", L"SET DESTINATION AND EXTRACT", WS_DISABLED | WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 5, 90, 375, 30, hWnd, (HMENU)SETDEST, NULL, NULL), WM_SETFONT, WPARAM(defaultFont), TRUE);
}

DWORD WINAPI thrOutputAllFiles(LPVOID lpParam) {

	PTR_THREADDTA thrDataArray = (PTR_THREADDTA)lpParam;

	//dtaFileInst->WriteAllFiles(hProg, hProgTitle, "");
	dtaFileInst->WriteAllFiles(userCancel, fileCounter, thrDataArray->hProgressBar, thrDataArray->hProgressTitle, thrDataArray->str_savePath);
	dtaFileInst->CloseFile();

	if (!userCancel)
		EndDialog(GetParent(thrDataArray->hProgressTitle), LOWORD(IDCANCEL));

	SetWindowTextA(hTitle, "Please select the .dta file you want to unpack");
	EnableWindow(hBtn2, false);

	return 0;
}


static std::string OpenFile(HWND hWnd, const char* filter) {

	OPENFILENAMEA ofn;
	CHAR szFile[MAX_PATH] = { 0 };
	ZeroMemory(&ofn, sizeof(OPENFILENAME));

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = filter;
	ofn.nFilterIndex = 1;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
	if (GetOpenFileNameA(&ofn) == TRUE) {
		return ofn.lpstrFile;
	}
	return std::string();
}



static std::string SaveSingleFile(HWND hWnd, const char* filter) {

	OPENFILENAMEA ofn;
	CHAR szFile[MAX_PATH] = { 0 };
	ZeroMemory(&ofn, sizeof(OPENFILENAME));

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = filter;
	ofn.nFilterIndex = 1;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
	if (GetSaveFileNameA(&ofn) == TRUE) {
		return ofn.lpstrFile;
	}
	return std::string();
}


static std::string ToSavePath(HWND hWnd) {

	// get directoy from selected file to extract
	// so that the user can start browsing from the .dta location
	std::string bowseStart;
	uint32_t find = dtaFileInst->filePath.rfind('\\');
	if (find != std::string::npos) {
		bowseStart = dtaFileInst->filePath.substr(0, find+1);
	}
	else {
		bowseStart = dtaFileInst->filePath;
	}


	BROWSEINFOA bwi;
	ZeroMemory(&bwi, sizeof(BROWSEINFOA));
	bwi.lpfn = BrowseCallbackProc;
	bwi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;
	bwi.hwndOwner = hWnd;
	bwi.lpszTitle = "Please select the folder you want to extract to.";
	bwi.lParam = (LPARAM)bowseStart.c_str();

	LPCITEMIDLIST pidl = NULL;
	if (pidl = SHBrowseForFolderA(&bwi)) {
		CHAR szFolder[MAX_PATH] { 0 };
		if (SHGetPathFromIDListA(pidl, szFolder)) {
			return szFolder;
		}
	}

	return std::string();
}
