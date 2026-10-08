#include "Performance.hpp"
#include "PracticeRun.hpp"

#include <Geode/modify/CCParticleSystem.hpp>
#include <Geode/modify/CCParticleSystemQuad.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/ShaderLayer.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/utils/cocos.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <unordered_map>
#include <vector>

using namespace geode::prelude;

namespace perf {
	namespace {
		Options g_options;
		bool g_dirty = true;
		// bumped every time the settings are re-read, so the particle hook can
		// tell whether its own copy of the options is still current
		unsigned int g_version = 0;
		std::chrono::steady_clock::time_point g_lastRefresh{};

		int g_autoLevel = 0;
		float g_fps = 0.f;
		float g_sampleTime = 0.f;
		int g_sampleFrames = 0;
		float g_lowTime = 0.f;
		float g_highTime = 0.f;
		bool g_fpsChanged = false;

		bool read(char const* key) {
			return Mod::get()->getSettingValue<bool>(key);
		}

	}

	void markDirty() {
		g_dirty = true;
	}

	Options const& options() {
		constexpr auto REFRESH_EVERY = std::chrono::milliseconds(300);

		auto now = std::chrono::steady_clock::now();
		if (!g_dirty && now - g_lastRefresh < REFRESH_EVERY) {
			return g_options;
		}

		g_lastRefresh = now;
		g_dirty = false;

		g_options.hideParticles    = read("perf-hide-particles");
		g_options.hideGlow         = read("perf-hide-glow");
		g_options.hidePlayerTrail  = read("perf-hide-trail");
		g_options.hideShaders      = read("perf-hide-shaders");
		g_options.hideGround       = read("perf-hide-ground");
		g_options.hideBackground   = read("perf-hide-background");
		g_options.hidePortals      = read("perf-hide-portals");
		g_options.forceLowDetail   = read("perf-low-detail");
		g_options.noShake          = read("perf-no-shake");
		g_options.noDeathEffect    = read("perf-no-death-effect");

		g_options.autoPerf         = read("perf-auto-fps");
		g_options.halfParticles    = read("perf-half-particles");
		g_options.halfBursts       = read("perf-half-bursts");
		g_options.effectCap        = read("perf-effect-cap");
		g_options.freezeOffscreen  = read("perf-freeze-offscreen");
		g_options.noObjectGlow     = read("perf-no-object-glow");

		if (g_options.autoPerf) {
			switch (g_autoLevel) {
				case 3:
					g_options.hideShaders      = true;
					g_options.hideGround       = true;
					g_options.hideBackground   = true;
					g_options.noDeathEffect    = true;
					g_options.noShake          = true;
					[[fallthrough]];
				case 2:
					g_options.halfParticles    = true;
					g_options.halfBursts       = true;
					g_options.effectCap        = true;
					g_options.freezeOffscreen  = true;
					g_options.noObjectGlow     = true;
					g_options.hideGlow         = true;
					g_options.forceLowDetail   = true;
					[[fallthrough]];
				case 1:
					g_options.hideParticles    = true;
					g_options.hidePlayerTrail  = true;
					break;
				default:
					break;
			}
		}

		g_options.autoLevel = g_autoLevel;
		g_options.fps = g_fps;

		++g_version;
		return g_options;
	}

	namespace {
		bool g_lastHidePortals = false;
		bool g_lowDetailSaved = false;
		bool g_lowDetailWas = false;

		std::vector<GameObject*> g_hiddenPortals;

		bool g_effectCapActive = false;
		int g_effectCapWas = 0;

		bool g_glowHidden = false;

		void forgetParticleStates();

	}

	void onLevelStart() {
		g_lastHidePortals = false;
		g_lowDetailSaved = false;
		g_lowDetailWas = false;
		g_hiddenPortals.clear();
		g_effectCapActive = false;
		g_effectCapWas = 0;
		g_glowHidden = false;
		g_autoLevel = 0;
		g_lowTime = g_highTime = 0.f;
		forgetParticleStates();

		markDirty();
	}

	void onObjectAdded(GameObject* object) {
		if (!object) return;

		if (options().hidePortals && isPortal(object)) {
			object->setVisible(false);
			g_hiddenPortals.push_back(object);
		}

		if (options().noObjectGlow && object->m_glowSprite) {
			object->m_glowSprite->setVisible(false);
		}
	}

	bool isPortal(GameObject* object) {
		if (!object) return false;
		switch (object->m_objectType) {
			case GameObjectType::InverseGravityPortal:
			case GameObjectType::NormalGravityPortal:
			case GameObjectType::ShipPortal:
			case GameObjectType::CubePortal:
			case GameObjectType::InverseMirrorPortal:
			case GameObjectType::NormalMirrorPortal:
			case GameObjectType::BallPortal:
			case GameObjectType::RegularSizePortal:
			case GameObjectType::MiniSizePortal:
			case GameObjectType::UfoPortal:
			case GameObjectType::DualPortal:
			case GameObjectType::SoloPortal:
			case GameObjectType::WavePortal:
			case GameObjectType::RobotPortal:
			case GameObjectType::TeleportPortal:
			case GameObjectType::SpiderPortal:
			case GameObjectType::SwingPortal:
			case GameObjectType::GravityTogglePortal:
				return true;
			default:
				return false;
		}
	}

	namespace {
		struct SystemState {
			bool seen = false;
			bool hiddenByUs = false;
			bool frozen = false;
			bool skipNext = false;
			float testTime = 0.f;
		};

		std::unordered_map<cocos2d::CCParticleSystem*, SystemState> g_systems;
		unsigned int g_nextPhase = 0;

		constexpr float OFFSCREEN_MARGIN = 150.f;

		// how often an effect is asked where it actually is
		constexpr float RECHECK_SECONDS = 0.25f;

		// follows the parent chain, so effects inside a moving object work too
		bool isOnScreen(cocos2d::CCNode* node) {
			auto parent = node->getParent();
			auto where = parent
				? parent->convertToWorldSpace(node->getPosition())
				: node->getPosition();
			auto screen = cocos2d::CCDirector::sharedDirector()->getWinSize();

			return where.x > -OFFSCREEN_MARGIN && where.x < screen.width + OFFSCREEN_MARGIN
				&& where.y > -OFFSCREEN_MARGIN && where.y < screen.height + OFFSCREEN_MARGIN;
		}

		void forgetParticleStates() {
			g_systems.clear();
			g_nextPhase = 0;
		}

	}

	float particleUpdateTime(cocos2d::CCParticleSystem* system, float dt) {
		if (!system) return dt;

		// every effect asks for the options on every frame it is updated, but they
		// are only really re-read when the settings changed - this keeps its own
		// copy of them until then
		static unsigned int optionsVersion = 0;
		static Options frameOptions;
		if (optionsVersion != g_version) {
			frameOptions = options();
			optionsVersion = g_version;
		}
		auto const& o = frameOptions;

		if (!o.hideParticles && !o.halfParticles && !o.freezeOffscreen) {
			// nothing to do - except for an effect that "No particles" hid a moment
			// ago, which has to be shown again
			auto found = g_systems.find(system);
			if (found != g_systems.end() && found->second.hiddenByUs) {
				found->second.hiddenByUs = false;
				system->setVisible(true);
			}
			return dt;
		}

		auto& state = g_systems[system];
		if (!state.seen) {
			state.seen = true;
			// effects take turns: half of them skip one frame, half the next
			state.skipNext = g_nextPhase != 0;
			g_nextPhase ^= 1u;
		}

		if (o.hideParticles) {
			// set it every time - the address may have been reused by a new effect
			state.hiddenByUs = true;
			system->setVisible(false);
			return -1.f;
		}
		if (state.hiddenByUs) {
			state.hiddenByUs = false;
			system->setVisible(true);
		}

		if (o.freezeOffscreen) {
			state.testTime += dt;
			if (state.testTime >= RECHECK_SECONDS) {
				state.testTime = 0.f;
				state.frozen = !isOnScreen(system);
			}
			if (state.frozen) return -1.f;
		}
		else if (state.frozen) {
			state.frozen = false;
		}

		if (o.halfParticles) {
			state.skipNext = !state.skipNext;
			if (state.skipNext) return -1.f;

			// two frames' worth of time on the frame this effect does run
			return std::min(dt, 0.1f) * 2.f;
		}

		return dt;
	}

	unsigned int clampParticleCount(unsigned int requested) {
		if (!options().halfBursts) return requested;
		if (requested <= 16u) return requested;
		return std::max(16u, requested / 2u);
	}

	namespace {
		void announceAutoLevel(int level) {
			std::string text;
			if (level == 0) {
				text = "Auto performance: full effects back on";
			}
			else {
				text = fmt::format("Auto performance: stepped up to {}/3 (low fps)", level);
			}
			Notification::create(text, NotificationIcon::Info, 1.6f)->show();
		}

	}

	void watchFrameRate(float dt, GJBaseGameLayer* layer) {
		// only the level being played is measured - the editor runs on the same class
		if (!layer || PlayLayer::get() != layer) return;

		auto const& o = options();
		if (!o.autoPerf) {
			g_fps = 0.f;
			g_sampleTime = 0.f;
			g_sampleFrames = 0;
			g_lowTime = g_highTime = 0.f;
			return;
		}

		if (dt <= 0.f || dt > 0.2f) return;

		g_sampleTime += dt;
		++g_sampleFrames;

		if (g_sampleTime < 0.5f) return;

		float fps = static_cast<float>(g_sampleFrames) / g_sampleTime;

		g_fps = (g_fps > 1.f) ? (g_fps * 0.5f + fps * 0.5f) : fps;
		g_sampleTime = 0.f;
		g_sampleFrames = 0;
		g_fpsChanged = true;

		// 1.5 s below 50 fps steps up a level, 4 s above 58 hands one back
		if (g_fps < 50.f && g_autoLevel < 3) {
			g_highTime = 0.f;
			g_lowTime += 0.5f;
			if (g_lowTime >= 1.5f) {
				g_lowTime = 0.f;
				++g_autoLevel;
				markDirty();
				announceAutoLevel(g_autoLevel);
			}
			return;
		}

		if (g_fps > 58.f && g_autoLevel > 0) {
			g_lowTime = 0.f;
			g_highTime += 0.5f;
			if (g_highTime >= 4.f) {
				g_highTime = 0.f;
				--g_autoLevel;
				markDirty();
				if (g_autoLevel == 0) announceAutoLevel(0);
			}
			return;
		}

		g_lowTime = g_highTime = 0.f;
	}

	bool frameRateChanged() {
		bool changed = g_fpsChanged;
		g_fpsChanged = false;
		return changed;
	}

	std::string frameRateSuffix() {
		auto const& o = options();
		if (!o.autoPerf) return {};
		if (o.fps <= 1.f) return " | fps ...";
		if (o.autoLevel > 0) {
			return fmt::format(" | {:.0f} fps (auto {}/3)", o.fps, o.autoLevel);
		}
		return fmt::format(" | {:.0f} fps", o.fps);
	}

	namespace {
		std::array<cocos2d::CCSpriteBatchNode*, 9> glowLayers(GJBaseGameLayer* layer) {
			return {
				layer->m_glowLayerT1, layer->m_glowLayerT2,
				layer->m_glowLayerT3, layer->m_glowLayerT4,
				layer->m_glowLayerB1, layer->m_glowLayerB2,
				layer->m_glowLayerB3, layer->m_glowLayerB4,
				layer->m_glowLayerB5,
			};
		}

		void setObjectGlow(GJBaseGameLayer* layer, bool visible) {
			if (!layer || !layer->m_objects) return;
			for (auto object : CCArrayExt<GameObject*>(layer->m_objects)) {
				if (object && object->m_glowSprite) {
					object->m_glowSprite->setVisible(visible);
				}
			}
		}

	}

	void applyToLayer(GJBaseGameLayer* layer) {
		if (!layer) return;
		auto const& o = options();

		if (layer->m_background) {
			layer->m_background->setVisible(!o.hideBackground);
		}
		for (auto ground : { layer->m_groundLayer, layer->m_groundLayer2 }) {
			if (ground) ground->setVisible(!o.hideGround);
		}
		if (layer->m_middleground) {
			layer->m_middleground->setVisible(!o.hideGround);
		}

		// a hidden effect keeps simulating, so the glitter has to be stopped too
		static CCParticleSystemQuad* lastGlitter = nullptr;
		static bool lastGlitterHidden = false;
		if (layer->m_glitterParticles) {
			auto glitter = layer->m_glitterParticles;
			if (glitter != lastGlitter || o.hideParticles != lastGlitterHidden) {
				lastGlitter = glitter;
				lastGlitterHidden = o.hideParticles;
				glitter->setVisible(!o.hideParticles);
				if (o.hideParticles) {
					glitter->stopSystem();
				}
				else {
					glitter->resetSystem();
				}
			}
		}

		if (layer->m_shaderLayer) {
			layer->m_shaderLayer->setVisible(!o.hideShaders);
		}

		if (o.forceLowDetail) {
			if (!g_lowDetailSaved) {
				g_lowDetailWas = layer->m_lowDetailMode;
				g_lowDetailSaved = true;
			}
			layer->m_lowDetailMode = true;
		}
		else if (g_lowDetailSaved) {
			layer->m_lowDetailMode = g_lowDetailWas;
			g_lowDetailSaved = false;
		}

		if (o.effectCap) {
			// remember the game's own limit, so the option can be undone again
			if (!g_effectCapActive) {
				g_effectCapWas = layer->m_particleSystemLimit;
				g_effectCapActive = true;
			}

			if (g_effectCapWas > 20) {
				layer->m_particleSystemLimit = std::max(20, g_effectCapWas / 2);
			}
		}
		else if (g_effectCapActive) {
			if (g_effectCapWas > 0) {
				layer->m_particleSystemLimit = g_effectCapWas;
			}
			g_effectCapActive = false;
		}

		if (o.hidePortals != g_lastHidePortals) {
			g_lastHidePortals = o.hidePortals;
			if (!o.hidePortals) {
				for (auto portal : g_hiddenPortals) {
					portal->setVisible(true);
				}
				g_hiddenPortals.clear();
			}
		}

		if (o.noObjectGlow != g_glowHidden) {
			g_glowHidden = o.noObjectGlow;
			for (auto glow : glowLayers(layer)) {
				if (glow) glow->setVisible(!o.noObjectGlow);
			}
			setObjectGlow(layer, !o.noObjectGlow);
		}
	}

	void applyToPlayer(PlayerObject* player) {
		if (!player) return;
		auto const& o = options();

		if (o.hideGlow) {
			if (player->m_iconGlow) player->m_iconGlow->setVisible(false);
			if (player->m_vehicleGlow) player->m_vehicleGlow->setVisible(false);
		}

		if (o.hidePlayerTrail) {
			player->deactivateStreak(true);
		}
	}

	void applyAll(PlayLayer* layer) {
		if (!layer) return;
		applyToLayer(layer);
		applyToPlayer(layer->m_player1);
		applyToPlayer(layer->m_player2);
	}
}

class $modify(PracticeRunPerfLayer, GJBaseGameLayer) {
	void update(float dt) {
		perf::watchFrameRate(dt, this);

		if (perf::options().autoPerf && perf::frameRateChanged()) {
			if (auto playLayer = PlayLayer::get()) practice_run::refreshLabel(playLayer);
		}

		perf::applyToLayer(this);
		perf::applyToPlayer(this->m_player1);
		perf::applyToPlayer(this->m_player2);

		GJBaseGameLayer::update(dt);
	}

	void shakeCamera(float duration, float strength, float interval) {
		if (perf::options().noShake) return;
		GJBaseGameLayer::shakeCamera(duration, strength, interval);
	}
};

class $modify(PracticeRunPerfPlayLayer, PlayLayer) {
	bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
		if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
		perf::onLevelStart();
		return true;
	}

	void addObject(GameObject* object) {
		PlayLayer::addObject(object);
		perf::onObjectAdded(object);
	}
};

class $modify(PracticeRunPerfPlayer, PlayerObject) {
	void update(float dt) {
		PlayerObject::update(dt);

		auto const& o = perf::options();
		if (o.hideGlow) {
			if (this->m_iconGlow) this->m_iconGlow->setVisible(false);
			if (this->m_vehicleGlow) this->m_vehicleGlow->setVisible(false);
		}
		if (o.hidePlayerTrail) {
			this->deactivateStreak(true);
		}
	}

	void playerDestroyed(bool noEffects) {
		PlayerObject::playerDestroyed(noEffects || perf::options().noDeathEffect);
	}
};

class $modify(PracticeRunPerfShader, ShaderLayer) {
	void performCalculations() {
		if (perf::options().hideShaders) return;
		ShaderLayer::performCalculations();
	}
};

class $modify(PracticeRunPerfParticles, cocos2d::CCParticleSystem) {
	void update(float dt) {
		float use = perf::particleUpdateTime(this, dt);
		if (use < 0.f) return;
		CCParticleSystem::update(use);
	}

	void setTotalParticles(unsigned int count) {
		CCParticleSystem::setTotalParticles(perf::clampParticleCount(count));
	}

	bool initWithTotalParticles(unsigned int count, bool textureLoaded) {
		return CCParticleSystem::initWithTotalParticles(
			perf::clampParticleCount(count), textureLoaded);
	}
};

class $modify(PracticeRunPerfParticlesQuad, cocos2d::CCParticleSystemQuad) {
	void setTotalParticles(unsigned int count) {
		CCParticleSystemQuad::setTotalParticles(perf::clampParticleCount(count));
	}

	bool initWithTotalParticles(unsigned int count, bool textureLoaded) {
		return CCParticleSystemQuad::initWithTotalParticles(
			perf::clampParticleCount(count), textureLoaded);
	}
};
