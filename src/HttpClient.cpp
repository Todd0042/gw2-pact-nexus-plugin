#include "HttpClient.h"
#include "Utility.h"
#include <Windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

namespace LegendaryImpactEventmanager
{
    std::string HttpClient::ReadResponse(void* requestHandle)
    {
        auto request = static_cast<HINTERNET>(requestHandle);
        std::string body;
        DWORD available = 0;

        while (WinHttpQueryDataAvailable(request, &available) && available > 0)
        {
            std::string buffer(available, '\0');
            DWORD downloaded = 0;
            if (!WinHttpReadData(request, buffer.data(), available, &downloaded)) break;
            buffer.resize(downloaded);
            body += buffer;
        }
        return body;
    }

    bool HttpClient::Get(const std::string& url, std::string& response, std::string& error) const
    {
        response.clear();
        error.clear();

        std::wstring wideUrl = Utility::ToWide(url);
        if (wideUrl.empty()) { error = "Ungueltige URL."; return false; }

        URL_COMPONENTS components = {};
        components.dwStructSize = sizeof(components);
        wchar_t host[256] = {};
        wchar_t path[2048] = {};
        components.lpszHostName = host;
        components.dwHostNameLength = ARRAYSIZE(host);
        components.lpszUrlPath = path;
        components.dwUrlPathLength = ARRAYSIZE(path);

        if (!WinHttpCrackUrl(&wideUrl[0], 0, 0, &components))
        {
            error = "URL konnte nicht verarbeitet werden.";
            return false;
        }

        const bool isHttps = components.nScheme == INTERNET_SCHEME_HTTPS;
        HINTERNET session = WinHttpOpen(L"LegendaryImpactEventmanager/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session) { error = "WinHttpOpen fehlgeschlagen."; return false; }
        WinHttpSetTimeouts(session, 5000, 5000, 5000, 10000);

        HINTERNET connection = WinHttpConnect(session, components.lpszHostName, components.nPort, 0);
        if (!connection)
        {
            WinHttpCloseHandle(session);
            error = "WinHttpConnect fehlgeschlagen.";
            return false;
        }

        HINTERNET request = WinHttpOpenRequest(connection, L"GET", components.lpszUrlPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, isHttps ? WINHTTP_FLAG_SECURE : 0);
        if (!request)
        {
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            error = "WinHttpOpenRequest fehlgeschlagen.";
            return false;
        }

        BOOL sent = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
        if (!sent || !WinHttpReceiveResponse(request, nullptr))
        {
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            error = "Request fehlgeschlagen.";
            return false;
        }

        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

        response = ReadResponse(request);

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);

        if (statusCode < 200 || statusCode >= 300)
        {
            error = "HTTP Fehler: " + std::to_string(statusCode);
            return false;
        }
        return true;
    }
}
