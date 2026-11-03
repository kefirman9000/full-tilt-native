#pragma once

enum class MessageCode;
class TPinballComponent;
class TPinballTable;

// Recovered rules of the Full Tilt 1 tables beyond Space Cadet (Dragon's Keep now, Skulduggery next), ported by hand
// from their original programs with the tools in ft2/tools (ft1lift / ft1rosetta / ft1draft). Components whose rule is
// ported here run it; every other component keeps the generic rules (ft2_control), so the table stays playable while
// the port is under way.
namespace ft1_control
{
	// Called when a table is linked; returns true if this table has recovered rules (and links them)
	bool Link(TPinballTable* table);
	void Clear();
	// Runs the component's recovered rule. False if it has none, then the generic rules handle the message.
	bool Handle(MessageCode code, TPinballComponent* cmp);
	// Number of components with recovered rules on the current table (for the log / status)
	int PortedCount();
}
