#include "race_control/weather.h"
#include <chrono>

WeatherSystem::WeatherSystem(MpscQueue<RaceControlEvent>& event_queue)
    : events_(event_queue) {}

void WeatherSystem::update(int current_lap) {
    if (current_lap - last_update_lap_ < 5) return; // don't change too often
    last_update_lap_ = current_lap;

    if (coin_(rng_) > CHANGE_PROBABILITY) return; // no change this update

    WeatherState new_state;
    {
        std::unique_lock write {mutex_};
        // Transition: DRY->DAMP->WET->DRY (cycle)
        switch (state_) {
            case WeatherState::DRY:  state_ = WeatherState::DAMP; break;
            case WeatherState::DAMP: state_ = WeatherState::WET;  break;
            case WeatherState::WET:  state_ = WeatherState::DRY;  break;
        }
        new_state = state_;
    }

    static const char* names[] = {"DRY", "DAMP", "WET"};
    RaceControlEvent ev;
    ev.type      = RaceControlEvent::Type::WEATHER_CHANGE;
    ev.lap       = current_lap;
    ev.message   = std::string("Weather: ") + names[static_cast<int>(new_state)];
    ev.timestamp = std::chrono::steady_clock::now();
    events_.push(std::move(ev));
}

WeatherState WeatherSystem::current() const {
    std::shared_lock read{mutex_}; // concurrent reads allowed
    return state_;
}

float WeatherSystem::grip_factor() const {
    std::shared_lock read{mutex_};
    switch (state_) {
        case WeatherState::DRY:  return 1.00f;
        case WeatherState::DAMP: return 0.85f;
        case WeatherState::WET:  return 0.70f;
    }
    return 1.0f;
}