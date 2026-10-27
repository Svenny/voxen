namespace { int counter; }
[[gnu::noinline]] static int bump() { return ++counter; }
int get() { return bump() + counter; }
