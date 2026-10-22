#pragma once
#include <tamashii/public.hpp>

#include <string>
#include <array>
#include <map>

T_BEGIN_NAMESPACE
enum class EventType
{
	NONE,
	KEY,						// value != 0 when key down, x and y are absolute window coordinates
	MOUSE_DELTA,				// value, x and y are signed delta of mouse
	MOUSE_WHEEL_DELTA,			// value, x and y are signed delta of mouse wheel
	MOUSE_ABSOLUTE,				// value unused, x and y are absolute window coordinates
	MOUSE_LEAVE,				// mouse left area value , x and y are undefined
	ACTION						// menu action
};
// do not change order!
enum class Input {
	NONE,
	KEY_ESCAPE, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0, KEY_MINUS, KEY_EQUALS, KEY_BACKSPACE, KEY_TAB, KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_U, KEY_I, KEY_O, KEY_P, KEY_LBRACKET, KEY_RBRACKET, KEY_ENTER, KEY_LCTRL, KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K, KEY_L, KEY_SEMICOLON, KEY_APOSTROPHE, KEY_GRAVE, KEY_LSHIFT, KEY_BACKSLASH, KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_N, KEY_M, KEY_COMMA, KEY_PERIOD, KEY_SLASH, KEY_RSHIFT, KEY_KP_STAR, KEY_LALT, KEY_SPACE, KEY_CAPSLOCK, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_NUMLOCK, KEY_SCROLL, KEY_F11 = 0x57, KEY_F12 = 0x58, // directly maps from scan code 
	// MOUSE
	MOUSE_LEFT,
	MOUSE_RIGHT,
	MOUSE_WHEEL,
	MOUSE_WHEEL_UP,
	MOUSE_WHEEL_DOWN,
	MOUSE_WHEEL_LEFT,
	MOUSE_WHEEL_RIGHT,
	// AFTER LAST KEY ONLY WINDOW MENU INPUTS
	LAST_KEY,
	// ACTIONS
	A_NEW_SCENE, A_OPEN_SCENE, A_ADD_SCENE, A_ADD_MODEL, A_ADD_LIGHT, A_EXPORT_SCENE, A_WINDOW_RESIZE, A_RELOAD_BACKEND_IMPL, A_CHANGE_BACKEND_IMPL, A_CLEAR_CACHE, A_TAKE_SCREENSHOT, A_EXIT
};

struct Event
{
	Event() : mType(EventType::NONE), mInput(Input::NONE), mValue(-1), mX(-1), mY(-1) {}
	EventType		mType;
	Input			mInput;
	int				mValue;
	float			mX;
	float			mY;
	std::string		mMessage;

	bool			isKeyEvent() const
	{
		return mType == EventType::KEY;
	}
	bool			isMouseRelativeEvent() const
	{
		return mType == EventType::MOUSE_DELTA;
	}
	bool			isMouseWheelRelativeEvent() const
	{
		return mType == EventType::MOUSE_WHEEL_DELTA;
	}
	bool			isMouseAbsoluteEvent() const
	{
		return mType == EventType::MOUSE_ABSOLUTE;
	}
	bool			isWindowEvent() const
	{
		return mType == EventType::ACTION;
	}
	bool			isKeyDown() const
	{
		return	mValue != 0;
	}
	Input			getInput() const
	{
		return mInput;
	}
	float			getXCoord() const
	{
		return mX;
	}
	float			getYCoord() const
	{
		return mY;
	}
	int				getValue() const
	{
		return mValue;
	}
	std::string		getMessage() const
	{
		return mMessage;
	}
};


class InputSystem {
public:
					static InputSystem& getInstance()
					{
						static InputSystem instance;
						return instance;
					}
					InputSystem(InputSystem const&) = delete; 

					// only true for the frame that the key was pressed
	bool			wasPressed(Input aInput) const;
					// true as long as the key is down
	bool			isDown(Input aInput) const;
					// only true for the frame that the key was released
	bool			wasReleased(Input aInput) const;
					// returns absolute window position, (-1,-1) if mouse is outside the window
	glm::vec2		getMousePosAbsolute() const;
					// returns mouse delta in x and y direction
	glm::vec2		getMousePosRelative() const;
					// returns scroll wheel delta in x and y direction
	glm::vec2		getMouseWheelRelative() const;

private:
					InputSystem() = default;
					~InputSystem() = default;
	friend class	EventSystem;
	struct Key
	{
		bool		mDown = false;
		bool		mReleased = false;
		bool		mPressed = false;
		void		reset() { mDown = mReleased = mPressed = false; }
	};
	Key mKeys[static_cast<int>(Input::LAST_KEY)];

	glm::vec2		mMousePosAbsolute = { 0, 0 };
	glm::vec2		mMousePosRelative = { 0, 0 };
	glm::vec2		mMouseWheelRelative = { 0, 0};
};



class EventSystem {
public:
					static EventSystem& getInstance()
					{
						static EventSystem instance;
						return instance;
					}
					EventSystem(EventSystem const&) = delete;

					// add one event to the queue
	static void		queueEvent(EventType aType, Input aInput, int aValue = -1, float aX = -1, float aY = -1, const std::string&
		                           aString = "");
					// read all events and process them
	void			eventLoop();

	void			clearEventQueue();
	void			reset();

	void			setCallback(EventType aEventType, Input aInput, const std::function<bool(const Event&)>& aCallback);
private:
					EventSystem() = default;
					~EventSystem() = default;
				
	Event			getEvent();
	void			processEvent(const Event& aEvent);

	// queue
#define	MAX_QUED_EVENTS	256
#define	MASK_QUED_EVENTS ( MAX_QUED_EVENTS - 1 )
	std::array<Event, MAX_QUED_EVENTS> mEventQue;
	int								mEventHead = 0;
	int								mEventTail = 0;
	std::mutex						mMutex;

	std::map<EventType,std::map<Input,std::function<bool(const Event&)>>> mCallbacks;
};

T_END_NAMESPACE