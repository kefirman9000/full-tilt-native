#pragma once

class TPinballTable;
class TPinballComponent;
enum class MessageCode;

// Skulduggery's rules, translated one to one from PIRATES.EXE code segment 23 (the Full Tilt 1 extra table). Same
// framework as Dragon's Keep (ft1_dragon.h): a rule table of 74 entries linked by 23:05ad, the rules' message
// handler 23:066f (the mission system 23:d7e3 hears every message first), the table's own messages 23:06e7.
// Notes on the original engine: ft2/ft1_engine.md.
//
// Work in progress: used only with FT2_DEBUG=1 FT1_PIRATES_FULL=1 until every function it needs is translated.
namespace ft1_pirates
{
	bool Wanted();
	bool Link(TPinballTable* table);
	void Clear();
	bool Handle(MessageCode code, TPinballComponent* cmp);
	bool TableHandler(MessageCode code); // the table's own messages (23:06e7), true when the translation is active
}
