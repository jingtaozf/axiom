# Neutralise the globally-installed literate-agent plugin inside axiom.
#
# That plugin's hooks assume an org-mode LP layout (lp/*.org tangling
# to .py/.ts/...) and would otherwise false-block every .py edit here.
# Axiom's literate discipline is pamphlet-based and enforced by this
# repo's OWN hooks (block-generated-edit.sh & friends in this folder);
# it does not depend on literate-agent.
export LITERATE_AGENT_TANGLED_OUTPUT_EXTS=".lp-disabled-in-axiom"
export LITERATE_AGENT_TANGLED_ROOTS=""
