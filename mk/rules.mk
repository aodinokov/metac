# all targets have to have TPL- var with type of template
define meta_rules_tpl
$$(foreach tpl,$$(TPL-$2),$$(call $$(tpl),$1,$2))
endef
meta_rules = $(eval $(call meta_rules_tpl,$1,$2))

define meta_rules_clean_tpl
$$(foreach tpl,$$(TPL-$2),$$(call $$(tpl)_clean,$1,$2))
endef
meta_rules_clean = $(eval $(call meta_rules_clean_tpl,$1,$2))