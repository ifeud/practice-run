#include "PracticeRun.hpp"
#include "Performance.hpp"

#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

namespace practice_run {
	namespace {
		// "enable by default" only ever starts one run per level
		bool g_autoStartDone = false;

	}

	State& state() {
		static State s_state;
		return s_state;
	}

	void begin() {
		state() = State{};
		state().active = true;
	}

	void end() {
		state() = State{};

		refreshPauseButton();
	}

	bool isActive() {
		return state().active;
	}

	int deathCount() {
		return state().deaths;
	}

	void disable(PlayLayer* layer) {
		end();

		if (!layer) return;

		// the same two flags the game's own practice button flips - we just skip
		// the restart it does afterwards
		if (layer->m_isPracticeMode) {
			layer->m_isPracticeMode = false;
			layer->m_practiceMusicSync = false;
			layer->toggleMusicInPractice();
			layer->removeAllCheckpoints();
		}

		refreshLabel(layer);
		log::info("Practice Run: switched off - the level is back in normal mode.");
	}

	void autoStart(PlayLayer* layer) {
		if (!layer || g_autoStartDone) return;
		g_autoStartDone = true;

		if (!Mod::get()->getSettingValue<bool>("enable-by-default")) return;
		if (isActive()) return;

		begin();

		// exactly what the vanilla practice button does, restart included
		if (!layer->m_isPracticeMode) {
			layer->togglePracticeMode(true);
		}

		refreshLabel(layer);
		log::info("Practice Run: started automatically (Enable by default)");
	}

	void createLabel(PlayLayer* layer) {
		auto label = CCLabelBMFont::create("", "bigFont.fnt");
		label->setID(LABEL_ID);
		label->setAnchorPoint({ 0.f, 0.f });
		label->setPosition({ 8.f, 8.f });
		label->setScale(0.4f);
		label->setOpacity(190);
		label->setVisible(false);
		layer->addChild(label, 10);
	}

	void refreshLabel(PlayLayer* layer) {
		if (!layer) return;

		auto label = typeinfo_cast<CCLabelBMFont*>(layer->getChildByID(LABEL_ID));
		if (!label) return;

		bool show = state().active && Mod::get()->getSettingValue<bool>("show-run-label");
		label->setVisible(show);
		if (show) {
			std::string text = fmt::format("Practice Run - deaths: {}", state().deaths);

			text += perf::frameRateSuffix();
			label->setString(text.c_str());
		}
	}

	void refreshPauseButton() {
		auto director = CCDirector::sharedDirector();
		auto scene = director ? director->getRunningScene() : nullptr;
		if (!scene) return;

		auto button = typeinfo_cast<CCMenuItemSpriteExtra*>(
			scene->getChildByIDRecursive("practice-run-button"_spr)
		);
		if (!button) return;

		auto sprite = CCSprite::createWithSpriteFrameName(
			(Mod::get()->getID() + (state().active ? "/practiceIconActive.png" : "/practiceIcon.png")).c_str()
		);
		if (!sprite) return;

		auto wanted = button->getContentSize();
		auto have = sprite->getContentSize();
		if (wanted.width > 1.f && have.width > 0.f) {
			sprite->setScale(wanted.width / have.width);
		}
		button->setSprite(sprite);
	}

	void scheduleAutoStart(int triesLeft) {
		g_autoStartDone = false;

		// the level is still being built inside init(), so wait for the end of
		// the frame instead of restarting it from here
		geode::queueInMainThread([triesLeft] {
			auto layer = PlayLayer::get();
			if (!layer) {
				if (triesLeft > 0) {
					scheduleAutoStart(triesLeft - 1);
				}
				return;
			}
			autoStart(layer);
		});
	}
}

class $modify(PracticeRunPlayLayer, PlayLayer) {
	bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
		if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

		practice_run::end();
		practice_run::createLabel(this);

		practice_run::scheduleAutoStart(8);

		return true;
	}

	void destroyPlayer(PlayerObject* player, GameObject* object) {
		// the game ignores calls after you are already dead and skips the
		// anti-cheat spike, so this counts one death per death and not per touch
		bool isRealDeath = !this->m_playerDied;
		if (object && m_anticheatSpike && object->m_uniqueID == m_anticheatSpike->m_uniqueID) {
			isRealDeath = false;
		}

		auto& run = practice_run::state();
		if (isRealDeath && run.active && this->m_isPracticeMode) {
			run.deaths += 1;
			log::info("Practice Run: death number {}", run.deaths);
			practice_run::refreshLabel(this);
		}

		PlayLayer::destroyPlayer(player, object);
	}

	void levelComplete() {
		auto& run = practice_run::state();

		if (run.active && run.deaths == 0 && this->m_isPracticeMode) {
			log::info("Practice Run: finished with 0 deaths - saving a normal mode completion!");
			// this is the flag the game reads when it decides what to save
			this->m_isPracticeMode = false;
		}
		else if (run.active) {
			log::info("Practice Run: finished after {} death(s) - it stays a practice completion.", run.deaths);
		}

		practice_run::end();

		PlayLayer::levelComplete();
		practice_run::refreshLabel(this);
	}
};
