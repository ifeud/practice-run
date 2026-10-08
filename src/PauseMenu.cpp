#include "PracticeRun.hpp"
#include "PerfSettings.hpp"
#include "Performance.hpp"

#include <Geode/modify/PauseLayer.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/utils/cocos.hpp>

#include <algorithm>
#include <vector>

using namespace geode::prelude;

namespace {
// the game's own mode button - one frame per mode
constexpr char const* MODE_BUTTON_FRAMES[] = {
	"GJ_practiceBtn_001.png",
	"GJ_normalBtn_001.png",
};

constexpr char const* PLAY_BUTTON_FRAME = "GJ_playBtn2_001.png";

std::string iconFrameName(bool active) {
	return std::string(Mod::get()->getID()) +
		(active ? "/practiceIconActive.png" : "/practiceIcon.png");
}

std::string settingsFrameName() {
	return std::string(Mod::get()->getID()) + "/settingsIcon.png";
}

// the row is a plain CCMenu with no id, so we look for the game's practice
// button inside it instead
CCMenu* findButtonRow(PauseLayer* layer) {
	for (auto child : CCArrayExt<CCNode*>(layer->getChildren())) {
		auto menu = typeinfo_cast<CCMenu*>(child);
		if (!menu) continue;

		for (auto frame : MODE_BUTTON_FRAMES) {
			if (cocos::getChildBySpriteFrameName(menu, frame)) {
				return menu;
			}
		}
		if (cocos::getChildBySpriteFrameName(menu, PLAY_BUTTON_FRAME)) {
			return menu;
		}
	}
	return nullptr;
}

CCMenuItem* findModeButton(CCMenu* menu) {
	for (auto frame : MODE_BUTTON_FRAMES) {
		if (auto node = cocos::getChildBySpriteFrameName(menu, frame)) {
			if (auto item = typeinfo_cast<CCMenuItem*>(node)) {
				return item;
			}
		}
	}
	return nullptr;
}

void startTrackedRun(PauseLayer* layer) {
	auto playLayer = PlayLayer::get();
	if (!playLayer) return;

	practice_run::begin();
	practice_run::refreshPauseButton();

	if (!playLayer->m_isPracticeMode) {
		playLayer->togglePracticeMode(true);
	}

	playLayer->resetLevelFromStart();

	practice_run::refreshLabel(playLayer);
	layer->onResume(nullptr);
}

void onPracticeButtonClicked(PauseLayer* layer) {
	if (!PlayLayer::get()) return;

	// a run in progress means the click switches the mod off again
	if (practice_run::isActive()) {
		practice_run::disable(PlayLayer::get());
		Notification::create(
			"Practice Run switched off - normal mode",
			NotificationIcon::Success,
			1.4f
		)->show();

		layer->onResume(nullptr);
		return;
	}

	startTrackedRun(layer);
}

CCMenuItemSpriteExtra* makeIconButton(CCMenuItem* modeButton, PauseLayer* layer) {
	bool active = practice_run::isActive();
	auto sprite = CCSprite::createWithSpriteFrameName(iconFrameName(active).c_str());
	if (!sprite) {
		log::error("Could not find sprite frame '{}'", iconFrameName(active));
		return nullptr;
	}

	// copy the game's button size, the pause menu is scaled on small screens
	auto wanted = modeButton->getContentSize();
	auto have = sprite->getContentSize();
	if (wanted.width > 1.f && have.width > 0.f) {
		sprite->setScale(wanted.width / have.width);
	}
	else {
		log::warn("Could not measure the game's button, using the icon's own size");
	}

	return CCMenuItemExt::createSpriteExtra(sprite, [layer](CCMenuItemSpriteExtra*) {
		onPracticeButtonClicked(layer);
	});
}

// makes room for our button by pushing both halves of the row outwards
void placeInRow(CCMenu* menu, CCMenuItem* modeButton, CCMenuItemSpriteExtra* ourButton) {
	std::vector<CCMenuItem*> row;
	for (auto child : CCArrayExt<CCNode*>(menu->getChildren())) {
		auto item = typeinfo_cast<CCMenuItem*>(child);
		if (!item) continue;
		if (item->getContentSize().width <= 1.f) continue;
		row.push_back(item);
	}
	std::sort(row.begin(), row.end(), [](CCMenuItem* a, CCMenuItem* b) {
		return a->getPositionX() < b->getPositionX();
	});

	// measured off the game's buttons rather than hard-coded, so the spacing
	// stays right if the row ever changes
	float gap = 0.f;
	bool haveGap = false;
	for (size_t i = 1; i < row.size(); ++i) {
		float previousRight = row[i - 1]->getPositionX() + row[i - 1]->getContentSize().width / 2.f;
		float currentLeft = row[i]->getPositionX() - row[i]->getContentSize().width / 2.f;
		float distance = currentLeft - previousRight;
		if (distance > 1.f && (!haveGap || distance < gap)) {
			gap = distance;
			haveGap = true;
		}
	}
	if (!haveGap) {
		gap = 10.f;
	}
	gap = std::clamp(gap, 2.f, 40.f);

	float halfOurWidth = ourButton->getContentSize().width / 2.f;
	float added = halfOurWidth * 2.f + gap;

	float modeX = modeButton->getPositionX();

	for (auto item : row) {
		if (item == modeButton) continue;
		float x = item->getPositionX();
		item->setPositionX(x < modeX ? x - added / 2.f : x + added / 2.f);
	}

	modeButton->setPositionX(modeX - added / 2.f);

	ourButton->setPosition({
		modeX + halfOurWidth + gap / 2.f,
		modeButton->getPositionY(),
	});
}
}

class $modify(PracticeRunPauseLayer, PauseLayer) {
	void customSetup() {
		PauseLayer::customSetup();

		if (!Mod::get()->getSettingValue<bool>("show-pause-button")) {
			return;
		}

		if (!PlayLayer::get()) return;
		if (this->getChildByID("practice-run-button"_spr)) return;

		auto menu = findButtonRow(this);
		if (!menu) {
			log::warn("Could not find the pause menu button row, not adding the button");
			return;
		}

		auto modeButton = findModeButton(menu);
		if (!modeButton) {
			log::warn("Could not find the normal/practice mode button, not adding the button");
			return;
		}

		auto button = makeIconButton(modeButton, this);
		if (!button) return;
		button->setID("practice-run-button"_spr);

		placeInRow(menu, modeButton, button);
		menu->addChild(button);

		log::info("Added the purple practice button to the pause menu");

		addSettingsButton();
	}

	void addSettingsButton() {
		if (this->getChildByID("practice-run-settings-button"_spr)) return;

		auto sprite = CCSprite::createWithSpriteFrameName(settingsFrameName().c_str());
		if (!sprite) {
			log::error("Could not find sprite frame '{}'", settingsFrameName());
			return;
		}

		auto button = CCMenuItemExt::createSpriteExtra(sprite, [this](CCMenuItemSpriteExtra*) {
			if (auto popup = PerfSettingsPopup::create()) {
				popup->show();
			}
		});
		button->setID("practice-run-settings-button"_spr);

		auto menu = CCMenu::create();
		menu->setID("practice-run-settings-menu"_spr);
		menu->addChild(button);

		auto screen = this->getContentSize();
		menu->setPosition({
			-screen.width / 2.f + 42.f,
			-screen.height / 2.f + 42.f,
		});
		this->addChild(menu, 10);

		log::info("Added the performance settings button to the pause menu");
	}
};
