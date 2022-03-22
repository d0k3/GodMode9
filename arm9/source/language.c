#define STRING(what, def) const char *STR_##what = def;
#include "language.en.inl"
#undef STRING

// TODO read from file
