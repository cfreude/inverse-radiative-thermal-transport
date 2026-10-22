#include <tamashii/engine/scene/render_cmd_system.hpp>
#include <tamashii/engine/scene/light.hpp>
#include <tamashii/engine/scene/model.hpp>
#include <tamashii/engine/scene/image.hpp>
#include <tamashii/engine/scene/material.hpp>

T_USE_NAMESPACE
RenderCmdSystem tamashii::renderCmdSystem;

void RenderCmdSystem::addBeginFrameCmd()
{
	RCmd_s cmd{};
	cmd.mType = RenderCommand::BEGIN_FRAME;
	addCmd(cmd);
}

void RenderCmdSystem::addEndFrameCmd()
{
	RCmd_s cmd{};
	cmd.mType = RenderCommand::END_FRAME;
	addCmd(cmd);
	mFrames.fetch_add(1);
}

void RenderCmdSystem::addDrawSurfCmd(viewDef_s* aViewDef)
{
	RCmd_s cmd{};
	cmd.mType = RenderCommand::DRAW_VIEW;
	cmd.mViewDef = aViewDef;
	addCmd(cmd);
}

void RenderCmdSystem::addDrawUICmd(uiConf_s* aUiConf)
{
	RCmd_s cmd{};
	cmd.mType = RenderCommand::DRAW_UI;
	cmd.mUiConf = aUiConf;
	addCmd(cmd);
}

void RenderCmdSystem::addEntityAddedCmd(Ref_s* aRef)
{
	RCmd_s cmd{};
	cmd.mType = RenderCommand::ENTITY_ADDED;
	cmd.mEntity = aRef;
	addCmd(cmd);
}

void RenderCmdSystem::addEntityRemovedCmd(Ref_s* aRef)
{
	RCmd_s cmd{};
	cmd.mType = RenderCommand::ENTITY_REMOVED;
	cmd.mEntity = aRef;
	addCmd(cmd);
}

void RenderCmdSystem::addAssetAddedCmd(Asset* aAsset)
{
	RCmd_s cmd{};
	cmd.mType = RenderCommand::ASSET_ADDED;
	cmd.mAsset = aAsset;
	addCmd(cmd);
}

void RenderCmdSystem::addAssetRemovedCmd(Asset* aAsset)
{
	RCmd_s cmd{};
	cmd.mType = RenderCommand::ASSET_REMOVED;
	cmd.mAsset = aAsset;
	addCmd(cmd);
}

void RenderCmdSystem::addScreenshotCmd(const std::string& aFileName)
{
	RCmd_s cmd{};
	cmd.mType = RenderCommand::SCREENSHOT;
	cmd.mScreenshotInfo = new screenshotInfo_s;
	cmd.mScreenshotInfo->name = aFileName;
	addCmd(cmd);
}

void RenderCmdSystem::addImplScreenshotCmd()
{
	RCmd_s cmd{};
	cmd.mType = RenderCommand::IMPL_SCREENSHOT;
	addCmd(cmd);
}

void RenderCmdSystem::addDrawOnMeshCmd(const DrawInfo_s* aDrawInfo, const HitInfo_s* aHitInfo)
{
	RCmd_s cmd{};
	cmd.mType = RenderCommand::IMPL_DRAW_ON_MESH;
	cmd.mDrawInfo = new DrawInfo_s();
	std::memcpy(cmd.mDrawInfo, aDrawInfo, sizeof(DrawInfo_s));
	addCmd(cmd);
}

bool RenderCmdSystem::nextCmd()
{
	const std::lock_guard lock(mMutex);
	return !mCmdList.empty();
}

RCmd_s RenderCmdSystem::popNextCmd()
{
	const std::lock_guard lock(mMutex);
	if (!mCmdList.empty()) {
		const RCmd_s cmd = mCmdList.front();
		mCmdList.pop_front();
		return cmd;
	}
	return RCmd_s{ RenderCommand::EMPTY, {nullptr} };
}

void RenderCmdSystem::deleteCmd(const RCmd_s& aCmd)
{
	switch (aCmd.mType) {
		case RenderCommand::DRAW_VIEW:
			delete aCmd.mViewDef;
			break;
		case RenderCommand::DRAW_UI:
			delete aCmd.mUiConf;
			break;
		case RenderCommand::ENTITY_ADDED:
			break;
		case RenderCommand::ENTITY_REMOVED:
			switch (aCmd.mEntity->type)
			{
			case Ref_s::Type::Mesh: delete static_cast<RefMesh_s*>(aCmd.mEntity); break;
			case Ref_s::Type::Model: delete static_cast<RefModel_s*>(aCmd.mEntity); break;
			case Ref_s::Type::Camera: delete static_cast<RefCameraPrivate_s*>(aCmd.mEntity); break;
			case Ref_s::Type::Light: delete static_cast<RefLight_s*>(aCmd.mEntity); break;
			}
			break;
		case RenderCommand::ASSET_ADDED:
			break;
		case RenderCommand::ASSET_REMOVED:
			switch (aCmd.mAsset->getAssetType())
			{
			case Asset::Type::UNDEFINED: break;
			case Asset::Type::MESH:
				delete static_cast<Mesh*>(aCmd.mAsset);
				break;
			case Asset::Type::MODEL:
				delete static_cast<Model*>(aCmd.mAsset);
				break;
			case Asset::Type::CAMERA:
				delete static_cast<Camera*>(aCmd.mAsset);
				break;
			case Asset::Type::LIGHT:
			{
				const auto l = static_cast<Light*>(aCmd.mAsset);
				switch (l->getType()) {
				case Light::Type::DIRECTIONAL:
					delete static_cast<DirectionalLight*>(l);
					break;
				case Light::Type::POINT:
					delete static_cast<PointLight*>(l);
					break;
				case Light::Type::SPOT:
					delete static_cast<SpotLight*>(l);
					break;
				case Light::Type::SURFACE:
					delete static_cast<SurfaceLight*>(l);
					break;
				case Light::Type::IES:
					delete static_cast<IESLight*>(l);
					break;
				}
				break;
			}
			case Asset::Type::IMAGE:
				delete static_cast<Image*>(aCmd.mAsset);
				break;
			case Asset::Type::MATERIAL:
				delete static_cast<Material*>(aCmd.mAsset);
				break;
			case Asset::Type::NODE:
				delete static_cast<Node*>(aCmd.mAsset);
				break;
			case Asset::Type::SCENE: break;
			}
			break;
		case RenderCommand::SCREENSHOT:
			delete[] aCmd.mScreenshotInfo->data;
			delete aCmd.mScreenshotInfo;
			break;
		case RenderCommand::IMPL_SCREENSHOT: break;
		case RenderCommand::IMPL_DRAW_ON_MESH:
			delete aCmd.mDrawInfo;
			break;
		case RenderCommand::BEGIN_FRAME: break;
		case RenderCommand::END_FRAME:
			mFrames.fetch_sub(1);
			break;
		case RenderCommand::EMPTY: break;
	}
}

uint32_t RenderCmdSystem::frames() const
{
	return mFrames.load();
}

void RenderCmdSystem::addCmd(const RCmd_s& aCmd)
{
	const std::lock_guard lock(mMutex);
	mCmdList.push_back(aCmd);
}
