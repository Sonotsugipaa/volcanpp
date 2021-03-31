#include <iostream>
#include "util.hpp"

#include <chrono>
#include <thread>
#include <string_view>
#include <type_traits>



namespace {

	#ifndef NDEBUG
		std::string count_allocs(const std::string& nm, unsigned n, char c) {
			using namespace std::string_literals;
			constexpr unsigned MAX_CHARS = 6;
			if(n <= MAX_CHARS) {
				return std::string(MAX_CHARS-n, ' ') + std::string(n, c) + " "s + nm; }
			auto r = std::to_string(n);
			if(r.size() > (MAX_CHARS-1)) {
				r = std::string(MAX_CHARS-1, '?');
			} else {
				r = std::string((MAX_CHARS-1)-r.size(), ' ') + r;
			}
			return r + "+ "s + nm;
		}
	#endif


	uint_fast64_t get_time() {
		using namespace std::chrono;
		using namespace std::chrono_literals;
		using nanos = std::chrono::duration<uint_fast64_t, std::nano>;
		return std::chrono::time_point_cast<nanos, std::chrono::steady_clock>(
			std::chrono::steady_clock::now()).time_since_epoch().count();
	}

}



// Small utility functions
namespace util {

	constexpr size_t FILE_BUFFER_SIZE = 4096;

	#define TYPE_ARGLIST std::ostream::char_type, std::ostream::traits_type
		decltype(std::flush<TYPE_ARGLIST>)& flush = std::endl<TYPE_ARGLIST>;
	#undef TYPE_ARGLIST

	Log log = Log(std::cout);
	Log logGeneral() { return log(LOG_GENERAL); }
	Log logDebug() { return log(LOG_DEBUG); }
	Log logError() { return log(LOG_ERROR); }
	Log logAlloc() { return log(LOG_ALLOC); }
	Log logTime() { return log(LOG_TIME) << " [" << get_time() << "ns] "; }
	Log logVkDebug() { return log(LOG_VK_DEBUG); }
	Log logVkEvent() { return log(LOG_VK_EVENT); }
	Log logVkError() { return log(LOG_VK_ERROR); }

	#ifndef NDEBUG
		AllocTracker alloc_tracker;
	#endif

	Identity::value_t identity_counter = 1;


	Log& endl(Log& os) {
		if(os.currentLevelEnabled())  os.newline();
		return os;
	}


	double sleep_s(double s) {
		using Ns = std::chrono::nanoseconds;
		auto ns = Ns(
			static_cast<Ns::rep>(s * 1000000000.0));
		using Clock = std::chrono::steady_clock;
		auto now = std::chrono::time_point_cast<Ns>(Clock::now());
		std::this_thread::sleep_for(ns);
		return (
			now - std::chrono::time_point_cast<Ns>(Clock::now())
		).count() / 1000000000.0;
	}


	std::string read_stream(std::istream& src) {
		using std::ios_base;
		char buffer[FILE_BUFFER_SIZE];
		ios_base::iostate iostate_old = src.exceptions();
		src.exceptions(ios_base::badbit);
		std::string r;
		do {
			src.read(buffer, FILE_BUFFER_SIZE);
			r.insert(r.end(), buffer, buffer + src.gcount());
		} while(src);
		src.exceptions(iostate_old);
		return r;
	}

}



// AllocTracker class
#ifndef NDEBUG
namespace util {

	constexpr std::string_view ALLOC_1 = " loose allocation of ";
	constexpr std::string_view ALLOC_N = " loose allocations of ";
	constexpr std::string_view DEALLOC_1 = " unmatched dellocation of ";
	constexpr std::string_view DEALLOC_N = " unmatched dellocations of ";


	AllocTracker::~AllocTracker() {
		for(auto& alloc : _allocs) {
			if(alloc.second > 0) {
				util::logDebug()
					<< alloc.second << (alloc.second > +1? ALLOC_N : ALLOC_1)
					<< '"' << alloc.first << '"' << util::endl;
			} else
			if(alloc.second < 0) {
				util::logDebug()
					<< alloc.second << (alloc.second < -1? DEALLOC_N : DEALLOC_1)
					<< '"' << alloc.first << '"' << util::endl;
			}
		}
	}


	void AllocTracker::alloc(const std::string& nm, unsigned n) {
		util::logAlloc() << count_allocs(nm, n, '+') << util::endl;
		_allocs[nm] += n;
	}

	void AllocTracker::alloc(const std::string& nm, std::string what, unsigned n) {
		alloc(nm + " # " + what, n);
	}


	void AllocTracker::dealloc(const std::string& nm, unsigned n) {
		util::logAlloc() << count_allocs(nm, n, '-') << util::endl;
		_allocs[nm] -= n;
	}

	void AllocTracker::dealloc(const std::string& nm, std::string what, unsigned n) {
		alloc(nm + " # " + what, n);
	}

}
#endif



// Log class
namespace util {

	Log::Log(std::ostream& target):
			_target(target),
			_line_beg(true),
			_current(LOG_GENERAL),
			_timer(get_time())
	{ }


	Log& Log::operator()(level_e lvl) {
		_current = lvl;
		return *this;
	}


	Log& Log::setLevel(level_e lvl, bool val) {
		lvl = static_cast<level_e>(1 << lvl);
		bool enabled = (lvl & _enabled_levels) != 0;
		_enabled_levels =
			((_enabled_levels ^ lvl) * (val != enabled)) |
			(_enabled_levels * (val == enabled));
		return *this;
	}


	Log& Log::resetTimer() {
		_timer = get_time();
		return *this;
	}


	Log& Log::operator<<(Log& (*manipulator)(Log&)) {
		return manipulator(*this); }


	std::string_view Log::level_str(level_e lvl) {
		#define MK_CASE(_NM) case _NM: return #_NM+4;
		switch(lvl) {
			MK_CASE(LOG_GENERAL)
			MK_CASE(LOG_DEBUG)
			MK_CASE(LOG_ERROR)
			MK_CASE(LOG_ALLOC)
			MK_CASE(LOG_VK_DEBUG)
			MK_CASE(LOG_VK_EVENT)
			MK_CASE(LOG_VK_ERROR)
			default:  return "<invalid>";
		}
		#undef MK_CASE
	}

}



// TimeGateNs class
namespace util {
#define NOW_NS \
	std::chrono::time_point_cast< \
		std::chrono::duration<decltype(TimeGateNs::_last), std::nano>, \
		std::chrono::steady_clock \
	>(std::chrono::steady_clock::now()).time_since_epoch().count()

	TimeGateNs::TimeGate(): _last(NOW_NS) { }


	bool TimeGateNs::set(precision_t x) {
		precision_t now = NOW_NS;
		precision_t r = (now >= _last + x);
		_last = (now * r) | (_last * !r);
		return r;
	}


	bool TimeGateNs::forward(precision_t x, precision_t y) {
		precision_t r = (NOW_NS >= _last + x);
		_last = ((_last + (x+y)) * r) | (_last * !r);
		return r;
	}


	bool TimeGateNs::check(precision_t x) const {
		return NOW_NS >= _last + x;
	}


	TimeGateNs::precision_t TimeGateNs::now() const {
		return _last;
	}

#undef NOW_NS
}



// Identity class
namespace util {

	Identity::Identity(): _value(identity_counter++) { }
	Identity::Identity(const Identity&): _value(identity_counter++) { }
	Identity::Identity(Identity&& mov): _value(std::move(mov._value)) { }

	Identity& Identity::operator=(const Identity&) { return *this; }
	Identity& Identity::operator=(Identity&& mov) { _value = std::move(mov._value); return *this; }

}
