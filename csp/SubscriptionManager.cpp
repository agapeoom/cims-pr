#include "SubscriptionManager.h"
#include "Log.h"

CSubscriptionManager gclsSubscriptionManager;

CSubscriptionManager::CSubscriptionManager() {
}

CSubscriptionManager::~CSubscriptionManager() {
}

void CSubscriptionManager::AddSubscription(const std::string& strResourceUri, const SubscriptionInfo& info) {
    std::unique_lock<std::recursive_mutex> lock(m_mutex);
    
    // Check if subscription already exists for this CallID (Update)
    auto range = m_mapSubs.equal_range(strResourceUri);
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second.strCallId == info.strCallId) {
            // Update existing
            it->second = info;
            CLog::Print(LOG_DEBUG, "Subscription Updated: Resource=%s, Sub=%s, Expires=%d, CallId=%s",
                strResourceUri.c_str(), info.strSubscriberUri.c_str(), info.iExpires, info.strCallId.c_str());
            return;
        }
    }

    // Insert new
    m_mapSubs.insert(std::make_pair(strResourceUri, info));
    CLog::Print(LOG_INFO, "Subscription Added: Resource=%s, Sub=%s, Expires=%d, CallId=%s",
        strResourceUri.c_str(), info.strSubscriberUri.c_str(), info.iExpires, info.strCallId.c_str());
}

void CSubscriptionManager::RemoveSubscription(const std::string& strResourceUri, const std::string& strSubscriberUri) {
    std::unique_lock<std::recursive_mutex> lock(m_mutex);

    auto range = m_mapSubs.equal_range(strResourceUri);
    for (auto it = range.first; it != range.second; ) {
        if (it->second.strSubscriberUri == strSubscriberUri) { // Or match by CallID strictly? simplified by SubscriberURI for now
            CLog::Print(LOG_INFO, "Subscription Removed: Resource=%s, Sub=%s", strResourceUri.c_str(), strSubscriberUri.c_str());
            it = m_mapSubs.erase(it);
        } else {
            ++it;
        }
    }
}

void CSubscriptionManager::GetSubscribers(const std::string& strResourceUri, std::list<SubscriptionInfo>& outList) {
    std::unique_lock<std::recursive_mutex> lock(m_mutex);
    
    auto range = m_mapSubs.equal_range(strResourceUri);
    for (auto it = range.first; it != range.second; ++it) {
        outList.push_back(it->second);
    }
}

void CSubscriptionManager::CheckExpired() {
    std::unique_lock<std::recursive_mutex> lock(m_mutex);
    time_t now = time(NULL);

    for (auto it = m_mapSubs.begin(); it != m_mapSubs.end(); ) {
        time_t tExpireAt = it->second.tStartTime + it->second.iExpires;
        if (now > tExpireAt) {
            CLog::Print(LOG_INFO, "Subscription Expired: Resource=%s, Sub=%s, CallId=%s", 
                it->first.c_str(), it->second.strSubscriberUri.c_str(), it->second.strCallId.c_str());
            it = m_mapSubs.erase(it);
        } else {
            ++it;
        }
    }
}
