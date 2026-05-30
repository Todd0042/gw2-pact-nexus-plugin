#include "EventService.h"
#include "Constants.h"
#include "Utility.h"
#include "nlohmann/json.hpp"
#include <fstream>
#include <unordered_set>
#include <direct.h>
#include <utility>

using json = nlohmann::json;

namespace
{
    std::string JsonString(const json& item, const char* key, const std::string& fallback = "")
    {
        if (!item.contains(key) || !item[key].is_string()) return fallback;
        return item[key].get<std::string>();
    }

    void EventToJson(nlohmann::json& target, const LegendaryImpactEventmanager::EventItem& event)
    {
        target["id"] = event.id;
        target["title"] = event.title;
        target["description"] = event.description;
        target["start"] = event.start;
        target["end"] = event.end;
        target["tag"] = event.tag;
        target["location"] = event.location;
        target["url"] = event.url;
        target["leaderName"] = event.leaderName;
        target["leaderAccount"] = event.leaderAccount;
        target["isPublic"] = event.isPublic;
        target["isViewerAttending"] = event.isViewerAttending;
        target["attendeeCount"] = event.attendeeCount;
        target["slotCount"] = event.slotCount;
        target["attendees"] = nlohmann::json::array();

        for (const auto& attendee : event.attendees)
        {
            nlohmann::json attendeeJson;
            attendeeJson["username"] = attendee.username;
            attendeeJson["gw2Account"] = attendee.gw2Account;
            attendeeJson["role"] = attendee.role;
            attendeeJson["boon"] = attendee.boon;
            attendeeJson["flexRoles"] = nlohmann::json::array();

            for (const auto& flex : attendee.flexRoles)
            {
                attendeeJson["flexRoles"].push_back({
                    { "role", flex.role },
                    { "boon", flex.boon }
                    });
            }
            target["attendees"].push_back(attendeeJson);
        }
    }

    LegendaryImpactEventmanager::EventItem EventFromJson(const nlohmann::json& item)
    {
        LegendaryImpactEventmanager::EventItem event;
        event.id = item.value("id", "");
        event.title = item.value("title", "Unbenannt");
        event.description = item.value("description", "");
        event.start = item.value("start", "");
        event.end = item.value("end", "");
        event.tag = item.value("tag", "");
        event.location = item.value("location", "");
        event.url = item.value("url", "");
        event.leaderName = item.value("leaderName", "");
        event.leaderAccount = item.value("leaderAccount", "");
        event.isPublic = item.value("isPublic", false);
        event.isViewerAttending = item.value("isViewerAttending", false);
        event.attendeeCount = item.value("attendeeCount", 0);
        event.slotCount = item.value("slotCount", 0);
        if (item.contains("attendees") && item["attendees"].is_array())
        {
            for (const auto& attendeeJson : item["attendees"])
            {
                LegendaryImpactEventmanager::EventAttendee attendee;
                attendee.username = attendeeJson.value("username", "");
                attendee.gw2Account = attendeeJson.value("gw2Account", "");
                attendee.role = attendeeJson.value("role", "");
                attendee.boon = attendeeJson.value("boon", "");

                if (attendeeJson.contains("flexRoles") && attendeeJson["flexRoles"].is_array())
                {
                    for (const auto& flexJson : attendeeJson["flexRoles"])
                    {
                        LegendaryImpactEventmanager::EventFlexRole flex;
                        flex.role = flexJson.value("role", "");
                        flex.boon = flexJson.value("boon", "");
                        attendee.flexRoles.push_back(flex);
                    }
                }
                event.attendees.push_back(attendee);
            }
        }
        return event;
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
        m_SharedState.UpdateState([&](PluginState& state) {
            state.lastSync = "Fehler: " + error;
            });
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
            std::unordered_set<std::string> oldEventIds;

            m_SharedState.WithStateRead([&](const PluginState& state) {
                for (const auto& event : state.events)
                {
                    if (!event.id.empty())
                    {
                        oldEventIds.insert(event.id);
                    }
                }
                });

            json data = json::parse(body);
            PluginState nextState;
            nextState.lastSync = Utility::FormatLocalNow();

            if (data.contains("viewer") && data["viewer"].is_object())
            {
                nextState.viewerUsername = JsonString(data["viewer"], "username");
                nextState.viewerGw2Account = JsonString(data["viewer"], "gw2Account");
            }

            if (data.contains("events") && data["events"].is_array())
            {
                for (const auto& item : data["events"])
                {
                    EventItem event;
                    event.id = JsonString(item, "id");
                    event.title = JsonString(item, "title", "Unbenannt");
                    event.description = JsonString(item, "description");
                    event.location = JsonString(item, "location");
                    event.start = JsonString(item, "start");
                    event.end = JsonString(item, "end");
                    event.tag = JsonString(item, "tag");
                    event.url = JsonString(item, "url");
                    event.isViewerAttending = item.value("isViewerAttending", false);
                    event.isPublic = item.value("isPublic", false);

                    if (item.contains("creator") && item["creator"].is_object())
                    {
                        event.leaderName = JsonString(item["creator"], "username");
                        event.leaderAccount = JsonString(item["creator"], "gw2Account");
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
                                attendee.username = JsonString(attendeeJson["user"], "username");
                                attendee.gw2Account = JsonString(attendeeJson["user"], "gw2Account");
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
                    if (!event.id.empty() && !oldEventIds.empty() && !oldEventIds.contains(event.id))
                    {
                        nextState.newEventIds.push_back(event.id);
                    }

                    nextState.events.push_back(std::move(event));
                }
            }
            SaveCachedEvents(nextState);

            m_SharedState.UpdateState([&](PluginState& state) {
                state.lastSync = std::move(nextState.lastSync);
                state.viewerUsername = std::move(nextState.viewerUsername);
                state.viewerGw2Account = std::move(nextState.viewerGw2Account);
                state.events = std::move(nextState.events);
                state.newEventIds = std::move(nextState.newEventIds);
                });
        }
        catch (const std::exception& ex)
        {
            StoreError(std::string("JSON Fehler: ") + ex.what());
        }

        onExit();
    }

    void EventService::LoadCachedEvents()
    {
        std::ifstream file(Constants::EventsCacheFile);
        if (!file.is_open()) return;

        try
        {
            json data;
            file >> data;

            PluginState cachedState;

            cachedState.lastSync = data.value("lastSync", "-");
            cachedState.viewerUsername = data.value("viewerUsername", "");
            cachedState.viewerGw2Account = data.value("viewerGw2Account", "");

            if (data.contains("events") && data["events"].is_array())
            {
                for (const auto& item : data["events"])
                {
                    cachedState.events.push_back(EventFromJson(item));
                }
            }

            m_SharedState.UpdateState([&](PluginState& state) {
                state.lastSync = std::move(cachedState.lastSync);
                state.viewerUsername = std::move(cachedState.viewerUsername);
                state.viewerGw2Account = std::move(cachedState.viewerGw2Account);
                state.events = std::move(cachedState.events);
                state.newEventIds.clear();
                });
        }
        catch (...) {}
    }

    void EventService::SaveCachedEvents(const PluginState& state) const
    {
        _mkdir("addons");
        _mkdir(Constants::SettingsDir);

        json data;

        data["lastSync"] = state.lastSync;
        data["viewerUsername"] = state.viewerUsername;
        data["viewerGw2Account"] = state.viewerGw2Account;
        data["events"] = json::array();

        for (const auto& event : state.events)
        {
            json eventJson;
            EventToJson(eventJson, event);
            data["events"].push_back(eventJson);
        }

        std::ofstream file(Constants::EventsCacheFile);

        if (file.is_open())
        {
            file << data.dump(4);
        }
    }
}
