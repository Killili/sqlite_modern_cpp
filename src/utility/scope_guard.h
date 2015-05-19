#include <functional>

namespace sqlite {
	namespace utility {
		class scope_guard {
		public:
			template<class Callable>
			scope_guard(Callable && undo_func): f(std::forward<Callable>(undo_func)) {}

			~scope_guard() {
				if(f) f();
			}

			/** Same as commit, prevent the undo */
			void dismiss() throw() {
				f = nullptr;
			}

		private:
			std::function<void()> f;
		};
	}
}