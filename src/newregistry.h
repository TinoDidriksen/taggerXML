#ifndef _new_registry_h_
#define _new_registry_h_

#include "sysdep.h"

#include <lmdb++.h>
#include <unordered_map>
#include <unordered_set>
#include <string_view>
#include <array>
#include <vector>

// Specialization for std::array
template<typename T>
struct array_hash {
	auto operator()(const T& val) const {
		std::hash<typename T::value_type> hashf;
		size_t rv = 0;
		for (auto& v : val) {
			rv = rv ^ (hashf(v) << 1);
		}
		return rv;
	}
};

using NewDarray = std::vector<std::array<std::string_view, 6>>;
using NewRegistry = std::unordered_map<std::string_view, std::string_view>;
using SV_Set = std::unordered_set<std::string_view>;
using SV2 = std::array<std::string_view, 2>;
using SV2_Set = std::unordered_set<SV2, array_hash<SV2>>;

template<typename C, typename T>
inline auto find_or_default(C& cont, const T& key) {
	auto it = cont.find(key);
	if (it == cont.end()) {
		return typename C::mapped_type{};
	}
	return it->second;
}

#endif
