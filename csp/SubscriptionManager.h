#ifndef _SUBSCRIPTION_MANAGER_H_
#define _SUBSCRIPTION_MANAGER_H_

#include <string>
#include <map>
#include <list>
#include <mutex>
#include <ctime>

/**
 * @ingroup CspServer
 * @brief Subscription Info Structure
 */
struct SubscriptionInfo {
    std::string strSubscriberUri; // From URI
    std::string strCallId;        // SIP Dialog Call-ID
    std::string strFromTag;
    std::string strToTag;
    int iExpires;                 // Expires in seconds
    time_t tStartTime;            // Subscription Start Time
    std::string strContact;       // Contact Header value
};

/**
 * @ingroup CspServer
 * @brief Manages SIP Subscriptions (SUBSCRIBE/NOTIFY)
 */
class CSubscriptionManager {
public:
    CSubscriptionManager();
    ~CSubscriptionManager();

    /**
     * @brief Add or Update a subscription
     */
    void AddSubscription(const std::string& strResourceUri, const SubscriptionInfo& info);
    
    /**
     * @brief Remove a subscription (e.g. Expires=0 or Terminated)
     */
    void RemoveSubscription(const std::string& strResourceUri, const std::string& strSubscriberUri);

    /**
     * @brief Get all subscribers for a resource
     */
    void GetSubscribers(const std::string& strResourceUri, std::list<SubscriptionInfo>& outList);
    
    /**
     * @brief Check and remove expired subscriptions
     */
    void CheckExpired();

private:
    // Map: Resource URI -> Subscription Info
    // Multimap allowed if multiple devices for same user subscribe? 
    // Usually one subscription per Dialog.
    // We use multimap to allow multiple subscribers per resource.
    std::multimap<std::string, SubscriptionInfo> m_mapSubs;
    std::recursive_mutex m_mutex;
};

extern CSubscriptionManager gclsSubscriptionManager;

#endif
