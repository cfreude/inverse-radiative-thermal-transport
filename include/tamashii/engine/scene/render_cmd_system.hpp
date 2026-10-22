#pragma once
#include <tamashii/public.hpp>
#include <tamashii/engine/scene/render_scene.hpp>
#include <tamashii/engine/scene/ref_entities.hpp>

#include <deque>

T_BEGIN_NAMESPACE
class RenderSystem;
struct RefMesh_s;
struct RefModel_s;

struct drawSurf_s {
	RefMesh_s*					refMesh;			// the mesh to draw
	glm::mat4*					model_matrix;
	int							ref_model_index;	// index of the ref model this surface is part of
	RefModel_s*					ref_model_ptr;		// pointer to ref model this surface is part of
};

struct renderInfo_s
{
	bool						headless;
	uint32_t					frameCount;
	glm::ivec2					target_size;
};

struct viewDef_s {
	scene_s						scene;
	Frustum						view_frustum;
	SceneUpdates_s				updates;			// was there a update since the last frame
	glm::mat4					projection_matrix;
	glm::mat4					view_matrix;
	glm::mat4					inv_projection_matrix;
	glm::mat4					inv_view_matrix;

	glm::vec3					view_pos;
	glm::vec3					view_dir;

	uint32_t					frame_index;
	bool						headless;
	glm::ivec2					target_size;
	// processed data (after culling, front to back ordering, etc)
	std::deque<drawSurf_s>		surfaces;
	std::deque<RefModel_s*>		ref_models;
	std::deque<RefLight_s*>		lights;
};

struct uiConf_s {
	// scene
	RenderScene*				scene;
	RenderSystem*				system;
	// draw
	DrawInfo_s*					draw_info;
};

struct screenshotInfo_s {
	std::string					name;
	int							width;
	int							height;
	int							channels;
	uint8_t*					data;
};

enum class RenderCommand
{
	EMPTY,
	BEGIN_FRAME,
	END_FRAME,
	DRAW_VIEW,
	DRAW_UI,
	ENTITY_ADDED,
	ENTITY_REMOVED,
	ASSET_ADDED,
	ASSET_REMOVED,
	SCREENSHOT,
	IMPL_SCREENSHOT,
	IMPL_DRAW_ON_MESH
};

struct RCmd_s
{
	RenderCommand				mType;
	union {
		// will be deleted after the commands were processed
		viewDef_s*				mViewDef;			// if type == DRAW_VIEW
		uiConf_s*				mUiConf;			// if type == DRAW_UI
		screenshotInfo_s*		mScreenshotInfo;	// if type == SCREENSHOT
		Ref_s*					mEntity;			// if type == ENTITY_ADDED || type == ENTITY_REMOVED
		Asset*					mAsset;				// if type == ASSET_ADDED || type == ASSET_REMOVED
		DrawInfo_s*				mDrawInfo;			// if type == IMPL_DRAW_ON_MESH
	};
};

class RenderCmdSystem {
public:

	void					addBeginFrameCmd();
	void					addEndFrameCmd();
    void					addDrawSurfCmd(viewDef_s* aViewDef);
    void					addDrawUICmd(uiConf_s* aUiConf);
	void					addScreenshotCmd(const std::string& aFileName);
	void					addImplScreenshotCmd();
	void					addEntityAddedCmd(Ref_s* aRef);
	void					addEntityRemovedCmd(Ref_s* aRef);
	void					addAssetAddedCmd(Asset* aAsset);
	void					addAssetRemovedCmd(Asset* aAsset);
	void					addDrawOnMeshCmd(const DrawInfo_s* aDrawInfo, const HitInfo_s* aHitInfo);

	bool					nextCmd();
	RCmd_s					popNextCmd();
	void					deleteCmd(const RCmd_s& aCmd);

	uint32_t				frames() const;
private:
	void					addCmd(const RCmd_s& aCmd);

	std::deque<RCmd_s>		mCmdList;
	std::mutex				mMutex;
	std::atomic_uint32_t	mFrames;
};
extern RenderCmdSystem renderCmdSystem;
T_END_NAMESPACE
