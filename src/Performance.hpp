#pragma once

#include <Geode/Geode.hpp>

namespace perf {
	// the checkboxes, plus the two numbers the watchdog fills in; options()
	// hands this out with the automatic choices already folded in
	struct Options {
		bool hideParticles = false;
		bool hideGlow = false;
		bool hidePlayerTrail = false;
		bool hideShaders = false;
		bool hideGround = false;
		bool hideBackground = false;
		bool hidePortals = false;
		bool forceLowDetail = false;
		bool noShake = false;
		bool noDeathEffect = false;

		bool autoPerf = false;
		bool halfParticles = false;
		bool halfBursts = false;
		bool effectCap = false;
		bool freezeOffscreen = false;
		bool noObjectGlow = false;

		int autoLevel = 0;

		float fps = 0.f;
	};

	Options const& options();

	void markDirty();

	bool isPortal(GameObject* object);

	void onLevelStart();

	void onObjectAdded(GameObject* object);

	void applyToLayer(GJBaseGameLayer* layer);

	void applyToPlayer(PlayerObject* player);

	void applyAll(PlayLayer* layer);

	void watchFrameRate(float dt, GJBaseGameLayer* layer);

	float particleUpdateTime(cocos2d::CCParticleSystem* system, float dt);

	unsigned int clampParticleCount(unsigned int requested);

	std::string frameRateSuffix();

	bool frameRateChanged();
}
