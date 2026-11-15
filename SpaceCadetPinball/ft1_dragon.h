#pragma once

enum class MessageCode;
class TPinballComponent;
class TPinballTable;

// Dragon's Keep (Full Tilt 1): a function-by-function translation of the table's own rules, code segment 23 of
// DRAGON.EXE. Every function carries its original address; ft2/tools/ft1draft.py --all and ft1condense.py list the
// original code for checking. Notes on the original engine (message codes, table fields, fonts): ft2/ft1_engine.md.
//
// Work in progress: the translation is only used when FT1_DRAGON_FULL=1 is set with FT2_DEBUG=1, until every
// function it needs is translated. Until then Dragon's Keep plays with the few rules in ft1_control.cpp and the
// generic rules.
namespace ft1_dragon
{
	bool Wanted();
	bool Link(TPinballTable* table);
	void Clear();
	bool Handle(MessageCode code, TPinballComponent* cmp);
	int TranslatedCount();
}
