#include "PerfSettings.hpp"
#include "Performance.hpp"

#include <Geode/ui/Button.hpp>
#include <Geode/ui/Notification.hpp>

#include <cstring>

using namespace geode::prelude;

namespace {
	// reading order: two columns, filled top to bottom
	constexpr char const* ROWS[] = {
		"perf-auto-fps",
		"perf-hide-particles",
		"perf-half-particles",
		"perf-freeze-offscreen",
		"perf-effect-cap",
		"perf-half-bursts",
		"perf-no-object-glow",
		"perf-hide-trail",

		"perf-hide-shaders",
		"perf-hide-glow",
		"perf-low-detail",
		"perf-hide-ground",
		"perf-hide-background",
		"perf-no-shake",
		"perf-no-death-effect",
		"perf-hide-portals",
	};

	// what the "Recommended" button switches on - options that save work
	// without hiding anything you need to see
	constexpr char const* RECOMMENDED[] = {
		"perf-auto-fps",
		"perf-hide-particles",
		"perf-half-particles",
		"perf-freeze-offscreen",
		"perf-effect-cap",
		"perf-half-bursts",
		"perf-no-object-glow",
		"perf-hide-trail",
		"perf-hide-shaders",
		"perf-low-detail",
	};

	bool isRecommended(char const* key) {
		for (auto pick : RECOMMENDED) {
			if (std::strcmp(pick, key) == 0) return true;
		}
		return false;
	}

	// all sizes are in the game's design units, the screen is 320 units tall
	constexpr int ROWS_PER_COLUMN = 8;
	constexpr float COLUMN_WIDTH = 172.f;
	constexpr float COLUMN_GAP = 14.f;
	constexpr float ROW_HEIGHT = 23.f;
	constexpr float HEADER = 74.f;
	constexpr float BOTTOM_PADDING = 34.f;

	constexpr float POPUP_WIDTH = COLUMN_WIDTH * 2 + COLUMN_GAP + 28.f;
	constexpr float POPUP_HEIGHT = HEADER + ROWS_PER_COLUMN * ROW_HEIGHT + BOTTOM_PADDING;

	// off options are dimmed, so the ones that are on stand out
	constexpr GLubyte TEXT_OPACITY_OFF = 145;
	constexpr GLubyte TEXT_OPACITY_ON = 255;
	constexpr GLubyte DESCRIPTION_OFF = 110;
	constexpr GLubyte DESCRIPTION_ON = 175;
}

void PerfSettingsPopup::refreshRowStates() {
	for (auto& [toggler, row] : m_optionRows) {
		bool on = row.toggler && row.toggler->isToggled();
		if (row.name) {
			row.name->setOpacity(on ? TEXT_OPACITY_ON : TEXT_OPACITY_OFF);
		}
		if (row.description) {
			row.description->setOpacity(on ? DESCRIPTION_ON : DESCRIPTION_OFF);
		}
	}
}

void PerfSettingsPopup::syncTogglers() {
	for (auto& [toggler, row] : m_optionRows) {
		if (!toggler) continue;
		toggler->toggle(Mod::get()->getSettingValue<bool>(row.key.c_str()));
	}
	refreshRowStates();
}

void PerfSettingsPopup::applyPreset(bool recommended) {
	int switchedOn = 0;
	for (auto key : ROWS) {
		bool wanted = recommended && isRecommended(key);
		Mod::get()->setSettingValue<bool>(key, wanted);
		if (wanted) ++switchedOn;
	}

	perf::markDirty();
	syncTogglers();

	if (recommended) {
		Notification::create(
			fmt::format("Performance: {} options on", switchedOn),
			NotificationIcon::Success,
			1.4f
		)->show();
	}
	else {
		Notification::create("Performance: all options off", NotificationIcon::Info, 1.4f)->show();
	}
}

void PerfSettingsPopup::addRow(cocos2d::CCMenu* menu, char const* key, float x, float y) {
	if (!menu) return;

	auto toggler = CCMenuItemExt::createTogglerWithStandardSprites(
		0.5f,
		[this, key](CCMenuItemToggler* item) {
			Mod::get()->setSettingValue<bool>(key, !item->isToggled());
			perf::markDirty();
			refreshRowStates();
		}
	);
	toggler->toggle(Mod::get()->getSettingValue<bool>(key));
	toggler->setPosition({ x, y });
	menu->addChild(toggler);

	auto setting = Mod::get()->getSetting(key);
	std::string name = setting ? setting->getDisplayName() : key;
	std::string description;
	if (setting) {
		if (auto desc = setting->getDescription()) description = *desc;
	}

	constexpr float TEXT_LEFT = 14.f;
	constexpr float TEXT_RIGHT = 6.f;
	float available = COLUMN_WIDTH - TEXT_LEFT - TEXT_RIGHT;

	auto nameLabel = CCLabelBMFont::create(name.c_str(), "bigFont.fnt");
	nameLabel->setAnchorPoint({ 0.f, 0.5f });
	nameLabel->setPosition({ x + TEXT_LEFT, y + 5.5f });
	nameLabel->setScale(0.36f);
	if (nameLabel->getContentSize().width * nameLabel->getScale() > available) {
		nameLabel->setScale(available / nameLabel->getContentSize().width);
	}
	menu->addChild(nameLabel);

	auto descLabel = CCLabelBMFont::create(description.c_str(), "chatFont.fnt");
	descLabel->setAnchorPoint({ 0.f, 0.5f });
	descLabel->setPosition({ x + TEXT_LEFT, y - 6.5f });
	descLabel->setScale(0.32f);
	if (descLabel->getContentSize().width * descLabel->getScale() > available) {
		descLabel->setScale(available / descLabel->getContentSize().width);
	}
	menu->addChild(descLabel);

	m_optionRows[toggler] = Row{ toggler, nameLabel, descLabel, key };
}

bool PerfSettingsPopup::init() {
	if (!Popup::init(POPUP_WIDTH, POPUP_HEIGHT, "GJ_square01.png")) {
		return false;
	}

	this->setID("practice-run-perf-popup"_spr);
	this->setTitle("Performance");

	auto intro = CCLabelBMFont::create(
		"Turn these on for a framerate boost - they work instantly\n"
		"and can be switched back off at any time.",
		"bigFont.fnt"
	);
	intro->setScale(0.3f);
	intro->setAlignment(kCCTextAlignmentCenter);
	intro->setOpacity(220);
	m_mainLayer->addChildAtPosition(intro, Anchor::Center, { 0.f, POPUP_HEIGHT / 2.f - 34.f });

	auto menu = CCMenu::create();
	menu->setContentSize({ POPUP_WIDTH, POPUP_HEIGHT });
	m_mainLayer->addChildAtPosition(menu, Anchor::Center);

	for (size_t i = 0; i < std::size(ROWS); ++i) {
		int column = static_cast<int>(i) / ROWS_PER_COLUMN;
		int row = static_cast<int>(i) % ROWS_PER_COLUMN;

		float columnLeft = (column == 0)
			? (-COLUMN_GAP / 2.f - COLUMN_WIDTH)
			: (+COLUMN_GAP / 2.f);
		float x = columnLeft + 10.f;
		float y = POPUP_HEIGHT / 2.f - HEADER - row * ROW_HEIGHT - ROW_HEIGHT / 2.f;

		addRow(menu, ROWS[i], x, y);
	}

	auto recommendedButton = Button::createWithLabel("Recommended", "bigFont.fnt", [this](Button*) {
		applyPreset(true);
	});
	recommendedButton->setScale(0.5f);
	recommendedButton->setID("practice-run-recommended-button"_spr);
	m_mainLayer->addChildAtPosition(recommendedButton, Anchor::Bottom, { -74.f, 20.f });

	auto resetButton = Button::createWithLabel("Reset", "bigFont.fnt", [this](Button*) {
		applyPreset(false);
	});
	resetButton->setScale(0.5f);
	resetButton->setID("practice-run-reset-button"_spr);
	m_mainLayer->addChildAtPosition(resetButton, Anchor::Bottom, { 74.f, 20.f });

	refreshRowStates();

	return true;
}

PerfSettingsPopup* PerfSettingsPopup::create() {
	auto popup = new PerfSettingsPopup();
	if (popup->init()) {
		popup->autorelease();
		return popup;
	}
	delete popup;
	return nullptr;
}
