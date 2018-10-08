#ifndef _new_registry_h_
#define _new_registry_h_

#include "sysdep.h"

#include <lmdb++.h>
#include <unordered_map>
#include <unordered_set>
#include <string_view>
#include <array>
#include <vector>

#define BOOST_DATE_TIME_NO_LIB 1
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>

namespace bi = ::boost::interprocess;
struct mmap_region {
	bi::file_mapping fmap;
	bi::mapped_region mreg;
	const char* buf;
	size_t size;

	mmap_region() {}

	mmap_region(const std::string& fname)
		: fmap(fname.c_str(), bi::read_only)
		, mreg(fmap, bi::read_only)
		, buf(reinterpret_cast<const char*>(mreg.get_address()))
		, size(mreg.get_size()) {}
};

template<typename C>
inline auto is_space(const C& c) {
	return ((c == ' ') || (c == '\t') || (c == '\r') || (c == '\n'));
}

template<typename Str>
inline void trim(Str& str) {
	while (!str.empty() && is_space(str.back())) {
		str.pop_back();
	}
	while (!str.empty() && is_space(str.front())) {
		str.erase(str.begin());
	}
}

inline auto nextline(mmap_region& reg, std::string_view line = {}) {
	const char* start = reg.buf;
	const char* end = reg.buf + reg.size;

	// If we were already reading lines, start from the previous line's end
	if (!line.empty()) {
		start = &line.back() + 1;
	}

	// Skip leading space, which also skips the newline that the old back() might be parked behind
	// Also skips empty lines, which is desired behavior
	while (start < end && is_space(*start)) {
		++start;
	}
	if (start >= end) {
		return std::string_view{};
	}

	// Find the next newline...
	const char* stop = start;
	while (stop < end && *stop != '\n') {
		++stop;
	}
	// ...but trim trailing whitespace
	while (stop > start && is_space(stop[-1])) {
		--stop;
	}

	return std::string_view{ start, static_cast<size_t>(stop - start) };
};

template<size_t N>
inline auto split(std::string_view str) {
	std::array<std::string_view, N> strs;

	while (!str.empty() && is_space(str.front())) {
		str.remove_prefix(1);
	}

	auto start = &str.front();
	auto end = &str.back() + 1;

	// Split the first N-1 nicely...
	for (size_t n = 0; n < N - 1 && start < end; ++n) {
		auto stop = start;
		while (stop < end && !is_space(*stop)) {
			++stop;
		}

		strs[n] = std::string_view{ start, static_cast<size_t>(stop - start) };

		start = stop;
		while (start < end && is_space(*start)) {
			++start;
		}
	}
	// ...but the last part should just contain whatever is left in the input
	strs[N - 1] = std::string_view{ start, static_cast<size_t>(end - start) };

	return strs;
}

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
using SV_Set = std::unordered_set<std::string_view>;

struct lmdb_reader {
	lmdb::txn txn;
	lmdb::dbi dbi;

	lmdb_reader(lmdb::env& env)
		: txn(lmdb::txn::begin(env, nullptr, MDB_RDONLY))
		, dbi(lmdb::dbi::open(txn))
	{}
};

struct lmdb_writer {
	lmdb::txn txn;
	lmdb::dbi dbi;

	lmdb_writer(lmdb::env& env)
		: txn(lmdb::txn::begin(env))
		, dbi(lmdb::dbi::open(txn))
	{}
};

inline auto& join(std::string& trg, std::string_view a, char g, std::string_view b) {
	trg.clear();
	trg += a;
	trg += g;
	trg += b;
	return trg;
}

inline bool exists(lmdb_reader& rd, std::string_view key) {
	if (key.empty()) {
		return false;
	}
	lmdb::val k{key.data(), key.size()};
	lmdb::val v{};
	auto rv = rd.dbi.get(rd.txn, k, v);
	return rv;
}

inline auto size(lmdb::env& env) {
	MDB_stat mdbs;
	lmdb::env_stat(env, &mdbs);
	return mdbs.ms_entries;
}

inline std::string_view find_or_default(lmdb_reader& rd, std::string_view key) {
	if (key.empty()) {
		return {};
	}
	lmdb::val k{ key.data(), key.size() };
	lmdb::val v{};
	auto rv = rd.dbi.get(rd.txn, k, v);
	if (!rv) {
		return {};
	}
	return {v.data(), v.size()};
}

inline std::string_view find_or_default(lmdb_reader& rd, const char* key) {
	return find_or_default(rd, std::string_view{ key });
}

//*
template<typename C, typename T>
inline auto find_or_default(C& cont, const T& key) {
	auto it = cont.find(key);
	if (it == cont.end()) {
		return typename C::mapped_type{};
	}
	return it->second;
}
//*/

inline auto strcmp(std::string_view a, const char* b) {
	return a.compare(b);
}

inline auto strcmp(std::string_view a, std::string_view b) {
	return a.compare(b);
}

#endif
