#pragma once
#include <string>

namespace LegendaryImpactEventmanager
{
    class HttpClient
    {
    public:
        bool Get(const std::string& url, std::string& response, std::string& error) const;

    private:
        static std::string ReadResponse(void* requestHandle);
    };
}
