#pragma once

#include <cstdint>
#include <vector>



namespace perf {

	class PerfTracker {
	public:
		using utime_t = std::uintmax_t;
		using stime_t = std::make_signed_t<utime_t>;

		struct State {
			const char* id;
			utime_t time;
		};

		struct Record {
			const char* id;
			utime_t avgTime;
			size_t count;
		};

	private:
		std::vector<Record> records_;
		mutable size_t recordsHintIdx_; // Used to hint where to search first in the records vector

	public:
		double movingAverageDecay = 0.5;

		static void resetRuntimeEpoch();

		PerfTracker();
		PerfTracker(const PerfTracker&) = default;
		PerfTracker(PerfTracker&&) = default;
		PerfTracker& operator=(const PerfTracker&) = default;
		PerfTracker& operator=(PerfTracker&&) = default;

		State startTimer(const char*);
		void stopTimer(State&);

		template<typename fn_t, typename... args_t>
		void measure(const char* id, fn_t fn, args_t... args) {
			auto timer = startTimer(id);
			fn(args...);
			stopTimer(timer);
		}

		template<typename fn_t, typename... args_t>
		void measure(fn_t fn, args_t... args) {
			auto timer = startTimer();
			fn(args...);
			stopTimer(timer);
		}

		void reset();

		utime_t ns(const char*) const noexcept;

		utime_t us(const char* id) const noexcept { return ns(id) / 1000; }
		utime_t ms(const char* id) const noexcept { return ns(id) / 1000000; }

		State startTimer() { return startTimer(""); }
		utime_t ns() const noexcept { return ns(""); }
		utime_t us() const noexcept { return us(""); }
		utime_t ms() const noexcept { return ms(""); }

		PerfTracker operator|(const PerfTracker& rh);
		PerfTracker& operator|=(const PerfTracker& rh);
	};


	/** Defines a no-op mirror of PerfTracker, useful for disabling
	 * its usage at compile time with trivial preprocessor directives. */
	namespace nop {

		class PerfTracker {
		public:
			class State { };

			using utime_t = std::uintmax_t;
			using stime_t = std::make_signed_t<utime_t>;

			static void resetRuntimeEpoch() { }

			PerfTracker() { }
			PerfTracker(const PerfTracker&) = default;
			PerfTracker(PerfTracker&&) = default;
			PerfTracker& operator=(const PerfTracker&) = default;
			PerfTracker& operator=(PerfTracker&&) = default;

			State startTimer(const char*) { return { }; }
			void stopTimer(State&) { }

			template<typename fn_t, typename... args_t>
			void measure(fn_t fn, args_t... args) { fn(args...); }

			template<typename fn_t, typename... args_t>
			void measure(const char*, fn_t fn, args_t... args) { fn(args...); }

			utime_t ns(const char*) const noexcept;

			utime_t us(const char*) const noexcept { return { }; }
			utime_t ms(const char*) const noexcept { return { }; }

			State startTimer() { return { }; }
			utime_t ns() const noexcept { return { }; }
			utime_t us() const noexcept { return { }; }
			utime_t ms() const noexcept { return { }; }

			PerfTracker operator|(const PerfTracker&) { return { }; }
			PerfTracker& operator|=(const PerfTracker&) { return *this; }
		};

	};


	template<bool usage> struct SelectPerfTrackerProxy;
	template<bool usage> using SelectPerfTracker = SelectPerfTrackerProxy<usage>::type;

	template<> struct SelectPerfTrackerProxy<true> { using type = PerfTracker; };
	template<> struct SelectPerfTrackerProxy<false> { using type = nop::PerfTracker; };

}
