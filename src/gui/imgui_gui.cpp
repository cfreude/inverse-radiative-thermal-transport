#include <tamashii/gui/imgui_gui.hpp>
#include <tamashii/engine/common/input.hpp>
#include <tamashii/engine/common/common.hpp>
#include <tamashii/engine/common/math.hpp>
#include <tamashii/engine/exporter/exporter.hpp>
#include <tamashii/engine/scene/render_cmd_system.hpp>
#include <tamashii/engine/render/render_system.hpp>
#include <tamashii/engine/render/render_backend_implementation.hpp>
#include <tamashii/engine/common/vars.hpp>
#include <imgui.h>
#include <ImGuizmo.h>
#include <imoguizmo.hpp>

T_USE_NAMESPACE

namespace {
    bool decomposeTransform(const glm::mat4& aTransform, glm::vec3& aTranslation, glm::quat& aRotation, glm::vec3& aScale)
    {
        // From glm::decompose in matrix_decompose.inl
        // thanks cherno
        using namespace glm;
        using T = float;

        mat4 LocalMatrix(aTransform);
        // Normalize the matrix.
        if (epsilonEqual(LocalMatrix[3][3], static_cast<float>(0), epsilon<T>())) return false;
        // First, isolate perspective.  This is the messiest.
        if (
            epsilonNotEqual(LocalMatrix[0][3], static_cast<T>(0), epsilon<T>()) ||
            epsilonNotEqual(LocalMatrix[1][3], static_cast<T>(0), epsilon<T>()) ||
            epsilonNotEqual(LocalMatrix[2][3], static_cast<T>(0), epsilon<T>()))
        {
            // Clear the perspective partition
            LocalMatrix[0][3] = LocalMatrix[1][3] = LocalMatrix[2][3] = static_cast<T>(0);
            LocalMatrix[3][3] = static_cast<T>(1);
        }
        // Next take care of translation (easy).
        aTranslation = vec3(LocalMatrix[3]);
        LocalMatrix[3] = vec4(0, 0, 0, LocalMatrix[3].w);

        vec3 Row[3];
        // Now get scale.
        for (length_t i = 0; i < 3; ++i)
            for (length_t j = 0; j < 3; ++j)
                Row[i][j] = LocalMatrix[i][j];

        // Compute X scale factor and normalize first row.
        aScale.x = length(Row[0]);
        Row[0] = detail::scale(Row[0], static_cast<T>(1));
        aScale.y = length(Row[1]);
        Row[1] = detail::scale(Row[1], static_cast<T>(1));
        aScale.z = length(Row[2]);
        Row[2] = detail::scale(Row[2], static_cast<T>(1));
        // rotation
        int i, j, k = 0;
        T root, trace = Row[0].x + Row[1].y + Row[2].z;
        if (trace > static_cast<T>(0))
        {
            root = sqrt(trace + static_cast<T>(1.0));
            aRotation.w = static_cast<T>(0.5) * root;
            root = static_cast<T>(0.5) / root;
            aRotation.x = root * (Row[1].z - Row[2].y);
            aRotation.y = root * (Row[2].x - Row[0].z);
            aRotation.z = root * (Row[0].y - Row[1].x);
        } // End if > 0
        else
        {
            static int Next[3] = { 1, 2, 0 };
            i = 0;
            if (Row[1].y > Row[0].x) i = 1;
            if (Row[2].z > Row[i][i]) i = 2;
            j = Next[i];
            k = Next[j];

            root = sqrt(Row[i][i] - Row[j][j] - Row[k][k] + static_cast<T>(1.0));

            aRotation[i] = static_cast<T>(0.5) * root;
            root = static_cast<T>(0.5) / root;
            aRotation[j] = root * (Row[i][j] + Row[j][i]);
            aRotation[k] = root * (Row[i][k] + Row[k][i]);
            aRotation.w = root * (Row[j][k] - Row[k][j]);
        } // End if <= 0

        return true;
    }
	void updateSceneGraphFromModelMatrix(const Ref_s* aRef)
    {
		if(aRef->transforms.empty()) return;
		glm::mat4 sg_model_matrix = glm::mat4(1.0f);
		for (TRS* trs : aRef->transforms) {
			sg_model_matrix *= trs->getMatrix(0);
		}

		glm::vec3 sg_scale;
		glm::quat sg_rotation;
		glm::vec3 sg_translation;
		ASSERT(decomposeTransform(sg_model_matrix, sg_translation, sg_rotation, sg_scale), "decompose error");

		glm::vec3 c_scale;
		glm::quat c_rotation;
		glm::vec3 c_translation;
		ASSERT(decomposeTransform(aRef->model_matrix, c_translation, c_rotation, c_scale), "decompose error");

		const bool translationNeedsUpdate = glm::any(glm::notEqual(sg_translation, c_translation));
		const bool scaleNeedsUpdate = glm::any(glm::notEqual(sg_scale, c_scale));
		const bool rotationNeedsUpdate = glm::any(glm::notEqual(sg_rotation, c_rotation));
		const bool trsNeedsUpdate = (translationNeedsUpdate || scaleNeedsUpdate || rotationNeedsUpdate);
		if (!trsNeedsUpdate) return;

		TRS* trs = aRef->transforms.front();
		if (translationNeedsUpdate)
		{
			const glm::vec3 diff = c_translation - sg_translation;
			trs->translation += diff;
			if (trs->hasTranslationAnimation()) for (glm::vec3& v : trs->translation_steps) v += diff;
		}
		if (scaleNeedsUpdate)
		{
			// old_scale * x = new_scale
			// x = new_scale/old_scale
			const glm::vec3 diff = c_scale / sg_scale;
			if (trs->hasScale()) trs->scale *= diff;
			else trs->scale = diff;
			if (trs->hasScaleAnimation()) for (glm::vec3& v : trs->scale_steps) v *= diff;
		}
		if (rotationNeedsUpdate)
		{
			const glm::quat diffQuad = c_rotation * glm::inverse(sg_rotation);

			if (trs->hasRotation())
			{
				const glm::quat newQuad = diffQuad * glm::quat(trs->rotation[3], trs->rotation[0], trs->rotation[1], trs->rotation[2]);
				trs->rotation = glm::make_vec4(&newQuad[0]);
			}
			else trs->rotation = glm::make_vec4(&diffQuad[0]);
			if (trs->hasRotationAnimation()) for (glm::vec4& rotVec : trs->rotation_steps) {
				const glm::quat newRotation = diffQuad * glm::quat(rotVec[3], rotVec[0], rotVec[1], rotVec[2]);
				rotVec = glm::make_vec4(&newRotation[0]);
			}
		}
    }
}

MainGUI::MainGUI(): mIo(nullptr), mUc(nullptr), mVerticalOffsetMenuBar(20),
                    mShowHelp(false), mShowAbout(false), mShowSaveScene(false)
{
}

void MainGUI::draw(uiConf_s* aUiConf)
{
	mIo = &ImGui::GetIO();
	mUc = aUiConf;
	// highlight lights
	markLights();
	//markCamera();
	// draw overlay
	showDraw();
	// imguizmo
	drawImGuizmo();
	if (mUc->system->getConfig().show_gui) {
		// gizmo
		drawGizmo();
		// menubar or console
		drawMenuBar();
		//drawConsole();
		// menu
		drawMenu();
		// edit
		drawEdit();
		// draw
		drawDraw();
		// right click menu
		drawRightClickMenu();
		// info
		drawInfo();
		// save scene
		if (mShowSaveScene) showSaveScene();
		// help
		if (mShowHelp) showHelp();
		// about
		if (mShowAbout) showAbout();
	}
}

void MainGUI::markLights()
{
	
	if (((RenderSystem*)mUc->system)->getConfig().mark_lights) {
		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
		ImGui::SetNextWindowSize(mIo->DisplaySize);
		const auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus;
		if (ImGui::Begin("Lights", NULL, flags))
		{
			for (RefLight_s *ref : *mUc->scene->getLightList()) {
				if (glm::any(glm::isnan(ref->position))) continue;
				RefCameraPrivate_s* cam = static_cast<RefCameraPrivate_s*>(mUc->scene->getCurrentCamera());
				glm::mat4 projection_matrix = cam->camera->getProjectionMatrix();
				glm::mat4 view_matrix = cam->view_matrix;


				glm::ivec3 c = var::light_overlay_color.getInt3();
				ImU32 color = IM_COL32(c.x, c.y, c.z, 255);
				Ref_s* selection = mUc->scene->getSelection();
				if (selection && selection->type == Ref_s::Type::Light && ((RefLight_s*)selection)->ref_light_index == ref->ref_light_index) {
					color = IM_COL32(225, 156, 99, 255);
				}

				if (ref->light->getType() != Light::Type::SURFACE) {

					glm::vec4 center = projection_matrix * (view_matrix * glm::vec4(ref->position, 1.0f));
					glm::vec3 ndcSpaceCenter = glm::vec3(center) / center.w;
					glm::vec2 windowSpaceCenter = ((glm::vec2(ndcSpaceCenter) + glm::vec2(1.0)) / glm::vec2(2.0)) * glm::vec2(mIo->DisplaySize.x, mIo->DisplaySize.y);

					if (center.w < 0 || windowSpaceCenter.x < 0 || windowSpaceCenter.y < 0 || windowSpaceCenter.x > mIo->DisplaySize.x || windowSpaceCenter.y > mIo->DisplaySize.y) continue;

					glm::vec4 radius = projection_matrix * (view_matrix * glm::vec4(ref->position + (glm::vec3(LIGHT_OVERLAY_RADIUS) * glm::vec3(view_matrix[0][0], view_matrix[1][0], view_matrix[2][0])), 1.0f));
					glm::vec3 ndcSpaceRadius = glm::vec3(radius) / radius.w;
					glm::vec2 windowSpacePosRadius = ((glm::vec2(ndcSpaceRadius) + glm::vec2(1.0)) / glm::vec2(2.0)) * glm::vec2(mIo->DisplaySize.x, mIo->DisplaySize.y);
					float r = glm::abs(glm::length(windowSpaceCenter - windowSpacePosRadius));


					// TODO: different overlays for different light types
					ImGui::GetBackgroundDrawList()->AddCircle(ImVec2(windowSpaceCenter.x, windowSpaceCenter.y), r, color);

					if (ref->light->getType() != Light::Type::POINT) {
						glm::vec4 dir = projection_matrix * (view_matrix * glm::vec4(ref->position + ref->direction * LIGHT_OVERLAY_RADIUS * 10.0f, 1.0f));
						glm::vec3 ndcSpaceDir = glm::vec3(dir) / dir.w;
						glm::vec2 windowSpaceDir = ((glm::vec2(ndcSpaceDir) + glm::vec2(1.0)) / glm::vec2(2.0)) * glm::vec2(mIo->DisplaySize.x, mIo->DisplaySize.y);
						ImGui::GetBackgroundDrawList()->AddLine(ImVec2(windowSpaceCenter.x, windowSpaceCenter.y), ImVec2(windowSpaceDir.x, windowSpaceDir.y), color);
					}
					else {
						ImGui::GetBackgroundDrawList()->AddCircleFilled(ImVec2(windowSpaceCenter.x, windowSpaceCenter.y), 1, color);
					}
				}
				else {
					SurfaceLight* sl = (SurfaceLight*)ref->light;
					const glm::mat4 mvp_mat = projection_matrix * view_matrix * ref->model_matrix;
					if (sl->getShape() == SurfaceLight::Shape::SQUARE || sl->getShape() == SurfaceLight::Shape::RECTANGLE) {
						glm::vec4 center = mvp_mat * sl->getCenter();
						glm::vec4 direction = mvp_mat * (sl->getCenter() + sl->getDefaultDirection() * 0.5f);
						glm::vec4 p0 = mvp_mat * (sl->getCenter() + glm::vec4(-0.5, -0.5, 0, 0));
						glm::vec4 p1 = mvp_mat * (sl->getCenter() + glm::vec4(-0.5, 0.5, 0, 0));
						glm::vec4 p2 = mvp_mat * (sl->getCenter() + glm::vec4(0.5, -0.5, 0, 0));
						glm::vec4 p3 = mvp_mat * (sl->getCenter() + glm::vec4(0.5, 0.5, 0, 0));

						glm::vec3 ndcSpaceCenter = glm::vec3(center) / center.w;
						glm::vec2 windowSpaceCenter = ((glm::vec2(ndcSpaceCenter) + glm::vec2(1.0)) / glm::vec2(2.0)) * glm::vec2(mIo->DisplaySize.x, mIo->DisplaySize.y);
						glm::vec3 ndcSpaceDirection = glm::vec3(direction) / direction.w;
						glm::vec2 windowSpaceDirection = ((glm::vec2(ndcSpaceDirection) + glm::vec2(1.0)) / glm::vec2(2.0)) * glm::vec2(mIo->DisplaySize.x, mIo->DisplaySize.y);
						glm::vec3 ndcSpaceP0 = glm::vec3(p0) / p0.w;
						glm::vec2 windowSpaceP0 = ((glm::vec2(ndcSpaceP0) + glm::vec2(1.0)) / glm::vec2(2.0)) * glm::vec2(mIo->DisplaySize.x, mIo->DisplaySize.y);
						glm::vec3 ndcSpaceP1 = glm::vec3(p1) / p1.w;
						glm::vec2 windowSpaceP1 = ((glm::vec2(ndcSpaceP1) + glm::vec2(1.0)) / glm::vec2(2.0)) * glm::vec2(mIo->DisplaySize.x, mIo->DisplaySize.y);
						glm::vec3 ndcSpaceP2 = glm::vec3(p2) / p2.w;
						glm::vec2 windowSpaceP2 = ((glm::vec2(ndcSpaceP2) + glm::vec2(1.0)) / glm::vec2(2.0)) * glm::vec2(mIo->DisplaySize.x, mIo->DisplaySize.y);
						glm::vec3 ndcSpaceP3 = glm::vec3(p3) / p3.w;
						glm::vec2 windowSpaceP3 = ((glm::vec2(ndcSpaceP3) + glm::vec2(1.0)) / glm::vec2(2.0)) * glm::vec2(mIo->DisplaySize.x, mIo->DisplaySize.y);

						if (p0.w > 0 && p1.w > 0) ImGui::GetBackgroundDrawList()->AddLine(ImVec2(windowSpaceP0.x, windowSpaceP0.y), ImVec2(windowSpaceP1.x, windowSpaceP1.y), color);
						if (p0.w > 0 && p2.w > 0) ImGui::GetBackgroundDrawList()->AddLine(ImVec2(windowSpaceP0.x, windowSpaceP0.y), ImVec2(windowSpaceP2.x, windowSpaceP2.y), color);
						if (p3.w > 0 && p1.w > 0) ImGui::GetBackgroundDrawList()->AddLine(ImVec2(windowSpaceP3.x, windowSpaceP3.y), ImVec2(windowSpaceP1.x, windowSpaceP1.y), color);
						if (p3.w > 0 && p2.w > 0) ImGui::GetBackgroundDrawList()->AddLine(ImVec2(windowSpaceP3.x, windowSpaceP3.y), ImVec2(windowSpaceP2.x, windowSpaceP2.y), color);
						if (center.w > 0 && direction.w > 0) ImGui::GetBackgroundDrawList()->AddLine(ImVec2(windowSpaceCenter.x, windowSpaceCenter.y), ImVec2(windowSpaceDirection.x, windowSpaceDirection.y), color);
					}
					else if (sl->getShape() == SurfaceLight::Shape::DISK || sl->getShape() == SurfaceLight::Shape::ELLIPSE) {
						glm::vec4 center = mvp_mat * sl->getCenter();
						glm::vec4 direction = mvp_mat * (sl->getCenter() + sl->getDefaultDirection() * 0.5f);

						glm::vec3 ndcSpaceCenter = glm::vec3(center) / center.w;
						glm::vec2 windowSpaceCenter = ((glm::vec2(ndcSpaceCenter) + glm::vec2(1.0)) / glm::vec2(2.0)) * glm::vec2(mIo->DisplaySize.x, mIo->DisplaySize.y);
						glm::vec3 ndcSpaceDirection = glm::vec3(direction) / direction.w;
						glm::vec2 windowSpaceDirection = ((glm::vec2(ndcSpaceDirection) + glm::vec2(1.0)) / glm::vec2(2.0)) * glm::vec2(mIo->DisplaySize.x, mIo->DisplaySize.y);
						if (center.w > 0 && direction.w > 0) ImGui::GetBackgroundDrawList()->AddLine(ImVec2(windowSpaceCenter.x, windowSpaceCenter.y), ImVec2(windowSpaceDirection.x, windowSpaceDirection.y), color);
	
						glm::vec4 t = glm::vec4(0.5f, 0, 0, 0);
						glm::vec4 b = glm::vec4(0, 0.5f, 0, 0);

						int sides = 360 / 360;
						std::vector<ImVec2> points;
						points.reserve(sides);
						int count = 0;
						for (float a = 0; a < 361; a += sides) {
							float rad = glm::radians(a);
							glm::vec3 p0 = (t) * std::sin(rad) + (b) * std::cos(rad);
							glm::vec4 p = mvp_mat * (sl->getCenter() + glm::vec4(p0,0));
							glm::vec3 ndcSpaceP = glm::vec3(p) / p.w;
							glm::vec2 windowSpaceP = ((glm::vec2(ndcSpaceP) + glm::vec2(1.0)) / glm::vec2(2.0)) * glm::vec2(mIo->DisplaySize.x, mIo->DisplaySize.y);
							//if (p.x < 0 || p.y < 0 || p.x > io->DisplaySize.x || p.y > io->DisplaySize.y) continue;
							points.emplace_back(windowSpaceP.x, windowSpaceP.y);
							count++;
						}
                        ImGui::GetBackgroundDrawList()->AddPolyline(points.data(), count, color, 0, 1);
					}
				}
			}
			ImGui::End();
		}
	}
	
}

void MainGUI::markCamera()
{
	RefCamera_s* cc = mUc->scene->getCurrentCamera();
	std::deque<RefCamera_s*>* cams = mUc->scene->getAvailableCameras();
	for (RefCamera_s* rc : *cams) {
		if (rc == cc) continue;
		Frustum frust(rc);
		const glm::mat4 mvp_mat = cc->camera->getProjectionMatrix() * cc->view_matrix;
		ImU32 color = (ImU32)IM_COL32(220, 220, 220, 255);

		glm::vec4 center = mvp_mat * glm::vec4(frust.origin, 1);
		glm::vec4 p0 = mvp_mat * glm::vec4(frust.near_corners[0], 1);
		glm::vec4 p1 = mvp_mat * glm::vec4(frust.far_corners[0], 1);

		glm::vec4 p2 = mvp_mat * glm::vec4(frust.near_corners[1], 1);
		glm::vec4 p3 = mvp_mat * glm::vec4(frust.far_corners[1], 1);

		glm::vec4 p4 = mvp_mat * glm::vec4(frust.near_corners[2], 1);
		glm::vec4 p5 = mvp_mat * glm::vec4(frust.far_corners[2], 1);

		glm::vec4 p6 = mvp_mat * glm::vec4(frust.near_corners[3], 1);
		glm::vec4 p7 = mvp_mat * glm::vec4(frust.far_corners[3], 1);

		glm::vec3 ndcSpaceCenter = glm::vec3(center) / center.w;
		glm::vec2 windowSpaceCenter = ((glm::vec2(ndcSpaceCenter) + glm::vec2(1.0)) / glm::vec2(2.0)) * glm::vec2(mIo->DisplaySize.x, mIo->DisplaySize.y);
		glm::vec3 ndcSpaceP0 = glm::vec3(p0) / p0.w;
		glm::vec2 windowSpaceP0 = ((glm::vec2(ndcSpaceP0) + glm::vec2(1.0)) / glm::vec2(2.0)) * glm::vec2(mIo->DisplaySize.x, mIo->DisplaySize.y);
		glm::vec3 ndcSpaceP1 = glm::vec3(p1) / p1.w;
		glm::vec2 windowSpaceP1 = ((glm::vec2(ndcSpaceP1) + glm::vec2(1.0)) / glm::vec2(2.0)) * glm::vec2(mIo->DisplaySize.x, mIo->DisplaySize.y);

		glm::vec3 ndcSpaceP2 = glm::vec3(p2) / p2.w;
		glm::vec2 windowSpaceP2 = ((glm::vec2(ndcSpaceP2) + glm::vec2(1.0)) / glm::vec2(2.0)) * glm::vec2(mIo->DisplaySize.x, mIo->DisplaySize.y);
		glm::vec3 ndcSpaceP3 = glm::vec3(p3) / p3.w;
		glm::vec2 windowSpaceP3 = ((glm::vec2(ndcSpaceP3) + glm::vec2(1.0)) / glm::vec2(2.0)) * glm::vec2(mIo->DisplaySize.x, mIo->DisplaySize.y);

		glm::vec3 ndcSpaceP4 = glm::vec3(p4) / p4.w;
		glm::vec2 windowSpaceP4 = ((glm::vec2(ndcSpaceP4) + glm::vec2(1.0)) / glm::vec2(2.0)) * glm::vec2(mIo->DisplaySize.x, mIo->DisplaySize.y);
		glm::vec3 ndcSpaceP5 = glm::vec3(p5) / p5.w;
		glm::vec2 windowSpaceP5 = ((glm::vec2(ndcSpaceP5) + glm::vec2(1.0)) / glm::vec2(2.0)) * glm::vec2(mIo->DisplaySize.x, mIo->DisplaySize.y);

		glm::vec3 ndcSpaceP6 = glm::vec3(p6) / p6.w;
		glm::vec2 windowSpaceP6 = ((glm::vec2(ndcSpaceP6) + glm::vec2(1.0)) / glm::vec2(2.0)) * glm::vec2(mIo->DisplaySize.x, mIo->DisplaySize.y);
		glm::vec3 ndcSpaceP7 = glm::vec3(p7) / p7.w;
		glm::vec2 windowSpaceP7 = ((glm::vec2(ndcSpaceP7) + glm::vec2(1.0)) / glm::vec2(2.0)) * glm::vec2(mIo->DisplaySize.x, mIo->DisplaySize.y);

		if (p0.w > 0 && p1.w > 0) ImGui::GetBackgroundDrawList()->AddLine(ImVec2(windowSpaceP0.x, windowSpaceP0.y), ImVec2(windowSpaceP1.x, windowSpaceP1.y), color);
		if (p2.w > 0 && p3.w > 0) ImGui::GetBackgroundDrawList()->AddLine(ImVec2(windowSpaceP2.x, windowSpaceP2.y), ImVec2(windowSpaceP3.x, windowSpaceP3.y), color);
		if (p4.w > 0 && p5.w > 0) ImGui::GetBackgroundDrawList()->AddLine(ImVec2(windowSpaceP4.x, windowSpaceP4.y), ImVec2(windowSpaceP5.x, windowSpaceP5.y), color);
		if (p6.w > 0 && p7.w > 0) ImGui::GetBackgroundDrawList()->AddLine(ImVec2(windowSpaceP6.x, windowSpaceP6.y), ImVec2(windowSpaceP7.x, windowSpaceP7.y), color);

		if (p0.w > 0 && p2.w > 0) ImGui::GetBackgroundDrawList()->AddLine(ImVec2(windowSpaceP0.x, windowSpaceP0.y), ImVec2(windowSpaceP2.x, windowSpaceP2.y), color);
		if (p2.w > 0 && p4.w > 0) ImGui::GetBackgroundDrawList()->AddLine(ImVec2(windowSpaceP2.x, windowSpaceP2.y), ImVec2(windowSpaceP4.x, windowSpaceP4.y), color);
		if (p4.w > 0 && p6.w > 0) ImGui::GetBackgroundDrawList()->AddLine(ImVec2(windowSpaceP4.x, windowSpaceP4.y), ImVec2(windowSpaceP6.x, windowSpaceP6.y), color);
		if (p6.w > 0 && p0.w > 0) ImGui::GetBackgroundDrawList()->AddLine(ImVec2(windowSpaceP6.x, windowSpaceP6.y), ImVec2(windowSpaceP0.x, windowSpaceP0.y), color);

		if (p0.w > 0 && center.w > 0) ImGui::GetBackgroundDrawList()->AddLine(ImVec2(windowSpaceP0.x, windowSpaceP0.y), ImVec2(windowSpaceCenter.x, windowSpaceCenter.y), color);
		if (p2.w > 0 && center.w > 0) ImGui::GetBackgroundDrawList()->AddLine(ImVec2(windowSpaceP2.x, windowSpaceP2.y), ImVec2(windowSpaceCenter.x, windowSpaceCenter.y), color);
		if (p4.w > 0 && center.w > 0) ImGui::GetBackgroundDrawList()->AddLine(ImVec2(windowSpaceP4.x, windowSpaceP4.y), ImVec2(windowSpaceCenter.x, windowSpaceCenter.y), color);
		if (p6.w > 0 && center.w > 0) ImGui::GetBackgroundDrawList()->AddLine(ImVec2(windowSpaceP6.x, windowSpaceP6.y), ImVec2(windowSpaceCenter.x, windowSpaceCenter.y), color);

		if (p1.w > 0 && p3.w > 0) ImGui::GetBackgroundDrawList()->AddLine(ImVec2(windowSpaceP1.x, windowSpaceP1.y), ImVec2(windowSpaceP3.x, windowSpaceP3.y), color);
		if (p3.w > 0 && p5.w > 0) ImGui::GetBackgroundDrawList()->AddLine(ImVec2(windowSpaceP3.x, windowSpaceP3.y), ImVec2(windowSpaceP5.x, windowSpaceP5.y), color);
		if (p5.w > 0 && p7.w > 0) ImGui::GetBackgroundDrawList()->AddLine(ImVec2(windowSpaceP5.x, windowSpaceP5.y), ImVec2(windowSpaceP7.x, windowSpaceP7.y), color);
		if (p7.w > 0 && p1.w > 0) ImGui::GetBackgroundDrawList()->AddLine(ImVec2(windowSpaceP7.x, windowSpaceP7.y), ImVec2(windowSpaceP1.x, windowSpaceP1.y), color);
	}
}

void MainGUI::showDraw()
{
	if (mUc->draw_info->mDrawMode && mUc->draw_info->mHoverOver && mUc->scene->getCurrentCamera()->mode == RefCamera_s::Mode::EDITOR) {
		ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
		ImGui::SetNextWindowSize(mIo->DisplaySize);
		constexpr auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus;
		if (ImGui::Begin("DrawOverlay", nullptr, flags))
		{
			ImColor color(mUc->draw_info->mCursorColor.x, mUc->draw_info->mCursorColor.y, mUc->draw_info->mCursorColor.z);
			auto cam = static_cast<RefCameraPrivate_s*>(mUc->scene->getCurrentCamera());
			glm::mat4 projection_matrix = cam->camera->getProjectionMatrix();
			glm::mat4 view_matrix = cam->view_matrix;
			DrawInfo_s di = *mUc->draw_info;
			const glm::mat4 vp_mat = projection_matrix * view_matrix;
			glm::vec4 center = vp_mat * glm::vec4(di.mPositionWs, 1);
			//glm::vec4 direction = vp_mat * (glm::vec4(di.pos, 1) + glm::vec4(di.triangle.geo_n,0));

			glm::vec4 direction = vp_mat * (glm::vec4(di.mPositionWs, 1) + (glm::vec4(di.mNormalWsNorm,0) * mUc->draw_info->mRadius));

			glm::vec3 ndcSpaceCenter = glm::vec3(center) / center.w;
			glm::vec2 windowSpaceCenter = ((glm::vec2(ndcSpaceCenter) + glm::vec2(1.0)) / glm::vec2(2.0)) * glm::vec2(mIo->DisplaySize.x, mIo->DisplaySize.y);
			glm::vec3 ndcSpaceDirection = glm::vec3(direction) / direction.w;
			glm::vec2 windowSpaceDirection = ((glm::vec2(ndcSpaceDirection) + glm::vec2(1.0)) / glm::vec2(2.0)) * glm::vec2(mIo->DisplaySize.x, mIo->DisplaySize.y);
			if (center.w > 0 && direction.w > 0) ImGui::GetBackgroundDrawList()->AddLine(ImVec2(windowSpaceCenter.x, windowSpaceCenter.y), ImVec2(windowSpaceDirection.x, windowSpaceDirection.y), color);
	
            glm::vec3 b_ws_norm = glm::cross(di.mNormalWsNorm, di.mTangentWsNorm);

			int sides = 360 / 360;
			std::vector<ImVec2> points;
			points.reserve(sides);
			int count = 0;
			for (float a = 0; a < 361; a += sides) {
				float rad = glm::radians(a);
                glm::vec3 p0 = di.mTangentWsNorm * std::sin(rad) + b_ws_norm * std::cos(rad);
				glm::vec4 p = vp_mat * (glm::vec4(di.mPositionWs, 1) + (glm::vec4(normalize(p0),0) * mUc->draw_info->mRadius));
				glm::vec3 ndcSpaceP = glm::vec3(p) / p.w;
				glm::vec2 windowSpaceP = ((glm::vec2(ndcSpaceP) + glm::vec2(1.0)) / glm::vec2(2.0)) * glm::vec2(mIo->DisplaySize.x, mIo->DisplaySize.y);
				//if (p.x < 0 || p.y < 0 || p.x > io->DisplaySize.x || p.y > io->DisplaySize.y) continue;
				points.emplace_back(windowSpaceP.x, windowSpaceP.y);
				count++;
			}
            ImGui::GetBackgroundDrawList()->AddPolyline(points.data(), count, color, 0, 1);

			ImGui::End();
		}
	}
}

void MainGUI::drawImGuizmo()
{
	static ImGuizmo::OPERATION	op = ImGuizmo::TRANSLATE;
	static ImGuizmo::MODE mode = ImGuizmo::WORLD;

	if (Ref_s *selection = mUc->scene->getSelection()) {
		ImGuizmo::BeginFrame();
		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetRect(0, 0, mIo->DisplaySize.x, mIo->DisplaySize.y);
		ImGuizmo::AllowAxisFlip(false);

		// disable imguizmo when rotating
		if (InputSystem::getInstance().isDown(Input::MOUSE_WHEEL)) ImGuizmo::Enable(false);
		else ImGuizmo::Enable(true);

		// camera data
		RefCameraPrivate_s* cam = static_cast<RefCameraPrivate_s*>(mUc->scene->getCurrentCamera());
		glm::mat4 projection_matrix = cam->camera->getProjectionMatrix();
		glm::mat4 view_matrix = cam->view_matrix;
		if(cam->y_flipped) { view_matrix[0][1] *= -1; view_matrix[1][1] *= -1; view_matrix[2][1] *= -1; view_matrix[3][1] *= -1; }
 
		// switch modes
		if (InputSystem::getInstance().wasReleased(Input::KEY_T)) op = ImGuizmo::TRANSLATE;
		else if (InputSystem::getInstance().wasReleased(Input::KEY_R)) op = ImGuizmo::ROTATE;
		else if (InputSystem::getInstance().wasReleased(Input::KEY_S)) op = ImGuizmo::SCALE;
		if (InputSystem::getInstance().wasReleased(Input::KEY_W)) mode = ImGuizmo::WORLD;
		else if (InputSystem::getInstance().wasReleased(Input::KEY_L)) mode = ImGuizmo::LOCAL;

		if(op == ImGuizmo::TRANSLATE) mOp = "Translate";
		else if (op == ImGuizmo::ROTATE) mOp = "Rotate";
		else if (op == ImGuizmo::SCALE) mOp = "Scale";
		if (mode == ImGuizmo::LOCAL) mMode = "Local";
		else if (mode == ImGuizmo::WORLD) mMode = "World";

		// save model matrix to history
		static bool drag = false;
		if (!drag && ImGuizmo::IsUsing()) {
			mHistory.emplace_back(selection, selection->model_matrix);
		};
		drag = ImGuizmo::IsUsing();

		// manipulate
		glm::mat4 delta(1.0f);
		static float snap[3] = { 1,1,1 };
		const float* pSnap = InputSystem::getInstance().isDown(Input::KEY_LCTRL) ? &snap[0] : nullptr;
		if(ImGuizmo::Manipulate(glm::value_ptr(view_matrix), glm::value_ptr(projection_matrix), op, mode,
			glm::value_ptr(selection->model_matrix), glm::value_ptr(delta), pSnap)) {

			TRS* trs = nullptr;
			if (selection->type == Ref_s::Type::Light) {
				RefLight_s* r = (RefLight_s*)selection;
				r->position = r->model_matrix * glm::vec4(0, 0, 0, 1);
				r->direction = glm::normalize(r->model_matrix * r->light->getDefaultDirection());
				if (r->light->getType() == Light::Type::SURFACE) {
					const float scale_x = glm::length(glm::vec3(r->model_matrix[0]));
					const float scale_y = glm::length(glm::vec3(r->model_matrix[1]));
					const float scale_z = glm::length(glm::vec3(r->model_matrix[2]));
					((SurfaceLight*)((RefLight_s*)selection)->light)->setDimensions(glm::vec3(scale_x, scale_y, scale_z));
				}
				mUc->scene->requestLightUpdate();
				if(!r->transforms.empty()) trs = r->transforms.front();
			}
			else if (selection->type == Ref_s::Type::Model) {
				mUc->scene->requestModelInstanceUpdate();
				bool light = false;
				for (const RefMesh_s* refMesh : ((RefModel_s*)selection)->refMeshes) light |= refMesh->mesh->getMaterial()->isLight();
				if (light) mUc->scene->requestLightUpdate();
				if (!((RefModel_s*)selection)->transforms.empty()) trs = ((RefModel_s*)selection)->transforms.front();
			}
			if(trs)
			{
				glm::vec3 scale;
				glm::quat rotation;
				glm::vec3 translation;
                ASSERT(decomposeTransform(delta, translation, rotation, scale), "decompose error");
				/* trans  */
				if (op == ImGuizmo::OPERATION::TRANSLATE) {
					trs->translation += translation;
					if (trs->hasTranslationAnimation()) for (glm::vec3& v : trs->translation_steps) v += translation;
				}
				/* scale  */
				if (op == ImGuizmo::OPERATION::SCALE) {
					if (trs->hasScale()) trs->scale *= scale;
					else trs->scale = scale;
					if (trs->hasScaleAnimation()) for (glm::vec3& v : trs->scale_steps) v *= scale;
				}
				/* rotate */
				if (op == ImGuizmo::OPERATION::ROTATE) {
					if (trs->hasRotation()) {
						glm::quat newRotatio = rotation * glm::make_quat(&trs->rotation[0]);
						trs->rotation = glm::make_vec4(&newRotatio[0]);
					}
					else trs->rotation = glm::make_vec4(&rotation[0]);
					if (trs->hasRotationAnimation()) for (glm::vec4& v : trs->rotation_steps) {
						glm::quat newRotatio = rotation * glm::make_quat(&v[0]);
						v = glm::make_vec4(&newRotatio[0]);
					}
				}
			}
		}
	}
	if (mUc->scene->getCurrentCamera()->mode == RefCamera_s::Mode::EDITOR && InputSystem::getInstance().isDown(Input::KEY_LCTRL) && InputSystem::getInstance().wasPressed(Input::KEY_Z) && mHistory.size()) {
		while (mHistory.size() && mHistory.back().first == nullptr) mHistory.pop_back();
		mHistory.back().first->model_matrix = mHistory.back().second;
		if (mHistory.back().first->type == Ref_s::Type::Light) {
			RefLight_s* r = (RefLight_s*)mHistory.back().first;
			r->position = r->model_matrix * glm::vec4(0, 0, 0, 1);
			r->direction = glm::normalize(r->model_matrix * r->light->getDefaultDirection());
			mUc->scene->requestLightUpdate();
		}
		if (mHistory.back().first->type == Ref_s::Type::Model) {
			mUc->scene->requestModelInstanceUpdate();
		}
		mHistory.pop_back();
	}
}

void MainGUI::drawGizmo()
{
	float pivotDistance = -1.0f;
	const auto pcam = reinterpret_cast<RefCameraPrivate_s*>(mUc->scene->getCurrentCamera());
	if(mUc->scene->getCurrentCamera()->mode == RefCamera_s::Mode::EDITOR)
	{
		pivotDistance = pcam->editor_data.zoom;
	}
	ImOGuizmo::SetRect(0.0f, mIo->DisplaySize.y - 120.0f, 120.0f);
	ImOGuizmo::BeginFrame();
	const glm::mat4 projection_matrix = pcam->camera->getProjectionMatrix();
	glm::mat4 view_matrix = mUc->scene->getCurrentCamera()->view_matrix;
	if(pcam->y_flipped) { view_matrix[0][1] *= -1; view_matrix[1][1] *= -1; view_matrix[2][1] *= -1; view_matrix[3][1] *= -1; }
	if(ImOGuizmo::DrawGizmo(value_ptr(view_matrix), value_ptr(projection_matrix), pivotDistance))
	{
		glm::mat4 mm = glm::inverse(view_matrix);
		pcam->setModelMatrix(mm, true);
		mUc->scene->requestCameraUpdate();
	}

}

void MainGUI::drawMenuBar()
{
	if (InputSystem::getInstance().isDown(Input::KEY_LCTRL) && InputSystem::getInstance().wasPressed(Input::KEY_S)) {
		Common::getInstance().getMainWindow()->ungrabMouse();// free mouse when in fps mode
		mShowSaveScene = true;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1);
	ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(41.0f / 255.0f, 45.0f / 255.0f, 55.0f / 255.0f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(41.0f / 255.0f, 45.0f / 255.0f, 55.0f / 255.0f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(82.0f / 255.0f, 86.0f / 255.0f, 97.0f / 255.0f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(82.0f / 255.0f, 86.0f / 255.0f, 97.0f / 255.0f, 1.0f));
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New Scene")) EventSystem::queueEvent(EventType::ACTION, Input::A_NEW_SCENE);
			else if (ImGui::MenuItem("Open Scene", "CTRL+O")) Common::openFileDialogOpenScene();
			ImGui::Separator();
			if (ImGui::MenuItem("Add Scene", "CTRL+A")) Common::openFileDialogAddScene();
			else if (ImGui::MenuItem("Add Model", "CTRL+M")) Common::openFileDialogOpenModel();
			else if (ImGui::MenuItem("Add Light", "CTRL+L")) Common::openFileDialogOpenLight();

			ImGui::Separator();
			if (ImGui::MenuItem("Export Scene", "CTRL+S")) mShowSaveScene = true;
			ImGui::Separator();
			if (ImGui::MenuItem("Exit", "ALT+F4"))
			{
				EventSystem::queueEvent(EventType::ACTION, Input::A_EXIT, 0, 0, 0, "");
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("View"))
		{
			RenderConfig_s &rc = ((RenderSystem*)mUc->system)->getConfig();
			if (ImGui::MenuItem("Show UI", "F1", rc.show_gui)) {
				rc.show_gui = !(rc.show_gui);
			}
			if (ImGui::MenuItem("Show Lights", "F2", rc.mark_lights)) {
				rc.mark_lights = !(rc.mark_lights);
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Help"))
		{
			if (ImGui::MenuItem("Usage")) {
				mShowHelp = true;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("About")) {
				mShowAbout = true;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Clear Cache")) {
				EventSystem::queueEvent(EventType::ACTION, Input::A_CLEAR_CACHE);
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
	ImGui::PopStyleVar(1);
	ImGui::PopStyleColor(4);
}

void MainGUI::drawConsole()
{
	ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always, ImVec2(0.0f, 0.0f));
	ImGui::SetNextWindowSize(ImVec2(mIo->DisplaySize.x, 0.0f));

	constexpr auto flags =
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoSavedSettings;

	const ImGuiInputTextCallback cb = [](ImGuiInputTextCallbackData* data)
	{
		return 0;
	};

	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	if (ImGui::Begin("Console", nullptr, flags))
	{
		char a[256] = {};
		ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
		ImGui::SetKeyboardFocusHere(0);
		ImGui::InputText("##asd", &a[0], 30, ImGuiInputTextFlags_CallbackEdit, cb);
		ImGui::PopItemWidth();
		ImGui::End();
	}
	ImGui::PopStyleVar(2);
}

void MainGUI::drawMenu()
{
	const float distance = 10.0f;
	const ImVec2 pos = ImVec2(distance, mVerticalOffsetMenuBar + distance);
	const ImVec2 posPivot = ImVec2(0.0f, 0.0f);
	ImGui::SetNextWindowPos(pos, ImGuiCond_Always, posPivot);

	constexpr auto flags =
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoSavedSettings;

	if (ImGui::Begin("Settings", NULL, flags))
	{
		/*ImGui::Text("Info");
		ImGui::Separator();
		ImGui::BulletText("F5: take Screenshot");
		ImGui::Text("FPS Camera Controls");
		ImGui::Separator();
		ImGui::BulletText("Enter Camera: Left Click");
		ImGui::BulletText("Move: WASDQE");
		ImGui::BulletText("Change Speed: Scroll Wheel");
		ImGui::BulletText("Exit Camera: ESC");
		ImGui::NewLine();*/

		// select a camera
		ImGui::Text("Cameras:");
		if (mUc->scene->getAvailableCameras()->size() != 0) {
			if (ImGui::BeginCombo("##Camera Combo", mUc->scene->getCurrentCamera()->camera->getName().c_str(), ImGuiComboFlags_NoArrowButton)) {
				for (RefCamera_s *cam : *mUc->scene->getAvailableCameras()) {
					const bool is_selected = (cam->ref_camera_index == mUc->scene->getCurrentCamera()->ref_camera_index);
					if (ImGui::Selectable((cam->camera->getName() + "##" + std::to_string(cam->ref_camera_index)).c_str(), is_selected)) mUc->scene->setCurrentCamera(cam);
					if (is_selected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			RefCameraPrivate_s* pcam = ((RefCameraPrivate_s*)mUc->scene->getCurrentCamera());
			RefCamera_s::Mode mode = pcam->mode;
			const float width = ImGui::GetContentRegionAvail().x * (pcam->default_camera ? 0.5f : 0.3333f);
			if(pcam->mode == RefCamera_s::Mode::EDITOR)
			{
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.4f, 1.0f });
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.3f, 0.6f, 0.4f, 1.0f });
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.4f, 0.7f, 0.5f, 1.0f });
			}
			if (ImGui::Button("Editor", ImVec2(width, 0))) mode = RefCamera_s::Mode::EDITOR;
			if (pcam->mode == RefCamera_s::Mode::EDITOR) ImGui::PopStyleColor(3);
			// do not display static choice with default cam
			if (!pcam->default_camera)
			{
				ImGui::SameLine();
				if (pcam->mode == RefCamera_s::Mode::STATIC) {
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.4f, 1.0f });
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.3f, 0.6f, 0.4f, 1.0f });
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.4f, 0.7f, 0.5f, 1.0f });
				}
				if(ImGui::Button("Static", ImVec2(width, 0.0f))) mode = RefCamera_s::Mode::STATIC;
				if (pcam->mode == RefCamera_s::Mode::STATIC) ImGui::PopStyleColor(3);
			}
			ImGui::SameLine();
			if (pcam->mode == RefCamera_s::Mode::FPS)
			{
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.4f, 1.0f });
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.3f, 0.6f, 0.4f, 1.0f });
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.4f, 0.7f, 0.5f, 1.0f });
			}
			if (ImGui::Button("FPS", ImVec2(-1, 0))) mode = RefCamera_s::Mode::FPS;
			if (pcam->mode == RefCamera_s::Mode::FPS) ImGui::PopStyleColor(3);
			pcam->mode = mode;
			
		}
		ImGui::Separator();

		if (mUc->scene->getCycleTime() != 0.0f) {
			ImGui::Text("Animation:");
			if (mUc->scene->animation()) {
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.8f, 0.3f, 0.4f, 1.0f });
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.6f, 0.3f, 0.4f, 1.0f });
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.7f, 0.4f, 0.5f, 1.0f });
			}
			else {
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.4f, 1.0f });
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.3f, 0.6f, 0.4f, 1.0f });
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.4f, 0.7f, 0.5f, 1.0f });
			}

			if ((mUc->scene->animation() && ImGui::Button("Stop", ImVec2(50, 0))) || (!mUc->scene->animation() && ImGui::Button("Start", ImVec2(50, 0))))  mUc->scene->setAnimation(!mUc->scene->animation());
			ImGui::PopStyleColor(3);
			ImGui::SameLine();
			if (ImGui::Button("Reset", ImVec2(50, 0))) mUc->scene->resetAnimation();
			ImGui::SameLine();
			float current_scene_time = std::fmod(mUc->scene->getCurrentTime(), mUc->scene->getCycleTime());
			if (std::isnan(current_scene_time)) current_scene_time = 0;
			ImGui::Text("  %.2f / %.2f", current_scene_time, mUc->scene->getCycleTime());
			ImGui::Separator();
		}

		// implementation selection box
		ImGui::PushItemWidth(155);
		const RenderSystem* system = mUc->system;
		const std::vector<RenderBackendImplementation*>& availableImpl = system->getAvailableBackendImplementations();
		if (availableImpl.size() > 1) {
			ImGui::Text("Implementation:");

			RenderBackendImplementation* current_impl = system->getCurrentBackendImplementations();
			int next_impl_index = -1;
			if (ImGui::BeginCombo("##Implementation Combo", current_impl->getName(), ImGuiComboFlags_NoArrowButton)) {
                for (uint32_t i = 0; i < availableImpl.size(); i++)
				{
					const bool isSelected = availableImpl.at(i) == current_impl;
					if (ImGui::Selectable((std::string(availableImpl.at(i)->getName()) + "##" + std::to_string(i)).c_str(), isSelected)) next_impl_index = i;
					if (isSelected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			if (next_impl_index != -1 && current_impl != availableImpl.at(next_impl_index)) EventSystem::queueEvent(EventType::ACTION, Input::A_CHANGE_BACKEND_IMPL, next_impl_index);
		}
		//ImGui::PopItemWidth();
		//ImGui::SameLine();
		//if(ImGui::Button("Reload", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) EventSystem::queueEvent(EventType::ACTION, Input::A_RELOAD_BACKEND_IMPL);

		ImGui::End();
	}
	if (InputSystem::getInstance().wasPressed(Input::KEY_ESCAPE)) mShowHelp = mShowAbout = false;
}

void MainGUI::drawEdit()
{

	if (Ref_s* selection = mUc->scene->getSelection()) {
		bool lightsRequiereUpdate = false;
		bool materialsRequiereUpdate = false;
		bool modelInstancesRequiereUpdate = false;

		constexpr float distance = 10.0f;
		const ImVec2 pos = ImVec2(mIo->DisplaySize.x - distance, mIo->DisplaySize.y - distance);
		const ImVec2 posPivot = ImVec2(1.0f, 1.0f);
		ImGui::SetNextWindowPos(pos, ImGuiCond_Always, posPivot);

		constexpr auto flags =
			ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoSavedSettings;

		if (ImGui::Begin("Edit", nullptr, flags))
		{
			ImGui::Text("Edit Mode");
			std::string name;
			std::string selectionType;
			if (selection->type == Ref_s::Type::Model) {
				name = static_cast<RefModel_s*>(selection)->model->getName();
				selectionType = "Model";
			}
			else if (selection->type == Ref_s::Type::Light) {
				name = static_cast<RefLight_s*>(selection)->light->getName();
				Light::Type type = static_cast<RefLight_s*>(selection)->light->getType();
				switch (type) {
				case Light::Type::POINT: selectionType = "Point Light"; break;
				case Light::Type::SPOT: selectionType = "Spot Light"; break;
				case Light::Type::DIRECTIONAL: selectionType = "Directional Light"; break;
				case Light::Type::SURFACE: selectionType = "Surface Light"; break;
				case Light::Type::IES: selectionType = "IES Light"; break;
				}
			}
			ImGui::Text("Selected: %s", name.c_str());
			ImGui::Text("Type: %s", selectionType.c_str());
			ImGui::Text("Operation: %s", mOp.c_str());
			ImGui::Text("System: %s", mMode.c_str());
			bool model_mat_requieres_update = false;
			ImGui::Text("Model Mat:");
			ImGui::SameLine();
			model_mat_requieres_update |= ImGui::DragFloat4("##c0", &selection->model_matrix[0][0], 0.1f, 0, 0, "%.3f", 0);
			ImGui::Text("          ");
			ImGui::SameLine();
			model_mat_requieres_update |= ImGui::DragFloat4("##c1", &selection->model_matrix[1][0], 0.1f, 0, 0, "%.3f", 0);
			ImGui::Text("          ");
			ImGui::SameLine();
			model_mat_requieres_update |= ImGui::DragFloat4("##c2", &selection->model_matrix[2][0], 0.1f, 0, 0, "%.3f", 0);
			ImGui::Text("          ");
			ImGui::SameLine();
			model_mat_requieres_update |= ImGui::DragFloat4("##c3", &selection->model_matrix[3][0], 0.1f, 0, 0, "%.3f", 0);
			// todo use function in Ref_s
			if (model_mat_requieres_update) updateSceneGraphFromModelMatrix(selection);
			if (selection->type == Ref_s::Type::Light) {
				lightsRequiereUpdate |= model_mat_requieres_update;

				auto refLight = static_cast<RefLight_s*>(selection);
				if (model_mat_requieres_update) {
					refLight->position = refLight->model_matrix * glm::vec4(0, 0, 0, 1);
					refLight->direction = glm::normalize(refLight->model_matrix * refLight->light->getDefaultDirection());
					if (refLight->light->getType() == Light::Type::SURFACE) {
						const float scale_x = glm::length(glm::vec3(refLight->model_matrix[0]));
						const float scale_y = glm::length(glm::vec3(refLight->model_matrix[1]));
						const float scale_z = glm::length(glm::vec3(refLight->model_matrix[2]));
						//((SurfaceLight*)((RefLight_s*)uc->selection)->light)->setSize(glm::vec2((scale_x + scale_y) / 2.0f));
						static_cast<SurfaceLight*>(static_cast<RefLight_s*>(selection)->light)->setDimensions(glm::vec3(scale_x, scale_y, scale_z));
					}
				}
				glm::vec3 c = refLight->light->getColor();
				ImGui::Text("Color:    ");
				ImGui::SameLine();
				lightsRequiereUpdate |= ImGui::ColorEdit3("##light_color", &c[0]);
				static_cast<RefLight_s*>(selection)->light->setColor(c);

				float intensity = static_cast<RefLight_s*>(selection)->light->getIntensity();
				ImGui::Text("Intensity:");
				ImGui::SameLine();
				lightsRequiereUpdate |= ImGui::DragFloat("##intensity", &intensity, 1.0f, 0.0f, std::numeric_limits<float>::max(), "%f W");
				static_cast<RefLight_s*>(selection)->light->setIntensity(intensity);

				Light::Type type = static_cast<RefLight_s*>(selection)->light->getType();
				if (type == Light::Type::SPOT || type == Light::Type::POINT) {
					float range;
					if (type == Light::Type::SPOT) range = dynamic_cast<SpotLight*>(static_cast<RefLight_s*>(selection)->light)->getRange();
					else if (type == Light::Type::POINT) range = dynamic_cast<PointLight*>(static_cast<RefLight_s*>(selection)->light)->getRange();
					ImGui::Text("Range:    ");
					ImGui::SameLine();
					lightsRequiereUpdate |= ImGui::DragFloat("##range", &range, 0.1f, 0.0f, std::numeric_limits<float>::max(), "%f m");
					if (type == Light::Type::SPOT) dynamic_cast<SpotLight*>(static_cast<RefLight_s*>(selection)->light)->setRange(range);
					else if (type == Light::Type::POINT) dynamic_cast<PointLight*>(static_cast<RefLight_s*>(selection)->light)->setRange(range);
				}

				if (type == Light::Type::SPOT || type == Light::Type::POINT || type == Light::Type::SURFACE) {
					if (type == Light::Type::SPOT || type == Light::Type::POINT) {
						float radius;
						if (type == Light::Type::SPOT) radius = dynamic_cast<SpotLight*>(static_cast<RefLight_s*>(selection)->light)->getRadius();
						else if (type == Light::Type::POINT) radius = dynamic_cast<PointLight*>(static_cast<RefLight_s*>(selection)->light)->getRadius();
						ImGui::Text("Radius:   ");
						ImGui::SameLine();
						lightsRequiereUpdate |= ImGui::DragFloat("##radius", &radius, 0.01f, 0.0f, std::numeric_limits<float>::max(), "%f m");
						if (type == Light::Type::SPOT) dynamic_cast<SpotLight*>(static_cast<RefLight_s*>(selection)->light)->setRadius(radius);
						else if (type == Light::Type::POINT) dynamic_cast<PointLight*>(static_cast<RefLight_s*>(selection)->light)->setRadius(radius);
					}

					if (type == Light::Type::SPOT) {
						float inner = dynamic_cast<SpotLight*>(static_cast<RefLight_s*>(selection)->light)->getInnerConeAngle();
						float outer = dynamic_cast<SpotLight*>(static_cast<RefLight_s*>(selection)->light)->getOuterConeAngle();
						ImGui::Text("Inner:    ");
						ImGui::SameLine();
						lightsRequiereUpdate |= ImGui::SliderFloat("##inner", &inner, 0.0f, outer - 0.00001f, "%f rad");
						ImGui::Text("Outer:    ");
						ImGui::SameLine();
						lightsRequiereUpdate |= ImGui::SliderFloat("##outer", &outer, inner + 0.00001f, 1.57079632679/*PI/2*/, "%f rad");
						static_cast<SpotLight*>(static_cast<RefLight_s*>(selection)->light)->setCone(inner, outer);
					}
					else if (type == Light::Type::SURFACE) {
						auto sl = dynamic_cast<SurfaceLight*>(static_cast<RefLight_s*>(selection)->light);
						glm::vec3 size = sl->getDimensions();
						SurfaceLight::Shape shape = sl->getShape();
						if (!sl->is3D()) {
							if (shape == SurfaceLight::Shape::RECTANGLE || shape == SurfaceLight::Shape::ELLIPSE) {
								if (shape == SurfaceLight::Shape::RECTANGLE) ImGui::Text("Width:    ");
								else ImGui::Text("Dia Horiz:");
								ImGui::SameLine();
								lightsRequiereUpdate |= ImGui::DragFloat("##width", &size.x, 0.1f, 0.0001f, std::numeric_limits<float>::max(), "%f m");
								if (shape == SurfaceLight::Shape::RECTANGLE) ImGui::Text("Height:   ");
								else ImGui::Text("Dia Vert: ");
								ImGui::SameLine();
								lightsRequiereUpdate |= ImGui::DragFloat("##height", &size.y, 0.1f, 0.0001f, std::numeric_limits<float>::max(), "%f m");
							}
							else {
								if (shape == SurfaceLight::Shape::SQUARE) ImGui::Text("Size:     ");
								else if (shape == SurfaceLight::Shape::DISK) ImGui::Text("Diameter: ");
								ImGui::SameLine();
								lightsRequiereUpdate |= ImGui::DragFloat("##size", &size.x, 0.1f, 0.0001f, std::numeric_limits<float>::max(), "%f m");
								size.y = size.x;
							}
							size.z = (size.y + size.x) * 0.5f;
							if (size.x == 0.0f) size.x = 0.0001f;
							if (size.y == 0.0f) size.y = 0.0001f;
							if (size.z == 0.0f) size.z = 0.0001f;
							refLight->model_matrix[0] = glm::normalize(refLight->model_matrix[0]) * size.x;
							refLight->model_matrix[1] = glm::normalize(refLight->model_matrix[1]) * size.y;
							refLight->model_matrix[2] = glm::normalize(refLight->model_matrix[2]) * size.z;
							sl->setDimensions(size);
						}
					}
				}
				if (type == Light::Type::DIRECTIONAL) {
					float angle = dynamic_cast<DirectionalLight*>(static_cast<RefLight_s*>(selection)->light)->getAngle();
					ImGui::Text("Angle:    ");
					ImGui::SameLine();
					lightsRequiereUpdate |= ImGui::DragFloat("##angle", &angle, 0.1f, 0.0f, std::numeric_limits<float>::max(), "%f Deg");
					dynamic_cast<DirectionalLight*>(static_cast<RefLight_s*>(selection)->light)->setAngle(angle);
				}

			}
			else if (selection->type == Ref_s::Type::Model) {
				modelInstancesRequiereUpdate |= model_mat_requieres_update;
				float emission_strength = 1;
				float transmission;
				float metallic;
				float roughness;
				float ior;
				bool doubleSided;
				static uint32_t mesh_selection = 0;
				
				static RefModel_s* refModel = nullptr;
				if (refModel != static_cast<RefModel_s*>(selection)) mesh_selection = 0;
				refModel = static_cast<RefModel_s*>(selection);

				auto it = refModel->refMeshes.begin();
				ImGui::Text("Mesh:     "); ImGui::SameLine();
				if (ImGui::BeginCombo("##meshcombo", std::to_string(mesh_selection).c_str(), ImGuiComboFlags_NoArrowButton)) {
					for (uint32_t i = 0; i < refModel->refMeshes.size(); i++) {
						const bool isSelected = (i == mesh_selection);
						if (ImGui::Selectable(std::to_string(i).c_str(), isSelected)) mesh_selection = i;
						if (isSelected) ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
				
				std::advance(it, mesh_selection);
				Mesh* mesh = (*it)->mesh;

				emission_strength = mesh->getMaterial()->getEmissionStrength();
				metallic = mesh->getMaterial()->getMetallicFactor();
				roughness = mesh->getMaterial()->getRoughnessFactor();
				transmission = mesh->getMaterial()->getTransmissionFactor();
				ior = mesh->getMaterial()->getIOR();
				doubleSided = !mesh->getMaterial()->getCullBackface();

				ImGui::PushItemWidth(64);
				ImGui::Text("Metallic: ");
				ImGui::SameLine();
				materialsRequiereUpdate |= ImGui::DragFloat("##metallic", &metallic, 0.005f, 0.0f, 1, "%.3f");
				ImGui::Text("Roughness:");
				ImGui::SameLine();
				materialsRequiereUpdate |= ImGui::DragFloat("##roughness", &roughness, 0.005f, 0.0f, 1, "%.3f");
				ImGui::Text("Emission Strength:");
				ImGui::SameLine();
				bool emission_changed = ImGui::DragFloat("##emission_strength", &emission_strength, 0.1f, 0.0f, std::numeric_limits<float>::max(), "%.3f");
				materialsRequiereUpdate |= emission_changed;
				lightsRequiereUpdate |= emission_changed;
				ImGui::Text("Transmission: ");
				ImGui::SameLine();
				materialsRequiereUpdate |= ImGui::DragFloat("##transmission", &transmission, 0.005f, 0.0f, 1, "%.3f");
				ImGui::Text("Ior: ");
				ImGui::SameLine();
				materialsRequiereUpdate |= ImGui::DragFloat("##ior", &ior, 0.005f, 0.0f, 10.0f, "%.3f");
				ImGui::Text("Double Sided: ");
				ImGui::SameLine();
				materialsRequiereUpdate |= ImGui::Checkbox("##doublesided", &doubleSided);
				ImGui::PopItemWidth();

				mesh->getMaterial()->setEmissionStrength(emission_strength);
				mesh->getMaterial()->setMetallicFactor(metallic);
				mesh->getMaterial()->setRoughnessFactor(roughness);
				mesh->getMaterial()->setTransmissionFactor(transmission);
				mesh->getMaterial()->setIOR(ior);
				mesh->getMaterial()->setCullBackface(!doubleSided);
			}
			ImGui::End();
		}
		if(lightsRequiereUpdate) mUc->scene->requestLightUpdate();
		if(materialsRequiereUpdate) mUc->scene->requestMaterialUpdate();
		if(modelInstancesRequiereUpdate) mUc->scene->requestModelInstanceUpdate();
	}
}

void MainGUI::drawDraw() const
{
	if (mUc->draw_info->mDrawMode) {
		bool materialsRequiereUpdate = false;
		bool modelInstancesRequiereUpdate = false;

		constexpr float distance = 10.0f;
		const ImVec2 pos = ImVec2(mIo->DisplaySize.x - distance, mIo->DisplaySize.y - distance);
		const ImVec2 posPivot = ImVec2(1.0f, 1.0f);
		ImGui::SetNextWindowPos(pos, ImGuiCond_Always, posPivot);

		constexpr auto flags =
			ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoSavedSettings;

		if (ImGui::Begin("Draw", nullptr, flags))
		{
			ImGui::Text("Draw Mode");
			ImGui::Text("Target:"); ImGui::SameLine();
			ImGui::PushItemWidth(228);
			const std::vector modes = { "Vertex Color", "Custom" };
			if (ImGui::BeginCombo("##drawcombocolor", modes[static_cast<uint32_t>(mUc->draw_info->mTarget)], ImGuiComboFlags_NoArrowButton)) {
				for (uint32_t i = 0; i < modes.size(); i++) {
					const bool isSelected = (i == static_cast<uint32_t>(mUc->draw_info->mTarget));
					if (ImGui::Selectable(modes[i], isSelected)) mUc->draw_info->mTarget = static_cast<DrawInfo_s::Target_e>(i);
					if (isSelected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			ImGui::PopItemWidth();

			ImGui::Text("Cursor:");
			ImGui::SameLine();
			ImGui::ColorEdit3("##cursor_color_pick", &mUc->draw_info->mCursorColor[0]);

			ImGui::Text("Color0:");
			ImGui::SameLine();
			ImGui::ColorEdit4("##color_draw0_pick", &mUc->draw_info->mColor0[0]);
			ImGui::Text("Color1:");
			ImGui::SameLine();
			ImGui::ColorEdit4("##color_draw1_pick", &mUc->draw_info->mColor1[0]);

			ImGui::Text("Radius ");
			ImGui::SameLine();
			ImGui::DragFloat("##draw_radius", &mUc->draw_info->mRadius, 0.01f, 0.0f, std::numeric_limits<float>::max(), "%f");
			ImGui::Checkbox("Draw RGB", &mUc->draw_info->mDrawRgb);
			ImGui::Checkbox("Draw ALPHA", &mUc->draw_info->mDrawAlpha);
			const bool softBrushOld = mUc->draw_info->mSoftBrush;
			const bool drawAllOld = mUc->draw_info->mDrawAll;
			ImGui::Checkbox("Soft Brush", &mUc->draw_info->mSoftBrush);
			ImGui::Checkbox("Set Whole Mesh", &mUc->draw_info->mDrawAll);
			if (mUc->draw_info->mDrawAll != drawAllOld && mUc->draw_info->mDrawAll) mUc->draw_info->mSoftBrush = false;
			else if (mUc->draw_info->mSoftBrush != softBrushOld && mUc->draw_info->mSoftBrush) mUc->draw_info->mDrawAll = false;
			ImGui::End();
		}
	}
}

void MainGUI::drawRightClickMenu()
{
	if (!mUc->scene->getCurrentCamera() || mUc->scene->getCurrentCamera()->mode != RefCamera_s::Mode::EDITOR) return;
	if (!mUc->draw_info->mDrawMode && InputSystem::getInstance().wasReleased(Input::MOUSE_RIGHT)) ImGui::OpenPopup(("Popup"), ImGuiPopupFlags_MouseButtonRight);
	if (ImGui::BeginPopup("Popup"))
	{
		if (ImGui::BeginMenu("Add Light"))
		{
			Light *l = nullptr;
			if (ImGui::MenuItem("Point")) l = new PointLight();
			else if (ImGui::MenuItem("Spot")) {
				l = new SpotLight();
			}
			else if (ImGui::MenuItem("Directional")) {
				l = new DirectionalLight();
			}
			else if (ImGui::BeginMenu("Surface"))
			{
				if (ImGui::MenuItem("Square")) {
					l = new SurfaceLight();
					static_cast<SurfaceLight*>(l)->setShape(SurfaceLight::Shape::SQUARE);
				}
				else if (ImGui::MenuItem("Rectangle")) {
					l = new SurfaceLight();
					static_cast<SurfaceLight*>(l)->setShape(SurfaceLight::Shape::RECTANGLE);
				}
				else if (ImGui::MenuItem("Disk")) {
					l = new SurfaceLight();
					static_cast<SurfaceLight*>(l)->setShape(SurfaceLight::Shape::DISK);
				}
				else if (ImGui::MenuItem("Ellipse")) {
					l = new SurfaceLight();
					static_cast<SurfaceLight*>(l)->setShape(SurfaceLight::Shape::ELLIPSE);
				}
				ImGui::EndMenu();
			}
			//else if (ImGui::MenuItem("Surface")) {}
			if (l) {
				const auto cam = static_cast<RefCameraPrivate_s*>(mUc->scene->getCurrentCamera());
				glm::quat quat = glm::quat(glm::radians(glm::vec3(-90, 0, 0)));
				mUc->scene->addLightRef(l, cam->editor_data.pivot, glm::make_vec4(&quat[0]));
			}
			ImGui::EndMenu();
		}

		ImGui::EndPopup();
	}
}

void MainGUI::drawInfo() const
{
	constexpr float distance = 10.0f;
	const ImVec2 pos = ImVec2(mIo->DisplaySize.x - distance, mVerticalOffsetMenuBar + distance);
	const ImVec2 posPivot = ImVec2(1.0f, 0.0f);
	ImGui::SetNextWindowPos(pos, ImGuiCond_Always, posPivot);
	ImGui::SetNextWindowBgAlpha(0.3f); // Transparent background

	constexpr auto flags =
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoSavedSettings;

	if (ImGui::Begin("Statistics", nullptr, flags))
	{
		const glm::vec3 cam_pos = mUc->scene->getCurrentCameraPosition();
		const glm::vec3 cam_view_dir = mUc->scene->getCurrentCameraDirection();
		const RenderConfig_s rc = ((RenderSystem*)mUc->system)->getConfig();
		const Window* w = Common::getInstance().getMainWindow()->window();
		const glm::ivec2 ws = w->getSize();
		ImGui::Text("Frame rate: %.1f fps", static_cast<double>(rc.framerate_smooth));
		ImGui::Text("Frame time: %5.2f ms", static_cast<double>(rc.frametime_smooth));
		//ImGui::Separator();
		ImGui::Text("Window:  %d x %d", ws.x, ws.y);
		ImGui::Separator();
		ImGui::Text("View position:");
		ImGui::Text("%.1f %.1f %.1f", static_cast<double>(cam_pos[0]), static_cast<double>(cam_pos[1]), static_cast<double>(cam_pos[2]));
		ImGui::Text("View direction:");
		ImGui::Text("%.1f %.1f %.1f", static_cast<double>(cam_view_dir[0]), static_cast<double>(cam_view_dir[1]), static_cast<double>(cam_view_dir[2]));

		ImGui::End();
	}
}

void MainGUI::showSaveScene()
{
	ImGui::SetNextWindowPos(ImVec2(mIo->DisplaySize.x * 0.5f, mIo->DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	constexpr auto flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
	if (ImGui::Begin("Export Scene", &mShowSaveScene, flags))
	{
		const std::vector<std::string> formats = { "glTF" };
		static Exporter::SceneExportSettings exportSettings;
		ImGui::Text("Export as");
		ImGui::SameLine();

		static uint32_t formatSelection = 0;
		if (ImGui::BeginCombo("##exportcombo", formats[0].c_str(), ImGuiComboFlags_NoArrowButton)) {
			for (uint32_t i = 0; i < formats.size(); i++) {
				const bool isSelected = (i == formatSelection);
				if (ImGui::Selectable(formats[i].c_str(), isSelected)) formatSelection = i;
				if (isSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		if (formats[formatSelection] == "glTF") exportSettings.mFormat = Exporter::SceneExportSettings::Format::glTF;
		if (formatSelection == 0) {
			ImGui::Checkbox("Embed Images", &exportSettings.mEmbedImages);
			ImGui::Checkbox("Embed Buffer", &exportSettings.mEmbedBuffers);
			ImGui::Checkbox("As Binary", &exportSettings.mWriteBinary);
			ImGui::Checkbox("Exclude Lights", &exportSettings.mExcludeLights);
			ImGui::Checkbox("Exclude Models", &exportSettings.mExcludeModels);
			ImGui::Checkbox("Exclude Cameras", &exportSettings.mExcludeCameras);
		}
		if(ImGui::Button("Save"))
		{
			mShowSaveScene = false;
			Common::openFileDialogExportScene(exportSettings.encode());
		}
		if (ImGui::IsKeyPressed(ImGuiKey_Escape)) mShowSaveScene = false;
		ImGui::End();
	}
}
void MainGUI::showHelp()
{
	constexpr float distance = 10.0f;
	//const ImVec2 pos = ImVec2(distance + 100, vertical_offset_menu_bar + distance+ 100);
	const ImVec2 pos = ImVec2(distance, mVerticalOffsetMenuBar + distance);
	const ImVec2 posPivot = ImVec2(0.0f, 0.0f);
	ImGui::SetNextWindowPos(pos, ImGuiCond_Always, posPivot);

	const ImVec2 maxSize = ImVec2(mIo->DisplaySize.x - distance - distance, mIo->DisplaySize.y - distance - (mVerticalOffsetMenuBar + distance));
	ImGui::SetNextWindowSize(ImVec2(std::max(200.0f, maxSize.x), std::max(200.0f, maxSize.y)));
	//ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.5 - distance - distance, io.DisplaySize.y * 0.5 - distance - (vertical_offset_menu_bar + distance)));

	constexpr auto flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
	if (ImGui::Begin("Usage", &mShowHelp, flags))
	{
		// column 1
		ImGui::Columns(3, "locations", false);
		ImGui::SetColumnWidth(-1, 170);

		ImGui::Text("Editor Camera");
		ImGui::BulletText("Rotate Camera:");
		ImGui::BulletText("Move Camera:");
		ImGui::BulletText("Zoom:");
		ImGui::BulletText("Select Object:");
		ImGui::BulletText("Unselect Object:");
		ImGui::NewLine();
		ImGui::BulletText("Local Space:");
		ImGui::BulletText("World Space:");
		ImGui::BulletText("Translate:");
		ImGui::BulletText("Rotate:");
		ImGui::BulletText("Scale:");
		ImGui::NewLine();
		ImGui::BulletText("Delete:");
		ImGui::BulletText("Undo:");
		ImGui::NewLine();
		ImGui::BulletText("Draw Mode:");
		ImGui::NewLine();
		ImGui::Text("- Draw Mode Active -");
		ImGui::BulletText("Draw Color 0:");
		ImGui::BulletText("Draw Color 1:");
		ImGui::BulletText("Change Radius:");
		ImGui::BulletText("Exit Draw Mode:");
		ImGui::NewLine();
		// fps
		ImGui::Text("FPS Camera");
		ImGui::BulletText("Enter Camera:");
		ImGui::BulletText("Move:");
		ImGui::BulletText("Change Speed:");
		ImGui::BulletText("Exit Camera:");
		ImGui::NewLine();
		// UI
		ImGui::Text("UI");
		ImGui::BulletText("Slider Input:");
		ImGui::NewLine();
		// Other
		ImGui::Text("Other");
		ImGui::BulletText("Screenshot:");
		ImGui::BulletText("Impl-Screenshot:");
		ImGui::NewLine();

		// column 2
		ImGui::NextColumn();
		ImGui::SetColumnWidth(-1, 250);
		// editor
		ImGui::NewLine();
		ImGui::Text("Middle Mouse Button");
		ImGui::Text("Shift + Middle Mouse Button");
		ImGui::Text("Mouse Wheel");
		ImGui::Text("Left Click");
		ImGui::Text("ESC");
		ImGui::NewLine();
		ImGui::Text("L");
		ImGui::Text("W");
		ImGui::Text("T");
		ImGui::Text("R");
		ImGui::Text("S");
		ImGui::NewLine();
		ImGui::Text("X");
		ImGui::Text("CTRL + Z");
		ImGui::NewLine();
		ImGui::Text("D");
		ImGui::NewLine();
		ImGui::NewLine();
		ImGui::Text("Left Click");
		ImGui::Text("Right Click");
		ImGui::Text("CTRL + Scroll Wheel");
		ImGui::Text("ESC");
		ImGui::NewLine();
		
		// fps
		ImGui::NewLine();
		ImGui::Text("Left Click");
		ImGui::Text("WASDQE");
		ImGui::Text("Scroll Wheel");
		ImGui::Text("ESC");
		ImGui::NewLine();
		// UI
		ImGui::NewLine();
		ImGui::Text("CTRL + Left Click");
		ImGui::NewLine();
		// OTHER
		ImGui::NewLine();
		ImGui::Text("F5");
		ImGui::Text("F6");

		// column 3
		ImGui::NextColumn();
		ImGui::Text("This is Tamashii a rendering framework");
		ImGui::Columns();

		//ImGui::Indent((ImGui::GetWindowSize().x * 0.5f) - (20 + ImGui::GetCursorPosX()) );
		//ImGui::Button("Ok", ImVec2(40, 40));

		ImGui::End();
	}
}

void MainGUI::showAbout()
{
    //const float distance = 10.0f;
	const ImVec2 size = ImVec2(150, 150);
	const ImVec2 pos = ImVec2(mIo->DisplaySize.x * 0.5f - size.x * 0.5f, mIo->DisplaySize.y * 0.5f - size.y * 0.5f);
	ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
	ImGui::SetNextWindowSize(size);

	const auto flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
	if (ImGui::Begin("About", &mShowAbout, flags))
	{
		ImGui::Text("Tamashii v0.01");
		ImGui::End();
	}
}
