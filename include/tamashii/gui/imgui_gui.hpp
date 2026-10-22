#pragma once
#include <tamashii/public.hpp>
#include <deque>

struct ImGuiIO;
T_BEGIN_NAMESPACE
struct uiConf_s;
struct Ref_s;
class MainGUI {
public:
												MainGUI();
	void										draw(uiConf_s* aUiConf);

private:
	void										markLights();
	void										markCamera();
	void										showDraw();
	void										drawImGuizmo();
	void										drawGizmo();

	void										drawMenuBar();
	void										drawConsole();
	void										drawMenu();
	void										drawEdit();
	void										drawDraw() const;

	void										drawRightClickMenu();

	void										drawInfo() const;
	void										showSaveScene();
	void										showHelp();
	void										showAbout();

	ImGuiIO										*mIo;
	uiConf_s									*mUc;
	const float									mVerticalOffsetMenuBar;

	// imguizmo
	std::string									mOp;	// "Translate", "Scale", "Rotate"
	std::string									mMode;	// "Local", World
	std::deque<std::pair<Ref_s*, glm::mat4>>	mHistory;

	bool										mShowHelp;
	bool										mShowAbout;
	bool										mShowSaveScene;
};
T_END_NAMESPACE