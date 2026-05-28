#include "EventService.h"
#include "Constants.h"
#include "Utility.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace
{
    std::string JsonString(const json& item, const char* key, const std::string& fallback = "")
    {
        if (!item.contains(key) || !item[key].is_string()) return fallback;
        return item[key].get<std::string>();
    }
}

namespace LegendaryImpactEventmanager
{
    EventService::EventService(SharedState& sharedState, HttpClient& httpClient)
        : m_SharedState(sharedState), m_HttpClient(httpClient)
    {
    }

    std::string EventService::BuildEventsUrl(const PluginConfig& config) const
    {
        if (!config.token.empty()) return std::string(Constants::ApiBaseUrl) + "/api/nexus/events/" + config.token;
        return std::string(Constants::ApiBaseUrl) + "/api/nexus/events/public";
    }

    void EventService::StoreError(const std::string& error)
    {
        auto oldState = m_SharedState.GetState();
        auto nextState = std::make_shared<PluginState>();
        if (oldState)
        {
            nextState->events = oldState->events;
            nextState->viewerUsername = oldState->viewerUsername;
            nextState->viewerGw2Account = oldState->viewerGw2Account;
            nextState->lastSync = oldState->lastSync;
        }
        nextState->lastSync = "Fehler: " + Utility::UiText(error);
        m_SharedState.SetState(nextState);
    }

    void EventService::FetchEvents()
    {
        if (!m_SharedState.TryBeginFetch()) return;

        auto onExit = [&]() { m_SharedState.EndFetch(); };
        auto config = m_SharedState.GetConfig();
        if (!config)
        {
            StoreError("Keine Konfiguration vorhanden.");
            onExit();
            return;
        }

        std::string body;
        std::string error;
        std::string url = BuildEventsUrl(*config);

        if (!m_HttpClient.Get(url, body, error))
        {
            StoreError(error);
            onExit();
            return;
        }

        try
        {
            json data = json::parse(body);
            auto nextState = std::make_shared<PluginState>();
            nextState->lastSync = Utility::FormatLocalNow();

            if (data.contains("viewer") && data["viewer"].is_object())
            {
                nextState->viewerUsername = Utility::UiText(JsonString(data["viewer"], "username"));
                nextState->viewerGw2Account = Utility::UiText(JsonString(data["viewer"], "gw2Account"));
            }

            if (data.contains("events") && data["events"].is_array())
            {
                for (const auto& item : data["events"])
                {
                    EventItem event;
                    event.id = JsonString(item, "id");
                    event.title = Utility::UiText(JsonString(item, "title", "Unbenannt"));
                    event.description = Utility::UiText(JsonString(item, "description"));
                    event.location = Utility::UiText(JsonString(item, "location"));
                    event.start = JsonString(item, "start");
                    event.end = JsonString(item, "end");
                    event.tag = JsonString(item, "tag");
                    event.url = JsonString(item, "url");
                    event.isViewerAttending = item.value("isViewerAttending", false);

                    if (item.contains("creator") && item["creator"].is_object())
                    {
                        event.leaderName = Utility::UiText(JsonString(item["creator"], "username"));
                        event.leaderAccount = Utility::UiText(JsonString(item["creator"], "gw2Account"));
                    }

                    if (item.contains("_count") && item["_count"].contains("attendees"))
                    {
                        event.attendeeCount = item["_count"].value("attendees", 0);
                    }

                    if (item.contains("roleSlots") && item["roleSlots"].is_array())
                    {
                        for (const auto& slot : item["roleSlots"])
                        {
                            event.slotCount += slot.value("count", 0);
                        }
                    }

                    if (item.contains("attendees") && item["attendees"].is_array())
                    {
                        for (const auto& attendeeJson : item["attendees"])
                        {
                            EventAttendee attendee;
                            if (attendeeJson.contains("user") && attendeeJson["user"].is_object())
                            {
                                attendee.username = Utility::UiText(JsonString(attendeeJson["user"], "username"));
                                attendee.gw2Account = Utility::UiText(JsonString(attendeeJson["user"], "gw2Account"));
                            }
                            if (attendeeJson.contains("slot") && attendeeJson["slot"].is_object())
                            {
                                attendee.role = JsonString(attendeeJson["slot"], "group");
                            }
                            attendee.boon = JsonString(attendeeJson, "selectedBoon");

                            if (attendeeJson.contains("flexRoles") && attendeeJson["flexRoles"].is_array())
                            {
                                for (const auto& flexJson : attendeeJson["flexRoles"])
                                {
                                    EventFlexRole flexRole;
                                    flexRole.role = JsonString(flexJson, "group");
                                    flexRole.boon = JsonString(flexJson, "boon");
                                    if (!flexRole.role.empty() || !flexRole.boon.empty()) attendee.flexRoles.push_back(flexRole);
                                }
                            }
                            event.attendees.push_back(attendee);
                        }
                    }
                    nextState->events.push_back(event);
                }
            }
            m_SharedState.SetState(nextState);
        }
        catch (const std::exception& ex)
        {
            StoreError(std::string("JSON Fehler: ") + ex.what());
        }

        onExit();
    }
}
