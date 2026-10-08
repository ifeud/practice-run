#pragma once

#include <Geode/Geode.hpp>

namespace practice_run {
	struct State {
		bool active = false;
		int deaths = 0;
	};

	// geode puts the mod id in front of the node id, which keeps it unique
	inline auto const LABEL_ID = "practice-run-label"_spr;

	State& state();

	void begin();

	void end();

	bool isActive();

	int deathCount();

	void disable(PlayLayer* layer);

	void autoStart(PlayLayer* layer);

	void createLabel(PlayLayer* layer);

	void refreshLabel(PlayLayer* layer);

	void refreshPauseButton();
}
