/*
 * This file is part of EasyRPG Player.
 *
 * EasyRPG Player is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Player is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _UTIL_MACRO_H_
#define _UTIL_MACRO_H_

// Macros / Templates
#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

template <typename T>
T max(T x, T y) {
    return (x >= y) ? x : y;
}

template <typename T>
T min(T x, T y) {
    return (x < y) ? x : y;
}

// Used for creating event handler boilerplate in cpp files
// In h files of SomeClass create
// using on_some_func = std::function<void(int, char*)>;
// static void AddOnSomethingListener(on_some_func func);
// static bool RemoveOnSomethingListener(on_some_func func);
// static void OnSomething(int, char*); <- must match std::function signature

// in cpp file create
// DECLARE_EVENT_HANDLER(SomeClass, on_some_func, Something)
// void SomeClass::OnSomething(int i, char* c) {
//     INVOKE_EVENT_HANDLER(SomeClass, Something, i, c)
// }

#define DECLARE_EVENT_HANDLER(impl_class, func_type, listener_name) \
static std::vector<impl_class::func_type> listener_name##listeners; \
void impl_class::AddOn##listener_name##Listener(func_type func) {\
	listener_name##listeners.push_back(func);\
}\
bool impl_class::RemoveOn##listener_name##Listener(func_type func) {\
	auto result = std::find_if(listener_name##listeners.begin(), listener_name##listeners.end(), [&](func_type f) { return f.target<impl_class::func_type>() == func.target<impl_class::func_type>(); });\
	if (result != listener_name##listeners.end()) {\
		listener_name##listeners.erase(result);\
		return true;\
	}\
	return false;\
}

#define INVOKE_EVENT_HANDLER(impl_class, listener_name, ...) {\
	for (auto& i : listener_name##listeners) {\
		i(__VA_ARGS__);\
	}\
}

#endif
