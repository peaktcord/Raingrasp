#ifndef COMMON_REPLAY_HEADLESS_HPP
#define COMMON_REPLAY_HEADLESS_HPP

namespace headless {

template <typename GameT>
void runJobInline(GameT *game, int job) {
    game->runHelperJob(job);
}

template <typename Splash>
bool finishSplash(Splash *splash, int guard = 200) {
    if (splash == nullptr) return false;
    for (int n1 = 0; n1 < guard; ++n1) {
        if (!splash->splashStep(500)) return true;
    }
    return false;
}

}

#endif
