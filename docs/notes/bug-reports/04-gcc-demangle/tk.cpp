template <class T> concept C = sizeof(T) > 1;

template <C T> void f(T) {}
template void f<int>(int);

template <class... A> struct S {
	template <C F> S(F &&) {}
};
void use() { S<int> s{0}; }

template <class T, int (*)[sizeof(T)] = nullptr> void g(T) {}
template void g<int>(int);

template <bool B, class T = void> struct enable_if {};
template <class T> struct enable_if<true, T> { using type = T; };
template <class T, typename enable_if<(sizeof(T) > 1), int>::type = 0> void h(T) {}
template void h<int>(int);

auto lam = []<class T>(T) {};
void use2() { lam(1); }
