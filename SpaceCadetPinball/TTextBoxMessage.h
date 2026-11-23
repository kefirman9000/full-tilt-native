#pragma once
class TTextBoxMessage
{
public:
	TTextBoxMessage* NextMessage;
	char* Text;
	float Time;
	int EndTicks;
	bool LowPriority;
	struct score_msg_font_type* Font = nullptr; // a font of its own (Full Tilt 1 pictures), else the box's

	TTextBoxMessage(const char* text, float time, bool lowPriority);
	~TTextBoxMessage();
	float TimeLeft() const;
	void Refresh(float time);
};
