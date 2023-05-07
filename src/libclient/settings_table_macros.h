#ifdef SETTING_FULL
#	undef SETTING_BIND
#	undef SETTING_FULL
#	undef SETTING_GETSET
#	undef SETTING
#elif defined(DP_SETTINGS_HEADER) || defined(DP_SETTINGS_BODY) || defined(DP_SETTINGS_REBIND)

#	define SETTING_BIND(name, upperName) \
	template <typename... Args> \
	auto bind##upperName(Args &&... args) { \
		return bind(name(), &Settings::name##Changed, &Settings::set##upperName, std::forward<Args>(args)...); \
	} \
	template <typename T, typename... Args> \
	auto bind##upperName##As(Args &&... args) { \
		return bindAs<T>(name(), &Settings::name##Changed, &Settings::set##upperName, std::forward<Args>(args)...); \
	}

#	ifdef DP_SETTINGS_HEADER
	#	define SETTING_FULL(version, name, upperName, baseKey, defaultValue, getter, setter, notifier) \
		protected: \
			static const ::libclient::settings::SettingMeta meta##upperName; \
		public: \
			using upperName##Type = std::decay_t<decltype(defaultValue)>; \
			auto name() const { \
				const auto value = get(meta##upperName); \
				if (value.canConvert<upperName##Type>()) { \
					return value.value<upperName##Type>(); \
				} else { \
					qWarning() << #name << "cannot convert from" << value.typeName() << "- defaulting"; \
					return defaultValue; \
				} \
			} \
			void set##upperName(upperName##Type value) { set(meta##upperName, QVariant::fromValue(value)); } \
			SETTING_BIND(name, upperName) \
			Q_SIGNAL void name##Changed(upperName##Type value);
#	elif defined(DP_SETTINGS_BODY)
#		define SETTING_FULL(version, name, upperName, baseKey, defaultValue, getter, setter, notifier) \
		const ::libclient::settings::SettingMeta Settings::meta##upperName = { \
			::libclient::settings::SettingMeta::Version::version, \
			baseKey, \
			QVariant::fromValue(defaultValue), \
			getter, \
			setter, \
			notifier \
		};
#	elif defined(DP_SETTINGS_REBIND)
#		define SETTING_FULL(version, name, upperName, baseKey, defaultValue, getter, setter, notifier) \
			SETTING_BIND(name, upperName)
#	endif

#	define SETTING_GETSET_V(version, name, upperName, baseKey, defaultValue, getter, setter) \
		SETTING_FULL(version, name, upperName, baseKey, defaultValue, \
			getter, \
			setter, \
			([](const ::libclient::settings::SettingMeta &, ::libclient::settings::Settings &base) { \
				auto &settings = ::libclient::settings::maybe_static_cast<Settings &>(base); \
				settings.name##Changed(settings.name()); \
			}))

#	define SETTING_GETSET(name, upperName, baseKey, defaultValue, getter, setter) \
		SETTING_GETSET_V(V0, name, upperName, baseKey, defaultValue, getter, setter)

#	define SETTING(name, upperName, baseKey, defaultValue) \
		SETTING_GETSET(name, upperName, baseKey, defaultValue, \
			&::libclient::settings::any::get, \
			&::libclient::settings::any::set \
		)
#else
#	error Missing DP_SETTINGS_HEADER or DP_SETTINGS_BODY or DP_SETTINGS_REBIND
#endif
