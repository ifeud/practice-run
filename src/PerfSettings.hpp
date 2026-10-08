#pragma once

#include <Geode/ui/Popup.hpp>

#include <map>
#include <string>

class PerfSettingsPopup : public geode::Popup {
protected:
	// one option: the checkbox, its name and its one-line description
	struct Row {
		CCMenuItemToggler* toggler = nullptr;
		cocos2d::CCLabelBMFont* name = nullptr;
		cocos2d::CCLabelBMFont* description = nullptr;
		std::string key;
	};

	// keyed by the checkbox, which is what the toggle callback is handed
	std::map<CCMenuItemToggler*, Row> m_optionRows;

	void refreshRowStates();
	void syncTogglers();
	void applyPreset(bool recommended);
	void addRow(cocos2d::CCMenu* menu, char const* key, float x, float y);

	bool init() override;

public:
	static PerfSettingsPopup* create();
};
