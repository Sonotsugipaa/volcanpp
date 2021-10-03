/* This header provides small utility functions and types that do
 * not deserve their own header, and that are small enought not to
 * warrant being in a all-in-one package.
 *
 * If the UTIL_INLINE_ONLY macro is defined, declarations that
 * depend on externally defined symbols are omitted;
 * if the NO_UTIL_MACROS is defined, this header shall not
 * define macros to be used outside of it. */

#pragma once

#include <string>

// Only <string> is required for inline utils
#ifndef UTIL_INLINE_ONLY
	#include <istream>
	#include <ratio>
	#include <map>
#endif



namespace perf { class PerfTracker; }
namespace perf::nop { class PerfTracker; }



#ifndef NO_UTIL_MACROS

	/* The following macro is a time-saver for creating private fields
	 * that can be accessed (and eventually modified) in a const-correct
	 * way, while skipping a lot of boilerplate code.
	 * It has a long name, but feel free to define abbreviations for it.
	 *
	 * Practical example:
	 * >   #define MK CLASS_UTIL_MK_PRIVATE_FIELD
	 * >   struct MyStruct {
	 * >      MK(std::vector<int>, _nums, std::vector<int>&, numbers)
	 * >   public:
	 * >      MyStruct(): _nums{1,2,3} { }
	 * >   };
	 * >   int my_func(MyStruct& s) {
	 * >      return s.numbers()[1];
	 * >   } */
	#define CLASS_UTIL_MK_PRIVATE_FIELD(FIELDTYPE,FIELDNAME,RETURNTYPE,METHODNAME) \
				private: FIELDTYPE FIELDNAME; \
				public: \
					RETURNTYPE METHODNAME() { return FIELDNAME; } \
					const RETURNTYPE METHODNAME() const { return FIELDNAME; } \
				private:


	#define ENUM_STR_CASE(_ENUM_STR) case _ENUM_STR: return #_ENUM_STR;

#endif



namespace util {

	/* Template prototype for getting the string of an enumeration;
	 * the definitions may be written using the ENUM_STR_CASE utility macro. */
	template<typename EnumType, typename StringType = std::string>
	StringType enum_str(EnumType);

	#ifndef UTIL_INLINE_ONLY

		#ifdef ENABLE_PERF_TRACKER
			using perf::PerfTracker;
		#else
			using perf::nop::PerfTracker;
		#endif
		extern util::PerfTracker& perfTracker;


		/** Sleeps for the given amount of time (in seconds),
		 * then returns how much time passed. */
		double sleep_s(double s);


		/** Empties a stream, inserting it into a string. */
		std::string read_stream(std::istream&);


		#ifdef NDEBUG
			class AllocTracker {
			public:
				inline void alloc(const std::string&, unsigned = 1) { }
				inline void alloc(const std::string&, std::string, unsigned _ = 1) { }
				inline void dealloc(const std::string&, unsigned = 1) { }
				inline void dealloc(const std::string&, std::string, unsigned _ = 1) { }
			};

			inline AllocTracker alloc_tracker = { };
		#else
			class AllocTracker {
			private:
				std::map<std::string, int> _allocs;

			public:
				AllocTracker() = default;
				~AllocTracker();

				void alloc(const std::string&, unsigned count = 1);
				void alloc(const std::string&, std::string, unsigned count = 1);
				void dealloc(const std::string&, unsigned count = 1);
				void dealloc(const std::string&, std::string, unsigned count = 1);
			};

			extern AllocTracker alloc_tracker;
		#endif


		extern decltype(std::flush<std::ostream::char_type, std::ostream::traits_type>)&
			flush;

		using level_t = unsigned;
		enum level_e : level_t {
			LOG_GENERAL = 0,
			LOG_DEBUG, LOG_ERROR, LOG_ALLOC,
			LOG_TIME,
			LOG_VK_DEBUG, LOG_VK_EVENT, LOG_VK_ERROR
		};

		class Log {
		private:
			std::ostream& _target;
			bool _line_beg;
			level_e _current;
			uint_fast64_t _timer;

			level_t _enabled_levels =
				(1 << LOG_GENERAL) |
				(1 << LOG_VK_ERROR);

		public:
			Log(std::ostream&);

			Log& operator()(level_e);

			Log& setLevel(level_e, bool do_enable);

			Log& resetTimer();

			template<typename T>
			Log& operator<<(T arg) {
				if(((1 << _current) & _enabled_levels) != 0) {
					if(_line_beg) {
						_target << '[' << level_str(_current) << "] ";
						_line_beg = false;
					}
					_target << arg;
				}
				return *this;
			}

			Log& operator<<(Log& (*)(Log&));

			Log& newline() {
				_line_beg = true; _target << std::endl;
				return *this;
			}

			bool currentLevelEnabled() const { return ((1 << _current) & _enabled_levels) != 0; }

			static std::string_view level_str(level_e);
		};

		Log& endl(Log&);

		extern Log log;
		Log logGeneral();
		Log logDebug();
		Log logError();
		Log logAlloc();
		Log logTime();
		Log logVkDebug();
		Log logVkEvent();
		Log logVkError();


		/** @brief A utility class for checking whether arbitrary amounts
		 * of time have passed between events.
		 * @tparam Period The seconds/unit ratio */
		template<typename Period>
		class TimeGate;

		template<>
		class TimeGate<std::nano> {
		public:
			using precision_t = uint64_t;
			using period_t = std::nano;

		protected:
			precision_t _last;

		public:
			TimeGate();

			/** Check if X nanoseconds have passed;
			 * if so, advance the internal counter by X+Y. */
			bool forward(decltype(_last) x, decltype(_last) y = 0);

			/** Check if X nanoseconds have passed;
			 * if so, set the internal counter to the current time. */
			bool set(decltype(_last) x = 0);

			/** Check if X nanoseconds have passed; the TimeGate's
			 * internal counter is not altered. */
			bool check(decltype(_last) x) const;

			/** Returns an arbitrary number, so that the return values of two
			 * consecutive calls will be the approximate difference in time
			 * between the first call and the second one expressed in
			 * nanoseconds. The returned value is always the same for multiple
			 * calls before another non-const member function is called - thus,
			 * this function is reentrant.
			 *
			 * No guarantee is made on the point of reference of the returned
			 * values, except that it is consistent throughout the object's
			 * lifetime.
			 *
			 * @returns The current value of the internal counter. */
			precision_t now() const;
		};

		using TimeGateNs = TimeGate<std::nano>;

		template<typename Period>
		class TimeGate : TimeGateNs {
		private:
			using _convert_p_t = std::ratio_divide<TimeGateNs::period_t, Period>;

		public:
			using period_t = Period;

			TimeGate(): TimeGateNs() { }

			/** Check if X time units have passed;
			 * if so, advance the internal counter by X+Y. */
			bool forward(decltype(_last) x, decltype(_last) y = 0) {
				x = x * _convert_p_t::den / _convert_p_t::num;
				y = y * _convert_p_t::den / _convert_p_t::num;
				return TimeGateNs::forward(x, y);
			}

			/** Check if X time units have passed;
			 * if so, set the internal counter to the current time. */
			bool set(decltype(_last) x = 0) {
				x = x * _convert_p_t::den / _convert_p_t::num;
				return TimeGateNs::set(x);
			}

			/** Check if X time units have passed; the TimeGate's
			 * internal counter is not altered. */
			bool check(decltype(_last) x) const {
				x = x * _convert_p_t::den / _convert_p_t::num;
				return TimeGateNs::check(x);
			}

			/** Returns an arbitrary number, so that the return values of two
			 * consecutive calls will be the approximate difference in time
			 * between the first call and the second one expressed in
			 * time units. The returned value is always the same for multiple
			 * calls before another non-const member function is called - thus,
			 * this function is reentrant.
			 *
			 * No guarantee is made on the point of reference of the returned
			 * values, except that it is consistent throughout the object's
			 * lifetime.
			 *
			 * @returns The current value of the internal counter. */
			precision_t now() const {
				return (TimeGateNs::now() * _convert_p_t::num) / _convert_p_t::den;
			}
		};


		/* Generates a new number when it's constructed,
		* starting from 1 and increasing every time;
		* mainly used for debugging. */
		class Identity {
		public:
			using value_t = uint64_t;

		private:
			value_t _value;

		public:
			Identity();
			Identity(const Identity&);

			/** The move constructor does NOT create a new Identity.
			 * Logically, anything that holds a non-mutable Identity should
			 * be considered invalid when moved from, and should not be reused
			 * before the Identity is reconstructed. */
			Identity(Identity&&);

			Identity& operator=(const Identity&);

			/** The move assignment operator does NOT create a new Identity.
			 * Logically, anything that holds a non-mutable Identity should
			 * be considered invalid when moved from, and should not be reused
			 * before the Identity is reconstructed. */
			Identity& operator=(Identity&&);

			constexpr operator value_t() const { return _value; }
			operator value_t() { return _value; }
		};

		#ifndef NDEBUG
			/** An alias for util::Identity, only defined if the
			* NDEBUG macro is not. Useful for temporary printf-style
			* debugging, where the code should fail to compile
			* for release builds when using member functions. */
			using DebugIdentity = Identity;
		#else
			/** An alias for util::Identity, only defined if the
			* NDEBUG macro is not. Useful for temporary printf-style
			* debugging, where the code should fail to compile
			* for release builds when using member functions. */
			struct DebugIdentity { };
		#endif

	#endif

}
