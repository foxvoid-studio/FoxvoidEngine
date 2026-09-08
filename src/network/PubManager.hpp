#pragma once

#include <string>
#include <functional>

class PubManager {
    public:
        // Callbacks for the ad lifecycle
        using RewardCallback = std::function<void()>;
        using ErrorCallback = std::function<void(const std::string&)>;

        // Initializes SDKs if needeed.
        static void Initialize();

        // Requests and displays a rewarded ad based on the active platform
        static void ShowRewardedAd(
            const std::string& placementId,
            RewardCallback onReward,
            ErrorCallback onError
        );
};
