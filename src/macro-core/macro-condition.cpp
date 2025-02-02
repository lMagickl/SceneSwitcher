#include "macro-condition.hpp"

namespace advss {

const std::map<LogicType, LogicTypeInfo> MacroCondition::logicTypes = {
	{LogicType::NONE, {"AdvSceneSwitcher.logic.none"}},
	{LogicType::AND, {"AdvSceneSwitcher.logic.and"}},
	{LogicType::OR, {"AdvSceneSwitcher.logic.or"}},
	{LogicType::AND_NOT, {"AdvSceneSwitcher.logic.andNot"}},
	{LogicType::OR_NOT, {"AdvSceneSwitcher.logic.orNot"}},
	{LogicType::ROOT_NONE, {"AdvSceneSwitcher.logic.rootNone"}},
	{LogicType::ROOT_NOT, {"AdvSceneSwitcher.logic.not"}},
};

void DurationModifier::Save(obs_data_t *obj, const char *condName,
			    const char *duration) const
{
	obs_data_set_int(obj, condName, static_cast<int>(_type));
	_dur.Save(obj, duration);
}

void DurationModifier::Load(obs_data_t *obj, const char *condName,
			    const char *duration)
{
	// For backwards compatability check if duration value exist without
	// time constraint condition - if so assume DurationCondition::MORE
	if (!obs_data_has_user_value(obj, condName) &&
	    obs_data_has_user_value(obj, duration)) {
		obs_data_set_int(obj, condName, static_cast<int>(Type::MORE));
	}

	_type = static_cast<Type>(obs_data_get_int(obj, condName));
	_dur.Load(obj, duration);

	// TODO: remove this fallback
	if (obs_data_has_user_value(obj, "displayUnit")) {
		_dur.SetUnit(static_cast<Duration::Unit>(
			obs_data_get_int(obj, "displayUnit")));
	}
}

bool DurationModifier::DurationReached()
{
	switch (_type) {
	case DurationModifier::Type::NONE:
		return true;
		break;
	case DurationModifier::Type::MORE:
		return _dur.DurationReached();
		break;
	case DurationModifier::Type::EQUAL:
		if (_dur.DurationReached() && !_timeReached) {
			_timeReached = true;
			return true;
		}
		break;
	case DurationModifier::Type::LESS:
		return !_dur.DurationReached();
		break;
	case DurationModifier::Type::WITHIN:
		if (_dur.IsReset()) {
			return false;
		}
		return !_dur.DurationReached();
		break;
	default:
		break;
	}
	return false;
}

void DurationModifier::Reset()
{
	_timeReached = false;
	_dur.Reset();
}

MacroCondition::MacroCondition(Macro *m, bool supportsVariableValue)
	: MacroSegment(m, supportsVariableValue)
{
}

bool MacroCondition::Save(obs_data_t *obj) const
{
	MacroSegment::Save(obj);
	obs_data_set_string(obj, "id", GetId().c_str());
	obs_data_set_int(obj, "logic", static_cast<int>(_logic));

	// To avoid conflicts with conditions which also use the Duration class
	// save the duration modifier in a separate obj
	auto durObj = obs_data_create();
	_duration.Save(durObj);
	obs_data_set_obj(obj, "durationModifier", durObj);
	obs_data_release(durObj);

	return true;
}

bool MacroCondition::Load(obs_data_t *obj)
{
	MacroSegment::Load(obj);
	_logic = static_cast<LogicType>(obs_data_get_int(obj, "logic"));
	if (obs_data_has_user_value(obj, "durationModifier")) {
		auto durObj = obs_data_get_obj(obj, "durationModifier");
		_duration.Load(durObj);
		obs_data_release(durObj);
	} else {
		// For backwards compatability
		_duration.Load(obj);
	}
	return true;
}

void MacroCondition::ResetDuration()
{
	_duration.Reset();
}

void MacroCondition::CheckDurationModifier(bool &val)
{
	if (_duration.GetType() != DurationModifier::Type::WITHIN && !val) {
		_duration.Reset();
	}
	if (_duration.GetType() == DurationModifier::Type::WITHIN && val) {
		_duration.Reset();
	}
	switch (_duration.GetType()) {
	case DurationModifier::Type::NONE:
	case DurationModifier::Type::MORE:
	case DurationModifier::Type::EQUAL:
	case DurationModifier::Type::LESS:
		val = val && _duration.DurationReached();
		return;
	case DurationModifier::Type::WITHIN:
		if (val) {
			_duration.SetTimeRemaining(
				_duration.GetDuration().Seconds());
		}
		val = val || _duration.DurationReached();
		break;
	default:
		break;
	}
}

void MacroCondition::SetDurationModifier(DurationModifier::Type m)
{
	_duration.SetModifier(m);
}

void MacroCondition::SetDuration(const Duration &duration)
{
	_duration.SetValue(duration);
}

MacroRefCondition::MacroRefCondition(Macro *m, bool supportsVariableValue)
	: MacroCondition(m, supportsVariableValue)
{
}

bool MacroRefCondition::PostLoad()
{
	_macro.PostLoad();
	return true;
}

MultiMacroRefCondtition::MultiMacroRefCondtition(Macro *m,
						 bool supportsVariableValue)
	: MacroCondition(m, supportsVariableValue)
{
}

bool MultiMacroRefCondtition::PostLoad()
{
	for (auto &macro : _macros) {
		macro.PostLoad();
	}
	return true;
}

} // namespace advss
