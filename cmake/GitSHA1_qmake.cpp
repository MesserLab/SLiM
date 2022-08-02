// For setting up the Git commit SHA-1 as a global
// This is used when building under qmake

extern const char g_GIT_SHA1[];

#ifdef GIT_SHA1
const char g_GIT_SHA1[] = GIT_SHA1;
#else
#warning No GIT_SHA1 definition supplied by qmake; check QtSLiM.pro
const char g_GIT_SHA1[] = "unknown (qmake has not defined GIT_SHA1)";
#endif
